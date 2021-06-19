/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Transport Layer Security
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(_WIN32) || defined(ANDROID) || defined(__APPLE__)
#include "freerdp_config.h"
#endif

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/sspi.h>
#include <winpr/ssl.h>

#include <winpr/stream.h>
#include <freerdp/utils/ringbuffer.h>

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>
#include "../core/tcp.h"
#include "opensslcompat.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#define TAG FREERDP_TAG("crypto")

/**
 * Earlier Microsoft iOS RDP clients have sent a null or even double null
 * terminated hostname in the SNI TLS extension.
 * If the length indicator does not equal the hostname strlen OpenSSL
 * will abort (see openssl:ssl/t1_lib.c).
 * Here is a tcpdump segment of Microsoft Remote Desktop Client Version
 * 8.1.7 running on an iPhone 4 with iOS 7.1.2 showing the transmitted
 * SNI hostname TLV blob when connection to server "abcd":
 * 00                  name_type 0x00 (host_name)
 * 00 06               length_in_bytes 0x0006
 * 61 62 63 64 00 00   host_name "abcd\0\0"
 *
 * Currently the only (runtime) workaround is setting an openssl tls
 * extension debug callback that sets the SSL context's servername_done
 * to 1 which effectively disables the parsing of that extension type.
 *
 * Nowadays this workaround is not required anymore but still can be
 * activated by adding the following define:
 *
 * #define MICROSOFT_IOS_SNI_BUG
 */

struct _BIO_RDP_TLS
{
	SSL* ssl;
	CRITICAL_SECTION lock;
};
typedef struct _BIO_RDP_TLS BIO_RDP_TLS;

static int tls_verify_certificate(rdpTls* tls, CryptoCert cert, const char* hostname, UINT16 port);
static void tls_print_certificate_name_mismatch_error(const char* hostname, UINT16 port,
                                                      const char* common_name, char** alt_names,
                                                      int alt_names_count);
static void tls_print_certificate_error(const char* hostname, UINT16 port, const char* fingerprint,
                                        const char* hosts_file);

int tls_connect(rdpTls* tls, BIO* underlying)
{
	return TRUE;
}

BOOL tls_accept(rdpTls* tls, BIO* underlying, rdpSettings* settings)
{
	return TRUE;
}

BOOL tls_send_alert(rdpTls* tls)
{
	if (!tls)
		return FALSE;

	if (!tls->ssl)
		return TRUE;

		/**
		 * FIXME: The following code does not work on OpenSSL > 1.1.0 because the
		 *        SSL struct is opaqe now
		 */
#if (!defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER < 0x10100000L)) || \
    (defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER <= 0x2080300fL))

	if (tls->alertDescription != TLS_ALERT_DESCRIPTION_CLOSE_NOTIFY)
	{
		/**
		 * OpenSSL doesn't really expose an API for sending a TLS alert manually.
		 *
		 * The following code disables the sending of the default "close notify"
		 * and then proceeds to force sending a custom TLS alert before shutting down.
		 *
		 * Manually sending a TLS alert is necessary in certain cases,
		 * like when server-side NLA results in an authentication failure.
		 */
		SSL_SESSION* ssl_session = SSL_get_session(tls->ssl);
		SSL_CTX* ssl_ctx = SSL_get_SSL_CTX(tls->ssl);
		SSL_set_quiet_shutdown(tls->ssl, 1);

		if ((tls->alertLevel == TLS_ALERT_LEVEL_FATAL) && (ssl_session))
			SSL_CTX_remove_session(ssl_ctx, ssl_session);

		tls->ssl->s3->alert_dispatch = 1;
		tls->ssl->s3->send_alert[0] = tls->alertLevel;
		tls->ssl->s3->send_alert[1] = tls->alertDescription;

		if (tls->ssl->s3->wbuf.left == 0)
			tls->ssl->method->ssl_dispatch_alert(tls->ssl);
	}

#endif
	return TRUE;
}

int tls_write_all(rdpTls* tls, const BYTE* data, int length)
{
	int status;
	int offset = 0;
	BIO* bio = tls->bio;

	while (offset < length)
	{
		status = BIO_write(bio, &data[offset], length - offset);

		if (status > 0)
		{
			offset += status;
		}
		else
		{
			if (!BIO_should_retry(bio))
				return -1;

			if (BIO_write_blocked(bio))
				status = BIO_wait_write(bio, 100);
			else if (BIO_read_blocked(bio))
				status = BIO_wait_read(bio, 100);
			else
				USleep(100);

			if (status < 0)
				return -1;
		}
	}

	return length;
}

int tls_set_alert_code(rdpTls* tls, int level, int description)
{
	tls->alertLevel = level;
	tls->alertDescription = description;
	return 0;
}

static BOOL tls_match_hostname(const char* pattern, const size_t pattern_length,
                               const char* hostname)
{
	if (strlen(hostname) == pattern_length)
	{
		if (_strnicmp(hostname, pattern, pattern_length) == 0)
			return TRUE;
	}

	if ((pattern_length > 2) && (pattern[0] == '*') && (pattern[1] == '.') &&
	    ((strlen(hostname)) >= pattern_length))
	{
		const char* check_hostname = &hostname[strlen(hostname) - pattern_length + 1];

		if (_strnicmp(check_hostname, &pattern[1], pattern_length - 1) == 0)
		{
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL is_redirected(rdpTls* tls)
{
	rdpSettings* settings = tls->settings;

	if (LB_NOREDIRECT & settings->RedirectionFlags)
		return FALSE;

	return settings->RedirectionFlags != 0;
}

static BOOL is_accepted(rdpTls* tls, const BYTE* pem, size_t length)
{
	rdpSettings* settings = tls->settings;
	char* AccpetedKey;
	UINT32 AcceptedKeyLength;

	if (tls->isGatewayTransport)
	{
		AccpetedKey = settings->GatewayAcceptedCert;
		AcceptedKeyLength = settings->GatewayAcceptedCertLength;
	}
	else if (is_redirected(tls))
	{
		AccpetedKey = settings->RedirectionAcceptedCert;
		AcceptedKeyLength = settings->RedirectionAcceptedCertLength;
	}
	else
	{
		AccpetedKey = settings->AcceptedCert;
		AcceptedKeyLength = settings->AcceptedCertLength;
	}

	if (AcceptedKeyLength > 0)
	{
		if (AcceptedKeyLength == length)
		{
			if (memcmp(AccpetedKey, pem, AcceptedKeyLength) == 0)
				return TRUE;
		}
	}

	if (tls->isGatewayTransport)
	{
		free(settings->GatewayAcceptedCert);
		settings->GatewayAcceptedCert = NULL;
		settings->GatewayAcceptedCertLength = 0;
	}
	else if (is_redirected(tls))
	{
		free(settings->RedirectionAcceptedCert);
		settings->RedirectionAcceptedCert = NULL;
		settings->RedirectionAcceptedCertLength = 0;
	}
	else
	{
		free(settings->AcceptedCert);
		settings->AcceptedCert = NULL;
		settings->AcceptedCertLength = 0;
	}

	return FALSE;
}

static BOOL accept_cert(rdpTls* tls, const BYTE* pem, UINT32 length)
{
	rdpSettings* settings = tls->settings;
	char* dupPem = _strdup((const char*)pem);

	if (!dupPem)
		return FALSE;

	if (tls->isGatewayTransport)
	{
		settings->GatewayAcceptedCert = dupPem;
		settings->GatewayAcceptedCertLength = length;
	}
	else if (is_redirected(tls))
	{
		settings->RedirectionAcceptedCert = dupPem;
		settings->RedirectionAcceptedCertLength = length;
	}
	else
	{
		settings->AcceptedCert = dupPem;
		settings->AcceptedCertLength = length;
	}

	return TRUE;
}

static BOOL tls_extract_pem(CryptoCert cert, BYTE** PublicKey, DWORD* PublicKeyLength)
{
	BIO* bio;
	int status, count, x;
	size_t offset;
	size_t length = 0;
	BOOL rc = FALSE;
	BYTE* pemCert = NULL;

	if (!PublicKey || !PublicKeyLength)
		return FALSE;

	*PublicKey = NULL;
	*PublicKeyLength = 0;
	/**
	 * Don't manage certificates internally, leave it up entirely to the external client
	 * implementation
	 */
	bio = BIO_new(BIO_s_mem());

	if (!bio)
	{
		WLog_ERR(TAG, "BIO_new() failure");
		return FALSE;
	}

	status = PEM_write_bio_X509(bio, cert->px509);

	if (status < 0)
	{
		WLog_ERR(TAG, "PEM_write_bio_X509 failure: %d", status);
		goto fail;
	}

	if (cert->px509chain)
	{
		count = sk_X509_num(cert->px509chain);
		for (x = 0; x < count; x++)
		{
			X509* c = sk_X509_value(cert->px509chain, x);
			status = PEM_write_bio_X509(bio, c);
			if (status < 0)
			{
				WLog_ERR(TAG, "PEM_write_bio_X509 failure: %d", status);
				goto fail;
			}
		}
	}

	offset = 0;
	length = 2048;
	pemCert = (BYTE*)malloc(length + 1);

	if (!pemCert)
	{
		WLog_ERR(TAG, "error allocating pemCert");
		goto fail;
	}

	status = BIO_read(bio, pemCert, length);

	if (status < 0)
	{
		WLog_ERR(TAG, "failed to read certificate");
		goto fail;
	}

	offset += (size_t)status;

	while (offset >= length)
	{
		int new_len;
		BYTE* new_cert;
		new_len = length * 2;
		new_cert = (BYTE*)realloc(pemCert, new_len + 1);

		if (!new_cert)
			goto fail;

		length = new_len;
		pemCert = new_cert;
		status = BIO_read(bio, &pemCert[offset], length - offset);

		if (status < 0)
			break;

		offset += status;
	}

	if (status < 0)
	{
		WLog_ERR(TAG, "failed to read certificate");
		goto fail;
	}

	length = offset;
	pemCert[length] = '\0';
	*PublicKey = pemCert;
	*PublicKeyLength = length;
	rc = TRUE;
fail:

	if (!rc)
		free(pemCert);

	BIO_free_all(bio);
	return rc;
}

int tls_verify_certificate(rdpTls* tls, CryptoCert cert, const char* hostname, UINT16 port)
{
	int match;
	int index;
	DWORD length;
	BOOL certificate_status;
	char* common_name = NULL;
	int common_name_length = 0;
	char** dns_names = 0;
	int dns_names_count = 0;
	int* dns_names_lengths = NULL;
	int verification_status = -1;
	BOOL hostname_match = FALSE;
	rdpCertificateData* certificate_data = NULL;
	freerdp* instance = (freerdp*)tls->settings->instance;
	BYTE* pemCert = NULL;
	DWORD flags = VERIFY_CERT_FLAG_NONE;

	if (!tls_extract_pem(cert, &pemCert, &length))
		goto end;

	/* Check, if we already accepted this key. */
	if (is_accepted(tls, pemCert, length))
	{
		verification_status = 1;
		goto end;
	}

	if (tls->isGatewayTransport || is_redirected(tls))
		flags |= VERIFY_CERT_FLAG_LEGACY;

	if (tls->isGatewayTransport)
		flags |= VERIFY_CERT_FLAG_GATEWAY;

	if (is_redirected(tls))
		flags |= VERIFY_CERT_FLAG_REDIRECT;

	/* Certificate management is done by the application */
	if (tls->settings->ExternalCertificateManagement)
	{
		if (instance->VerifyX509Certificate)
			verification_status =
			    instance->VerifyX509Certificate(instance, pemCert, length, hostname, port, flags);
		else
			WLog_ERR(TAG, "No VerifyX509Certificate callback registered!");

		if (verification_status > 0)
			accept_cert(tls, pemCert, length);
		else if (verification_status < 0)
		{
			WLog_ERR(TAG, "VerifyX509Certificate failed: (length = %d) status: [%d] %s", length,
			         verification_status, pemCert);
			goto end;
		}
	}
	/* ignore certificate verification if user explicitly required it (discouraged) */
	else if (tls->settings->IgnoreCertificate)
		verification_status = 1; /* success! */
	else if (!tls->isGatewayTransport && (tls->settings->AuthenticationLevel == 0))
		verification_status = 1; /* success! */
	else
	{
		/* if user explicitly specified a certificate name, use it instead of the hostname */
		if (!tls->isGatewayTransport && tls->settings->CertificateName)
			hostname = tls->settings->CertificateName;

		/* attempt verification using OpenSSL and the ~/.freerdp/certs certificate store */
		certificate_status = x509_verify_certificate(cert, tls->certificate_store->path);
		/* verify certificate name match */
		certificate_data = crypto_get_certificate_data(cert->px509, hostname, port);
		/* extra common name and alternative names */
		common_name = crypto_cert_subject_common_name(cert->px509, &common_name_length);
		dns_names = crypto_cert_get_dns_names(cert->px509, &dns_names_count, &dns_names_lengths);

		/* compare against common name */

		if (common_name)
		{
			if (tls_match_hostname(common_name, common_name_length, hostname))
				hostname_match = TRUE;
		}

		/* compare against alternative names */

		if (dns_names)
		{
			for (index = 0; index < dns_names_count; index++)
			{
				if (tls_match_hostname(dns_names[index], dns_names_lengths[index], hostname))
				{
					hostname_match = TRUE;
					break;
				}
			}
		}

		/* if the certificate is valid and the certificate name matches, verification succeeds */
		if (certificate_status && hostname_match)
			verification_status = 1; /* success! */

		if (!hostname_match)
			flags |= VERIFY_CERT_FLAG_MISMATCH;

		/* verification could not succeed with OpenSSL, use known_hosts file and prompt user for
		 * manual verification */
		if (!certificate_status || !hostname_match)
		{
			char* issuer;
			char* subject;
			char* fingerprint;
			DWORD accept_certificate = 0;
			issuer = crypto_cert_issuer(cert->px509);
			subject = crypto_cert_subject(cert->px509);
			fingerprint = crypto_cert_fingerprint(cert->px509);
			/* search for matching entry in known_hosts file */
			match = certificate_data_match(tls->certificate_store, certificate_data);

			if (match == 1)
			{
				/* no entry was found in known_hosts file, prompt user for manual verification */
				if (!hostname_match)
					tls_print_certificate_name_mismatch_error(hostname, port, common_name,
					                                          dns_names, dns_names_count);

				/* Automatically accept certificate on first use */
				if (tls->settings->AutoAcceptCertificate)
				{
					WLog_INFO(TAG, "No certificate stored, automatically accepting.");
					accept_certificate = 1;
				}
				else if (tls->settings->AutoDenyCertificate)
				{
					WLog_INFO(TAG, "No certificate stored, automatically denying.");
					accept_certificate = 0;
				}
				else if (instance->VerifyX509Certificate)
				{
					int rc = instance->VerifyX509Certificate(instance, pemCert, length, hostname,
					                                         port, flags);

					if (rc == 1)
						accept_certificate = 1;
					else if (rc > 1)
						accept_certificate = 2;
					else
						accept_certificate = 0;
				}
				else if (instance->VerifyCertificateEx)
				{
					accept_certificate = instance->VerifyCertificateEx(
					    instance, hostname, port, common_name, subject, issuer, fingerprint, flags);
				}
				else if (instance->VerifyCertificate)
				{
					WLog_WARN(TAG, "The VerifyCertificate callback is deprecated, migrate your "
					               "application to VerifyCertificateEx");
					accept_certificate = instance->VerifyCertificate(
					    instance, common_name, subject, issuer, fingerprint, !hostname_match);
				}
			}
			else if (match == -1)
			{
				char* old_subject = NULL;
				char* old_issuer = NULL;
				char* old_fingerprint = NULL;
				/* entry was found in known_hosts file, but fingerprint does not match. ask user to
				 * use it */
				tls_print_certificate_error(hostname, port, fingerprint,
				                            tls->certificate_store->file);

				if (!certificate_get_stored_data(tls->certificate_store, certificate_data,
				                                 &old_subject, &old_issuer, &old_fingerprint))
					WLog_WARN(TAG, "Failed to get certificate entry for %s:%d", hostname, port);

				if (tls->settings->AutoDenyCertificate)
				{
					WLog_INFO(TAG, "No certificate stored, automatically denying.");
					accept_certificate = 0;
				}
				else if (instance->VerifyX509Certificate)
				{
					const int rc =
					    instance->VerifyX509Certificate(instance, pemCert, length, hostname, port,
					                                    flags | VERIFY_CERT_FLAG_CHANGED);

					if (rc == 1)
						accept_certificate = 1;
					else if (rc > 1)
						accept_certificate = 2;
					else
						accept_certificate = 0;
				}
				else if (instance->VerifyChangedCertificateEx)
				{
					accept_certificate = instance->VerifyChangedCertificateEx(
					    instance, hostname, port, common_name, subject, issuer, fingerprint,
					    old_subject, old_issuer, old_fingerprint, flags | VERIFY_CERT_FLAG_CHANGED);
				}
				else if (instance->VerifyChangedCertificate)
				{
					WLog_WARN(TAG, "The VerifyChangedCertificate callback is deprecated, migrate "
					               "your application to VerifyChangedCertificateEx");
					accept_certificate = instance->VerifyChangedCertificate(
					    instance, common_name, subject, issuer, fingerprint, old_subject,
					    old_issuer, old_fingerprint);
				}

				free(old_subject);
				free(old_issuer);
				free(old_fingerprint);
			}
			else if (match == 0)
				accept_certificate = 2; /* success! */

			/* Save certificate or do a simple accept / reject */
			switch (accept_certificate)
			{
				case 1:

					/* user accepted certificate, add entry in known_hosts file */
					if (match < 0)
						verification_status =
						    certificate_data_replace(tls->certificate_store, certificate_data) ? 1
						                                                                       : -1;
					else
						verification_status =
						    certificate_data_print(tls->certificate_store, certificate_data) ? 1
						                                                                     : -1;

					break;

				case 2:
					/* user did accept temporaty, do not add to known hosts file */
					verification_status = 1;
					break;

				default:
					/* user did not accept, abort and do not add entry in known_hosts file */
					verification_status = -1; /* failure! */
					break;
			}

			free(issuer);
			free(subject);
			free(fingerprint);
		}

		if (verification_status > 0)
			accept_cert(tls, pemCert, length);
	}

end:
	certificate_data_free(certificate_data);
	free(common_name);

	if (dns_names)
		crypto_cert_dns_names_free(dns_names_count, dns_names_lengths, dns_names);

	free(pemCert);
	return verification_status;
}

void tls_print_certificate_error(const char* hostname, UINT16 port, const char* fingerprint,
                                 const char* hosts_file)
{
	WLog_ERR(TAG, "The host key for %s:%" PRIu16 " has changed", hostname, port);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!");
	WLog_ERR(TAG, "Someone could be eavesdropping on you right now (man-in-the-middle attack)!");
	WLog_ERR(TAG, "It is also possible that a host key has just been changed.");
	WLog_ERR(TAG, "The fingerprint for the host key sent by the remote host is %s", fingerprint);
	WLog_ERR(TAG, "Please contact your system administrator.");
	WLog_ERR(TAG, "Add correct host key in %s to get rid of this message.", hosts_file);
	WLog_ERR(TAG, "Host key for %s has changed and you have requested strict checking.", hostname);
	WLog_ERR(TAG, "Host key verification failed.");
}

void tls_print_certificate_name_mismatch_error(const char* hostname, UINT16 port,
                                               const char* common_name, char** alt_names,
                                               int alt_names_count)
{
	int index;
	assert(NULL != hostname);
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "@           WARNING: CERTIFICATE NAME MISMATCH!           @");
	WLog_ERR(TAG, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	WLog_ERR(TAG, "The hostname used for this connection (%s:%" PRIu16 ") ", hostname, port);
	WLog_ERR(TAG, "does not match %s given in the certificate:",
	         alt_names_count < 1 ? "the name" : "any of the names");
	WLog_ERR(TAG, "Common Name (CN):");
	WLog_ERR(TAG, "\t%s", common_name ? common_name : "no CN found in certificate");

	if (alt_names_count > 0)
	{
		assert(NULL != alt_names);
		WLog_ERR(TAG, "Alternative names:");

		for (index = 0; index < alt_names_count; index++)
		{
			assert(alt_names[index]);
			WLog_ERR(TAG, "\t %s", alt_names[index]);
		}
	}

	WLog_ERR(TAG, "A valid certificate for the wrong name should NOT be trusted!");
}

rdpTls* tls_new(rdpSettings* settings)
{
	rdpTls* tls;
	tls = (rdpTls*)calloc(1, sizeof(rdpTls));

	if (!tls)
		return NULL;

	tls->settings = settings;

	if (!settings->ServerMode)
	{
		tls->certificate_store = certificate_store_new(settings);

		if (!tls->certificate_store)
			goto out_free;
	}

	tls->alertLevel = TLS_ALERT_LEVEL_WARNING;
	tls->alertDescription = TLS_ALERT_DESCRIPTION_CLOSE_NOTIFY;
	return tls;
out_free:
	free(tls);
	return NULL;
}

void tls_free(rdpTls* tls)
{
	if (!tls)
		return;

	if (tls->ctx)
	{
		SSL_CTX_free(tls->ctx);
		tls->ctx = NULL;
	}

	/* tls->underlying is a stacked BIO under tls->bio.
	 * BIO_free_all will free recursivly. */
	BIO_free_all(tls->bio);
	tls->bio = NULL;
	tls->underlying = NULL;

	if (tls->PublicKey)
	{
		free(tls->PublicKey);
		tls->PublicKey = NULL;
	}

	if (tls->Bindings)
	{
		free(tls->Bindings->Bindings);
		free(tls->Bindings);
		tls->Bindings = NULL;
	}

	if (tls->certificate_store)
	{
		certificate_store_free(tls->certificate_store);
		tls->certificate_store = NULL;
	}

	free(tls);
}
