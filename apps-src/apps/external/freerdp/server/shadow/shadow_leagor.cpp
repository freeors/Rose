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
#include <freerdp/server/leagor.h>

#include "shadow_leagor.h"
#include <freerdp/channels/leagor_common_context.h>
#include <freerdp/channels/leagor_common2.hpp>

#include "base_instance.hpp"

// chromium
#include "base/bind.h"

using namespace std::placeholders;

#define TAG CLIENT_TAG("windows")


void leagorchannel_send_video_orientation_request(rdpContext* context, uint32_t initial, uint32_t current)
{
	rdpShadowClient* client = (rdpShadowClient*)context;
	if (client->leagor == nullptr) {
		// client does not support leagor ext, for example: mstsc.exe.
		return;
	}

	LeagorCommonContext* leagor = client->leagor;
	if (leagor->initialOrientation == initial && leagor->currentOrientation == current) {
		return;
	}

	leagor_send_orientation_update(leagor, initial, current);

	leagor->initialOrientation = initial;
	leagor->currentOrientation = current;
}

void leagorchannel_send_explorer_update(rdpContext* context, const LEAGOR_EXPLORER_UPDATE* update)
{
	rdpShadowClient* client = (rdpShadowClient*)context;
	if (client->leagor == nullptr) {
		// client does not support leagor ext, for example: mstsc.exe.
		return;
	}

	LeagorCommonContext* leagor = client->leagor;
	leagor_send_explorer_update(leagor, update);
}




int shadow_client_leagor_init(rdpShadowClient* client)
{
	SDL_Log("shadow_client_leagor_init, sizeof(FILEDESCRIPTOR): %i =>(592) MAX_PATH: %i =>(260), sizeof(wchar_t): %i", (int)sizeof(FILEDESCRIPTOR), MAX_PATH, (int)sizeof(wchar_t));

	LeagorCommonContext* cliprdr;
	cliprdr = client->leagor = leagor_server_context_new(client->vcm);

	if (!cliprdr)
	{
		return 0;
	}
	cliprdr->server = TRUE;
	VALIDATE(cliprdr->server, null_str);

	cliprdr->initialOrientation = nposm;
	cliprdr->currentOrientation = nposm;

	cliprdr->rdpcontext = (rdpContext*)client;

	// disable initialization sequence, for caps sync 

	// Set server callbacks
	cliprdr->RecvCapabilities = wf_leagor_server_receive_capabilities;
	cliprdr->RecvExplorerUpdate = wf_leagor_server_receive_explorer_update;


	// cliprdr->custom = NULL;
	cliprdr->Start(cliprdr);
	return 1;
}

void shadow_client_leagor_uninit(rdpShadowClient* client)
{
	SDL_Log("shadow_client_leagor_uninit--- client: %p", client);
	if (client->leagor)
	{
		client->leagor->Stop(client->leagor);

		leagor_server_context_free(client->leagor);
		client->leagor = NULL;
	}
	SDL_Log("---shadow_client_leagor_uninit X");
}