/**
 * WinPR: Windows Portable Runtime
 * Message Queue
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#if defined(_WIN32) || defined(ANDROID) || defined(__APPLE__)
#include "freerdp_config.h"
#endif

#include <winpr/crt.h>
#include <winpr/sysinfo.h>

#include <winpr/collections.h>

#include <SDL_log.h>

/**
 * Message Queue inspired from Windows:
 * http://msdn.microsoft.com/en-us/library/ms632590/
 */

/**
 * Properties
 */

/**
 * Gets an event which is set when the queue is non-empty
 */

HANDLE MessageQueue_Event(wMessageQueue* queue)
{
	return queue->event;
}

/**
 * Gets the queue size
 */

int MessageQueue_Size(wMessageQueue* queue)
{
	return queue->size;
}

/**
 * Methods
 */

BOOL MessageQueue_Wait(wMessageQueue* queue)
{
	BOOL status = FALSE;

	if (WaitForSingleObject(queue->event, INFINITE) == WAIT_OBJECT_0)
		status = TRUE;

	return status;
}

BOOL MessageQueue_Dispatch(wMessageQueue* queue, wMessage* message)
{
	BOOL ret = FALSE;
	if (!queue || !message)
		return FALSE;

	EnterCriticalSection(&queue->lock);

	if (queue->size == queue->capacity)
	{
		int old_capacity;
		int new_capacity;
		wMessage* new_arr;

		old_capacity = queue->capacity;
		new_capacity = queue->capacity * 2;

		new_arr = (wMessage*)realloc(queue->array, sizeof(wMessage) * new_capacity);
		if (!new_arr)
			goto out;
		queue->array = new_arr;
		queue->capacity = new_capacity;
		ZeroMemory(&(queue->array[old_capacity]), (new_capacity - old_capacity) * sizeof(wMessage));

		/* rearrange wrapped entries */
		if (queue->tail <= queue->head)
		{
			CopyMemory(&(queue->array[old_capacity]), queue->array, queue->tail * sizeof(wMessage));
			queue->tail += old_capacity;
		}
	}

	CopyMemory(&(queue->array[queue->tail]), message, sizeof(wMessage));
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->size++;

	message = &(queue->array[queue->tail]);
	message->time = GetTickCount64();

	if (queue->size > 0)
		SetEvent(queue->event);

	ret = TRUE;
out:
	LeaveCriticalSection(&queue->lock);
	return ret;
}

BOOL MessageQueue_Post(wMessageQueue* queue, void* context, UINT32 type, void* wParam, void* lParam)
{
	wMessage message;

	message.context = context;
	message.id = type;
	message.wParam = wParam;
	message.lParam = lParam;
	message.Free = NULL;

	return MessageQueue_Dispatch(queue, &message);
}

BOOL MessageQueue_PostQuit(wMessageQueue* queue, int nExitCode)
{
	return MessageQueue_Post(queue, NULL, WMQ_QUIT, (void*)(size_t)nExitCode, NULL);
}

int MessageQueue_Get(wMessageQueue* queue, wMessage* message)
{
	int status = -1;

	if (!MessageQueue_Wait(queue))
		return status;

	EnterCriticalSection(&queue->lock);

	if (queue->size > 0)
	{
		CopyMemory(message, &(queue->array[queue->head]), sizeof(wMessage));
		ZeroMemory(&(queue->array[queue->head]), sizeof(wMessage));
		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;

		if (queue->size < 1)
			ResetEvent(queue->event);

		status = (message->id != WMQ_QUIT) ? 1 : 0;
	}

	LeaveCriticalSection(&queue->lock);

	return status;
}

int MessageQueue_Peek(wMessageQueue* queue, wMessage* message, BOOL remove)
{
	int status = 0;

	EnterCriticalSection(&queue->lock);

	if (queue->size > 0)
	{
		CopyMemory(message, &(queue->array[queue->head]), sizeof(wMessage));
		status = 1;

		if (remove)
		{
			ZeroMemory(&(queue->array[queue->head]), sizeof(wMessage));
			queue->head = (queue->head + 1) % queue->capacity;
			queue->size--;

			if (queue->size < 1)
				ResetEvent(queue->event);
		}
	}

	LeaveCriticalSection(&queue->lock);

	return status;
}

/**
 * Construction, Destruction
 */

wMessageQueue* MessageQueue_New(const wObject* callback)
{
	wMessageQueue* queue = NULL;

	queue = (wMessageQueue*)calloc(1, sizeof(wMessageQueue));
	if (!queue)
		return NULL;

	queue->capacity = 32;
	queue->array = (wMessage*)calloc(queue->capacity, sizeof(wMessage));
	if (!queue->array)
		goto error_array;

	if (!InitializeCriticalSectionAndSpinCount(&queue->lock, 4000))
		goto error_spinlock;

	queue->event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!queue->event)
		goto error_event;

	if (callback)
		queue->object = *callback;

	return queue;

error_event:
	DeleteCriticalSection(&queue->lock);
error_spinlock:
	free(queue->array);
error_array:
	free(queue);
	return NULL;
}

void MessageQueue_Free(wMessageQueue* queue)
{
	if (!queue)
		return;

	SDL_Log("MessageQueue_Free--- queue: %p", queue);

	MessageQueue_Clear(queue);

	SDL_Log("MessageQueue_Free 1");

	CloseHandle(queue->event);
	SDL_Log("MessageQueue_Free 2");
	DeleteCriticalSection(&queue->lock);

	SDL_Log("MessageQueue_Free 3, queue->array: %p", queue->array);
	free(queue->array);

	SDL_Log("MessageQueue_Free 4, queue: %p", queue);
	free(queue);
	SDL_Log("---MessageQueue_Free X");
}

int MessageQueue_Clear(wMessageQueue* queue)
{
	int status = 0;

	EnterCriticalSection(&queue->lock);

	SDL_Log("MessageQueue_Clear--- queue->size: %i", queue->size);
	while (queue->size > 0)
	{
		wMessage* msg = &(queue->array[queue->head]);

		SDL_Log("MessageQueue_Clear, 1, queue->size: %i. queue->object.fnObjectUninit: %p", queue->size, queue->object.fnObjectUninit);
		/* Free resources of message. */
		if (queue->object.fnObjectUninit)
			queue->object.fnObjectUninit(msg);
		SDL_Log("MessageQueue_Clear, 2, queue->size: %i, queue->object.fnObjectFree: %p", queue->size, queue->object.fnObjectFree);
		if (queue->object.fnObjectFree)
			queue->object.fnObjectFree(msg);

		SDL_Log("MessageQueue_Clear, 3, queue->size: %i", queue->size);
		ZeroMemory(msg, sizeof(wMessage));

		queue->head = (queue->head + 1) % queue->capacity;
		queue->size--;
	}
	SDL_Log("MessageQueue_Clear, 4, queue->size: %i", queue->size);
	ResetEvent(queue->event);

	LeaveCriticalSection(&queue->lock);

	return status;
}
