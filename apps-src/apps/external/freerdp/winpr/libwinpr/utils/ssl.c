/**
 * WinPR: Windows Portable Runtime
 * OpenSSL Library Initialization
 *
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/ssl.h>
#include <winpr/thread.h>
#include <winpr/crypto.h>

#ifdef WITH_OPENSSL

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../log.h"
#define TAG WINPR_TAG("utils.ssl")

static BOOL g_winpr_openssl_initialized_by_winpr = FALSE;

/**
 * Note from OpenSSL 1.1.0 "CHANGES":
 * OpenSSL now uses a new threading API. It is no longer necessary to
 * set locking callbacks to use OpenSSL in a multi-threaded environment.
 */




BOOL winpr_InitializeSSL(DWORD flags)
{
	int ii = 0;
	return TRUE;
/* freerdp-rose-fix
	static INIT_ONCE once = INIT_ONCE_STATIC_INIT;

	if (!InitOnceExecuteOnce(&once, _winpr_openssl_initialize, &flags, NULL))
		return FALSE;

	return winpr_enable_fips(flags);
*/
}

BOOL winpr_CleanupSSL(DWORD flags)
{
/* freerdp-rose-fix
	if (flags & WINPR_SSL_CLEANUP_GLOBAL)
	{
		if (!g_winpr_openssl_initialized_by_winpr)
		{
			WLog_WARN(TAG, "ssl was not initialized by winpr");
			return FALSE;
		}

		g_winpr_openssl_initialized_by_winpr = FALSE;
#ifdef WINPR_OPENSSL_LOCKING_REQUIRED
		_winpr_openssl_cleanup_locking();
#endif
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
		CRYPTO_cleanup_all_ex_data();
		ERR_free_strings();
		EVP_cleanup();
#endif
#ifdef WINPR_OPENSSL_LOCKING_REQUIRED
		flags |= WINPR_SSL_CLEANUP_THREAD;
#endif
	}

#ifdef WINPR_OPENSSL_LOCKING_REQUIRED

	if (flags & WINPR_SSL_CLEANUP_THREAD)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || defined(LIBRESSL_VERSION_NUMBER)
		ERR_remove_state(0);
#else
		ERR_remove_thread_state(NULL);
#endif
	}

#endif
*/
	int ii = 0;
	return TRUE;
}

BOOL winpr_FIPSMode(void)
{
#if (OPENSSL_VERSION_NUMBER < 0x10001000L) || defined(LIBRESSL_VERSION_NUMBER)
	return FALSE;
#else
	return (FIPS_mode() == 1);
#endif
}

#else

BOOL winpr_InitializeSSL(DWORD flags)
{
	return TRUE;
}

BOOL winpr_CleanupSSL(DWORD flags)
{
	return TRUE;
}

BOOL winpr_FIPSMode(void)
{
	return FALSE;
}

#endif
