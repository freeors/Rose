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
#ifndef FREERDP_SERVER_SHADOW_CLIPRDR_H
#define FREERDP_SERVER_SHADOW_CLIPRDR_H

#include <freerdp/server/shadow.h>

#ifdef __cplusplus
extern "C"
{
#endif

	int shadow_client_cliprdr_init(rdpShadowClient* client);
	void shadow_client_cliprdr_uninit(rdpShadowClient* client);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_CLIPRDR_H */
