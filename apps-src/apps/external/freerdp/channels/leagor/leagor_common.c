/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cliprdr common
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/leagor_common_context.h>

#define TAG CHANNELS_TAG("cliprdr.common")

#include "leagor_common.h"

// @dataLen: An unsigned, 32-bit integer that specifies the size, in bytes, of the data which follows the Leagor PDU Header.
wStream* leagor_packet_new(UINT16 msgType, UINT16 msgFlags, UINT32 dataLen)
{
	wStream* s;
	s = Stream_New(NULL, dataLen + 8);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return NULL;
	}

	Stream_Write_UINT16(s, msgType);
	Stream_Write_UINT16(s, msgFlags);
	/* Write actual length after the entire packet has been constructed. */
	Stream_Seek(s, 4);
	return s;
}

wStream* leagor_common_send_explorer_update(const LEAGOR_EXPLORER_UPDATE* update)
{
	wStream* s;

	s = leagor_packet_new(LG_EXPLORER_UPDATE, 0, LG_EXPLORER_UPDATE_LEN);

	if (!s) {
		WLog_ERR(TAG, "leagor_packet_new failed!");
		return NULL;
	}

	Stream_Write_UINT32(s, update->code); /* initial orientation (4 bytes) */
	Stream_Write_UINT32(s, update->data1); /* current orientation (4 bytes) */
	Stream_Write_UINT32(s, update->data2); /* current orientation (4 bytes) */
	Stream_Write_UINT32(s, update->data3); /* current orientation (4 bytes) */
	return s;
}

UINT leagor_common_receive_explorer_update(LeagorCommonContext* context, wStream* s)
{
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < LG_EXPLORER_UPDATE_LEN)
		return ERROR_INVALID_DATA;

	LEAGOR_EXPLORER_UPDATE update;
	memset(&update, 0, sizeof(update));

	Stream_Read_UINT32(s, update.code);
	Stream_Read_UINT32(s, update.data1);
	Stream_Read_UINT32(s, update.data2);
	Stream_Read_UINT32(s, update.data3);

	if (context->RecvExplorerUpdate) {
		if ((error = context->RecvExplorerUpdate(context, &update)))
			WLog_ERR(TAG, "RecvExplorerUpdate with error %" PRIu32 "", error);
	}

	return error;
}




