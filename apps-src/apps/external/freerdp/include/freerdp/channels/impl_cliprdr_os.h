/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
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
#ifndef FREERDP_CHANNEL_IMPL_CLIPRDR_OS_H
#define FREERDP_CHANNEL_IMPL_CLIPRDR_OS_H

#include <freerdp/channels/cliprdr_common_context.h>
#include <freerdp/channels/cliprdr.h>

// chromiun
#include <rtc_base/event.h>
#include <rtc_base/thread_checker.h>
#include "base/threading/thread.h"

#include "thread.hpp"

#define CFSTR_FILEDESCRIPTORW_UTF8        "FileGroupDescriptorW"                // CF_FILEGROUPDESCRIPTORW
#define CFSTR_FILECONTENTS_UTF8           "FileContents"                        // CF_FILECONTENTS

#define FSTREAMID_FILECONTENTSRESPONSE_FAIL	0x5a5a

#ifndef _WIN32

typedef struct _FILEGROUPDESCRIPTOR { // fgd
     UINT cItems;
     FILEDESCRIPTOR fgd[1];
} FILEGROUPDESCRIPTOR;

#endif

#ifdef WITH_DEBUG_CLIPRDR
#define DEBUG_CLIPRDR(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_CLIPRDR(...) \
	do                     \
	{                      \
	} while (0)
#endif

struct formatMapping
{
	UINT32 remote_format_id;
	UINT32 local_format_id;
	wchar_t* name;
};

class wfClipboard
{
public:
	static threading::mutex deconstruct_mutex;

	wfClipboard()
		: context(nullptr)
		, sync(FALSE)
		, capabilities(0)
		, map_size(0)
		, map_capacity(0)
		, format_mappings(nullptr)
		, requestedFormatId(0)
		, response_data_event(true, false)
		, req_fdata_size(0)
		, req_fsize(0)
		, req_fstreamid(0)
		, req_fdata(nullptr)
		, req_fevent(true, false)
		, nFiles(0)
		, file_array_size(0)
		, file_names(nullptr)
		, fileDescriptor(nullptr)
		// GUI already provides a "Cancel" button, in theory can be set to rtc::Event::kForever. 
		// But for security, set a time, can be set to a larger.
		, event_overflow_ms(15000) // 15000, rtc::Event::kForever
	{}
	virtual ~wfClipboard() 
	{
		if (req_fdata != nullptr) {
			free(req_fdata);
		}
	}

	void start();
	void stop();
	virtual void text_clipboard_updated(const std::string& text) {}
	virtual void hdrop_copied(const std::vector<std::string>& files) {}
	virtual int hdrop_paste(const char* path, void* progress, char* err_msg, int max_bytes) { return 0; }
	virtual bool can_hdrop_paste() { return true; }
	virtual bool open_clipboard() { return true; }
	virtual bool close_clipboard() { return true; }
	virtual bool empty_clipboard() { return true; }
	virtual int count_clipboard_formats() { return 0; }
	virtual bool is_clipboard_format_available(uint32_t format) { return false; }
	virtual int get_clipboard_format_name_A(uint32_t format, char* format_name_ptr, int cchMaxCount) { return 0; }
	virtual uint32_t enum_clipboard_formats(uint32_t format) { return 0; }
	virtual uint32_t register_clipboard_format(const std::string& utf8) { return 0; }
	virtual int get_format_data(bool hdrop, uint32_t format, uint8_t** ppdata) { return 0; }
	virtual void cliprdr_thread_func() {}
	virtual void save_format_data_response(const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse) {}
	virtual void* set_clipboard_data(uint32_t uFormat, const void* hMem) { return nullptr; }
	virtual uint32_t set_dataobject_2_clipboard() { return 0; }
	virtual void clipboard_require_update(bool remote) {}
	virtual void pre_send_format_data_request() {}

private:
	void start_internal();
	virtual void trigger_quit() {}

public:
	const int event_overflow_ms;
	CliprdrCommonContext* context;

	BOOL sync;
	UINT32 capabilities;

	size_t map_size;
	size_t map_capacity;
	formatMapping* format_mappings;

	UINT32 requestedFormatId;

	std::unique_ptr<base::Thread> thread_;
	std::unique_ptr<rtc::ThreadChecker> clipboard_thread_checker_;

	rtc::Event response_data_event;

	uint32_t req_fdata_size;
	uint32_t req_fsize;
	UINT32 req_fstreamid;
	char* req_fdata;

	rtc::Event req_fevent;

	size_t nFiles;
	size_t file_array_size;
	wchar_t** file_names;
	FILEDESCRIPTOR** fileDescriptor;
};

wfClipboard* kosNewClipboard();
wfClipboard* kosCustom2Clipboard(void* custom);
UINT cliprdr_send_request_filecontents(wfClipboard* clipboard, const void* streamid, ULONG index,
                                       UINT32 flag, DWORD positionhigh, DWORD positionlow,
                                       ULONG nreq);
UINT32 get_remote_format_id(wfClipboard* clipboard, UINT32 local_format);
UINT cliprdr_send_data_request(wfClipboard* clipboard, UINT32 formatId);
UINT cliprdr_send_format_list(wfClipboard* clipboard);
BOOL wf_cliprdr_process_filename(wfClipboard* clipboard, const wchar_t* wFileName, size_t str_len);
void cliprdr_clear_file_array(wfClipboard* clipboard);
BOOL cliprdr_clear_format_map(wfClipboard* clipboard);

#endif /* FREERDP_SERVER_SHADOW_CLIPRDR_OS_H */
