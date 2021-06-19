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

#ifndef FREERDP_CHANNEL_LEAGOR_COMMON_CONTEXT_H
#define FREERDP_CHANNEL_LEAGOR_COMMON_CONTEXT_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>

#include <freerdp/channels/leagor.h>
#include <freerdp/client/leagor.h>

/**
 * Server/Client Interface
 */

typedef struct _leagor_common_context LeagorCommonContext;

typedef UINT (*psLeagorOpen)(LeagorCommonContext* context);
typedef UINT (*psLeagorClose)(LeagorCommonContext* context);
typedef UINT (*psLeagorStart)(LeagorCommonContext* context);
typedef UINT (*psLeagorStop)(LeagorCommonContext* context);
typedef HANDLE (*psLeagorGetEventHandle)(LeagorCommonContext* context);
typedef UINT (*psLeagorCheckEventHandle)(LeagorCommonContext* context);

typedef UINT (*psLeagorSendCapabilities)(LeagorCommonContext* context,
                                            const LEAGOR_CAPABILITIES* capabilities);
typedef UINT (*psLeagorRecvCapabilities)(LeagorCommonContext* context,
                                            const LEAGOR_CAPABILITIES* capabilities);
typedef UINT (*psServerOrientationUpdate)(LeagorCommonContext* context, uint32_t initial, uint32_t current);
typedef UINT (*psSendExplorerUpdate)(LeagorCommonContext* context, const LEAGOR_EXPLORER_UPDATE* update);
typedef UINT (*psRecvExplorerUpdate)(LeagorCommonContext* context, const LEAGOR_EXPLORER_UPDATE* update);

struct _leagor_common_context
{
	void* handle;
	// void* custom;

	/* server clipboard capabilities - set by server - updated by the channel after client
	 * capability exchange */
	BOOL canDropCopy;

	psLeagorOpen Open;
	psLeagorClose Close;
	psLeagorStart Start;
	psLeagorStop Stop;
	psLeagorGetEventHandle GetEventHandle;
	psLeagorCheckEventHandle CheckEventHandle;

	psLeagorSendCapabilities SendCapabilities;
	psLeagorRecvCapabilities RecvCapabilities;
	// server: I will send orientation update pdu. client: I reaceived orientation update pdu.
	psServerOrientationUpdate ServerOrientationUpdate;
	psSendExplorerUpdate SendExplorerUpdate;
	psRecvExplorerUpdate RecvExplorerUpdate;

	rdpContext* rdpcontext;
	BOOL server; // ture: used to server/false: used t client

	// used for client only.
	uint32_t initialOrientation;
	uint32_t currentOrientation;
};

#endif /* FREERDP_CHANNEL_LEAGOR_COMMON_CONTEXT_H */
