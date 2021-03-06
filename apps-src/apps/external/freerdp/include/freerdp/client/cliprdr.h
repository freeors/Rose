/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
 *
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_CHANNEL_CLIPRDR_CLIENT_CLIPRDR_H
#define FREERDP_CHANNEL_CLIPRDR_CLIENT_CLIPRDR_H

// #include <freerdp/types.h>

#include <freerdp/channels/cliprdr_common_context.h>
#include <freerdp/message.h>
// #include <freerdp/channels/cliprdr.h>
// #include <freerdp/freerdp.h>

/**
 * Client Interface
 */

// typedef CliprdrCommonContext CliprdrClientContext;
#define CliprdrClientContext	CliprdrCommonContext

struct _CLIPRDR_FORMAT_NAME
{
	UINT32 id;
	char* name;
	int length;
};
typedef struct _CLIPRDR_FORMAT_NAME CLIPRDR_FORMAT_NAME;

/**
 * Clipboard Events
 */

struct _RDP_CB_CLIP_CAPS
{
	wMessage event;
	UINT32 capabilities;
};
typedef struct _RDP_CB_CLIP_CAPS RDP_CB_CLIP_CAPS;

struct _RDP_CB_MONITOR_READY_EVENT
{
	wMessage event;
	UINT32 capabilities;
};
typedef struct _RDP_CB_MONITOR_READY_EVENT RDP_CB_MONITOR_READY_EVENT;

struct _RDP_CB_FORMAT_LIST_EVENT
{
	wMessage event;
	UINT32* formats;
	UINT16 num_formats;
	BYTE* raw_format_data;
	UINT32 raw_format_data_size;
	BOOL raw_format_unicode;
};
typedef struct _RDP_CB_FORMAT_LIST_EVENT RDP_CB_FORMAT_LIST_EVENT;

struct _RDP_CB_DATA_REQUEST_EVENT
{
	wMessage event;
	UINT32 format;
};
typedef struct _RDP_CB_DATA_REQUEST_EVENT RDP_CB_DATA_REQUEST_EVENT;

struct _RDP_CB_DATA_RESPONSE_EVENT
{
	wMessage event;
	BYTE* data;
	UINT32 size;
};
typedef struct _RDP_CB_DATA_RESPONSE_EVENT RDP_CB_DATA_RESPONSE_EVENT;

struct _RDP_CB_FILECONTENTS_REQUEST_EVENT
{
	wMessage event;
	UINT32 streamId;
	UINT32 lindex;
	UINT32 dwFlags;
	UINT32 nPositionLow;
	UINT32 nPositionHigh;
	UINT32 cbRequested;
	UINT32 clipDataId;
};
typedef struct _RDP_CB_FILECONTENTS_REQUEST_EVENT RDP_CB_FILECONTENTS_REQUEST_EVENT;

struct _RDP_CB_FILECONTENTS_RESPONSE_EVENT
{
	wMessage event;
	BYTE* data;
	UINT32 size;
	UINT32 streamId;
};
typedef struct _RDP_CB_FILECONTENTS_RESPONSE_EVENT RDP_CB_FILECONTENTS_RESPONSE_EVENT;

struct _RDP_CB_LOCK_CLIPDATA_EVENT
{
	wMessage event;
	UINT32 clipDataId;
};
typedef struct _RDP_CB_LOCK_CLIPDATA_EVENT RDP_CB_LOCK_CLIPDATA_EVENT;

struct _RDP_CB_UNLOCK_CLIPDATA_EVENT
{
	wMessage event;
	UINT32 clipDataId;
};
typedef struct _RDP_CB_UNLOCK_CLIPDATA_EVENT RDP_CB_UNLOCK_CLIPDATA_EVENT;

struct _RDP_CB_TEMPDIR_EVENT
{
	wMessage event;
	char dirname[520];
};
typedef struct _RDP_CB_TEMPDIR_EVENT RDP_CB_TEMPDIR_EVENT;

#endif /* FREERDP_CHANNEL_CLIPRDR_CLIENT_CLIPRDR_H */
