/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Server Interface
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

#ifndef FREERDP_CHANNEL_CLIPRDR_COMMON_CONTEXT_H
#define FREERDP_CHANNEL_CLIPRDR_COMMON_CONTEXT_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>

#include <freerdp/channels/cliprdr.h>
#include <freerdp/client/cliprdr.h>

enum {cliprdr_errcode_ok = 0, cliprdr_errcode_cancel = -1, cliprdr_errcode_format = -2, cliprdr_errcode_file = -3, 
	cliprdr_errcode_directory = -4, cliprdr_errcode_clipboardupdate = -5, cliprdr_errcode_nospace = -6};

/**
 * Server/Client Interface
 */

typedef struct _cliprdr_common_context CliprdrCommonContext;

typedef UINT (*psCliprdrOpen)(CliprdrCommonContext* context);
typedef UINT (*psCliprdrClose)(CliprdrCommonContext* context);
typedef UINT (*psCliprdrStart)(CliprdrCommonContext* context);
typedef UINT (*psCliprdrStop)(CliprdrCommonContext* context);
typedef HANDLE (*psCliprdrGetEventHandle)(CliprdrCommonContext* context);
typedef UINT (*psCliprdrCheckEventHandle)(CliprdrCommonContext* context);

typedef UINT (*psCliprdrServerCapabilities)(CliprdrCommonContext* context,
                                            const CLIPRDR_CAPABILITIES* capabilities);
typedef UINT (*psCliprdrClientCapabilities)(CliprdrCommonContext* context,
                                            const CLIPRDR_CAPABILITIES* capabilities);
typedef UINT (*psCliprdrMonitorReady)(CliprdrCommonContext* context,
                                      const CLIPRDR_MONITOR_READY* monitorReady);
typedef UINT (*psCliprdrTempDirectory)(CliprdrCommonContext* context,
                                       const CLIPRDR_TEMP_DIRECTORY* tempDirectory);
typedef UINT (*psCliprdrClientFormatList)(CliprdrCommonContext* context,
                                          const CLIPRDR_FORMAT_LIST* formatList);
typedef UINT (*psCliprdrServerFormatList)(CliprdrCommonContext* context,
                                          const CLIPRDR_FORMAT_LIST* formatList);
typedef UINT (*psCliprdrClientFormatListResponse)(
    CliprdrCommonContext* context, const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef UINT (*psCliprdrServerFormatListResponse)(
    CliprdrCommonContext* context, const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef UINT (*psCliprdrClientLockClipboardData)(
    CliprdrCommonContext* context, const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
typedef UINT (*psCliprdrServerLockClipboardData)(
    CliprdrCommonContext* context, const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
typedef UINT (*psCliprdrClientUnlockClipboardData)(
    CliprdrCommonContext* context, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
typedef UINT (*psCliprdrServerUnlockClipboardData)(
    CliprdrCommonContext* context, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
typedef UINT (*psCliprdrClientFormatDataRequest)(
    CliprdrCommonContext* context, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef UINT (*psCliprdrServerFormatDataRequest)(
    CliprdrCommonContext* context, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef UINT (*psCliprdrClientFormatDataResponse)(
    CliprdrCommonContext* context, const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
typedef UINT (*psCliprdrServerFormatDataResponse)(
    CliprdrCommonContext* context, const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
typedef UINT (*psCliprdrClientFileContentsRequest)(
    CliprdrCommonContext* context, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
typedef UINT (*psCliprdrServerFileContentsRequest)(
    CliprdrCommonContext* context, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
typedef UINT (*psCliprdrClientFileContentsResponse)(
    CliprdrCommonContext* context, const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse);
typedef UINT (*psCliprdrServerFileContentsResponse)(
    CliprdrCommonContext* context, const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse);

typedef void (*psTextClipboardUpdated)(CliprdrCommonContext* context, const char* text);
typedef void (*psHdropCopied)(CliprdrCommonContext* context, const char* c_str, int size);
typedef int (*psHdropPaste)(CliprdrCommonContext* context, const char* path, void* progress, char* err_msg, int max_bytes);
typedef BOOL (*psCanHdropPaste)(CliprdrCommonContext* context);

struct _cliprdr_common_context
{
	void* handle;
	void* custom;

	/* server clipboard capabilities - set by server - updated by the channel after client
	 * capability exchange */
	BOOL useLongFormatNames;
	BOOL streamFileClipEnabled;
	BOOL fileClipNoFilePaths;
	BOOL canLockClipData;

	psCliprdrOpen Open;
	psCliprdrClose Close;
	psCliprdrStart Start;
	psCliprdrStop Stop;
	psCliprdrGetEventHandle GetEventHandle;
	psCliprdrCheckEventHandle CheckEventHandle;

	// server: I will send Capabilities pdu. client: I received Capabilities pdu from server.
	psCliprdrServerCapabilities ServerCapabilities;
	psCliprdrClientCapabilities ClientCapabilities;
	psCliprdrMonitorReady MonitorReady;
	psCliprdrTempDirectory TempDirectory;
	psCliprdrClientFormatList ClientFormatList;
	psCliprdrServerFormatList ServerFormatList;
	psCliprdrClientFormatListResponse ClientFormatListResponse;
	psCliprdrServerFormatListResponse ServerFormatListResponse;
	psCliprdrClientLockClipboardData ClientLockClipboardData;
	psCliprdrServerLockClipboardData ServerLockClipboardData;
	psCliprdrClientUnlockClipboardData ClientUnlockClipboardData;
	psCliprdrServerUnlockClipboardData ServerUnlockClipboardData;
	psCliprdrClientFormatDataRequest ClientFormatDataRequest;
	psCliprdrServerFormatDataRequest ServerFormatDataRequest;
	psCliprdrClientFormatDataResponse ClientFormatDataResponse;
	psCliprdrServerFormatDataResponse ServerFormatDataResponse;
	psCliprdrClientFileContentsRequest ClientFileContentsRequest;
	psCliprdrServerFileContentsRequest ServerFileContentsRequest;
	psCliprdrClientFileContentsResponse ClientFileContentsResponse;
	psCliprdrServerFileContentsResponse ServerFileContentsResponse;

	psTextClipboardUpdated TextClipboardUpdated;
	psHdropCopied HdropCopied;
	psHdropPaste HdropPaste;
	psCanHdropPaste CanHdropPaste;

	rdpContext* rdpcontext;
	BOOL autoInitializationSequence;
	UINT32 lastRequestedFormatId;
	BOOL server; // ture: used to server/false: used t client
};

#endif /* FREERDP_CHANNEL_CLIPRDR_COMMON_CONTEXT_H */
