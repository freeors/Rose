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

#ifdef WITH_DEBUG_CLIPRDR
static const char* const CB_MSG_TYPE_STRINGS[] = { "",
	                                               "CB_FORMAT_LIST",
	                                               "CB_FORMAT_LIST_RESPONSE",
	                                               "CB_FORMAT_DATA_REQUEST",
	                                               "CB_FORMAT_DATA_RESPONSE",
	                                               "CB_TEMP_DIRECTORY",
	                                               "LG_CAPABILITIES",
	                                               "CB_FILECONTENTS_REQUEST",
	                                               "CB_FILECONTENTS_RESPONSE",
	                                               "CB_LOCK_CLIPDATA",
	                                               "CB_UNLOCK_CLIPDATA" };
#endif

LeagorCommonContext* leagor_get_client_interface(cliprdrPlugin* cliprdr)
{
	LeagorCommonContext* pInterface;

	if (!cliprdr)
		return NULL;

	pInterface = (LeagorCommonContext*)cliprdr->channelEntryPoints.pInterface;
	return pInterface;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_packet_send(cliprdrPlugin* cliprdr, wStream* s)
{
	size_t pos;
	UINT32 dataLen;
	UINT status = CHANNEL_RC_OK;
	pos = Stream_GetPosition(s);
	dataLen = pos - 8;
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);
#ifdef WITH_DEBUG_CLIPRDR
	WLog_DBG(TAG, "Cliprdr Sending (%" PRIu32 " bytes)", dataLen + 8);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	if (!cliprdr)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = cliprdr->channelEntryPoints.pVirtualChannelWriteEx(
		    cliprdr->InitHandle, cliprdr->OpenHandle, Stream_Buffer(s),
		    (UINT32)Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08" PRIX32 "]",
		         WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_general_capability(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT32 version;
	UINT32 generalFlags;
	LEAGOR_CAPABILITIES capabilities;
	LeagorCommonContext* context = leagor_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	if (!context)
	{
		WLog_ERR(TAG, "leagor_get_client_interface failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (Stream_GetRemainingLength(s) < 8)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, version);      /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */

	capabilities.msgType = LG_CAPABILITIES;
	capabilities.version = version;
	capabilities.flags = generalFlags;
	IFCALLRET(context->RecvCapabilities, error, context, &capabilities);

	if (error)
		WLog_ERR(TAG, "ServerCapabilities failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, wStream* s, UINT32 length,
                                      UINT16 flags)
{
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < LG_CAPSTYPE_GENERAL_LEN)
		return ERROR_INVALID_DATA;

	if ((error = cliprdr_process_general_capability(cliprdr, s))) {
		WLog_ERR(TAG, "cliprdr_process_general_capability failed with error %" PRIu32 "!",
					error);
		return error;
	}

	return error;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_order_recv(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT16 msgType;
	UINT16 msgFlags;
	UINT32 dataLen;
	UINT error;

	if (Stream_GetRemainingLength(s) < 8)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, msgType);  /* msgType (2 bytes) */
	Stream_Read_UINT16(s, msgFlags); /* msgFlags (2 bytes) */
	Stream_Read_UINT32(s, dataLen);  /* dataLen (4 bytes) */

	if (Stream_GetRemainingLength(s) < dataLen)
		return ERROR_INVALID_DATA;

#ifdef WITH_DEBUG_CLIPRDR
	WLog_DBG(TAG, "msgType: %s (%" PRIu16 "), msgFlags: %" PRIu16 " dataLen: %" PRIu32 "",
	         CB_MSG_TYPE_STRINGS[msgType], msgType, msgFlags, dataLen);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	switch (msgType)
	{
		case LG_CAPABILITIES:
			if ((error = cliprdr_process_clip_caps(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_clip_caps failed with error %" PRIu32 "!", error);

			break;

		case LG_ORIENTATION_UPDATE:
			if ((error = leagor_process_orientation_update(cliprdr, s, dataLen, msgFlags))) {
				WLog_ERR(TAG, "leagor_process_orientation_update failed with error %" PRIu32 "!", error);
			}

			break;

		case LG_EXPLORER_UPDATE:
			if ((error = leagor_process_explorer_update(cliprdr, s, dataLen, msgFlags))) {
				WLog_ERR(TAG, "leagor_process_explorer_update failed with error %" PRIu32 "!", error);
			}
			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			WLog_ERR(TAG, "unknown msgType %" PRIu16 "", msgType);
			break;
	}

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Callback Interface
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_capabilities(LeagorCommonContext* context,
                                        const LEAGOR_CAPABILITIES* capabilities)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)context->handle;
	s = leagor_packet_new(LG_CAPABILITIES, 0, LG_CAPSTYPE_GENERAL_LEN);

	if (!s)
	{
		WLog_ERR(TAG, "leagor_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, capabilities->version);             /* version */
	Stream_Write_UINT32(s, capabilities->flags);        /* generalFlags */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "cliprdr_client_capabilities, version: %u, flags: 0x%08x", capabilities->version, capabilities->flags);
	return cliprdr_packet_send(cliprdr, s);
}

static UINT leagor_client_send_explorer_update(LeagorCommonContext* context, const LEAGOR_EXPLORER_UPDATE* update)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)context->handle;

	s = leagor_common_send_explorer_update(update);
	if (!s) {
		WLog_ERR(TAG, "leagor_client_send_explorer_update failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_INFO(TAG, "leagor_client_send_explorer_update, code: %u", update->code);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_data_received(cliprdrPlugin* cliprdr, void* pData,
                                                        UINT32 dataLength, UINT32 totalLength,
                                                        UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (cliprdr->data_in)
			Stream_Free(cliprdr->data_in, TRUE);

		cliprdr->data_in = Stream_New(NULL, totalLength);
	}

	if (!(data_in = cliprdr->data_in))
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!Stream_EnsureRemainingCapacity(data_in, dataLength))
	{
		Stream_Free(cliprdr->data_in, TRUE);
		cliprdr->data_in = NULL;
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG, "cliprdr_plugin_process_received: read error");
			return ERROR_INTERNAL_ERROR;
		}

		cliprdr->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(cliprdr->queue, NULL, 0, (void*)data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE cliprdr_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
                                                            UINT event, LPVOID pData,
                                                            UINT32 dataLength, UINT32 totalLength,
                                                            UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)lpUserParam;

	if (!cliprdr || (cliprdr->OpenHandle != openHandle))
	{
		WLog_ERR(TAG, "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = cliprdr_virtual_channel_event_data_received(cliprdr, pData, dataLength,
			                                                         totalLength, dataFlags)))
				WLog_ERR(TAG, "failed with error %" PRIu32 "", error);

			break;

		case CHANNEL_EVENT_WRITE_CANCELLED:
		case CHANNEL_EVENT_WRITE_COMPLETE:
		{
			wStream* s = (wStream*)pData;
			Stream_Free(s, TRUE);
		}
		break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "cliprdr_virtual_channel_open_event_ex reported an error");
}

static DWORD WINAPI leagor_virtual_channel_client_thread(LPVOID arg)
{
	wStream* data;
	wMessage message;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)arg;
	UINT error = CHANNEL_RC_OK;

	WLog_INFO(TAG, "leagor_virtual_channel_client_thread---");

	while (1)
	{
		if (!MessageQueue_Wait(cliprdr->queue))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(cliprdr->queue, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*)message.wParam;

			if ((error = cliprdr_order_recv(cliprdr, data)))
			{
				WLog_ERR(TAG, "cliprdr_order_recv failed with error %" PRIu32 "!", error);
				break;
			}
		}
	}

	if (error && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "leagor_virtual_channel_client_thread reported an error");

	WLog_INFO(TAG, "---leagor_virtual_channel_client_thread, X");
	ExitThread(error);
	return error;
}

static void cliprdr_free_msg(void* obj)
{
	wMessage* msg = (wMessage*)obj;

	if (msg)
	{
		wStream* s = (wStream*)msg->wParam;
		Stream_Free(s, TRUE);
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_virtual_channel_event_connected(cliprdrPlugin* cliprdr, LPVOID pData,
                                                    UINT32 dataLength)
{
	UINT32 status;
	wObject obj = { 0 };
	status = cliprdr->channelEntryPoints.pVirtualChannelOpenEx(
	    cliprdr->InitHandle, &cliprdr->OpenHandle, cliprdr->channelDef.name,
	    cliprdr_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelOpen failed with %s [%08" PRIX32 "]",
		         WTSErrorToString(status), status);
		return status;
	}

	obj.fnObjectFree = cliprdr_free_msg;
	cliprdr->queue = MessageQueue_New(&obj);

	if (!cliprdr->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	if (!(cliprdr->thread = CreateThread(NULL, 0, leagor_virtual_channel_client_thread,
	                                     (void*)cliprdr, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		MessageQueue_Free(cliprdr->queue);
		cliprdr->queue = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_virtual_channel_event_disconnected(cliprdrPlugin* cliprdr)
{
	UINT rc;

	if (cliprdr->OpenHandle == 0)
		return CHANNEL_RC_OK;

	if (MessageQueue_PostQuit(cliprdr->queue, 0) &&
	    (WaitForSingleObject(cliprdr->thread, INFINITE) == WAIT_FAILED))
	{
		rc = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", rc);
		return rc;
	}

	MessageQueue_Free(cliprdr->queue);
	CloseHandle(cliprdr->thread);
	rc = cliprdr->channelEntryPoints.pVirtualChannelCloseEx(cliprdr->InitHandle,
	                                                        cliprdr->OpenHandle);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08" PRIX32 "]", WTSErrorToString(rc),
		         rc);
		return rc;
	}

	cliprdr->OpenHandle = 0;

	if (cliprdr->data_in)
	{
		Stream_Free(cliprdr->data_in, TRUE);
		cliprdr->data_in = NULL;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_virtual_channel_event_terminated(cliprdrPlugin* cliprdr)
{
	cliprdr->InitHandle = 0;
	free(cliprdr->context);
	free(cliprdr);
	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE cliprdr_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
                                                            UINT event, LPVOID pData,
                                                            UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)lpUserParam;

	if (!cliprdr || (cliprdr->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG, "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = leagor_virtual_channel_event_connected(cliprdr, pData, dataLength)))
				WLog_ERR(TAG,
				         "leagor_virtual_channel_event_connected failed with error %" PRIu32 "!",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = leagor_virtual_channel_event_disconnected(cliprdr)))
				WLog_ERR(TAG,
				         "leagor_virtual_channel_event_disconnected failed with error %" PRIu32
				         "!",
				         error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			if ((error = leagor_virtual_channel_event_terminated(cliprdr)))
				WLog_ERR(TAG,
				         "leagor_virtual_channel_event_terminated failed with error %" PRIu32 "!",
				         error);

			break;
	}

	if (error && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "cliprdr_virtual_channel_init_event reported an error");
}

/* cliprdr is always built-in */
#define VirtualChannelEntryEx leagor_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	cliprdrPlugin* cliprdr;
	LeagorCommonContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	cliprdr = (cliprdrPlugin*)calloc(1, sizeof(cliprdrPlugin));

	if (!cliprdr)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	cliprdr->channelDef.options = CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	                              CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL;
	sprintf_s(cliprdr->channelDef.name, ARRAYSIZE(cliprdr->channelDef.name), LEAGOR_SVC_CHANNEL_NAME);
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*)pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (LeagorCommonContext*)calloc(1, sizeof(LeagorCommonContext));

		if (!context)
		{
			free(cliprdr);
			WLog_ERR(TAG, "calloc failed!");
			return FALSE;
		}

		context->handle = (void*)cliprdr;
		context->SendCapabilities = cliprdr_client_capabilities;
		context->SendExplorerUpdate = leagor_client_send_explorer_update;

		cliprdr->context = context;
		context->rdpcontext = pEntryPointsEx->context;
	}

	cliprdr->log = WLog_Get("com.freerdp.channels.leagor.client");

	WLog_Print(cliprdr->log, WLOG_DEBUG, "VirtualChannelEntryEx");
	CopyMemory(&(cliprdr->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	cliprdr->InitHandle = pInitHandle;
	rc = cliprdr->channelEntryPoints.pVirtualChannelInitEx(
	    cliprdr, context, pInitHandle, &cliprdr->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	    cliprdr_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08" PRIX32 "]", WTSErrorToString(rc),
		         rc);
		free(cliprdr->context);
		free(cliprdr);
		return FALSE;
	}

	cliprdr->channelEntryPoints.pInterface = context;
	return TRUE;
}
