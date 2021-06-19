/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cliprdr LEAGOR
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPLEAGOR_COMMON_H
#define FREERDP_CHANNEL_RDPLEAGOR_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/leagor.h>
#include <freerdp/api.h>

FREERDP_LOCAL wStream* leagor_packet_new(UINT16 msgType, UINT16 msgFlags, UINT32 dataLen);
FREERDP_LOCAL wStream* leagor_common_send_explorer_update(const LEAGOR_EXPLORER_UPDATE* update);
FREERDP_LOCAL UINT leagor_common_receive_explorer_update(LeagorCommonContext* context, wStream* s);

#endif /* FREERDP_CHANNEL_RDPECLIP_COMMON_H */
