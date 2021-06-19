/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#define GETTEXT_DOMAIN "rose-lib"

#ifdef _WIN32
#if !defined(UNICODE)
#error "On windows, this file must complete Use Unicode Character Set"
#endif
#endif

#if defined(_WIN32) || defined(ANDROID) || defined(__APPLE__)
#include "freerdp_config.h"
#endif

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/channels/leagor_common2.hpp>
#include <freerdp/channels/leagor_common_context.h>

#include <kosapi/gui.h>




#define TAG CLIENT_TAG("windows")


/***********************************************************************************/
bool is_orientation_valid(int orientation)
{
	return orientation != KOS_DISPLAY_ORIENTATION_0 && orientation != KOS_DISPLAY_ORIENTATION_90 && orientation != KOS_DISPLAY_ORIENTATION_180 && orientation != KOS_DISPLAY_ORIENTATION_270; 
}

UINT leagor_send_orientation_update(LeagorCommonContext* context, uint32_t initial, uint32_t current)
{
	if (!context) {
		return ERROR_INTERNAL_ERROR;
	}

	if (is_orientation_valid(initial) || is_orientation_valid(current)) {
		return ERROR_INTERNAL_ERROR;
	}

	/* Ignore if other app is holding clipboard */
	UINT rc = context->ServerOrientationUpdate(context, initial, current);

	return rc;
}

UINT leagor_send_explorer_update(LeagorCommonContext* context, const LEAGOR_EXPLORER_UPDATE* update)
{
	if (context == nullptr) {
		return ERROR_INTERNAL_ERROR;
	}

	if (update == nullptr) {
		return ERROR_INTERNAL_ERROR;
	}

	/* Ignore if other app is holding clipboard */
	UINT rc = context->SendExplorerUpdate(context, update);

	return rc;
}


static UINT wf_leagor_send_client_capabilities(LeagorCommonContext* context)
{
	LEAGOR_CAPABILITIES capabilities;

	if (!context || !context->SendCapabilities)
		return ERROR_INTERNAL_ERROR;

	capabilities.version = LG_CAPS_VERSION_1;
	capabilities.flags = LG_CAP_DROP_COPY;
	return context->SendCapabilities(context, &capabilities);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT wf_leagor_common_recv_capabilities(LeagorCommonContext* context,
                                           const LEAGOR_CAPABILITIES* capabilities)
{
	if (!context || !capabilities)
		return ERROR_INTERNAL_ERROR;

	if (!context->server) {
		UINT rc = wf_leagor_send_client_capabilities(context);
		if (rc != CHANNEL_RC_OK) {
			return rc;
		}
	}
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT wf_leagor_common_orientation_update(LeagorCommonContext* context,
                                          uint32_t initial, uint32_t current)
{
	context->initialOrientation = initial;
	context->currentOrientation = current;

	return CHANNEL_RC_OK;
}

