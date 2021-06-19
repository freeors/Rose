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
#include <freerdp/server/cliprdr.h>

#include "shadow_cliprdr.h"
#include <freerdp/channels/impl_cliprdr_os.h>
#include <freerdp/channels/cliprdr_common2.hpp>

#include "base_instance.hpp"
#include "game_config.hpp"

// chromium
#include "base/bind.h"

using namespace std::placeholders;

#define TAG CLIENT_TAG("windows")


int shadow_client_cliprdr_init(rdpShadowClient* client)
{
	VALIDATE_IN_RDPD_THREAD();
	SDL_Log("shadow_client_cliprdr_init, sizeof(FILEDESCRIPTOR): %i =>(592) MAX_PATH: %i =>(260), sizeof(wchar_t): %i", (int)sizeof(FILEDESCRIPTOR), MAX_PATH, (int)sizeof(wchar_t));

	CliprdrServerContext* cliprdr;
	cliprdr = client->cliprdr = cliprdr_server_context_new(client->vcm);

	if (!cliprdr)
	{
		return 0;
	}
	cliprdr->server = TRUE;
	VALIDATE(cliprdr->server, null_str);

	wfClipboard* clipboard = kosNewClipboard();
	if (!clipboard) {
		goto error;
	}

	clipboard->context = cliprdr;
	clipboard->sync = FALSE;
	clipboard->map_capacity = 32;
	clipboard->map_size = 0;

	if (!(clipboard->format_mappings =
	          (formatMapping*)calloc(clipboard->map_capacity, sizeof(formatMapping))))
		goto error;

	clipboard->start();

	cliprdr->rdpcontext = (rdpContext*)client;

	/* enable all capabilities */
	cliprdr->useLongFormatNames = TRUE;
	cliprdr->streamFileClipEnabled = TRUE;
	cliprdr->fileClipNoFilePaths = TRUE;
	// cliprdr->canLockClipData = TRUE;
	cliprdr->canLockClipData = FALSE;

	/* disable initialization sequence, for caps sync */
	// cliprdr->autoInitializationSequence = FALSE;

	/* Set server callbacks */
	cliprdr->ClientCapabilities = wf_cliprdr_common_capabilities;
	// cliprdr->TempDirectory = pf_cliprdr_TempDirectory;
	cliprdr->ClientFormatList = wf_cliprdr_common_format_list;
	cliprdr->ClientFormatListResponse = wf_cliprdr_common_format_list_response;
	cliprdr->ClientLockClipboardData = wf_cliprdr_common_lock_clipboard_data;
	cliprdr->ClientUnlockClipboardData = wf_cliprdr_common_unlock_clipboard_data;
	cliprdr->ClientFormatDataRequest = wf_cliprdr_common_format_data_request;
	cliprdr->ClientFormatDataResponse = wf_cliprdr_common_format_data_response;
	cliprdr->ClientFileContentsRequest = wf_cliprdr_common_file_contents_request;
	cliprdr->ClientFileContentsResponse = wf_cliprdr_common_file_contents_response;
	cliprdr->TextClipboardUpdated = wf_cliprdr_text_clipboard_updated;
	cliprdr->HdropCopied = wf_cliprdr_hdrop_copied;
	cliprdr->HdropPaste = wf_cliprdr_hdrop_paste;
	cliprdr->CanHdropPaste = wf_cliprdr_can_hdrop_paste;

	cliprdr->custom = (void*)clipboard;
	cliprdr->Start(cliprdr);
	return 1;

error:
	shadow_client_cliprdr_uninit(client);
	return 0;
}

void shadow_client_cliprdr_uninit(rdpShadowClient* client)
{
	VALIDATE_IN_RDPD_THREAD();
	SDL_Log("shadow_client_cliprdr_uninit--- client: %p", client);
	if (client->cliprdr)
	{
		client->cliprdr->Stop(client->cliprdr);

		SDL_Log("%u, shadow_client_cliprdr_uninit 1", SDL_GetTicks());
		threading::lock lock(wfClipboard::deconstruct_mutex);
		wfClipboard* clipboard = kosCustom2Clipboard(client->cliprdr->custom);
		if (clipboard) {
			client->cliprdr->custom = NULL;

			SDL_Log("%u shadow_client_cliprdr_uninit 2", SDL_GetTicks());
			clipboard->stop();

			SDL_Log("shadow_client_cliprdr_uninit 3");
			cliprdr_clear_file_array(clipboard);

			SDL_Log("shadow_client_cliprdr_uninit 4");
			cliprdr_clear_format_map(clipboard);

			SDL_Log("shadow_client_cliprdr_uninit 5");
			free(clipboard->format_mappings);

			SDL_Log("shadow_client_cliprdr_uninit 6");
			delete clipboard;
		}
		SDL_Log("shadow_client_cliprdr_uninit 7");

		cliprdr_server_context_free(client->cliprdr);
		client->cliprdr = NULL;
	}
	SDL_Log("---shadow_client_cliprdr_uninit X");
}