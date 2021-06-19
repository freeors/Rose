/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#if defined(_WIN32) || defined(ANDROID) || defined(__APPLE__)
#include "freerdp_config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/client/leagor.h>

#include "leagor_main.h"
#include "leagor_format.h"
#include "../leagor_common.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT leagor_process_orientation_update(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen,
                                 UINT16 msgFlags)
{
	LeagorCommonContext* context = leagor_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	uint32_t initial = -1;
	uint32_t current = -1;
	Stream_Read_UINT32(s, initial); /* initial (4 bytes) */
	Stream_Read_UINT32(s, current); /* current (4 bytes) */

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerOrientationUpdate: initial: 0x%08x, current: 0x%08x",
	           initial, current);

	if (context->ServerOrientationUpdate) {
		if ((error = context->ServerOrientationUpdate(context, initial, current)))
			WLog_ERR(TAG, "ServerOrientationUpdate with error %" PRIu32 "", error);
	}

	return error;
}

UINT leagor_process_explorer_update(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen,
                                 UINT16 msgFlags)
{
	LeagorCommonContext* context = leagor_get_client_interface(cliprdr);
	return leagor_common_receive_explorer_update(context, s);
}

