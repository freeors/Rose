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

#ifndef FREERDP_CHANNEL_CLIPRDR_COMMON2_H
#define FREERDP_CHANNEL_CLIPRDR_COMMON2_H

#include <freerdp/channels/cliprdr_common_context.h>
#include <string>

// client special interface 
UINT wf_cliprdr_monitor_ready(CliprdrCommonContext* context,
                                     const CLIPRDR_MONITOR_READY* monitorReady);

/**
 * Server/Client Interface
 */

UINT wf_cliprdr_common_capabilities(CliprdrCommonContext* context,
                                           const CLIPRDR_CAPABILITIES* capabilities);
UINT wf_cliprdr_common_format_list(CliprdrCommonContext* context,
                                          const CLIPRDR_FORMAT_LIST* formatList);
UINT wf_cliprdr_common_format_list_response(CliprdrCommonContext* context,
                                       const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
UINT wf_cliprdr_common_lock_clipboard_data(CliprdrCommonContext* context,
                                      const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
UINT wf_cliprdr_common_unlock_clipboard_data(CliprdrCommonContext* context,
                                        const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
UINT wf_cliprdr_common_format_data_request(CliprdrCommonContext* context,
                                      const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
UINT wf_cliprdr_common_format_data_response(CliprdrCommonContext* context,
                                       const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
UINT wf_cliprdr_common_file_contents_request(CliprdrCommonContext* context,
                                        const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
UINT wf_cliprdr_common_file_contents_response(CliprdrCommonContext* context,
                                         const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse);

void wf_cliprdr_text_clipboard_updated(CliprdrCommonContext* context, const char* text);
void wf_cliprdr_hdrop_copied(CliprdrCommonContext* context, const char* c_str, int size);
int wf_cliprdr_hdrop_paste(CliprdrCommonContext* context, const char* path, void* progress, char* err_msg, int max_bytes);
BOOL wf_cliprdr_can_hdrop_paste(CliprdrCommonContext* context);

// util function
std::string cliprdr_code_2_str(const std::string& file, int errcode);

enum {cliprdr_msgid_copy, cliprdr_msgid_paste, cliprdr_msgid_pastingwarn, cliprdr_msgid_nooperator, cliprdr_msgid_noclient};
std::string cliprdr_msgid_2_str(int msgid, const std::string& symbol);

#endif /* FREERDP_CHANNEL_CLIPRDR_COMMON2_H */
