/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_LEAGOR_H
#define FREERDP_CHANNEL_LEAGOR_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/shell.h>

#define LEAGOR_SVC_CHANNEL_NAME "leagor"

/* CLIPRDR_HEADER.msgType */
#define LG_CAPABILITIES 0x0001
#define LG_ORIENTATION_UPDATE 0x0002
#define LG_EXPLORER_UPDATE 0x0003

/* CLIPRDR_GENERAL_CAPABILITY.lengthCapability */
#define LG_CAPSTYPE_GENERAL_LEN 8

/* CLIPRDR_GENERAL_CAPABILITY.version */
#define LG_CAPS_VERSION_1 0x00000001

/* CLIPRDR_GENERAL_CAPABILITY.flags */
#define LG_CAP_DROP_COPY 0x00000001

/* LEAGOR_ORIENTATION_UPDATE.dataLen */
#define LG_ORIENTATION_UPDATE_LEN 8

/* LEAGOR_EXPLORER.code */
#define LG_EXPLORER_CODE_SHOWN 0x00000001
#define LG_EXPLORER_CODE_HIDDEN 0x00000002
#define LG_EXPLORER_CODE_START_DRAG 0x00000004
#define LG_EXPLORER_CODE_END_DRAG 0x00000008
#define LG_EXPLORER_CODE_CAN_PASTE 0x00000010

#define LG_EXPLORER_UPDATE_LEN	16  // sizeof(LEAGOR_EXPLORER_UPDATE)

#define LG_START_DRAG_FORM_DATA3(files, has_folder)	((uint32_t)(((uint16_t)(files)) | ((uint32_t)((uint16_t)(has_folder))) << 16))
#define LG_START_DRAG_FILES_MASK	0xffff
#define LG_START_DRAG_HAS_FOLDER_FLAG	0x10000

#define LG_END_DRAG_FORM_DATA3(up_result)	((uint32_t)(((uint16_t)(0)) | ((uint32_t)((uint16_t)(up_result))) << 16))
#define LG_END_DRAG_UP_RESULT_FLAG	0x10000


/* Clipboard Messages */

#define DEFINE_LEAGOR_HEADER_COMMON() \
	UINT16 msgType;                    \
	UINT16 msgFlags;                   \
	UINT32 dataLen

struct _LEAGOR_HEADER
{
	DEFINE_LEAGOR_HEADER_COMMON();
};
typedef struct _LEAGOR_HEADER LEAGOR_HEADER;

struct _LEAGOR_CAPABILITIES
{
	DEFINE_LEAGOR_HEADER_COMMON();

	UINT32 version;
	UINT32 flags;
};
typedef struct _LEAGOR_CAPABILITIES LEAGOR_CAPABILITIES;

struct _LEAGOR_EXPLORER_UPDATE
{
	DEFINE_LEAGOR_HEADER_COMMON();
	UINT32 code;
	UINT32 data1;
	UINT32 data2;
	UINT32 data3;
};
typedef struct _LEAGOR_EXPLORER_UPDATE LEAGOR_EXPLORER_UPDATE;

#endif /* FREERDP_CHANNEL_LEAGOR_H */
