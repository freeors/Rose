/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Transport Layer
 *
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/utils/ringbuffer.h>

#include <openssl/bio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif /* _WIN32 */

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "tpkt.h"
#include "fastpath.h"
#include "transport.h"
#include "rdp.h"
#include "proxy.h"

#include <freerdp/server/cliprdr.h>

#define TAG FREERDP_TAG("core.transport")

#define BUFFER_SIZE 16384

#ifdef WITH_GSSAPI

#include <krb5.h>
#include <winpr/library.h>
static UINT32 transport_krb5_check_account(rdpTransport* transport, char* username, char* domain,
                                           char* passwd)
{
	krb5_error_code ret;
	krb5_context context = NULL;
	krb5_principal principal = NULL;
	char address[256];
	krb5_ccache ccache;
	krb5_init_creds_context ctx = NULL;
	_snprintf(address, sizeof(address), "%s@%s", username, domain);

	/* Create a krb5 library context */
	if ((ret = krb5_init_context(&context)) != 0)
		WLog_Print(transport->log, WLOG_ERROR, "krb5_init_context failed with error %d", (int)ret);
	else if ((ret = krb5_parse_name_flags(context, address, 0, &principal)) != 0)
		WLog_Print(transport->log, WLOG_ERROR, "krb5_parse_name_flags failed with error %d",
		           (int)ret);
	/* Find a credential cache with a specified client principal */
	else if ((ret = krb5_cc_cache_match(context, principal, &ccache)) != 0)
	{
		if ((ret = krb5_cc_default(context, &ccache)) != 0)
			WLog_Print(transport->log, WLOG_ERROR,
			           "krb5 failed to resolve credentials cache with error %d", (int)ret);
	}

	if (ret != KRB5KDC_ERR_NONE)
		goto out;
	/* Create a context for acquiring initial credentials */
	else if ((ret = krb5_init_creds_init(context, principal, NULL, NULL, 0, NULL, &ctx)) != 0)
	{
		WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_init returned error %d", (int)ret);
		goto out;
	}
	/* Set a password for acquiring initial credentials */
	else if ((ret = krb5_init_creds_set_password(context, ctx, passwd)) != 0)
	{
		WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_set_password returned error %d",
		           ret);
		goto out;
	}

	/* Acquire credentials using an initial credential context */
	ret = krb5_init_creds_get(context, ctx);
out:

	switch (ret)
	{
		case KRB5KDC_ERR_NONE:
			break;

		case KRB5_KDC_UNREACH:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: KDC unreachable");
			ret = FREERDP_ERROR_CONNECT_KDC_UNREACHABLE;
			break;

		case KRB5KRB_AP_ERR_BAD_INTEGRITY:
		case KRB5KRB_AP_ERR_MODIFIED:
		case KRB5KDC_ERR_PREAUTH_FAILED:
		case KRB5_GET_IN_TKT_LOOP:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: Password incorrect");
			ret = FREERDP_ERROR_AUTHENTICATION_FAILED;
			break;

		case KRB5KDC_ERR_KEY_EXP:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: Password has expired");
			ret = FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED;
			break;

		case KRB5KDC_ERR_CLIENT_REVOKED:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get: Password revoked");
			ret = FREERDP_ERROR_CONNECT_CLIENT_REVOKED;
			break;

		case KRB5KDC_ERR_POLICY:
			ret = FREERDP_ERROR_INSUFFICIENT_PRIVILEGES;
			break;

		default:
			WLog_Print(transport->log, WLOG_WARN, "krb5_init_creds_get");
			ret = FREERDP_ERROR_CONNECT_TRANSPORT_FAILED;
			break;
	}

	if (ctx)
		krb5_init_creds_free(context, ctx);

	krb5_free_context(context);
	return ret;
}
#endif /* WITH_GSSAPI */

static void transport_ssl_cb(SSL* ssl, int where, int ret)
{
	if (where & SSL_CB_ALERT)
	{
		rdpTransport* transport = (rdpTransport*)SSL_get_app_data(ssl);

		switch (ret)
		{
			case (SSL3_AL_FATAL << 8) | SSL_AD_ACCESS_DENIED:
			{
				if (!freerdp_get_last_error(transport->context))
				{
					WLog_Print(transport->log, WLOG_ERROR, "%s: ACCESS DENIED", __FUNCTION__);
					freerdp_set_last_error_log(transport->context,
					                           FREERDP_ERROR_AUTHENTICATION_FAILED);
				}
			}
			break;

			case (SSL3_AL_FATAL << 8) | SSL_AD_INTERNAL_ERROR:
			{
				if (transport->NlaMode)
				{
					UINT32 kret = 0;
#ifdef WITH_GSSAPI

					if ((strlen(transport->settings->Domain) != 0) &&
					    (strncmp(transport->settings->Domain, ".", 1) != 0))
					{
						kret = transport_krb5_check_account(
						    transport, transport->settings->Username, transport->settings->Domain,
						    transport->settings->Password);
					}
					else
#endif /* WITH_GSSAPI */
						kret = FREERDP_ERROR_CONNECT_PASSWORD_CERTAINLY_EXPIRED;

					freerdp_set_last_error_if_not(transport->context, kret);
				}

				break;

				case (SSL3_AL_WARNING << 8) | SSL3_AD_CLOSE_NOTIFY:
					break;

				default:
					WLog_Print(transport->log, WLOG_WARN,
					           "Unhandled SSL error (where=%d, ret=%d [%s, %s])", where, ret,
					           SSL_alert_type_string_long(ret), SSL_alert_desc_string_long(ret));
					break;
			}
		}
	}
}

wStream* transport_send_stream_init(rdpTransport* transport, int size)
{
	wStream* s;

	if (!(s = StreamPool_Take(transport->ReceivePool, size)))
		return NULL;

	if (!Stream_EnsureCapacity(s, size))
	{
		Stream_Release(s);
		return NULL;
	}

	Stream_SetPosition(s, 0);
	return s;
}

BOOL transport_connect_rdp(rdpTransport* transport)
{
	/* RDP encryption */
	return TRUE;
}

BOOL transport_accept_rdp(rdpTransport* transport)
{
	/* RDP encryption */
	return TRUE;
}

BOOL transport_accept_tls(rdpTransport* transport)
{
	rdpSettings* settings = transport->settings;
/*
	if (!transport->tls)
		transport->tls = tls_new(transport->settings);
*/
	transport->layer = TRANSPORT_LAYER_TLS;

	// transport->frontBio = transport->tls->bio;
	return TRUE;
}

static SSIZE_T transport_read_layer(rdpTransport* transport, BYTE* data, size_t bytes)
{
	rdpContext* context = transport->context;
	// freerdp_peer* client = context->peer;
	rdpRdp* rdp = transport->context->rdp;

	SSIZE_T read = rdp->rose.rose_read_layer(context, data, bytes);
	if (read > 0) {
		rdp->inBytes += read;
	}
	return read;
}

/**
 * @brief Tries to read toRead bytes from the specified transport
 *
 * Try to read toRead bytes from the transport to the stream.
 * In case it was not possible to read toRead bytes 0 is returned. The stream is always advanced by
 * the number of bytes read.
 *
 * The function assumes that the stream has enough capacity to hold the data.
 *
 * @param[in] transport rdpTransport
 * @param[in] s wStream
 * @param[in] toRead number of bytes to read
 * @return < 0 on error; 0 if not enough data is available (non blocking mode); 1 toRead bytes read
 */
static SSIZE_T transport_read_layer_bytes(rdpTransport* transport, wStream* s, size_t toRead)
{
	SSIZE_T status;
	if (toRead > SSIZE_MAX)
		return 0;

	status = transport_read_layer(transport, Stream_Pointer(s), toRead);

	if (status <= 0)
		return status;

	Stream_Seek(s, (size_t)status);
	return status == (SSIZE_T)toRead ? 1 : 0;
}

/**
 * @brief Try to read a complete PDU (NLA, fast-path or tpkt) from the underlying transport.
 *
 * If possible a complete PDU is read, in case of non blocking transport this might not succeed.
 * Except in case of an error the passed stream will point to the last byte read (correct
 * position). When the pdu read is completed the stream is sealed and the pointer set to 0
 *
 * @param[in] transport rdpTransport
 * @param[in] s wStream
 * @return < 0 on error; 0 if not enough data is available (non blocking mode); > 0 number of
 * bytes of the *complete* pdu read
 */
int transport_read_pdu(rdpTransport* transport, wStream* s)
{
	int status;
	size_t position;
	size_t pduLength;
	BYTE* header;
	pduLength = 0;

	if (!transport)
		return -1;

	if (!s)
		return -1;

	position = Stream_GetPosition(s);

	/* Make sure there is enough space for the longest header within the stream */
	if (!Stream_EnsureCapacity(s, 4))
		return -1;

	/* Make sure at least two bytes are read for further processing */
	if (position < 2 && (status = transport_read_layer_bytes(transport, s, 2 - position)) != 1)
	{
		/* No data available at the moment */
		return status;
	}

	/* update position value for further checks */
	position = Stream_GetPosition(s);
	header = Stream_Buffer(s);

	if (transport->NlaMode)
	{
		/*
		 * In case NlaMode is set TSRequest package(s) are expected
		 * 0x30 = DER encoded data with these bits set:
		 * bit 6 P/C constructed
		 * bit 5 tag number - sequence
		 */
		if (header[0] == 0x30)
		{
			/* TSRequest (NLA) */
			if (header[1] & 0x80)
			{
				if ((header[1] & ~(0x80)) == 1)
				{
					/* check for header bytes already was readed in previous calls */
					if (position < 3 &&
					    (status = transport_read_layer_bytes(transport, s, 3 - position)) != 1)
						return status;

					pduLength = header[2];
					pduLength += 3;
				}
				else if ((header[1] & ~(0x80)) == 2)
				{
					/* check for header bytes already was readed in previous calls */
					if (position < 4 &&
					    (status = transport_read_layer_bytes(transport, s, 4 - position)) != 1)
						return status;

					pduLength = (header[2] << 8) | header[3];
					pduLength += 4;
				}
				else
				{
					WLog_Print(transport->log, WLOG_ERROR, "Error reading TSRequest!");
					return -1;
				}
			}
			else
			{
				pduLength = header[1];
				pduLength += 2;
			}
		}
	}
	else
	{
		if (header[0] == 0x03)
		{
			/* TPKT header */
			/* check for header bytes already was readed in previous calls */
			if (position < 4 &&
			    (status = transport_read_layer_bytes(transport, s, 4 - position)) != 1)
				return status;

			pduLength = (header[2] << 8) | header[3];

			/* min and max values according to ITU-T Rec. T.123 (01/2007) section 8 */
			if (pduLength < 7 || pduLength > 0xFFFF)
			{
				WLog_Print(transport->log, WLOG_ERROR, "tpkt - invalid pduLength: %" PRIdz,
				           pduLength);
				return -1;
			}
		}
		else
		{
			/* Fast-Path Header */
			if (header[1] & 0x80)
			{
				/* check for header bytes already was readed in previous calls */
				if (position < 3 &&
				    (status = transport_read_layer_bytes(transport, s, 3 - position)) != 1)
					return status;

				pduLength = ((header[1] & 0x7F) << 8) | header[2];
			}
			else
				pduLength = header[1];

			/*
			 * fast-path has 7 bits for length so the maximum size, including headers is 0x8000
			 * The theoretical minimum fast-path PDU consists only of two header bytes plus one
			 * byte for data (e.g. fast-path input synchronize pdu)
			 */
			if (pduLength < 3 || pduLength > 0x8000)
			{
				WLog_Print(transport->log, WLOG_ERROR, "fast path - invalid pduLength: %" PRIdz,
				           pduLength);
				return -1;
			}
		}
	}

	if (!Stream_EnsureCapacity(s, Stream_GetPosition(s) + pduLength))
		return -1;

	status = transport_read_layer_bytes(transport, s, pduLength - Stream_GetPosition(s));

	if (status != 1)
		return status;

	if (Stream_GetPosition(s) >= pduLength)
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), pduLength, WLOG_PACKET_INBOUND);

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	return Stream_Length(s);
}

int transport_write(rdpTransport* transport, wStream* s)
{
	size_t length;
	int status = -1;
	int writtenlength = 0;
	rdpRdp* rdp = transport->context->rdp;
	rdpContext* context = transport->context;
	// freerdp_peer* server = context->peer; // if freerdp use as server
	// freerdp* client = context->peer; // if freerdp use as client

	if (!s)
		return -1;

	if (!transport)
		goto fail;

	EnterCriticalSection(&(transport->WriteLock));
	length = Stream_GetPosition(s);
	writtenlength = length;
	Stream_SetPosition(s, 0);

	if (length > 0)
	{
		rdp->outBytes += length;
		WLog_Packet(transport->log, WLOG_TRACE, Stream_Buffer(s), length, WLOG_PACKET_OUTBOUND);

		status = rdp->rose.rose_write_layer(context, Stream_Pointer(s), length);
		if (status <= 0) {
			status = -1;
			goto out_cleanup;
		}
		length -= status;
		Stream_Seek(s, status);
	}

	transport->written += writtenlength;
out_cleanup:

	if (status < 0)
	{
		// A write error indicates that the peer has dropped the connection 
		transport->layer = TRANSPORT_LAYER_CLOSED;
		freerdp_set_last_error_if_not(transport->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
	}

	LeaveCriticalSection(&(transport->WriteLock));
fail:
	Stream_Release(s);
	return status;
}

// freerdp-rose-fix
int rose_did_read(rdpRdp* rdp)
{
	// rdpRdp* rdp = client->context->rdp;
	rdpTransport* transport = rdp->transport;
	int status;
	int recv_status;
	wStream* received;

	/**
		* Note: transport_read_pdu tries to read one PDU from
		* the transport layer.
		* The ReceiveBuffer might have a position > 0 in case of a non blocking
		* transport. If transport_read_pdu returns 0 the pdu couldn't be read at
		* this point.
		* Note that transport->ReceiveBuffer is replaced after each iteration
		* of this loop with a fresh stream instance from a pool.
		*/
	if ((status = transport_read_pdu(transport, transport->ReceiveBuffer)) <= 0)
	{
		if (status < 0)
			WLog_Print(transport->log, WLOG_DEBUG,
				        "transport_check_fds: transport_read_pdu() - %i", status);

		// If the remaining data does not generate a full pdu, status is 0.
		return status;
	}

	received = transport->ReceiveBuffer;

	if (!(transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0)))
		return -1;
/*
	static int times = 0;
	WLog_INFO(TAG, "[%s] #%i transport_read_pdu(...), status: %i, len: %i", 
		__FUNCTION__, times, status, (int)received->length);
	times ++;
*/
	/**
		* status:
		* 	-1: error
		* 	 0: success
		* 	 1: redirection
		*/
	recv_status = transport->ReceiveCallback(transport, received, transport->ReceiveExtra);
	Stream_Release(received);

	/* session redirection or activation */
/*	if (recv_status == 1 || recv_status == 2)
	{
		return recv_status;
	}

	if (recv_status < 0)
	{
		WLog_Print(transport->log, WLOG_ERROR,
			        "transport_check_fds: transport->ReceiveCallback() - %i", recv_status);
		return -1;
	}
*/
	// At least one pdu has been processed.
	return 1;
}

void transport_set_gateway_enabled(rdpTransport* transport, BOOL GatewayEnabled)
{
	transport->GatewayEnabled = GatewayEnabled;
}

void transport_set_nla_mode(rdpTransport* transport, BOOL NlaMode)
{
	transport->NlaMode = NlaMode;
}

BOOL transport_disconnect(rdpTransport* transport)
{
	BOOL status = TRUE;

	if (!transport)
		return FALSE;
/*
	if (transport->tls)
	{
		tls_free(transport->tls);
		transport->tls = NULL;
	}
	else
	{
	}
*/

	// transport->frontBio = NULL;
	transport->layer = TRANSPORT_LAYER_TCP;
	return status;
}

rdpTransport* transport_new(rdpContext* context)
{
	rdpTransport* transport;
	transport = (rdpTransport*)calloc(1, sizeof(rdpTransport));

	if (!transport)
		return NULL;

	transport->log = WLog_Get(TAG);

	if (!transport->log)
		goto out_free_transport;

	transport->context = context;
	transport->settings = context->settings;
	transport->ReceivePool = StreamPool_New(TRUE, BUFFER_SIZE);

	if (!transport->ReceivePool)
		goto out_free_transport;

	/* receive buffer for non-blocking read. */
	transport->ReceiveBuffer = StreamPool_Take(transport->ReceivePool, 0);

	if (!transport->ReceiveBuffer)
		goto out_free_receivepool;

	transport->connectedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->connectedEvent || transport->connectedEvent == INVALID_HANDLE_VALUE)
		goto out_free_receivebuffer;

	transport->rereadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!transport->rereadEvent || transport->rereadEvent == INVALID_HANDLE_VALUE)
		goto out_free_connectedEvent;

	transport->haveMoreBytesToRead = FALSE;
	// transport->blocking = TRUE;
	transport->GatewayEnabled = FALSE;
	transport->layer = TRANSPORT_LAYER_TCP;

	if (!InitializeCriticalSectionAndSpinCount(&(transport->ReadLock), 4000))
		goto out_free_rereadEvent;

	if (!InitializeCriticalSectionAndSpinCount(&(transport->WriteLock), 4000))
		goto out_free_readlock;

	return transport;
out_free_readlock:
	DeleteCriticalSection(&(transport->ReadLock));
out_free_rereadEvent:
	CloseHandle(transport->rereadEvent);
out_free_connectedEvent:
	CloseHandle(transport->connectedEvent);
out_free_receivebuffer:
	StreamPool_Return(transport->ReceivePool, transport->ReceiveBuffer);
out_free_receivepool:
	StreamPool_Free(transport->ReceivePool);
out_free_transport:
	free(transport);
	return NULL;
}

void transport_free(rdpTransport* transport)
{
	if (!transport)
		return;

	transport_disconnect(transport);

	if (transport->ReceiveBuffer)
		Stream_Release(transport->ReceiveBuffer);

	StreamPool_Free(transport->ReceivePool);
	CloseHandle(transport->connectedEvent);
	CloseHandle(transport->rereadEvent);
	DeleteCriticalSection(&(transport->ReadLock));
	DeleteCriticalSection(&(transport->WriteLock));
	free(transport);
}

void rose_register_extra(rdpContext* context, did_rose_read_layer read_layer, 
	did_rose_write_layer write_layer, void* delegate, void* connection)
{
	rdpRdp* rdp = context->rdp;
	rdp->rose.rose_read_layer = read_layer;
	rdp->rose.rose_write_layer = write_layer;
	rdp->rose.rose_delegate = delegate;
	rdp->rose.rose_connection = connection;
}

BOOL rose_send_suppress_output(rdpContext* context, BOOL suppress)
{
	RECTANGLE_16 rect;
	rdpSettings* settings;
	rdpUpdate* update;

	if (!context || !context->settings || !context->update)
		return FALSE;
/*
	if (gdi->suppressOutput == suppress)
		return TRUE;

	gdi->suppressOutput = suppress;
*/
	settings = context->settings;
	update = context->update;
	rect.left = 0;
	rect.top = 0;
	rect.right = settings->DesktopWidth;
	rect.bottom = settings->DesktopHeight;
	return update->SuppressOutput(context, !suppress, &rect);
}