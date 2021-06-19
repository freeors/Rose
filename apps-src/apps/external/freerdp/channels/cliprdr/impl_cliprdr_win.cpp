/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef _WIN32
#error "This file is impletement of libkosapi.so on windows!"
#else
#if !defined(UNICODE)
#error "On windows, this file must complete Use Unicode Character Set"
#endif
#endif

#define CINTERFACE
#define COBJMACROS

#include <ole2.h>
#include <shlobj.h>
#include <windows.h>
#include <winuser.h>

#include <assert.h>
#include <strsafe.h>


#include <SDL_log.h>
#include <utility>

#include <kosapi/sys2.h>

#include "serialization/string_utils.hpp"
#include "thread.hpp"
#include "wml_exception.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"
#include "base_instance.hpp"
#include "rtc_base/bind.h"

#include <freerdp/channels/impl_cliprdr_os.h>

struct CliprdrEnumFORMATETC
{
	IEnumFORMATETC iEnumFORMATETC;

	LONG m_lRefCount;
	LONG m_nIndex;
	LONG m_nNumFormats;
	FORMATETC* m_pFormatEtc;
};

struct CliprdrStream
{
	IStream iStream;

	LONG m_lRefCount;
	ULONG m_lIndex;
	ULARGE_INTEGER m_lSize;
	ULARGE_INTEGER m_lOffset;
	FILEDESCRIPTOR m_Dsc;
	void* m_pData;
};

struct CliprdrDataObject
{
	IDataObject iDataObject;

	LONG m_lRefCount;
	FORMATETC* m_pFormatEtc;
	STGMEDIUM* m_pStgMedium;
	ULONG m_nNumFormats;
	ULONG m_nStreams;
	IStream** m_pStream;
	void* m_pData;
};

typedef BOOL(WINAPI* fnAddClipboardFormatListener)(HWND hwnd);
typedef BOOL(WINAPI* fnRemoveClipboardFormatListener)(HWND hwnd);
typedef BOOL(WINAPI* fnGetUpdatedClipboardFormats)(PUINT lpuiFormats, UINT cFormats,
                                                   PUINT pcFormatsOut);

class wfClipboard_win: public wfClipboard
{
public:
	wfClipboard_win();
	~wfClipboard_win() {}

	bool open_clipboard() override;
	bool close_clipboard() override;
	bool empty_clipboard() override;
	int count_clipboard_formats() override;
	bool is_clipboard_format_available(uint32_t format) override;
	int get_clipboard_format_name_A(uint32_t format, char* format_name_ptr, int cchMaxCount) override;
	uint32_t enum_clipboard_formats(uint32_t format) override;
	uint32_t register_clipboard_format(const std::string& utf8) override;
	int get_format_data(bool hdrop, uint32_t format, uint8_t** ppdata) override;
	void save_format_data_response(const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse) override;
	void* set_clipboard_data(uint32_t uFormat, const void* hMem) override;

private:
	void cliprdr_thread_func() override;
	uint32_t set_dataobject_2_clipboard() override;
	void trigger_quit() override;

	long IDataObject_GetData2(IDataObject* pDataObject, FORMATETC *pformatetcIn, STGMEDIUM *pmedium);
	void IDataObject_Release2(IDataObject* pDataObject);


public:
	HWND hwnd;
	HANDLE hmem;
	LPDATAOBJECT data_obj;

	// BOOL legacyApi; must be false
	HMODULE hUser32;
	fnAddClipboardFormatListener AddClipboardFormatListener;
	fnRemoveClipboardFormatListener RemoveClipboardFormatListener;
	fnGetUpdatedClipboardFormats GetUpdatedClipboardFormats;

};

wfClipboard_win::wfClipboard_win()
	: hwnd(NULL)
	, hmem(NULL)
	, data_obj(nullptr)
	, hUser32(NULL)
	, AddClipboardFormatListener(nullptr)
	, RemoveClipboardFormatListener(nullptr)
	, GetUpdatedClipboardFormats(nullptr)
{
	hUser32 = LoadLibraryA("user32.dll");
	if (hUser32) {
		AddClipboardFormatListener = (fnAddClipboardFormatListener)GetProcAddress(
		    hUser32, "AddClipboardFormatListener");
		RemoveClipboardFormatListener = (fnRemoveClipboardFormatListener)GetProcAddress(
		    hUser32, "RemoveClipboardFormatListener");
		GetUpdatedClipboardFormats = (fnGetUpdatedClipboardFormats)GetProcAddress(
		    hUser32, "GetUpdatedClipboardFormats");
	}

	VALIDATE(hUser32 && AddClipboardFormatListener && RemoveClipboardFormatListener && GetUpdatedClipboardFormats, "os must support Clipboard Format Listener");
}

bool wfClipboard_win::open_clipboard()
{
	size_t x;
	for (x = 0; x < 10; x++)
	{
		if (OpenClipboard(hwnd))
			return true;
		Sleep(10);
	}
	return false;
}

bool wfClipboard_win::close_clipboard() 
{ 
	return CloseClipboard(); 
}

bool wfClipboard_win::empty_clipboard()
{
	return EmptyClipboard();
}

int wfClipboard_win::count_clipboard_formats()
{
	return CountClipboardFormats();
}

bool wfClipboard_win::is_clipboard_format_available(uint32_t format)
{
	return IsClipboardFormatAvailable(format);
}

int wfClipboard_win::get_clipboard_format_name_A(uint32_t format, char* format_name_ptr, int cchMaxCount)
{
	// If the function succeeds, the return value is the length, in characters, of the string copied to the buffer.

	// If the function fails, the return value is zero, indicating that the requested format does not exist or is predefined. 
	// To get extended error information, call GetLastError.
	return GetClipboardFormatNameA(format, format_name_ptr, cchMaxCount);
}

uint32_t wfClipboard_win::enum_clipboard_formats(uint32_t format)
{
	// @format: A clipboard format that is known to be available.
	// To start an enumeration of clipboard formats, set format to zero. When format is zero, 
	// the function retrieves the first available clipboard format. For subsequent calls during an enumeration,
	// set format to the result of the previous EnumClipboardFormats call.

	// If the function succeeds, the return value is the clipboard format that follows the specified format,
	// namely the next available clipboard format.
	return EnumClipboardFormats(format);
}

uint32_t wfClipboard_win::register_clipboard_format(const std::string& utf8)
{
	const std::wstring wstr = utils::to_wstring(utf8);
	if (wstr.empty()) {
		std::stringstream err;
		err << "utf8: " << utf8;
		VALIDATE(false, err.str());
	}
	return RegisterClipboardFormat(wstr.c_str());
}

int wfClipboard_win::get_format_data(bool hdrop, uint32_t format, uint8_t** ppdata)
{
	int size = 0;
	uint8_t* buff = nullptr;
	*ppdata = nullptr;

	if (hdrop) {
		size_t len;
		size_t i;
		wchar_t* wFileName;
		HRESULT result;
		LPDATAOBJECT dataObj;
		FORMATETC format_etc;
		STGMEDIUM stg_medium;
		DROPFILES* dropFiles;
		FILEGROUPDESCRIPTORW* groupDsc;
		result = OleGetClipboard(&dataObj);

		if (FAILED(result)) {
			return nposm;
		}

		ZeroMemory(&format_etc, sizeof(FORMATETC));
		ZeroMemory(&stg_medium, sizeof(STGMEDIUM));
		// get DROPFILES struct from OLE
		format_etc.cfFormat = CF_HDROP;
		format_etc.tymed = TYMED_HGLOBAL;
		format_etc.dwAspect = 1;
		format_etc.lindex = -1;
		// result = IDataObject_GetData(dataObj, &format_etc, &stg_medium);
		result = IDataObject_GetData2(dataObj, &format_etc, &stg_medium);

		if (FAILED(result))
		{
			SDL_Log("wfClipboard_win::get_format_data, [hdrop] dataObj->GetData failed.");
			goto exit;
		}

		dropFiles = (DROPFILES*)GlobalLock(stg_medium.hGlobal);

		if (!dropFiles)
		{
			GlobalUnlock(stg_medium.hGlobal);
			ReleaseStgMedium(&stg_medium);
			nFiles = 0;
			goto exit;
		}

		cliprdr_clear_file_array(this);

		if (dropFiles->fWide)
		{
			// dropFiles contains file names
			for (wFileName = (wchar_t*)((char*)dropFiles + dropFiles->pFiles);
					(len = SDL_wcslen(wFileName)) > 0; wFileName += len + 1)
			{
				wf_cliprdr_process_filename(this, wFileName, SDL_wcslen(wFileName));
			}
		}
		else
		{
			char* p;

			for (p = (char*)((char*)dropFiles + dropFiles->pFiles); (len = strlen(p)) > 0;
					p += len + 1, this->nFiles++)
			{
				int cchWideChar;
				wchar_t* wFileName;
				cchWideChar = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, NULL, 0);
				wFileName = (LPWSTR)calloc(cchWideChar, sizeof(wchar_t));
				MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, wFileName, cchWideChar);
				wf_cliprdr_process_filename(this, wFileName, cchWideChar);
			}
		}

		GlobalUnlock(stg_medium.hGlobal);
		ReleaseStgMedium(&stg_medium);
	exit:
		size = 4 + this->nFiles * sizeof(FILEDESCRIPTOR);
		groupDsc = (FILEGROUPDESCRIPTOR*)malloc(size);

		if (groupDsc)
		{
			groupDsc->cItems = this->nFiles;

			for (i = 0; i < this->nFiles; i++)
			{
				if (this->fileDescriptor[i])
					groupDsc->fgd[i] = *this->fileDescriptor[i];
			}

			buff = (uint8_t*)groupDsc;
		}

		// IDataObject_Release(dataObj);
		IDataObject_Release2(dataObj);
	} else {
		char* globlemem = NULL;
		HANDLE hClipdata = NULL;

		*ppdata = nullptr;
		/* Ignore if other app is holding the clipboard */
		if (open_clipboard())
		{
			hClipdata = GetClipboardData(format);

			if (!hClipdata)
			{
				close_clipboard();
				return nposm;
			}

			globlemem = (char*)GlobalLock(hClipdata);
			size = (int)GlobalSize(hClipdata);
			buff = (uint8_t*)malloc(size);
			CopyMemory(buff, globlemem, size);
			GlobalUnlock(hClipdata);
			close_clipboard();
		}
	}

	*ppdata = buff;
	return size;
}

static void CliprdrDataObject_Delete(CliprdrDataObject* instance);

static CliprdrEnumFORMATETC* CliprdrEnumFORMATETC_New(ULONG nFormats, FORMATETC* pFormatEtc);
static void CliprdrEnumFORMATETC_Delete(CliprdrEnumFORMATETC* instance);

static void CliprdrStream_Delete(CliprdrStream* instance);

/**
 * IStream
 */

static HRESULT STDMETHODCALLTYPE CliprdrStream_QueryInterface(IStream* This, REFIID riid,
                                                              void** ppvObject)
{
	if (IsEqualIID(riid, IID_IStream) || IsEqualIID(riid, IID_IUnknown))
	{
		IStream_AddRef(This);
		*ppvObject = This;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

static ULONG STDMETHODCALLTYPE CliprdrStream_AddRef(IStream* This)
{
	CliprdrStream* instance = (CliprdrStream*)This;

	if (!instance)
		return 0;

	return InterlockedIncrement(&instance->m_lRefCount);
}

static ULONG STDMETHODCALLTYPE CliprdrStream_Release(IStream* This)
{
	LONG count;
	CliprdrStream* instance = (CliprdrStream*)This;

	if (!instance)
		return 0;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if (count == 0)
	{
		CliprdrStream_Delete(instance);
		return 0;
	}
	else
	{
		return count;
	}
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_Read(IStream* This, void* pv, ULONG cb,
                                                    ULONG* pcbRead)
{
	int ret;
	CliprdrStream* instance = (CliprdrStream*)This;
	wfClipboard* clipboard;

	if (!pv || !pcbRead || !instance)
		return E_INVALIDARG;

	clipboard = (wfClipboard*)instance->m_pData;
	*pcbRead = 0;

	if (instance->m_lOffset.QuadPart >= instance->m_lSize.QuadPart)
		return S_FALSE;

	ret = cliprdr_send_request_filecontents(clipboard, (void*)This, instance->m_lIndex,
	                                        FILECONTENTS_RANGE, instance->m_lOffset.HighPart,
	                                        instance->m_lOffset.LowPart, cb);

	if (ret != ERROR_SUCCESS)
		return E_FAIL;

	if (clipboard->req_fsize != 0)
	{
		CopyMemory(pv, clipboard->req_fdata, clipboard->req_fsize);
		// free(clipboard->req_fdata);
	}

	*pcbRead = clipboard->req_fsize;
	instance->m_lOffset.QuadPart += clipboard->req_fsize;

	if (clipboard->req_fsize < cb)
		return S_FALSE;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_Write(IStream* This, const void* pv, ULONG cb,
                                                     ULONG* pcbWritten)
{
	(void)This;
	(void)pv;
	(void)cb;
	(void)pcbWritten;
	return STG_E_ACCESSDENIED;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_Seek(IStream* This, LARGE_INTEGER dlibMove,
                                                    DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
{
	ULONGLONG newoffset;
	CliprdrStream* instance = (CliprdrStream*)This;

	if (!instance)
		return E_INVALIDARG;

	newoffset = instance->m_lOffset.QuadPart;

	switch (dwOrigin)
	{
		case STREAM_SEEK_SET:
			newoffset = dlibMove.QuadPart;
			break;

		case STREAM_SEEK_CUR:
			newoffset += dlibMove.QuadPart;
			break;

		case STREAM_SEEK_END:
			newoffset = instance->m_lSize.QuadPart + dlibMove.QuadPart;
			break;

		default:
			return E_INVALIDARG;
	}

	if (newoffset < 0 || newoffset >= instance->m_lSize.QuadPart)
		return E_FAIL;

	instance->m_lOffset.QuadPart = newoffset;

	if (plibNewPosition)
		plibNewPosition->QuadPart = instance->m_lOffset.QuadPart;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_SetSize(IStream* This, ULARGE_INTEGER libNewSize)
{
	(void)This;
	(void)libNewSize;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_CopyTo(IStream* This, IStream* pstm,
                                                      ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead,
                                                      ULARGE_INTEGER* pcbWritten)
{
	(void)This;
	(void)pstm;
	(void)cb;
	(void)pcbRead;
	(void)pcbWritten;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_Commit(IStream* This, DWORD grfCommitFlags)
{
	(void)This;
	(void)grfCommitFlags;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_Revert(IStream* This)
{
	(void)This;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_LockRegion(IStream* This, ULARGE_INTEGER libOffset,
                                                          ULARGE_INTEGER cb, DWORD dwLockType)
{
	(void)This;
	(void)libOffset;
	(void)cb;
	(void)dwLockType;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_UnlockRegion(IStream* This, ULARGE_INTEGER libOffset,
                                                            ULARGE_INTEGER cb, DWORD dwLockType)
{
	(void)This;
	(void)libOffset;
	(void)cb;
	(void)dwLockType;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_Stat(IStream* This, STATSTG* pstatstg,
                                                    DWORD grfStatFlag)
{
	CliprdrStream* instance = (CliprdrStream*)This;

	if (!instance)
		return E_INVALIDARG;

	if (pstatstg == NULL)
		return STG_E_INVALIDPOINTER;

	ZeroMemory(pstatstg, sizeof(STATSTG));

	switch (grfStatFlag)
	{
		case STATFLAG_DEFAULT:
			return STG_E_INSUFFICIENTMEMORY;

		case STATFLAG_NONAME:
			pstatstg->cbSize.QuadPart = instance->m_lSize.QuadPart;
			pstatstg->grfLocksSupported = LOCK_EXCLUSIVE;
			pstatstg->grfMode = GENERIC_READ;
			pstatstg->grfStateBits = 0;
			pstatstg->type = STGTY_STREAM;
			break;

		case STATFLAG_NOOPEN:
			return STG_E_INVALIDFLAG;

		default:
			return STG_E_INVALIDFLAG;
	}

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CliprdrStream_Clone(IStream* This, IStream** ppstm)
{
	(void)This;
	(void)ppstm;
	return E_NOTIMPL;
}

static CliprdrStream* CliprdrStream_New(ULONG index, void* pData, const FILEDESCRIPTOR* dsc)
{
	IStream* iStream;
	BOOL success = FALSE;
	BOOL isDir = FALSE;
	CliprdrStream* instance;
	wfClipboard* clipboard = (wfClipboard*)pData;
	instance = (CliprdrStream*)calloc(1, sizeof(CliprdrStream));

	if (instance)
	{
		instance->m_Dsc = *dsc;
		iStream = &instance->iStream;
		iStream->lpVtbl = (IStreamVtbl*)calloc(1, sizeof(IStreamVtbl));

		if (iStream->lpVtbl)
		{
			iStream->lpVtbl->QueryInterface = CliprdrStream_QueryInterface;
			iStream->lpVtbl->AddRef = CliprdrStream_AddRef;
			iStream->lpVtbl->Release = CliprdrStream_Release;
			iStream->lpVtbl->Read = CliprdrStream_Read;
			iStream->lpVtbl->Write = CliprdrStream_Write;
			iStream->lpVtbl->Seek = CliprdrStream_Seek;
			iStream->lpVtbl->SetSize = CliprdrStream_SetSize;
			iStream->lpVtbl->CopyTo = CliprdrStream_CopyTo;
			iStream->lpVtbl->Commit = CliprdrStream_Commit;
			iStream->lpVtbl->Revert = CliprdrStream_Revert;
			iStream->lpVtbl->LockRegion = CliprdrStream_LockRegion;
			iStream->lpVtbl->UnlockRegion = CliprdrStream_UnlockRegion;
			iStream->lpVtbl->Stat = CliprdrStream_Stat;
			iStream->lpVtbl->Clone = CliprdrStream_Clone;
			instance->m_lRefCount = 1;
			instance->m_lIndex = index;
			instance->m_pData = pData;
			instance->m_lOffset.QuadPart = 0;

			if (instance->m_Dsc.dwFlags & FD_ATTRIBUTES)
			{
				if (instance->m_Dsc.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					isDir = TRUE;
			}

			if (((instance->m_Dsc.dwFlags & FD_FILESIZE) == 0) && !isDir)
			// if (!isDir)
			{
				/* get content size of this stream */
				if (cliprdr_send_request_filecontents(clipboard, (void*)instance,
				                                      instance->m_lIndex, FILECONTENTS_SIZE, 0, 0,
				                                      8) == CHANNEL_RC_OK)
				{
					success = TRUE;
				}

				instance->m_lSize.QuadPart = *((LONGLONG*)clipboard->req_fdata);
				// free(clipboard->req_fdata);
			}
			else {
				instance->m_lSize.LowPart = dsc->nFileSizeLow;
				instance->m_lSize.HighPart = dsc->nFileSizeHigh;
				success = TRUE;
			}
		}
	}

	if (!success)
	{
		CliprdrStream_Delete(instance);
		instance = NULL;
	}

	return instance;
}

void CliprdrStream_Delete(CliprdrStream* instance)
{
	if (instance)
	{
		free(instance->iStream.lpVtbl);
		free(instance);
	}
}

/**
 * IDataObject
 */

static LONG cliprdr_lookup_format(CliprdrDataObject* instance, FORMATETC* pFormatEtc)
{
	ULONG i;

	if (!instance || !pFormatEtc)
		return -1;

	for (i = 0; i < instance->m_nNumFormats; i++)
	{
		if ((pFormatEtc->tymed & instance->m_pFormatEtc[i].tymed) &&
		    pFormatEtc->cfFormat == instance->m_pFormatEtc[i].cfFormat &&
		    pFormatEtc->dwAspect & instance->m_pFormatEtc[i].dwAspect)
		{
			return (LONG)i;
		}
	}

	return -1;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryInterface(IDataObject* This, REFIID riid,
                                                                  void** ppvObject)
{
	(void)This;

	if (!ppvObject)
		return E_INVALIDARG;

	if (IsEqualIID(riid, IID_IDataObject) || IsEqualIID(riid, IID_IUnknown))
	{
		IDataObject_AddRef(This);
		*ppvObject = This;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

static ULONG STDMETHODCALLTYPE CliprdrDataObject_AddRef(IDataObject* This)
{
	CliprdrDataObject* instance = (CliprdrDataObject*)This;

	if (!instance)
		return E_INVALIDARG;

	return InterlockedIncrement(&instance->m_lRefCount);
}

static ULONG STDMETHODCALLTYPE CliprdrDataObject_Release(IDataObject* This)
{
	LONG count;
	CliprdrDataObject* instance = (CliprdrDataObject*)This;

	if (!instance)
		return E_INVALIDARG;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if (count == 0)
	{
		CliprdrDataObject_Delete(instance);
		return 0;
	}
	else
		return count;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetData(IDataObject* This, FORMATETC* pFormatEtc,
                                                           STGMEDIUM* pMedium)
{
	ULONG i;
	LONG idx;
	CliprdrDataObject* instance = (CliprdrDataObject*)This;
	wfClipboard_win* clipboard;

	if (!pFormatEtc || !pMedium || !instance)
		return E_INVALIDARG;

	clipboard = (wfClipboard_win*)instance->m_pData;

	if (!clipboard)
		return E_INVALIDARG;

	if ((idx = cliprdr_lookup_format(instance, pFormatEtc)) == -1)
		return DV_E_FORMATETC;

	pMedium->tymed = instance->m_pFormatEtc[idx].tymed;
	pMedium->pUnkForRelease = 0;

	if (instance->m_pFormatEtc[idx].cfFormat == clipboard->register_clipboard_format(CFSTR_FILEDESCRIPTORW_UTF8))
	{
		FILEGROUPDESCRIPTOR* dsc;
		DWORD remote = get_remote_format_id(clipboard, instance->m_pFormatEtc[idx].cfFormat);

		if (cliprdr_send_data_request(clipboard, remote) != 0)
			return E_UNEXPECTED;

		pMedium->hGlobal = clipboard->hmem; /* points to a FILEGROUPDESCRIPTOR structure */
		/* GlobalLock returns a pointer to the first byte of the memory block,
		 * in which is a FILEGROUPDESCRIPTOR structure, whose first UINT member
		 * is the number of FILEDESCRIPTOR's */
		dsc = (FILEGROUPDESCRIPTOR*)GlobalLock(clipboard->hmem);
		instance->m_nStreams = dsc->cItems;
		GlobalUnlock(clipboard->hmem);

		if (instance->m_nStreams > 0)
		{
			if (!instance->m_pStream)
			{
				instance->m_pStream = (LPSTREAM*)calloc(instance->m_nStreams, sizeof(LPSTREAM));

				if (instance->m_pStream)
				{
					for (i = 0; i < instance->m_nStreams; i++)
					{
						instance->m_pStream[i] =
						    (IStream*)CliprdrStream_New(i, clipboard, &dsc->fgd[i]);

						if (!instance->m_pStream[i])
							return E_OUTOFMEMORY;
					}
				}
			}
		}

		if (!instance->m_pStream)
		{
			if (clipboard->hmem)
			{
				GlobalFree(clipboard->hmem);
				clipboard->hmem = NULL;
			}

			pMedium->hGlobal = NULL;
			return E_OUTOFMEMORY;
		}
	}
	else if (instance->m_pFormatEtc[idx].cfFormat == clipboard->register_clipboard_format(CFSTR_FILECONTENTS_UTF8))
	{
		if (pFormatEtc->lindex < (LONG)instance->m_nStreams)
		{
			pMedium->pstm = instance->m_pStream[pFormatEtc->lindex];
			IDataObject_AddRef(instance->m_pStream[pFormatEtc->lindex]);
		}
		else
			return E_INVALIDARG;
	}
	else
		return E_UNEXPECTED;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetDataHere(IDataObject* This,
                                                               FORMATETC* pformatetc,
                                                               STGMEDIUM* pmedium)
{
	(void)This;
	(void)pformatetc;
	(void)pmedium;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_QueryGetData(IDataObject* This,
                                                                FORMATETC* pformatetc)
{
	CliprdrDataObject* instance = (CliprdrDataObject*)This;

	if (!pformatetc)
		return E_INVALIDARG;

	if (cliprdr_lookup_format(instance, pformatetc) == -1)
		return DV_E_FORMATETC;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_GetCanonicalFormatEtc(IDataObject* This,
                                                                         FORMATETC* pformatectIn,
                                                                         FORMATETC* pformatetcOut)
{
	(void)This;
	(void)pformatectIn;

	if (!pformatetcOut)
		return E_INVALIDARG;

	pformatetcOut->ptd = NULL;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_SetData(IDataObject* This, FORMATETC* pformatetc,
                                                           STGMEDIUM* pmedium, BOOL fRelease)
{
	(void)This;
	(void)pformatetc;
	(void)pmedium;
	(void)fRelease;
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumFormatEtc(IDataObject* This,
                                                                 DWORD dwDirection,
                                                                 IEnumFORMATETC** ppenumFormatEtc)
{
	CliprdrDataObject* instance = (CliprdrDataObject*)This;

	if (!instance || !ppenumFormatEtc)
		return E_INVALIDARG;

	if (dwDirection == DATADIR_GET)
	{
		*ppenumFormatEtc = (IEnumFORMATETC*)CliprdrEnumFORMATETC_New(instance->m_nNumFormats,
		                                                             instance->m_pFormatEtc);
		return (*ppenumFormatEtc) ? S_OK : E_OUTOFMEMORY;
	}
	else
	{
		return E_NOTIMPL;
	}
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_DAdvise(IDataObject* This, FORMATETC* pformatetc,
                                                           DWORD advf, IAdviseSink* pAdvSink,
                                                           DWORD* pdwConnection)
{
	(void)This;
	(void)pformatetc;
	(void)advf;
	(void)pAdvSink;
	(void)pdwConnection;
	return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_DUnadvise(IDataObject* This, DWORD dwConnection)
{
	(void)This;
	(void)dwConnection;
	return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE CliprdrDataObject_EnumDAdvise(IDataObject* This,
                                                               IEnumSTATDATA** ppenumAdvise)
{
	(void)This;
	(void)ppenumAdvise;
	return OLE_E_ADVISENOTSUPPORTED;
}

static CliprdrDataObject* CliprdrDataObject_New(FORMATETC* fmtetc, STGMEDIUM* stgmed, ULONG count,
                                                void* data)
{
	int i;
	CliprdrDataObject* instance;
	IDataObject* iDataObject;
	instance = (CliprdrDataObject*)calloc(1, sizeof(CliprdrDataObject));

	if (!instance)
		goto error;

	iDataObject = &instance->iDataObject;
	iDataObject->lpVtbl = (IDataObjectVtbl*)calloc(1, sizeof(IDataObjectVtbl));

	if (!iDataObject->lpVtbl)
		goto error;

	iDataObject->lpVtbl->QueryInterface = CliprdrDataObject_QueryInterface;
	iDataObject->lpVtbl->AddRef = CliprdrDataObject_AddRef;
	iDataObject->lpVtbl->Release = CliprdrDataObject_Release;
	iDataObject->lpVtbl->GetData = CliprdrDataObject_GetData;
	iDataObject->lpVtbl->GetDataHere = CliprdrDataObject_GetDataHere;
	iDataObject->lpVtbl->QueryGetData = CliprdrDataObject_QueryGetData;
	iDataObject->lpVtbl->GetCanonicalFormatEtc = CliprdrDataObject_GetCanonicalFormatEtc;
	iDataObject->lpVtbl->SetData = CliprdrDataObject_SetData;
	iDataObject->lpVtbl->EnumFormatEtc = CliprdrDataObject_EnumFormatEtc;
	iDataObject->lpVtbl->DAdvise = CliprdrDataObject_DAdvise;
	iDataObject->lpVtbl->DUnadvise = CliprdrDataObject_DUnadvise;
	iDataObject->lpVtbl->EnumDAdvise = CliprdrDataObject_EnumDAdvise;
	instance->m_lRefCount = 1;
	instance->m_nNumFormats = count;
	instance->m_pData = data;
	instance->m_nStreams = 0;
	instance->m_pStream = NULL;

	if (count > 0)
	{
		instance->m_pFormatEtc = (FORMATETC*)calloc(count, sizeof(FORMATETC));

		if (!instance->m_pFormatEtc)
			goto error;

		instance->m_pStgMedium = (STGMEDIUM*)calloc(count, sizeof(STGMEDIUM));

		if (!instance->m_pStgMedium)
			goto error;

		for (i = 0; i < (int)count; i++)
		{
			instance->m_pFormatEtc[i] = fmtetc[i];
			instance->m_pStgMedium[i] = stgmed[i];
		}
	}

	return instance;
error:
	CliprdrDataObject_Delete(instance);
	return NULL;
}

void CliprdrDataObject_Delete(CliprdrDataObject* instance)
{
	if (instance)
	{
		free(instance->iDataObject.lpVtbl);
		free(instance->m_pFormatEtc);
		free(instance->m_pStgMedium);

		if (instance->m_pStream)
		{
			ULONG i;

			for (i = 0; i < instance->m_nStreams; i++)
				CliprdrStream_Release(instance->m_pStream[i]);

			free(instance->m_pStream);
		}

		free(instance);
	}
}

static BOOL wf_create_file_obj(wfClipboard* clipboard, IDataObject** ppDataObject)
{
	FORMATETC fmtetc[2];
	STGMEDIUM stgmeds[2];

	if (!ppDataObject)
		return FALSE;

	fmtetc[0].cfFormat = clipboard->register_clipboard_format(CFSTR_FILEDESCRIPTORW_UTF8);
	fmtetc[0].dwAspect = DVASPECT_CONTENT;
	fmtetc[0].lindex = 0;
	fmtetc[0].ptd = NULL;
	fmtetc[0].tymed = TYMED_HGLOBAL;
	stgmeds[0].tymed = TYMED_HGLOBAL;
	stgmeds[0].hGlobal = NULL;
	stgmeds[0].pUnkForRelease = NULL;
	fmtetc[1].cfFormat = clipboard->register_clipboard_format(CFSTR_FILECONTENTS_UTF8);
	fmtetc[1].dwAspect = DVASPECT_CONTENT;
	fmtetc[1].lindex = 0;
	fmtetc[1].ptd = NULL;
	fmtetc[1].tymed = TYMED_ISTREAM;
	stgmeds[1].tymed = TYMED_ISTREAM;
	stgmeds[1].pstm = NULL;
	stgmeds[1].pUnkForRelease = NULL;
	*ppDataObject = (IDataObject*)CliprdrDataObject_New(fmtetc, stgmeds, 2, clipboard);
	return (*ppDataObject) ? TRUE : FALSE;
}

static void wf_destroy_file_obj(IDataObject* instance)
{
	if (instance)
		IDataObject_Release(instance);
}

/**
 * IEnumFORMATETC
 */

static void cliprdr_format_deep_copy(FORMATETC* dest, FORMATETC* source)
{
	*dest = *source;

	if (source->ptd)
	{
		dest->ptd = (DVTARGETDEVICE*)CoTaskMemAlloc(sizeof(DVTARGETDEVICE));

		if (dest->ptd)
			*(dest->ptd) = *(source->ptd);
	}
}

static HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_QueryInterface(IEnumFORMATETC* This,
                                                                     REFIID riid, void** ppvObject)
{
	(void)This;

	if (IsEqualIID(riid, IID_IEnumFORMATETC) || IsEqualIID(riid, IID_IUnknown))
	{
		IEnumFORMATETC_AddRef(This);
		*ppvObject = This;
		return S_OK;
	}
	else
	{
		*ppvObject = 0;
		return E_NOINTERFACE;
	}
}

static ULONG STDMETHODCALLTYPE CliprdrEnumFORMATETC_AddRef(IEnumFORMATETC* This)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*)This;

	if (!instance)
		return 0;

	return InterlockedIncrement(&instance->m_lRefCount);
}

static ULONG STDMETHODCALLTYPE CliprdrEnumFORMATETC_Release(IEnumFORMATETC* This)
{
	LONG count;
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*)This;

	if (!instance)
		return 0;

	count = InterlockedDecrement(&instance->m_lRefCount);

	if (count == 0)
	{
		CliprdrEnumFORMATETC_Delete(instance);
		return 0;
	}
	else
	{
		return count;
	}
}

static HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Next(IEnumFORMATETC* This, ULONG celt,
                                                           FORMATETC* rgelt, ULONG* pceltFetched)
{
	ULONG copied = 0;
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*)This;

	if (!instance || !celt || !rgelt)
		return E_INVALIDARG;

	while ((instance->m_nIndex < instance->m_nNumFormats) && (copied < celt))
	{
		cliprdr_format_deep_copy(&rgelt[copied++], &instance->m_pFormatEtc[instance->m_nIndex++]);
	}

	if (pceltFetched != 0)
		*pceltFetched = copied;

	return (copied == celt) ? S_OK : E_FAIL;
}

static HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Skip(IEnumFORMATETC* This, ULONG celt)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*)This;

	if (!instance)
		return E_INVALIDARG;

	if (instance->m_nIndex + (LONG)celt > instance->m_nNumFormats)
		return E_FAIL;

	instance->m_nIndex += celt;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Reset(IEnumFORMATETC* This)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*)This;

	if (!instance)
		return E_INVALIDARG;

	instance->m_nIndex = 0;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE CliprdrEnumFORMATETC_Clone(IEnumFORMATETC* This,
                                                            IEnumFORMATETC** ppEnum)
{
	CliprdrEnumFORMATETC* instance = (CliprdrEnumFORMATETC*)This;

	if (!instance || !ppEnum)
		return E_INVALIDARG;

	*ppEnum =
	    (IEnumFORMATETC*)CliprdrEnumFORMATETC_New(instance->m_nNumFormats, instance->m_pFormatEtc);

	if (!*ppEnum)
		return E_OUTOFMEMORY;

	((CliprdrEnumFORMATETC*)*ppEnum)->m_nIndex = instance->m_nIndex;
	return S_OK;
}

CliprdrEnumFORMATETC* CliprdrEnumFORMATETC_New(ULONG nFormats, FORMATETC* pFormatEtc)
{
	ULONG i;
	CliprdrEnumFORMATETC* instance;
	IEnumFORMATETC* iEnumFORMATETC;

	if ((nFormats != 0) && !pFormatEtc)
		return NULL;

	instance = (CliprdrEnumFORMATETC*)calloc(1, sizeof(CliprdrEnumFORMATETC));

	if (!instance)
		goto error;

	iEnumFORMATETC = &instance->iEnumFORMATETC;
	iEnumFORMATETC->lpVtbl = (IEnumFORMATETCVtbl*)calloc(1, sizeof(IEnumFORMATETCVtbl));

	if (!iEnumFORMATETC->lpVtbl)
		goto error;

	iEnumFORMATETC->lpVtbl->QueryInterface = CliprdrEnumFORMATETC_QueryInterface;
	iEnumFORMATETC->lpVtbl->AddRef = CliprdrEnumFORMATETC_AddRef;
	iEnumFORMATETC->lpVtbl->Release = CliprdrEnumFORMATETC_Release;
	iEnumFORMATETC->lpVtbl->Next = CliprdrEnumFORMATETC_Next;
	iEnumFORMATETC->lpVtbl->Skip = CliprdrEnumFORMATETC_Skip;
	iEnumFORMATETC->lpVtbl->Reset = CliprdrEnumFORMATETC_Reset;
	iEnumFORMATETC->lpVtbl->Clone = CliprdrEnumFORMATETC_Clone;
	instance->m_lRefCount = 1;
	instance->m_nIndex = 0;
	instance->m_nNumFormats = nFormats;

	if (nFormats > 0)
	{
		instance->m_pFormatEtc = (FORMATETC*)calloc(nFormats, sizeof(FORMATETC));

		if (!instance->m_pFormatEtc)
			goto error;

		for (i = 0; i < nFormats; i++)
			cliprdr_format_deep_copy(&instance->m_pFormatEtc[i], &pFormatEtc[i]);
	}

	return instance;
error:
	CliprdrEnumFORMATETC_Delete(instance);
	return NULL;
}

void CliprdrEnumFORMATETC_Delete(CliprdrEnumFORMATETC* instance)
{
	LONG i;

	if (instance)
	{
		free(instance->iEnumFORMATETC.lpVtbl);

		if (instance->m_pFormatEtc)
		{
			for (i = 0; i < instance->m_nNumFormats; i++)
			{
				if (instance->m_pFormatEtc[i].ptd)
					CoTaskMemFree(instance->m_pFormatEtc[i].ptd);
			}

			free(instance->m_pFormatEtc);
		}

		free(instance);
	}
}

#define WM_CLIPRDR_MESSAGE (WM_USER + 156)
#define OLE_SETCLIPBOARD 1

static LRESULT CALLBACK cliprdr_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static wfClipboard_win* clipboard = NULL;

	switch (Msg)
	{
		case WM_CREATE:
			DEBUG_CLIPRDR("info: WM_CREATE");
			clipboard = (wfClipboard_win*)((CREATESTRUCT*)lParam)->lpCreateParams;
			clipboard->hwnd = hWnd;

			clipboard->AddClipboardFormatListener(hWnd);

			break;

		case WM_CLOSE:
			DEBUG_CLIPRDR("info: WM_CLOSE");

				clipboard->RemoveClipboardFormatListener(hWnd);

			break;

		case WM_CLIPBOARDUPDATE:
			DEBUG_CLIPRDR("info: WM_CLIPBOARDUPDATE");

			if (clipboard->sync)
			{
				if ((GetClipboardOwner() != clipboard->hwnd) &&
				    (S_FALSE == OleIsCurrentClipboard(clipboard->data_obj)))
				{
					if (clipboard->hmem)
					{
						GlobalFree(clipboard->hmem);
						clipboard->hmem = NULL;
					}

					cliprdr_send_format_list(clipboard);
				}
			}

			break;

		case WM_RENDERALLFORMATS:
			DEBUG_CLIPRDR("info: WM_RENDERALLFORMATS");

			/* discard all contexts in clipboard */
			if (!clipboard->open_clipboard())
			{
				DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
				break;
			}

			clipboard->empty_clipboard();
			clipboard->close_clipboard();
			break;

		case WM_RENDERFORMAT:
			DEBUG_CLIPRDR("info: WM_RENDERFORMAT");

			if (cliprdr_send_data_request(clipboard, (UINT32)wParam) != 0)
			{
				DEBUG_CLIPRDR("error: cliprdr_send_data_request failed.");
				break;
			}

			if (!clipboard->set_clipboard_data((UINT)wParam, clipboard->hmem))
			{
				DEBUG_CLIPRDR("SetClipboardData failed with 0x%x", GetLastError());

				if (clipboard->hmem)
				{
					GlobalFree(clipboard->hmem);
					clipboard->hmem = NULL;
				}
			}

			/* Note: GlobalFree() is not needed when success */
			break;

		case WM_CLIPRDR_MESSAGE:
			DEBUG_CLIPRDR("info: WM_CLIPRDR_MESSAGE");

			switch (wParam)
			{
				case OLE_SETCLIPBOARD:
					DEBUG_CLIPRDR("info: OLE_SETCLIPBOARD");

					if (S_FALSE == OleIsCurrentClipboard(clipboard->data_obj))
					{
						if (wf_create_file_obj(clipboard, &clipboard->data_obj))
						{
							if (OleSetClipboard(clipboard->data_obj) != S_OK)
							{
								wf_destroy_file_obj(clipboard->data_obj);
								clipboard->data_obj = NULL;
							}
						}
					}

					break;

				default:
					break;
			}

			break;

		case WM_DESTROYCLIPBOARD:
		case WM_ASKCBFORMATNAME:
		case WM_HSCROLLCLIPBOARD:
		case WM_PAINTCLIPBOARD:
		case WM_SIZECLIPBOARD:
		case WM_VSCROLLCLIPBOARD:
		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	return 0;
}

static int create_cliprdr_window(wfClipboard_win* clipboard)
{
	WNDCLASSEX wnd_cls;
	ZeroMemory(&wnd_cls, sizeof(WNDCLASSEX));
	wnd_cls.cbSize = sizeof(WNDCLASSEX);
	wnd_cls.style = CS_OWNDC;
	wnd_cls.lpfnWndProc = cliprdr_proc;
	wnd_cls.cbClsExtra = 0;
	wnd_cls.cbWndExtra = 0;
	wnd_cls.hIcon = NULL;
	wnd_cls.hCursor = NULL;
	wnd_cls.hbrBackground = NULL;
	wnd_cls.lpszMenuName = NULL;
	wnd_cls.lpszClassName = _T("ClipboardHiddenMessageProcessor");
	wnd_cls.hInstance = GetModuleHandle(NULL);
	wnd_cls.hIconSm = NULL;
	RegisterClassEx(&wnd_cls);
	clipboard->hwnd =
	    CreateWindowEx(WS_EX_LEFT, _T("ClipboardHiddenMessageProcessor"), _T("rdpclip"), 0, 0, 0, 0,
	                   0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), clipboard);

	if (!clipboard->hwnd)
	{
		DEBUG_CLIPRDR("error: CreateWindowEx failed with %x.", GetLastError());
		return -1;
	}

	return 0;
}

void wfClipboard_win::cliprdr_thread_func()
{
	int ret;
	MSG msg;
	BOOL mcode;
	OleInitialize(0);

	if ((ret = create_cliprdr_window(this)) != 0)
	{
		DEBUG_CLIPRDR("error: create clipboard window failed.");
		return;
	}

	while ((mcode = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (mcode == -1)
		{
			DEBUG_CLIPRDR("error: clipboard thread GetMessage failed.");
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	OleUninitialize();
}

void wfClipboard_win::save_format_data_response(const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	BYTE* data;
	HANDLE hMem;

	hMem = GlobalAlloc(GMEM_MOVEABLE, formatDataResponse->dataLen);

	if (!hMem) {
		return;
	}

	data = (BYTE*)GlobalLock(hMem);

	if (!data)
	{
		GlobalFree(hMem);
		return;
	}

	CopyMemory(data, formatDataResponse->requestedFormatData, formatDataResponse->dataLen);

	if (!GlobalUnlock(hMem) && GetLastError())
	{
		GlobalFree(hMem);
		return;
	}

	hmem = hMem;
}

void* wfClipboard_win::set_clipboard_data(uint32_t uFormat, const void* hMem)
{
	return SetClipboardData(uFormat, (HANDLE)hMem);
}

uint32_t wfClipboard_win::set_dataobject_2_clipboard()
{
	if (PostMessage(hwnd, WM_CLIPRDR_MESSAGE, OLE_SETCLIPBOARD, 0)) {
		return CHANNEL_RC_OK;
	}
	return ERROR_INTERNAL_ERROR;
}

void wfClipboard_win::trigger_quit()
{
	if (hwnd) {
		PostMessage(hwnd, WM_QUIT, 0, 0);
	}
}

long wfClipboard_win::IDataObject_GetData2(IDataObject* pDataObject, FORMATETC *pformatetcIn, STGMEDIUM *pmedium)
{
	return IDataObject_GetData(pDataObject, pformatetcIn, pmedium);
}

void wfClipboard_win::IDataObject_Release2(IDataObject* pDataObject)
{
	IDataObject_Release(pDataObject);
}











wfClipboard* kosNewClipboard()
{
	return new wfClipboard_win;
}

wfClipboard* kosCustom2Clipboard(void* custom)
{
	wfClipboard_win* clipboard = (wfClipboard_win*)custom;
	return clipboard;
}