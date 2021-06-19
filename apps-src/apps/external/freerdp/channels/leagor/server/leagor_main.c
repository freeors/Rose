/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/stream.h>

#include <freerdp/channels/log.h>
#include "leagor_main.h"
#include "../leagor_common.h"

/**
 *                                    Initialization Sequence\n
 *     Client                                                                    Server\n
 *        |                                                                         |\n
 *        |<----------------------Server Clipboard Capabilities PDU-----------------|\n
 *        |<-----------------------------Monitor Ready PDU--------------------------|\n
 *        |-----------------------Client Clipboard Capabilities PDU---------------->|\n
 *        |---------------------------Temporary Directory PDU---------------------->|\n
 *        |-------------------------------Format List PDU-------------------------->|\n
 *        |<--------------------------Format List Response PDU----------------------|\n
 *
 */

/**
 *                                    Data Transfer Sequences\n
 *     Shared                                                                     Local\n
 *  Clipboard Owner                                                           Clipboard Owner\n
 *        |                                                                         |\n
 *        |-------------------------------------------------------------------------|\n _
 *        |-------------------------------Format List PDU-------------------------->|\n  |
 *        |<--------------------------Format List Response PDU----------------------|\n _| Copy
 * Sequence
 *        |<---------------------Lock Clipboard Data PDU (Optional)-----------------|\n
 *        |-------------------------------------------------------------------------|\n
 *        |-------------------------------------------------------------------------|\n _
 *        |<--------------------------Format Data Request PDU-----------------------|\n  | Paste
 * Sequence Palette,
 *        |---------------------------Format Data Response PDU--------------------->|\n _| Metafile,
 * File List Data
 *        |-------------------------------------------------------------------------|\n
 *        |-------------------------------------------------------------------------|\n _
 *        |<------------------------Format Contents Request PDU---------------------|\n  | Paste
 * Sequence
 *        |-------------------------Format Contents Response PDU------------------->|\n _| File
 * Stream Data
 *        |<---------------------Lock Clipboard Data PDU (Optional)-----------------|\n
 *        |-------------------------------------------------------------------------|\n
 *
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_server_packet_send(LeagorServerPrivate* cliprdr, wStream* s)
{
	UINT rc;
	size_t pos, size;
	BOOL status;
	UINT32 dataLen;
	UINT32 written;
	pos = Stream_GetPosition(s);
	if ((pos < 8) || (pos > UINT32_MAX))
	{
		rc = ERROR_NO_DATA;
		goto fail;
	}

	dataLen = (UINT32)(pos - 8);
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);
	size = Stream_Length(s);
	if (size > UINT32_MAX)
	{
		rc = ERROR_INVALID_DATA;
		goto fail;
	}

	status = WTSVirtualChannelWrite(cliprdr->ChannelHandle, (PCHAR)Stream_Buffer(s), (UINT32)size,
	                                &written);
	rc = status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
fail:
	Stream_Free(s, TRUE);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_server_send_capabilities(LeagorCommonContext* context,
                                        const LEAGOR_CAPABILITIES* capabilities)
{
	size_t offset = 0;
	wStream* s;
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;

	if (capabilities->msgType != LG_CAPABILITIES)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          capabilities->msgType);

	s = leagor_packet_new(LG_CAPABILITIES, 0, LG_CAPSTYPE_GENERAL_LEN);

	if (!s)
	{
		WLog_ERR(TAG, "leagor_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, capabilities->version); /* cCapabilitiesSets (2 bytes) */
	Stream_Write_UINT32(s, capabilities->flags);    /* pad1 (2 bytes) */
	
	WLog_DBG(TAG, "ServerCapabilities, version: 0x%08x, flags: 0x%08x", capabilities->version, capabilities->flags);
	return leagor_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_server_orientation_update(LeagorCommonContext* context, uint32_t initial, uint32_t current)
{
	wStream* s;
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;

	s = leagor_packet_new(LG_ORIENTATION_UPDATE, 0, LG_ORIENTATION_UPDATE_LEN);

	if (!s) {
		WLog_ERR(TAG, "leagor_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, initial); /* initial orientation (4 bytes) */
	Stream_Write_UINT32(s, current); /* current orientation (4 bytes) */

	WLog_INFO(TAG, "leagor_server_orientation_update, initial: 0x%08x, current: 0x%08x", initial, current);

	return leagor_server_packet_send(cliprdr, s);
}

static UINT leagor_server_send_explorer_update(LeagorCommonContext* context, const LEAGOR_EXPLORER_UPDATE* update)
{
	wStream* s;
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;

	s = leagor_common_send_explorer_update(update);
	if (!s) {
		WLog_ERR(TAG, "leagor_common_send_explorer_update failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_INFO(TAG, "leagor_server_send_explorer_update, code: %u", update->code);

	return leagor_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_server_receive_capabilities(LeagorCommonContext* context, wStream* s,
                                                const LEAGOR_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;
	size_t cap_sets_size = 0;
	LEAGOR_CAPABILITIES capabilities;

	WINPR_UNUSED(header);

	Stream_Read_UINT32(s, capabilities.version); /* cCapabilitiesSets (2 bytes) */
	Stream_Read_UINT32(s, capabilities.flags); /* pad1 (2 bytes) */

	WLog_DBG(TAG, "leagor_server_receive_capabilities, version: %u, flags: 0x%08x", capabilities.version, capabilities.flags);

	context->canDropCopy = (capabilities.flags & LG_CAP_DROP_COPY) ? TRUE : FALSE;

	IFCALLRET(context->RecvCapabilities, error, context, &capabilities);
	return error;
}

static UINT leagor_server_receive_explorer_update(LeagorCommonContext* context, wStream* s, const LEAGOR_HEADER* header)
{
	return leagor_common_receive_explorer_update(context, s);
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_server_receive_pdu(LeagorCommonContext* context, wStream* s,
                                       const LEAGOR_HEADER* header)
{
	UINT error;
	WLog_DBG(TAG,
	         "CliprdrServerReceivePdu: msgType: %" PRIu16 " msgFlags: 0x%04" PRIX16
	         " dataLen: %" PRIu32 "",
	         header->msgType, header->msgFlags, header->dataLen);

	switch (header->msgType)
	{
		case LG_CAPABILITIES:
			if ((error = leagor_server_receive_capabilities(context, s, header)))
				WLog_ERR(TAG, "leagor_server_receive_capabilities failed with error %" PRIu32 "!",
				         error);

			break;

		case LG_EXPLORER_UPDATE:
			if ((error = leagor_server_receive_explorer_update(context, s, header)))
				WLog_ERR(TAG, "leagor_server_receive_explorer_update failed with error %" PRIu32 "!",
				         error);

			break;

		default:
			error = ERROR_INVALID_DATA;
			WLog_ERR(TAG, "Unexpected clipboard PDU type: %" PRIu16 "", header->msgType);
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_server_init(LeagorCommonContext* context)
{
	UINT32 generalFlags;
	UINT error;
	LEAGOR_CAPABILITIES capabilities = { 0 };

	generalFlags = LG_CAP_DROP_COPY;

	capabilities.msgType = LG_CAPABILITIES;
	capabilities.msgFlags = 0;
	capabilities.dataLen = LG_CAPSTYPE_GENERAL_LEN;
	capabilities.version = LG_CAPS_VERSION_1;
	capabilities.flags = generalFlags;

	if ((error = context->SendCapabilities(context, &capabilities)))
	{
		WLog_ERR(TAG, "ServerCapabilities failed with error %" PRIu32 "!", error);
		return error;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT leagor_server_read(LeagorCommonContext* context)
{
	wStream* s;
	size_t position;
	DWORD BytesToRead;
	DWORD BytesReturned;
	LEAGOR_HEADER header;
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;
	UINT error;
	DWORD status;
	s = cliprdr->s;

	if (Stream_GetPosition(s) < LEAGOR_HEADER_LENGTH)
	{
		BytesReturned = 0;
		BytesToRead = (UINT32)(LEAGOR_HEADER_LENGTH - Stream_GetPosition(s));
		status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		if (status == WAIT_TIMEOUT)
			return CHANNEL_RC_OK;

		if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0, (PCHAR)Stream_Pointer(s), BytesToRead,
		                           &BytesReturned))
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		Stream_Seek(s, BytesReturned);
	}

	if (Stream_GetPosition(s) >= LEAGOR_HEADER_LENGTH)
	{
		position = Stream_GetPosition(s);
		Stream_SetPosition(s, 0);
		Stream_Read_UINT16(s, header.msgType);  /* msgType (2 bytes) */
		Stream_Read_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
		Stream_Read_UINT32(s, header.dataLen);  /* dataLen (4 bytes) */

		if (!Stream_EnsureCapacity(s, (header.dataLen + LEAGOR_HEADER_LENGTH)))
		{
			WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		Stream_SetPosition(s, position);

		if (Stream_GetPosition(s) < (header.dataLen + LEAGOR_HEADER_LENGTH))
		{
			BytesReturned = 0;
			BytesToRead =
			    (UINT32)((header.dataLen + LEAGOR_HEADER_LENGTH) - Stream_GetPosition(s));
			status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

			if (status == WAIT_FAILED)
			{
				error = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
				return error;
			}

			if (status == WAIT_TIMEOUT)
				return CHANNEL_RC_OK;

			if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0, (PCHAR)Stream_Pointer(s),
			                           BytesToRead, &BytesReturned))
			{
				WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
				return ERROR_INTERNAL_ERROR;
			}

			Stream_Seek(s, BytesReturned);
		}

		if (Stream_GetPosition(s) >= (header.dataLen + LEAGOR_HEADER_LENGTH))
		{
			Stream_SetPosition(s, (header.dataLen + LEAGOR_HEADER_LENGTH));
			Stream_SealLength(s);
			Stream_SetPosition(s, LEAGOR_HEADER_LENGTH);

			if ((error = leagor_server_receive_pdu(context, s, &header)))
			{
				WLog_ERR(TAG, "leagor_server_receive_pdu failed with error code %" PRIu32 "!",
				         error);
				return error;
			}

			Stream_SetPosition(s, 0);
			/* check for trailing zero bytes */
			status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

			if (status == WAIT_FAILED)
			{
				error = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
				return error;
			}

			if (status == WAIT_TIMEOUT)
				return CHANNEL_RC_OK;

			BytesReturned = 0;
			BytesToRead = 4;

			if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0, (PCHAR)Stream_Pointer(s),
			                           BytesToRead, &BytesReturned))
			{
				WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
				return ERROR_INTERNAL_ERROR;
			}

			if (BytesReturned == 4)
			{
				Stream_Read_UINT16(s, header.msgType);  /* msgType (2 bytes) */
				Stream_Read_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */

				if (!header.msgType)
				{
					/* ignore trailing bytes */
					Stream_SetPosition(s, 0);
				}
			}
			else
			{
				Stream_Seek(s, BytesReturned);
			}
		}
	}

	return CHANNEL_RC_OK;
}

static DWORD WINAPI leagor_server_thread(LPVOID arg)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[8];
	HANDLE ChannelEvent;
	LeagorCommonContext* context = (LeagorCommonContext*)arg;
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;
	UINT error = CHANNEL_RC_OK;

	ChannelEvent = context->GetEventHandle(context);
	nCount = 0;
	events[nCount++] = cliprdr->StopEvent;
	events[nCount++] = ChannelEvent;

	if ((error = leagor_server_init(context)))
	{
		WLog_ERR(TAG, "leagor_server_init failed with error %" PRIu32 "!", error);
		goto out;
	}

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			goto out;
		}

		status = WaitForSingleObject(cliprdr->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
			break;

		status = WaitForSingleObject(ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
		{
			if ((error = context->CheckEventHandle(context)))
			{
				WLog_ERR(TAG, "CheckEventHandle failed with error %" PRIu32 "!", error);
				break;
			}
		}
	}

out:

	WLog_WARN(TAG, "leagor_server_thread 1");

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error, "leagor_server_thread reported an error");

	WLog_WARN(TAG, "leagor_server_thread 2");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_open(LeagorCommonContext* context)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;
	cliprdr->ChannelHandle = WTSVirtualChannelOpen(cliprdr->vcm, WTS_CURRENT_SESSION, LEAGOR_SVC_CHANNEL_NAME);

	if (!cliprdr->ChannelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen for cliprdr failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cliprdr->ChannelEvent = NULL;

	if (WTSVirtualChannelQuery(cliprdr->ChannelHandle, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned))
	{
		if (BytesReturned != sizeof(HANDLE))
		{
			WLog_ERR(TAG, "BytesReturned has not size of HANDLE!");
			return ERROR_INTERNAL_ERROR;
		}

		CopyMemory(&(cliprdr->ChannelEvent), buffer, sizeof(HANDLE));
		WTSFreeMemory(buffer);
	}

	if (!cliprdr->ChannelEvent)
	{
		WLog_ERR(TAG, "WTSVirtualChannelQuery for cliprdr failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_close(LeagorCommonContext* context)
{
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;

	WLog_WARN(TAG, "cliprdr_server_close--- cliprdr: %p", cliprdr);
	if (cliprdr->ChannelHandle)
	{
		WLog_WARN(TAG, "cliprdr_server_close 1, cliprdr->ChannelHandle: %p", cliprdr->ChannelHandle);
		WTSVirtualChannelClose(cliprdr->ChannelHandle);
		cliprdr->ChannelHandle = NULL;
	}
	WLog_WARN(TAG, "---cliprdr_server_close X");
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_start(LeagorCommonContext* context)
{
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;
	UINT error;

	if (!cliprdr->ChannelHandle)
	{
		if ((error = context->Open(context)))
		{
			WLog_ERR(TAG, "Open failed!");
			return error;
		}
	}

	if (!(cliprdr->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(cliprdr->Thread = CreateThread(NULL, 0, leagor_server_thread, (void*)context, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		CloseHandle(cliprdr->StopEvent);
		cliprdr->StopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_stop(LeagorCommonContext* context)
{
	UINT error = CHANNEL_RC_OK;
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;

	WLog_WARN(TAG, "cliprdr_server_stop--- cliprdr: %p", cliprdr);

	if (cliprdr->StopEvent)
	{
		SetEvent(cliprdr->StopEvent);

		WLog_WARN(TAG, "cliprdr_server_stop 1");
		if (WaitForSingleObject(cliprdr->Thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		WLog_WARN(TAG, "cliprdr_server_stop 2");
		CloseHandle(cliprdr->Thread);

		WLog_WARN(TAG, "cliprdr_server_stop 3");
		CloseHandle(cliprdr->StopEvent);
	}

	WLog_WARN(TAG, "cliprdr_server_stop 4");
	if (cliprdr->ChannelHandle) {
		WLog_WARN(TAG, "cliprdr_server_stop 5");
		return context->Close(context);
	}

	WLog_WARN(TAG, "---cliprdr_server_stop X");
	return error;
}

static HANDLE cliprdr_server_get_event_handle(LeagorCommonContext* context)
{
	LeagorServerPrivate* cliprdr = (LeagorServerPrivate*)context->handle;
	return cliprdr->ChannelEvent;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_check_event_handle(LeagorCommonContext* context)
{
	return leagor_server_read(context);
}

LeagorCommonContext* leagor_server_context_new(HANDLE vcm)
{
	LeagorCommonContext* context;
	LeagorServerPrivate* cliprdr;
	context = (LeagorCommonContext*)calloc(1, sizeof(LeagorCommonContext));

	if (context)
	{
		context->Open = cliprdr_server_open;
		context->Close = cliprdr_server_close;
		context->Start = cliprdr_server_start;
		context->Stop = cliprdr_server_stop;
		context->GetEventHandle = cliprdr_server_get_event_handle;
		context->CheckEventHandle = cliprdr_server_check_event_handle;
		context->SendCapabilities = leagor_server_send_capabilities;
		context->ServerOrientationUpdate = leagor_server_orientation_update;
		context->SendExplorerUpdate = leagor_server_send_explorer_update;

		cliprdr = context->handle = (LeagorServerPrivate*)calloc(1, sizeof(LeagorServerPrivate));

		if (cliprdr)
		{
			cliprdr->vcm = vcm;
			cliprdr->s = Stream_New(NULL, 4096);

			if (!cliprdr->s)
			{
				WLog_ERR(TAG, "Stream_New failed!");
				free(context->handle);
				free(context);
				return NULL;
			}
		}
		else
		{
			WLog_ERR(TAG, "calloc failed!");
			free(context);
			return NULL;
		}
	}

	return context;
}

void leagor_server_context_free(LeagorCommonContext* context)
{
	LeagorServerPrivate* cliprdr;

	if (!context)
		return;

	cliprdr = (LeagorServerPrivate*)context->handle;

	if (cliprdr)
	{
		Stream_Free(cliprdr->s, TRUE);
		free(cliprdr->temporaryDirectory);
	}

	free(context->handle);
	free(context);
}
