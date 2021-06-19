/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#define GETTEXT_DOMAIN "rose-lib"

#include <SDL_log.h>
#include <utility>

#include <freerdp/channels/impl_cliprdr_os.h>

#include "base_instance.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

#include "rtc_base/bind.h"

// chromium
#include "base/bind.h"

using namespace std::placeholders;

// 
struct tclip_data
{
	tclip_data()
	{
		clear();
	}

	bool valid() const { return format != nposm; }

	void set_text(const std::string& text)
	{
		VALIDATE(!text.empty(), null_str);
		format = CF_UNICODETEXT;

		wtext = utils::to_wstring(text);
		VALIDATE(!wtext.empty(), null_str);
	}

	void set_wtext(const std::wstring& _wtext)
	{
		VALIDATE(!_wtext.empty(), null_str);
		format = CF_UNICODETEXT;
		wtext = _wtext;
	}

	void set_hdrop(const std::vector<std::string>& _files)
	{
		VALIDATE(!_files.empty(), null_str);
		format = CF_HDROP;
		files = _files;
		remote_format = nposm;
	}

	void set_hdrop_remote(uint32_t _remote_format)
	{
		VALIDATE(_remote_format != nposm, null_str);
		format = CF_HDROP;
		remote_format = _remote_format;
		files.clear();
	}

	void clear()
	{
		format = nposm;
		wtext.clear();
		files.clear();
		remote_format = nposm;
	}

	uint32_t format;
	std::wstring wtext;
	std::vector<std::string> files;
	uint32_t remote_format;
};

#define FAKE_REGISTER_PID			100
#define REGISTER_FORMAT_MIN			0xc000
#define REGISTER_FORMAT_INTERVAL	40

class wfClipboard_kos: public wfClipboard
{
public:
	wfClipboard_kos();
	~wfClipboard_kos();

	void text_clipboard_updated(const std::string& text) override;
	void hdrop_copied(const std::vector<std::string>& files) override;
	int hdrop_paste(const char* path, void* progress, char* err_msg, int max_bytes) override;
	bool can_hdrop_paste() override;

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
	void clipboard_require_update(bool remote) override;

private:
	void text_clipboard_updated_internal();
	void hdrop_copied_internal();
	void hdrop_paste_internal(char* err_msg, int max_bytes);
	void render_format_internal(uint32_t format);
	void set_dataobject_2_clipboard_internal();
	void save_format_data_response_internal(uint8_t* data, int size);
	void cliprdr_thread_func() override;
	uint32_t set_dataobject_2_clipboard() override;
	void pre_send_format_data_request() override;
	void trigger_quit() override;

	void did_set_paste_event();

	uint32_t format_registered(int pid, const std::string& name) const;

public:
	std::string hmem;

private:
	
	struct tregister {
		tregister(int _pid, const std::string& _name)
			: pid(_pid)
			, name(_name)
		{
			VALIDATE(pid > 0, str_cast(pid));
			VALIDATE(!name.empty(), null_str);
		}

		int pid;
		std::string name;
	};
	std::map<uint32_t, tregister> registered_formats_;
	uint32_t next_register_format_;

	std::string current_text_clipboard_;
	std::vector<std::string> current_hdrop_files_;
	tclip_data clip_data_;

	threading::mutex paste_mutex_;
	static bool pasting_;
	static int errcode_;
	std::string paste_local_root_;
	std::string paste_message_;
	int paste_percentage_;
	int paste_remaining_;
};

bool wfClipboard_kos::pasting_ = false;
int wfClipboard_kos::errcode_ = cliprdr_errcode_ok;

wfClipboard_kos::wfClipboard_kos()
	: hmem()
	, next_register_format_(REGISTER_FORMAT_MIN)
	, paste_percentage_(nposm)
	, paste_remaining_(0)
{
	pasting_ = false;
	errcode_ = cliprdr_errcode_ok;
}

wfClipboard_kos::~wfClipboard_kos()
{
	SDL_Log("---wfClipboard_kos::~wfClipboard_kos()---");
}

void wfClipboard_kos::text_clipboard_updated_internal()
{
	clip_data_.set_text(current_text_clipboard_);
	if (sync) {
		cliprdr_send_format_list(static_cast<wfClipboard*>(this));
	}
}

void wfClipboard_kos::text_clipboard_updated(const std::string& text)
{
	if (thread_.get() == nullptr) {
		return;
	}
	if (pasting_) {
		SDL_Log("wfClipboard_kos::text_clipboard_updated, pasting_, do nothing");
		// return;
	}
	if (current_text_clipboard_ == text) {
		// peer(copy) --> me(update clipboard) ==> blipboard_updated.
		// require ignore it.
		return;
	}
	current_text_clipboard_ = text;
	if (!text.empty()) {
		thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&wfClipboard_kos::text_clipboard_updated_internal, base::Unretained(this)));
	}
}

void wfClipboard_kos::hdrop_copied_internal()
{
	clip_data_.set_hdrop(current_hdrop_files_);
	if (sync) {
		cliprdr_send_format_list(static_cast<wfClipboard*>(this));
	}
}

void wfClipboard_kos::hdrop_copied(const std::vector<std::string>& files)
{
	if (thread_.get() == nullptr) {
		return;
	}
	if (pasting_) {
		SDL_Log("wfClipboard_kos::hdrop_copied, pasting_, do nothing");
		// return;
	}

	current_hdrop_files_ = files;
	if (!files.empty()) {
		thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&wfClipboard_kos::hdrop_copied_internal, base::Unretained(this)));
	}
}

void wfClipboard_kos::did_set_paste_event()
{
	pasting_ = false;
}

void wfClipboard_kos::hdrop_paste_internal(char* err_msg, int max_bytes)
{
	DCHECK(clipboard_thread_checker_->CalledOnValidThread());
	tauto_destruct_executor destruct_executor(std::bind(&wfClipboard_kos::did_set_paste_event, this));

	if (paste_local_root_.empty()) {
		return;
	}

	if (clip_data_.format != CF_HDROP || clip_data_.remote_format == nposm) {
		return;
	}

	uint32_t local_format = register_clipboard_format(CFSTR_FILEDESCRIPTORW_UTF8);
	uint32_t remote = get_remote_format_id(this, local_format);
	if (remote != clip_data_.remote_format) {
		return;
	}

	{
		threading::lock lock(paste_mutex_);
		paste_message_ = _("Get clipboard format data");
	}

	// Since waiting event has an overflow time, there may be response to receive over overflow time.
	// before file transfor, reset relative event.
	this->req_fevent.Reset();

	if (cliprdr_send_data_request(this, remote) != ERROR_SUCCESS) {
		errcode_ = cliprdr_errcode_format;
		return;
	}

	if ((hmem.size() - 4) % sizeof(FILEDESCRIPTOR) != 0) {
		SDL_Log("hdrop_paste_internal, size of format data response is %i, error", (int)hmem.size());
		errcode_ = cliprdr_errcode_format;
		return;
	}

	// other code must keep hmem not change when pasting.
	FILEGROUPDESCRIPTOR* dsc = (FILEGROUPDESCRIPTOR*)(hmem.c_str());

	uint64_t total_bytes = 0;
	uint64_t received_bytes = 0;
	for (uint32_t i = 0; i < dsc->cItems; i++)
	{
		const FILEDESCRIPTOR& fgd = dsc->fgd[i];
		if (fgd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}
		total_bytes += posix_mku64(fgd.nFileSizeLow, fgd.nFileSizeHigh);
	}

	const uint32_t start_ticks = SDL_GetTicks();
	std::unique_ptr<tfile> file;
	const std::string current_path = paste_local_root_;
	for (uint32_t i = 0; i < dsc->cItems && errcode_ == cliprdr_errcode_ok; i++)
	{
		const FILEDESCRIPTOR& fgd = dsc->fgd[i];
		const std::string short_file_name = utils::from_uint16_ptr((uint16_t*)fgd.cFileName);
		const std::string file_name = utils::normalize_path(current_path + "/" + short_file_name);
		SDL_Log("hdrop_paste_internal, handle ing %s", file_name.c_str());

		const std::string cut = utils::truncate_to_max_bytes(short_file_name.c_str(), short_file_name.size(), max_bytes - 1);
		strcpy(err_msg, cut.c_str());

		if (fgd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			SDL_bool ret = SDL_MakeDirectory(file_name.c_str());
			if (!ret && errcode_ == cliprdr_errcode_ok) {
				errcode_ = cliprdr_errcode_directory;
			}
			continue;
		}
		{
			threading::lock lock(paste_mutex_);
			paste_message_ = short_file_name;
		}

		uint64_t fsize = posix_mku64(fgd.nFileSizeLow, fgd.nFileSizeHigh);
		uint64_t pos = 0;
		uint32_t m_lIndex = i;
		uint32_t cb = 256 * 1024;
		while (pos < fsize) {
			int ret = cliprdr_send_request_filecontents(this, (void*)&fgd, m_lIndex,
	                                        FILECONTENTS_RANGE, posix_hi32(pos),
	                                        posix_lo32(pos), cb);

			if (ret != ERROR_SUCCESS) {
				SDL_Log("hdrop_paste_internal, file: %s, cliprdr_send_request_filecontents fail, [1] errcode_: %i, max_bytes: %i", file_name.c_str(), errcode_, max_bytes);
				if (errcode_ == cliprdr_errcode_ok) {
					if (ret == ERROR_BAD_DESCRIPTOR_FORMAT) {
						errcode_ = cliprdr_errcode_clipboardupdate;
					} else {
						errcode_ = cliprdr_errcode_file;
					}
				}
				SDL_Log("hdrop_paste_internal, file: %s, cliprdr_send_request_filecontents fail, [2] errcode_: %i", file_name.c_str(), errcode_);
				break;
			}
			if (this->req_fsize == 0) {
				SDL_Log("hdrop_paste_internal, file: %s, this->req_fsize = 0, strange, but continue next file", file_name.c_str());
				break;
			}

			if (file.get() == nullptr) {
				file.reset(new tfile(file_name, GENERIC_WRITE, CREATE_ALWAYS));
				if (!file->valid()) {
					if (errcode_ == cliprdr_errcode_ok) {
						errcode_ = cliprdr_errcode_file;
					}
					break;
				}
			}
			uint32_t bytesrtd = posix_fwrite(file->fp, this->req_fdata, this->req_fsize);
			if (bytesrtd != this->req_fsize) {
				int64_t free_space = SDL_DiskFreeSpace(file_name.c_str());
				SDL_Log("hdrop_paste_internal, file: %s, free_space: %lli, pos: %llu, this->req_fsize: %u != bytertd: %u ==> nospace", 
					file_name.c_str(), free_space, pos, this->req_fsize, bytesrtd);
				errcode_ = cliprdr_errcode_nospace;
				break;
			}

			pos += this->req_fsize;
			received_bytes += this->req_fsize;

			this->req_fsize = 0;
			this->req_fstreamid = 0;

			{
				threading::lock lock(paste_mutex_);
				paste_percentage_ = received_bytes * 100 / total_bytes;
				if (paste_percentage_ > 100) {
					// The contents of the remote file change during pasting.
					// maybe increate fsize??
					paste_percentage_ = 100;
				}
				uint32_t elapse_second = (SDL_GetTicks() - start_ticks) / 1000;
				if (elapse_second != 0) {
					paste_remaining_ = 1.0 * (total_bytes - received_bytes) * elapse_second / received_bytes + 1;
					if (paste_remaining_ >= 15) {
						paste_remaining_ = posix_align_ceil2(paste_remaining_, 15);
					}
				}
			}
		}
		file.reset();
		if (errcode_ != cliprdr_errcode_ok) {
			// one file fail, halt paste.
			SDL_DeleteFiles(file_name.c_str());
			break;
		}
	}
}

int wfClipboard_kos::hdrop_paste(const char* path, void* _progress, char* err_msg, int max_bytes)
{
	VALIDATE(path != nullptr && path[0] != '\0', null_str);
	VALIDATE(max_bytes > 1, null_str);
	if (thread_.get() == nullptr) {
		return cliprdr_errcode_ok;
	}

	errcode_ = cliprdr_errcode_ok;
	if (clip_data_.format == CF_HDROP && clip_data_.remote_format != nposm) {
		pasting_ = true;
		paste_message_.clear();
		paste_percentage_ = nposm;
		paste_remaining_ = nposm;
		paste_local_root_ = path;
		gui2::tprogress_* progress = reinterpret_cast<gui2::tprogress_*>(_progress);
		thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&wfClipboard_kos::hdrop_paste_internal, base::Unretained(this), err_msg, max_bytes));
		utils::string_map symbols;
		while (pasting_) {
			{
				threading::lock lock(paste_mutex_);
/*
				if (!paste_message_.empty()) {
					progress->set_message(paste_message_);
					paste_message_.clear();
				}
*/
				if (paste_percentage_ != nposm) {
					progress->set_percentage(paste_percentage_);
					paste_percentage_ = nposm;
				}

				std::stringstream msg;
				if (paste_remaining_ != nposm) {
					symbols["time"] = format_elapse_hms(paste_remaining_, false);
					msg << vgettext2("About remaining $time", symbols) << "  ";
				}
				msg << paste_message_;
				progress->set_message(msg.str());
				// paste_message_.clear();
			}
			progress->show_slice();
			if (progress->task_cancelled()) {
				response_data_event.Set();
				req_fevent.Set();
				errcode_ = cliprdr_errcode_cancel;
			}
		}
	}
	return errcode_;
}

bool wfClipboard_kos::can_hdrop_paste()
{
	if (pasting_) {
		return false;
	}
	return clip_data_.format == CF_HDROP && clip_data_.remote_format != nposm;
}

void wfClipboard_kos::render_format_internal(uint32_t format)
{
	SDL_Log("wfClipboard_kos::render_format_internal--- format: %u", format);

	if (cliprdr_send_data_request(static_cast<wfClipboard*>(this), format) != 0) {
		SDL_Log("---wfClipboard_kos::render_format_internal, cliprdr_send_data_request failed.");
		return;
	}

	clip_data_.clear();
	// hmem is unicode16
	if (hmem.empty() || hmem.size() % 2 != 0) {
		return;
	}
	// const std::string text = utils::from_wchar_ptr((const wchar_t*)hmem.c_str());
	const std::string text = utils::from_uint16_ptr((const uint16_t*)hmem.c_str());
	current_text_clipboard_ = text;
	SDL_SetClipboardText(text.c_str());

	// std::wstring wtext((const wchar_t*)hmem.c_str());
	std::wstring wtext = utils::to_wstring(text);
	if (!wtext.empty()) {
		clip_data_.set_wtext(wtext);
	} else {
		SDL_Log("wfClipboard_kos::render_format_internal, strange, hmem isn't empty, but wtext is empty, text: %s", text.c_str());
	}
	SDL_Log("---wfClipboard_kos::render_format_internal format: %u", format);
}

bool wfClipboard_kos::open_clipboard()
{
	return true;
}

bool wfClipboard_kos::close_clipboard() 
{ 
	return true; 
}

bool wfClipboard_kos::empty_clipboard()
{
	if (pasting_) {
		SDL_Log("wfClipboard_kos::empty_clipboard, pasting_, do nothing");
		// return true;
	}
	clip_data_.clear();
	return true;
}

int wfClipboard_kos::count_clipboard_formats()
{
	if (!clip_data_.valid()) {
		return 0;
	}
	if (clip_data_.format == CF_UNICODETEXT) {
		return 1;
	} else {
		VALIDATE(clip_data_.format == CF_HDROP, null_str);
		// why is 2? 
		// 0: CFSTR_FILEDESCRIPTORW_UTF8
		// 1: CFSTR_FILECONTENTS_UTF8
		return 2;
	}
}

bool wfClipboard_kos::is_clipboard_format_available(uint32_t format)
{
	return clip_data_.format == format;
}

int wfClipboard_kos::get_clipboard_format_name_A(uint32_t format, char* format_name_ptr, int cchMaxCount)
{
	// If the function succeeds, the return value is the length, in characters, of the string copied to the buffer.

	// If the function fails, the return value is zero, indicating that the requested format does not exist or is predefined. 
	// To get extended error information, call GetLastError.
	std::map<uint32_t, tregister>::const_iterator it = registered_formats_.find(format);
	if (it != registered_formats_.end()) {
		strcpy(format_name_ptr, it->second.name.c_str());
	} else {
		format_name_ptr[0] = '\0';
	}
	return strlen(format_name_ptr);
}

uint32_t wfClipboard_kos::enum_clipboard_formats(uint32_t format)
{
	// @format: A clipboard format that is known to be available.
	// To start an enumeration of clipboard formats, set format to zero. When format is zero, 
	// the function retrieves the first available clipboard format. For subsequent calls during an enumeration,
	// set format to the result of the previous EnumClipboardFormats call.

	// If the function succeeds, the return value is the clipboard format that follows the specified format,
	// namely the next available clipboard format.
	if (format == 0) {
		return clip_data_.format != nposm? clip_data_.format: 0;
	}
	return 0;
}

uint32_t wfClipboard_kos::format_registered(int pid, const std::string& name) const
{
	for (std::map<uint32_t, tregister>::const_iterator it = registered_formats_.begin(); it != registered_formats_.end(); ++ it) {
		const tregister& _register = it->second;
		if (_register.pid == pid && _register.name == name) {
			return it->first;
		}
	}
	return nposm;
}

uint32_t wfClipboard_kos::register_clipboard_format(const std::string& utf8)
{
	VALIDATE(!utf8.empty(), null_str);

	uint32_t format = format_registered(FAKE_REGISTER_PID, utf8);
	if (format == nposm) {
		format = next_register_format_;
		registered_formats_.insert(std::make_pair(format, tregister(FAKE_REGISTER_PID, utf8)));

		next_register_format_ += REGISTER_FORMAT_INTERVAL;
		// ??? overflow
	}
	
	return format;
}

int wfClipboard_kos::get_format_data(bool hdrop, uint32_t format, uint8_t** ppdata)
{
	int size = 0;
	uint8_t* buff = NULL;
	*ppdata = nullptr;

	if (hdrop) {
		cliprdr_clear_file_array(this);
		if (clip_data_.format == CF_HDROP) {
			for (std::vector<std::string>::const_iterator it = clip_data_.files.begin(); it != clip_data_.files.end(); ++ it) {
				const std::wstring wFileName = utils::to_wstring(*it);
				wf_cliprdr_process_filename(this, wFileName.c_str(), wFileName.size());
			}

			FILEGROUPDESCRIPTOR* groupDsc;
			size = 4 + this->nFiles * sizeof(FILEDESCRIPTOR);
			groupDsc = (FILEGROUPDESCRIPTOR*)malloc(size);

			if (groupDsc)
			{
				groupDsc->cItems = this->nFiles;

				for (size_t i = 0; i < this->nFiles; i++) {
					if (this->fileDescriptor[i])
						groupDsc->fgd[i] = *this->fileDescriptor[i];
				}

				buff = (uint8_t*)groupDsc;
			}
		}

	} else {
		if (clip_data_.format == CF_UNICODETEXT && open_clipboard()) {
			size = (int)(clip_data_.wtext.size() + 1) * sizeof(wchar_t);
			buff = (uint8_t*)malloc(size * sizeof(uint16_t));
			// CopyMemory(buff, clip_data_.wtext.c_str(), size);
			utils::wchar_ptr_2_uint16_ptr((uint16_t*)buff, clip_data_.wtext.c_str(), size);

			close_clipboard();
		}
	}

	*ppdata = buff;
	return size;
}

void wfClipboard_kos::cliprdr_thread_func()
{
}

void wfClipboard_kos::save_format_data_response_internal(uint8_t* data, int size)
{
	if (data != nullptr) {
		hmem.assign((const char*)data, size);
		free(data);
	} else {
		hmem.clear();
	}
}

void wfClipboard_kos::save_format_data_response(const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	SDL_Log("wfClipboard_kos::save_format_data_response---");

	// Although not executed on a thread, but still can not use post, 
	// hdrop_paste_internal/render_format_internal is waiting for hmem to have data.
	hmem.assign((const char*)formatDataResponse->requestedFormatData, formatDataResponse->dataLen);
}

void* wfClipboard_kos::set_clipboard_data(uint32_t uFormat, const void* hMem)
{
	if (uFormat != CF_UNICODETEXT) {
		return nullptr;
	}
	if (pasting_) {
		SDL_Log("wfClipboard_kos::set_clipboard_data, pasting_, do nothing");
		// return nullptr;
	}
	if (hMem == nullptr) {
		if (thread_.get() != nullptr) {
			thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&wfClipboard_kos::render_format_internal, base::Unretained(this), uFormat));
		}
		return nullptr;
	}
	return nullptr;
}

void wfClipboard_kos::set_dataobject_2_clipboard_internal()
{
	uint32_t remote_format_id = nposm;
	clip_data_.clear();

	formatMapping* mapping;
	// CLIPRDR_FORMAT* format;
	const std::wstring DESCRIPTORW = utils::to_wstring(CFSTR_FILEDESCRIPTORW_UTF8);
	for (size_t i = 0; i < map_size; i++) {
		mapping = &(format_mappings[i]);

		if (!SDL_wcscmp(mapping->name, DESCRIPTORW.c_str())) {
			remote_format_id = mapping->remote_format_id;
			break;
		}
	}
	
	if (remote_format_id != nposm) {
		clip_data_.set_hdrop_remote(remote_format_id);
	}
}

uint32_t wfClipboard_kos::set_dataobject_2_clipboard()
{
	if (pasting_) {
		SDL_Log("wfClipboard_kos::set_dataobject_2_clipboard, pasting_, do nothing");
		// return CHANNEL_RC_OK;
	}
	if (thread_.get() != nullptr) {
		thread_->task_runner()->PostTask(FROM_HERE, base::BindOnce(&wfClipboard_kos::set_dataobject_2_clipboard_internal, base::Unretained(this)));
	}
	return CHANNEL_RC_OK;
}

void wfClipboard_kos::clipboard_require_update(bool remote)
{
	// wait, until pasting_ is ended.
	while (pasting_) {
		req_fevent.Set();
		response_data_event.Set();
		errcode_ = cliprdr_errcode_clipboardupdate;
		SDL_Delay(150);
	}
}

void wfClipboard_kos::pre_send_format_data_request()
{
	DCHECK(clipboard_thread_checker_->CalledOnValidThread());

	response_data_event.Reset();
	hmem.clear();
}

void wfClipboard_kos::trigger_quit()
{
}













wfClipboard* kosNewClipboard()
{
	wfClipboard* clipboard = new wfClipboard_kos;
	return static_cast<wfClipboard*>(clipboard);
}

wfClipboard* kosCustom2Clipboard(void* custom)
{
	wfClipboard_kos* clipboard = (wfClipboard_kos*)custom;
	return static_cast<wfClipboard*>(clipboard);
}