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

#ifndef FREERDP_CHANNEL_LEAGOR_COMMON2_H
#define FREERDP_CHANNEL_LEAGOR_COMMON2_H

#include <freerdp/channels/leagor_common_context.h>
#include <string>

/**
 * Server/Client Interface
 */

UINT wf_leagor_common_recv_capabilities(LeagorCommonContext* context,
                                           const LEAGOR_CAPABILITIES* capabilities);
UINT wf_leagor_common_orientation_update(LeagorCommonContext* context,
                                          uint32_t initial, uint32_t current);
UINT wf_leagor_common_recv_explorer_update(LeagorCommonContext* context, const LEAGOR_EXPLORER_UPDATE* update);

UINT leagor_send_explorer_update(LeagorCommonContext* context, const LEAGOR_EXPLORER_UPDATE* update);
UINT leagor_send_orientation_update(LeagorCommonContext* context, uint32_t initial, uint32_t current);

#endif /* FREERDP_CHANNEL_LEAGOR_COMMON2_H */
