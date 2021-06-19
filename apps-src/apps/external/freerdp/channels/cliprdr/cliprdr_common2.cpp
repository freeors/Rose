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
#include <freerdp/channels/cliprdr_common2.hpp>
#include <freerdp/channels/impl_cliprdr_os.h>

#include "base_instance.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"

// chromium
#include "base/bind.h"

using namespace std::placeholders;

#define TAG CLIENT_TAG("windows")


/***********************************************************************************/

static UINT32 get_local_format_id_by_name(wfClipboard* clipboard, const wchar_t* format_name)
{
	size_t i;
	formatMapping* map;

	if (!clipboard || !format_name)
		return 0;

	for (i = 0; i < clipboard->map_size; i++)
	{
		map = &clipboard->format_mappings[i];

		if (map->name)
		{
			if (SDL_wcscmp(map->name, format_name) == 0)
			{
				return map->local_format_id;
			}
		}
	}

	return 0;
}

static INLINE BOOL file_transferring(wfClipboard* clipboard)
{
	return get_local_format_id_by_name(clipboard, utils::to_wstring(CFSTR_FILEDESCRIPTORW_UTF8).c_str())? TRUE : FALSE;
}

UINT32 get_remote_format_id(wfClipboard* clipboard, UINT32 local_format)
{
	UINT32 i;
	formatMapping* map;

	if (!clipboard)
		return 0;

	for (i = 0; i < clipboard->map_size; i++)
	{
		map = &clipboard->format_mappings[i];

		if (map->local_format_id == local_format)
			return map->remote_format_id;
	}

	return local_format;
}

static void map_ensure_capacity(wfClipboard* clipboard)
{
	if (!clipboard)
		return;

	if (clipboard->map_size >= clipboard->map_capacity)
	{
		size_t new_size;
		formatMapping* new_map;
		new_size = clipboard->map_capacity * 2;
		new_map =
		    (formatMapping*)realloc(clipboard->format_mappings, sizeof(formatMapping) * new_size);

		if (!new_map)
			return;

		clipboard->format_mappings = new_map;
		clipboard->map_capacity = new_size;
	}
}

BOOL cliprdr_clear_format_map(wfClipboard* clipboard)
{
	size_t i;
	formatMapping* map;

	if (!clipboard)
		return FALSE;

	if (clipboard->format_mappings)
	{
		for (i = 0; i < clipboard->map_capacity; i++)
		{
			map = &clipboard->format_mappings[i];
			map->remote_format_id = 0;
			map->local_format_id = 0;
			free(map->name);
			map->name = NULL;
		}
	}

	clipboard->map_size = 0;
	return TRUE;
}

UINT cliprdr_send_format_list(wfClipboard* clipboard)
{
	UINT rc;
	int count = 0;
	UINT32 index;
	UINT32 numFormats = 0;
	UINT32 formatId = 0;
	char formatName[1024];
	CLIPRDR_FORMAT* formats = NULL;
	CLIPRDR_FORMAT_LIST formatList;

	if (!clipboard)
		return ERROR_INTERNAL_ERROR;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	/* Ignore if other app is holding clipboard */
	if (clipboard->open_clipboard())
	{
		count = clipboard->count_clipboard_formats();
		numFormats = (UINT32)count;
		if (count != 0) {
			formats = (CLIPRDR_FORMAT*)calloc(numFormats, sizeof(CLIPRDR_FORMAT));

			if (!formats)
			{
				clipboard->close_clipboard();
				return CHANNEL_RC_NO_MEMORY;
			}
		}

		index = 0;

		if (clipboard->is_clipboard_format_available(CF_HDROP))
		{
			formats[index++].formatId = clipboard->register_clipboard_format(CFSTR_FILEDESCRIPTORW_UTF8);
			formats[index++].formatId = clipboard->register_clipboard_format(CFSTR_FILECONTENTS_UTF8);
		}
		else
		{
			while ((formatId = clipboard->enum_clipboard_formats(formatId))) {
				formats[index++].formatId = formatId;
			}
		}

		numFormats = index;

		if (!clipboard->close_clipboard())
		{
			free(formats);
			return ERROR_INTERNAL_ERROR;
		}

		for (index = 0; index < numFormats; index++)
		{
			if (clipboard->get_clipboard_format_name_A(formats[index].formatId, formatName, sizeof(formatName))) {
				formats[index].formatName = _strdup(formatName);
			}
		}
	}

	formatList.numFormats = numFormats;
	formatList.formats = formats;
	if (clipboard->context->server) {
		rc = clipboard->context->ServerFormatList(clipboard->context, &formatList);
	} else {
		rc = clipboard->context->ClientFormatList(clipboard->context, &formatList);
	}

	for (index = 0; index < numFormats; index++)
		free(formats[index].formatName);

	free(formats);
	return rc;
}

UINT cliprdr_send_data_request(wfClipboard* clipboard, UINT32 formatId)
{
	UINT rc;
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

	if (!clipboard || !clipboard->context || !clipboard->context->ClientFormatDataRequest)
		return ERROR_INTERNAL_ERROR;

	clipboard->pre_send_format_data_request();

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = 0;
	formatDataRequest.dataLen = 4;
	formatDataRequest.requestedFormatId = formatId;
	clipboard->requestedFormatId = formatId;
	if (clipboard->context->server) {
		rc = clipboard->context->ServerFormatDataRequest(clipboard->context, &formatDataRequest);
	} else {
		rc = clipboard->context->ClientFormatDataRequest(clipboard->context, &formatDataRequest);
	}

	if (!clipboard->response_data_event.Wait(clipboard->event_overflow_ms)) {
		rc = ERROR_INTERNAL_ERROR;
	} else {
		clipboard->response_data_event.Reset();
	}
	return rc;
}

UINT cliprdr_send_request_filecontents(wfClipboard* clipboard, const void* streamid, ULONG index,
                                       UINT32 flag, DWORD positionhigh, DWORD positionlow,
                                       ULONG nreq)
{
	UINT rc;
	CLIPRDR_FILE_CONTENTS_REQUEST fileContentsRequest;

	if (!clipboard || !clipboard->context || !clipboard->context->ClientFileContentsRequest)
		return ERROR_INTERNAL_ERROR;

	fileContentsRequest.streamId = (UINT32)(unsigned long)streamid;
	fileContentsRequest.listIndex = index;
	fileContentsRequest.dwFlags = flag;
	fileContentsRequest.nPositionLow = positionlow;
	fileContentsRequest.nPositionHigh = positionhigh;
	fileContentsRequest.cbRequested = nreq;
	fileContentsRequest.clipDataId = 0;
	fileContentsRequest.msgType = CB_FILECONTENTS_REQUEST;
	fileContentsRequest.msgFlags = 0;
	if (clipboard->context->server) {
		rc = clipboard->context->ServerFileContentsRequest(clipboard->context, &fileContentsRequest);
	} else {
		rc = clipboard->context->ClientFileContentsRequest(clipboard->context, &fileContentsRequest);
	}

	{
		int ii = 0;
		WLog_INFO(TAG, "cliprdr_send_request_filecontents, streamId: 0x%08" PRIX32 ", nPosition: %u, cbRequested: %u", fileContentsRequest.streamId, fileContentsRequest.nPositionLow, fileContentsRequest.cbRequested);
	}

	if (!clipboard->req_fevent.Wait(clipboard->event_overflow_ms)) {
		rc = ERROR_INTERNAL_ERROR;
	} else {
		clipboard->req_fevent.Reset();
		if (clipboard->req_fsize == 0 && clipboard->req_fstreamid == FSTREAMID_FILECONTENTSRESPONSE_FAIL) {
			rc = ERROR_BAD_DESCRIPTOR_FORMAT;
		} else if (clipboard->req_fstreamid != fileContentsRequest.streamId) {
			rc = ERROR_INTERNAL_ERROR;
		}
	}

	return rc;
}

static UINT cliprdr_send_response_filecontents(wfClipboard* clipboard, UINT32 streamId, UINT32 size,
                                               BYTE* data)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE fileContentsResponse;

	if (!clipboard || !clipboard->context || !clipboard->context->ClientFileContentsResponse)
		return ERROR_INTERNAL_ERROR;

	fileContentsResponse.streamId = streamId;
	fileContentsResponse.cbRequested = size;
	fileContentsResponse.requestedData = data;
	fileContentsResponse.msgType = CB_FILECONTENTS_RESPONSE;
	fileContentsResponse.msgFlags = size > 0? CB_RESPONSE_OK: CB_RESPONSE_FAIL;
	if (clipboard->context->server) {
		return clipboard->context->ServerFileContentsResponse(clipboard->context,
	                                                      &fileContentsResponse);
	} else {
		return clipboard->context->ClientFileContentsResponse(clipboard->context,
	                                                      &fileContentsResponse);
	}
}



void cliprdr_clear_file_array(wfClipboard* clipboard)
{
	size_t i;

	if (!clipboard)
		return;

	/* clear file_names array */
	if (clipboard->file_names)
	{
		for (i = 0; i < clipboard->nFiles; i++)
		{
			free(clipboard->file_names[i]);
			clipboard->file_names[i] = NULL;
		}

		free(clipboard->file_names);
		clipboard->file_names = NULL;
	}

	/* clear fileDescriptor array */
	if (clipboard->fileDescriptor)
	{
		for (i = 0; i < clipboard->nFiles; i++)
		{
			free(clipboard->fileDescriptor[i]);
			clipboard->fileDescriptor[i] = NULL;
		}

		free(clipboard->fileDescriptor);
		clipboard->fileDescriptor = NULL;
	}

	clipboard->file_array_size = 0;
	clipboard->nFiles = 0;
}

static BOOL wf_cliprdr_get_file_contents(wfClipboard* clipboard, wchar_t* file_name, BYTE* buffer, LONG positionLow,
                                         LONG positionHigh, DWORD nRequested, DWORD* puSize)
{
	BOOL res = FALSE;
	DWORD nGet;

	if (!file_name || !buffer || !puSize)
	{
		WLog_ERR(TAG, "get file contents Invalid Arguments.");
		return FALSE;
	}

	const std::string file_name_utf8 = utils::from_wchar_ptr(file_name);
	tfile file(file_name_utf8, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		SDL_Log("[shadow_cliprdr.cpp]wf_cliprdr_get_file_contents, open %s for read fail", file_name_utf8.c_str());
		return FALSE;
	}

	posix_fseek(file.fp, posix_mki64(positionLow, positionHigh));
	nGet = posix_fread(file.fp, buffer, nRequested);

	res = TRUE;

// error:
	if (res)
		*puSize = nGet;

	return res;
}

#define UnixTimeToFileTime(ut)	\
	((ut) * 10000000 + INT64_C(116444736000000000))

/* path_name has a '\' at the end. e.g. c:\newfolder\, file_name is c:\newfolder\new.txt */
static FILEDESCRIPTOR* wf_cliprdr_get_file_descriptor(const wchar_t* file_name, size_t pathLen)
{
	FILEDESCRIPTOR* fd;
	fd = (FILEDESCRIPTOR*)calloc(1, sizeof(FILEDESCRIPTOR));

	if (!fd)
		return NULL;

	SDL_dirent stat;
	const std::string file_name_utf8 = utils::from_wchar_ptr(file_name);
	SDL_bool ret = SDL_GetStat(file_name_utf8.c_str(), &stat);
	if (!ret)
	{
		free(fd);
		return NULL;
	}
	fd->dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_WRITESTIME | FD_PROGRESSUI;
	fd->dwFileAttributes = SDL_DIRENT_DIR(stat.mode)? FILE_ATTRIBUTE_DIRECTORY: FILE_ATTRIBUTE_ARCHIVE;
	const uint64_t mtime = UnixTimeToFileTime(stat.mtime);
	fd->ftLastWriteTime.dwLowDateTime = posix_lo32(mtime);
	fd->ftLastWriteTime.dwHighDateTime = posix_hi32(mtime);
	fd->nFileSizeLow = stat.size;
	utils::wchar_ptr_2_uint16_ptr((uint16_t*)fd->cFileName, file_name + pathLen, sizeof(fd->cFileName) / sizeof(fd->cFileName[0]));
	return fd;
}

static BOOL wf_cliprdr_array_ensure_capacity(wfClipboard* clipboard)
{
	if (!clipboard)
		return FALSE;

	if (clipboard->nFiles == clipboard->file_array_size)
	{
		size_t new_size;
		FILEDESCRIPTOR** new_fd;
		wchar_t** new_name;
		new_size = (clipboard->file_array_size + 1) * 2;
		new_fd = (FILEDESCRIPTOR**)realloc(clipboard->fileDescriptor,
		                                    new_size * sizeof(FILEDESCRIPTOR*));

		if (new_fd)
			clipboard->fileDescriptor = new_fd;

		new_name = (wchar_t**)realloc(clipboard->file_names, new_size * sizeof(wchar_t*));

		if (new_name)
			clipboard->file_names = new_name;

		if (!new_fd || !new_name)
			return FALSE;

		clipboard->file_array_size = new_size;
	}

	return TRUE;
}

static BOOL wf_cliprdr_add_to_file_arrays(wfClipboard* clipboard, const wchar_t* full_file_name,
                                          size_t pathLen)
{
	if (!wf_cliprdr_array_ensure_capacity(clipboard))
		return FALSE;

	/* add to name array */
	clipboard->file_names[clipboard->nFiles] = (wchar_t*)malloc(MAX_PATH * sizeof(wchar_t));

	if (!clipboard->file_names[clipboard->nFiles])
		return FALSE;

	SDL_wcslcpy(clipboard->file_names[clipboard->nFiles], full_file_name, MAX_PATH);
	/* add to descriptor array */
	clipboard->fileDescriptor[clipboard->nFiles] =
	    wf_cliprdr_get_file_descriptor(full_file_name, pathLen);

	if (!clipboard->fileDescriptor[clipboard->nFiles])
	{
		free(clipboard->file_names[clipboard->nFiles]);
		return FALSE;
	}

	clipboard->nFiles++;
	return TRUE;
}

static UINT wf_cliprdr_send_client_capabilities(wfClipboard* clipboard)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	if (!clipboard || !clipboard->context || !clipboard->context->ClientCapabilities)
		return ERROR_INTERNAL_ERROR;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*)&(generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags =
	    CB_USE_LONG_FORMAT_NAMES | CB_STREAM_FILECLIP_ENABLED | CB_FILECLIP_NO_FILE_PATHS;
	return clipboard->context->ClientCapabilities(clipboard->context, &capabilities);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT wf_cliprdr_monitor_ready(CliprdrCommonContext* context,
                                     const CLIPRDR_MONITOR_READY* monitorReady)
{
	UINT rc;
	wfClipboard* clipboard = (wfClipboard*)context->custom;

	if (!context || !monitorReady)
		return ERROR_INTERNAL_ERROR;

	clipboard->sync = TRUE;
	rc = wf_cliprdr_send_client_capabilities(clipboard);

	if (rc != CHANNEL_RC_OK)
		return rc;

	return cliprdr_send_format_list(clipboard);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT wf_cliprdr_common_capabilities(CliprdrCommonContext* context,
                                           const CLIPRDR_CAPABILITIES* capabilities)
{
	UINT32 index;
	CLIPRDR_CAPABILITY_SET* capabilitySet;
	wfClipboard* clipboard = kosCustom2Clipboard(context->custom);

	if (!context || !capabilities)
		return ERROR_INTERNAL_ERROR;

	for (index = 0; index < capabilities->cCapabilitiesSets; index++)
	{
		capabilitySet = &(capabilities->capabilitySets[index]);

		if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) &&
		    (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN))
		{
			CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet =
			    (CLIPRDR_GENERAL_CAPABILITY_SET*)capabilitySet;
			clipboard->capabilities = generalCapabilitySet->generalFlags;
			break;
		}
	}

	clipboard->sync = TRUE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT wf_cliprdr_common_format_list(CliprdrCommonContext* context,
                                          const CLIPRDR_FORMAT_LIST* formatList)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT32 i;
	formatMapping* mapping;
	CLIPRDR_FORMAT* format;
	wfClipboard* clipboard = kosCustom2Clipboard(context->custom);

	clipboard->clipboard_require_update(true);

	if (!cliprdr_clear_format_map(clipboard))
		return ERROR_INTERNAL_ERROR;

	for (i = 0; i < formatList->numFormats; i++)
	{
		format = &(formatList->formats[i]);
		mapping = &(clipboard->format_mappings[i]);
		mapping->remote_format_id = format->formatId;

		if (format->formatName)
		{
			// recv has change unicode to utf8, so formatName is utf8.
			const std::wstring wname = utils::to_wstring(format->formatName);
			const int size = wname.size();
			mapping->name = (wchar_t*)calloc(size + 1, sizeof(wchar_t));

			if (mapping->name)
			{
				memcpy(mapping->name, wname.c_str(), (size + 1) * sizeof(wchar_t));
				mapping->local_format_id = clipboard->register_clipboard_format(utils::from_wchar_ptr(mapping->name));
			}
		}
		else
		{
			mapping->name = NULL;
			mapping->local_format_id = mapping->remote_format_id;
		}

		clipboard->map_size++;
		map_ensure_capacity(clipboard);
	}

	if (file_transferring(clipboard))
	{
		rc = clipboard->set_dataobject_2_clipboard();
	}
	else
	{
		if (!clipboard->open_clipboard())
			return CHANNEL_RC_OK; /* Ignore, other app holding clipboard */

		if (clipboard->empty_clipboard())
		{
			for (i = 0; i < (UINT32)clipboard->map_size; i++) {
				clipboard->set_clipboard_data(clipboard->format_mappings[i].local_format_id, NULL);
			}

			rc = CHANNEL_RC_OK;
		}

		if (!clipboard->close_clipboard() && GetLastError())
			return ERROR_INTERNAL_ERROR;
	}

	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT
wf_cliprdr_common_format_list_response(CliprdrCommonContext* context,
                                       const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	(void)context;
	(void)formatListResponse;

	if (formatListResponse->msgFlags != CB_RESPONSE_OK)
		return E_FAIL;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT
wf_cliprdr_common_lock_clipboard_data(CliprdrCommonContext* context,
                                      const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	(void)context;
	(void)lockClipboardData;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT
wf_cliprdr_common_unlock_clipboard_data(CliprdrCommonContext* context,
                                        const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	(void)context;
	(void)unlockClipboardData;
	return CHANNEL_RC_OK;
}

static bool collect_filename(const std::string& dir, const SDL_dirent2* dirent, wfClipboard* clipboard, size_t pathLen)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	const std::string ut8f_full_name = dir + "/" + dirent->name;
	const std::wstring full_name = utils::to_wstring(ut8f_full_name);
	if (isdir) {
		if (!wf_cliprdr_add_to_file_arrays(clipboard, full_name.c_str(), pathLen)) {
			return false;
		}

	} else {
		if (!wf_cliprdr_add_to_file_arrays(clipboard, full_name.c_str(), pathLen)) {
			return false;
		}
	}

	return true;
}

static BOOL wf_cliprdr_traverse_directory(wfClipboard* clipboard, const wchar_t* Dir, size_t pathLen)
{
	const std::string utf8_dir = utils::from_wchar_ptr(Dir);
	if (utf8_dir.empty()) {
		return FALSE;
	}
	::walk_dir(utf8_dir, true, std::bind(
				&collect_filename
				, _1, _2, clipboard, pathLen));
	return TRUE;
}

// @wFileName. full file name. 
// @str_len. chars of wFileName. don't include terminate '\0'. 37
// @pathLen. chars of wFileName's directory. include last '\'.

// wFIleName: C:\Users\ancientcc\Desktop\test-pro17
// str_len: 37
// pathLen: 27
BOOL wf_cliprdr_process_filename(wfClipboard* clipboard, const wchar_t* wFileName, size_t str_len)
{
	size_t pathLen;
	size_t offset = str_len;

	if (!clipboard || !wFileName)
		return FALSE;

	/* find the last '\' in full file name */
	const wchar_t slash = ASCII_2_UCS2('/');
	const wchar_t backslash = ASCII_2_UCS2('\\');
	while (offset > 0)
	{
		if (wFileName[offset] == slash || wFileName[offset] == backslash) {
			break;
		} else {
			offset--;
		}
	}

	pathLen = offset + 1;

	if (!wf_cliprdr_add_to_file_arrays(clipboard, wFileName, pathLen))
		return FALSE;

	if ((clipboard->fileDescriptor[clipboard->nFiles - 1]->dwFileAttributes &
	     FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		/* this is a directory */
		if (!wf_cliprdr_traverse_directory(clipboard, wFileName, pathLen))
			return FALSE;
	}

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT
wf_cliprdr_common_format_data_request(CliprdrCommonContext* context,
                                      const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	UINT rc;
	int size = 0;
	uint8_t* buff = NULL;
	UINT32 requestedFormatId;
	CLIPRDR_FORMAT_DATA_RESPONSE response;
	wfClipboard* clipboard;

	if (!context || !formatDataRequest)
		return ERROR_INTERNAL_ERROR;

	clipboard = kosCustom2Clipboard(context->custom);

	if (!clipboard)
		return ERROR_INTERNAL_ERROR;

	requestedFormatId = formatDataRequest->requestedFormatId;

	bool hdrop = requestedFormatId == clipboard->register_clipboard_format(CFSTR_FILEDESCRIPTORW_UTF8);
	size = clipboard->get_format_data(hdrop, requestedFormatId, &buff);
	if (size == nposm) {
		SDL_Log("wf_cliprdr_client_format_data_request, [%s] get_format_data(%u) fail", hdrop? "hdrop": "text", requestedFormatId);
		// return ERROR_INTERNAL_ERROR;
		// If here return error, freerdp's normal logic will result to exit cliprdr_server_thread,
		// It is not what I want to see. For now, return OK(rc = CHANNEL_RC_OK).
		size = 0;
	}

	response.msgType = CB_FORMAT_DATA_RESPONSE;
	response.msgFlags = size != nposm? CB_RESPONSE_OK: CB_RESPONSE_FAIL;
	response.dataLen = size != nposm? size: 0;
	response.requestedFormatData = buff;
	if (clipboard->context->server) {
		rc = clipboard->context->ServerFormatDataResponse(clipboard->context, &response);
	} else {
		rc = clipboard->context->ClientFormatDataResponse(clipboard->context, &response);
	}
	free(buff);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT
wf_cliprdr_common_format_data_response(CliprdrCommonContext* context,
                                       const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	wfClipboard* clipboard;

	if (!context || !formatDataResponse)
		return ERROR_INTERNAL_ERROR;

	clipboard = kosCustom2Clipboard(context->custom);

	if (!clipboard) {
		return ERROR_INTERNAL_ERROR;
	}

	if (formatDataResponse->msgFlags == CB_RESPONSE_OK) {
		clipboard->save_format_data_response(formatDataResponse);

	} else {
		SDL_Log("wf_cliprdr_client_format_data_response, formatDataResponse->msgFlags(0x%04x) != CB_RESPONSE_OK", 
			formatDataResponse->msgFlags);
		// return E_FAIL;
		// If here return error, freerdp's normal logic will result to exit cliprdr_server_thread,
		// It is not what I want to see. For now, return OK.
		// wfClipboard require preset error-flag know this format data request success or fail. 
		// For it, use pre_send_format_data_request, in this function set data-result to 'empty',
		// and above clipboard->save_format_data_response set data-result not 'empty'.
	}
	
	clipboard->response_data_event.Set();
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT
wf_cliprdr_common_file_contents_request(CliprdrCommonContext* context,
                                        const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	DWORD uSize = 0;
	BYTE* pData = NULL;
	HRESULT hRet = S_OK;
	wfClipboard* clipboard;
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT sRc;
	UINT32 cbRequested;

	if (!context || !fileContentsRequest)
		return ERROR_INTERNAL_ERROR;

	clipboard = kosCustom2Clipboard(context->custom);

	if (!clipboard)
		return ERROR_INTERNAL_ERROR;

	cbRequested = fileContentsRequest->cbRequested;
	if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
		cbRequested = sizeof(UINT64);

	pData = (BYTE*)calloc(1, cbRequested);

	if (!pData) {
		goto error;
	}

	// official's freerdp has tow method, one is IStream_Read/IStream_Seek, the other is below.
	// librose use second method only.
	if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
	{
		*((UINT32*)&pData[0]) =
			clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeLow;
		*((UINT32*)&pData[4]) =
			clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeHigh;
		uSize = cbRequested;
	}
	else if (fileContentsRequest->dwFlags == FILECONTENTS_RANGE)
	{
		BOOL bRet;
		bRet = wf_cliprdr_get_file_contents(clipboard,
			clipboard->file_names[fileContentsRequest->listIndex], pData,
			fileContentsRequest->nPositionLow, fileContentsRequest->nPositionHigh, cbRequested,
			&uSize);

		if (bRet == FALSE) {
			WLog_ERR(TAG, "get file contents failed.");
			uSize = 0;
			// goto error;
			// If here return error, freerdp's normal logic will result to exit cliprdr_server_thread,
			// It is not what I want to see. For now, return OK(rc = CHANNEL_RC_OK).
		}
	}

	rc = CHANNEL_RC_OK;
error:

	sRc = cliprdr_send_response_filecontents(clipboard, fileContentsRequest->streamId, uSize, pData);
	if (pData != nullptr) {
		free(pData);
		pData = nullptr;
	}

	if (sRc != CHANNEL_RC_OK) {
		return sRc;
	}

	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT
wf_cliprdr_common_file_contents_response(CliprdrCommonContext* context,
                                         const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	wfClipboard* clipboard;

	if (!context || !fileContentsResponse)
		return ERROR_INTERNAL_ERROR;

	clipboard = kosCustom2Clipboard(context->custom);
	if (!clipboard)
		return ERROR_INTERNAL_ERROR;

	if (fileContentsResponse->msgFlags == CB_RESPONSE_OK) {
		if ((uint32_t)fileContentsResponse->cbRequested > clipboard->req_fdata_size) {
			if (clipboard->req_fdata != nullptr) {
				free(clipboard->req_fdata);
			}
			clipboard->req_fdata_size = posix_align_ceil(fileContentsResponse->cbRequested, 4096);
			clipboard->req_fdata = (char*)malloc(clipboard->req_fdata_size);
		}

		if (!clipboard->req_fdata) {
			return ERROR_INTERNAL_ERROR;
		}

		clipboard->req_fsize = fileContentsResponse->cbRequested;
		clipboard->req_fstreamid = fileContentsResponse->streamId;
		CopyMemory(clipboard->req_fdata, fileContentsResponse->requestedData,
				   fileContentsResponse->cbRequested);

		{
			int ii = 0;
			WLog_INFO(TAG, "File Contents Response PDU, cbRequested: %u", 
				clipboard->req_fsize);
		}

	} else {
		SDL_Log("wf_cliprdr_client_file_contents_response, fileContentsResponse->msgFlags(0x%04x) != CB_RESPONSE_OK, req_fsize: %u", 
			fileContentsResponse->msgFlags, clipboard->req_fsize);
		// return E_FAIL;
		// If here return error, freerdp's normal logic will result to exit cliprdr_server_thread,
		// It is not what I want to see. For now, return OK, but set req_fsize/req_fstreamid to 0.
		// Of course, if the client is right, fileContentsResponse's two fields cbRequested and streamId should be 0, 
		// but for safe, we still assign additional 0 here.
		clipboard->req_fsize = 0;
		clipboard->req_fstreamid = FSTREAMID_FILECONTENTSRESPONSE_FAIL;
	}

	clipboard->req_fevent.Set();
	return CHANNEL_RC_OK;
}

void wf_cliprdr_text_clipboard_updated(CliprdrCommonContext* context, const char* text)
{
	wfClipboard* clipboard;

	if (!context) {
		return;
	}

	VALIDATE_IN_MAIN_THREAD();
	threading::lock lock(wfClipboard::deconstruct_mutex);

	clipboard = kosCustom2Clipboard(context->custom);
	if (!clipboard) {
		return;
	}

	clipboard->clipboard_require_update(false);
	clipboard->text_clipboard_updated(text);
}

static std::vector<std::string> split_with_null(const char* c_str, int size)
{
	VALIDATE(size >= 0, null_str);

	std::vector<std::string> ret;
	const char* file = nullptr;
	int len = 0;
	for (file = c_str; (len = strlen(file)) > 0; file += len + 1) {
		ret.push_back(file);
	}
	return ret;
}

void wf_cliprdr_hdrop_copied(CliprdrCommonContext* context, const char* c_str, int size)
{
	wfClipboard* clipboard;

	if (!context) {
		return;
	}

	VALIDATE_IN_MAIN_THREAD();
	threading::lock lock(wfClipboard::deconstruct_mutex);

	clipboard = kosCustom2Clipboard(context->custom);
	if (!clipboard) {
		return;
	}

	const std::vector<std::string> files = split_with_null(c_str, size);
	clipboard->clipboard_require_update(false);
	clipboard->hdrop_copied(files);
}

int wf_cliprdr_hdrop_paste(CliprdrCommonContext* context, const char* path, void* progress, char* err_msg, int max_bytes)
{
	wfClipboard* clipboard;

	if (!context) {
		return 0;
	}

	VALIDATE_IN_MAIN_THREAD();
	SDL_Log("%u, wf_cliprdr_hdrop_paste, pre lock(wfClipboard::deconstruct_mutex)", SDL_GetTicks());
	threading::lock lock(wfClipboard::deconstruct_mutex);

	clipboard = kosCustom2Clipboard(context->custom);
	if (!clipboard) {
		return 0;
	}

	// return clipboard->hdrop_paste(path, progress, err_msg, max_bytes);

	SDL_Log("%u, wf_cliprdr_hdrop_paste, pre clipboard->hdrop_paste(path, progress, err_msg, max_bytes)", SDL_GetTicks());
	int ret = clipboard->hdrop_paste(path, progress, err_msg, max_bytes);

	SDL_Log("%u, wf_cliprdr_hdrop_paste X", SDL_GetTicks());
	return ret;
}

BOOL wf_cliprdr_can_hdrop_paste(CliprdrCommonContext* context)
{
	wfClipboard* clipboard;

	if (!context) {
		return FALSE;
	}

	VALIDATE_IN_MAIN_THREAD();
	threading::lock lock(wfClipboard::deconstruct_mutex);

	clipboard = kosCustom2Clipboard(context->custom);
	if (!clipboard) {
		return FALSE;
	}

	return clipboard->can_hdrop_paste();
}


threading::mutex wfClipboard::deconstruct_mutex;

void wfClipboard::start_internal()
{
	VALIDATE_NOT_MAIN_THREAD();
	clipboard_thread_checker_.reset(new rtc::ThreadChecker);
	cliprdr_thread_func();
}

void wfClipboard::start()
{
	CHECK(thread_.get() == nullptr);
	thread_.reset(new base::Thread("ClipboardMainThread"));

	base::Thread::Options socket_thread_options;
	socket_thread_options.message_pump_type = base::MessagePumpType::IO;
	socket_thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
	CHECK(thread_->StartWithOptions(socket_thread_options));

	thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&wfClipboard::start_internal, base::Unretained(this)));
}

void wfClipboard::stop()
{
	// VALIDATE_IN_MAIN_THREAD();
	if (thread_.get() == nullptr) {
		return;
	}

	// Clipboard may be pasting
	response_data_event.Set();
	req_fevent.Set();
	trigger_quit();

	SDL_Log("%u wfClipboard::stop---", SDL_GetTicks());
	thread_.reset();
	SDL_Log("%u ---wfClipboard::stop X", SDL_GetTicks());
}


std::string cliprdr_code_2_str(const std::string& file, int errcode)
{
	std::string result;
	utils::string_map symbols;
	if (errcode == cliprdr_errcode_file) {
		symbols["file"] = file;
		result = vgettext2("Paste $file fail, Cancel paste", symbols);

	} else if (errcode == cliprdr_errcode_directory) {
		symbols["directory"] = file;
		result = vgettext2("Create $directory fail, Cancel paste", symbols);

	} else if (errcode == cliprdr_errcode_format) {
		result = vgettext2("Get remote format fail, Cancel paste", symbols);

	} else if (errcode == cliprdr_errcode_clipboardupdate) {
		symbols["file"] = file;
		result = vgettext2("Paste $file fail, remote clipboard's content was changed. Cancel paste", symbols);

	} else if (errcode == cliprdr_errcode_nospace) {
		symbols["file"] = file;
		result = vgettext2("Paste $file fail, write fail, may be not enough space. Cancel paste", symbols);

	} else if (errcode == cliprdr_errcode_cancel) {

	}
	return result;
}

std::string cliprdr_msgid_2_str(int msgid, const std::string& symbol)
{
	std::string result;
	utils::string_map symbols;
	if (msgid == cliprdr_msgid_copy) {
		result = _("Copy");

	} else if (msgid == cliprdr_msgid_paste) {
		result = _("Paste");

	} else if (msgid == cliprdr_msgid_pastingwarn) {
		VALIDATE(!symbol.empty(), null_str);
		symbols["app"] = symbol;
		result = vgettext2("When pasting, please Must not switch $app to background, otherwise will cause $app to exit illegally", symbols);

	} else if (msgid == cliprdr_msgid_nooperator) {
		result = _("Copy require select item. Paste require client copy item");

	} else if (msgid == cliprdr_msgid_noclient) {
		result = _("No client, cannot copy or paste");

	}

	return result;
}