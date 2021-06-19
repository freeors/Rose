/*
   minizip.c
   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#include "zip.h"
#include <SDL_filesystem.h>
#include "util.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "serialization/string_utils.hpp"
using namespace std::placeholders;


// MiniZip 1.01b, demo of zLib + Zip package written by Gilles Vollant
// more info at http://www.winimage.com/zLibDll/unzip.html

namespace minizip {
// dt: dostime
static uLong filetime(const char *f, tm_zip *tmzip,  uLong *dt)
{
	SDL_dirent stat;
	SDL_GetStat(f, &stat);
	*dt = stat.mtime;

	time_t tm_t = stat.mtime;
	struct tm* filedate = localtime(&tm_t);
	tmzip->tm_sec  = filedate->tm_sec;
	tmzip->tm_min  = filedate->tm_min;
	tmzip->tm_hour = filedate->tm_hour;
	tmzip->tm_mday = filedate->tm_mday;
	tmzip->tm_mon  = filedate->tm_mon ;
	tmzip->tm_year = filedate->tm_year;
	return 0;
}

// calculate the CRC32 of a file,
// because to encrypt a file, we need known the CRC32 of the file before
static void getFileCrc(tfile& file, uint8_t* buf, unsigned long size_buf, unsigned long* result_crc)
{
	unsigned long calculate_crc=0;
	unsigned long size_read = 0;
	unsigned long total_read = 0;

    do {
        size_read = posix_fread(file.fp, file.data, size_buf);
        if (size_read > 0) {
            calculate_crc = crc32(calculate_crc, (const uint8_t*)buf, size_read);
		}
        total_read += size_read;

    } while (size_read > 0);

    *result_crc=calculate_crc;
}

bool did_enum_files(const std::string& dir, const SDL_dirent2* dirent, std::vector<std::string>& block_files)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		const std::string file = dir + "/" + dirent->name;
		block_files.push_back(file);
	}

	return true;
}

bool zip_file(const std::string& zipfile, const std::vector<std::string>& files, const std::string& password)
{
	VALIDATE(!zipfile.empty() && !files.empty(), null_str);
    int opt_overwrite = 1;
    int opt_compress_level = Z_DEFAULT_COMPRESSION;
    int zipfilenamearg = 0;
    int err=0;

    int size_buf = 16 * 1024;
    uint8_t* buf = (uint8_t*)malloc(size_buf);
	VALIDATE(buf != nullptr, null_str);

    {
        zipFile zf;
        int errclose;
        zf = zipOpen2(zipfile.c_str(), (opt_overwrite==2)? 2 : 0);
		VALIDATE(zf != nullptr, null_str);

		std::vector<std::string> block_files;
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end() && err == ZIP_OK; ++ it) {
            int size_read;
            const std::string& desirezipfilename = utils::normalize_path(*it);
			VALIDATE(!desirezipfilename.empty() && desirezipfilename[desirezipfilename.size() - 1] != '/', null_str);
			VALIDATE(!SDL_IsRootPath(desirezipfilename.c_str()), null_str);

			const bool isfileinzip = SDL_IsFile(desirezipfilename.c_str());
			if (!isfileinzip && !SDL_IsDirectory(desirezipfilename.c_str())) {
				// desirezipfilename is not both file and directory.
				continue;
			}
			const std::string require_cut_prefix = extract_directory(desirezipfilename) + '/';

			block_files.clear();
			if (isfileinzip) {
				block_files.push_back(desirezipfilename);
			} else {
				::walk_dir(desirezipfilename, true, std::bind(
					&did_enum_files
					, _1, _2, std::ref(block_files)));
			}

			for (std::vector<std::string>::const_iterator it2 = block_files.begin(); it2 != block_files.end() && err == ZIP_OK; ++ it2) {
				// const std::string& filename = utils::normalize_path(*it);
				const std::string& filename = *it2;

				zip_fileinfo zi;
				unsigned long crcFile = 0;
				zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
				zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
				zi.dosDate = 0;
				zi.internal_fa = 0;
				zi.external_fa = 0;
				filetime(filename.c_str(), &zi.tmz_date, &zi.dosDate);

				tfile file(filename, GENERIC_READ, OPEN_EXISTING);
				if (!file.valid()) {
					err = ZIP_ERRNO;
					continue;
				}
				if (!password.empty() && (err==ZIP_OK)) {
					getFileCrc(file, buf, size_buf, &crcFile);
					posix_fseek(file.fp, 0);
				}

				std::string filenameinzip = filename.substr(require_cut_prefix.size());
				err = rose_zipOpenNewFileInZip3(zf, filenameinzip.c_str(), &zi,
									NULL,0,NULL,0,NULL /* comment*/,
									(opt_compress_level != 0) ? Z_DEFLATED : 0,
									opt_compress_level,0,
									/* -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, */
									-MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
									password.empty()? nullptr: password.c_str(), crcFile);
				if (err != ZIP_OK) {
					// error in opening %s in zipfile);
					continue;
				}

				do {
					size_read = (int)posix_fread(file.fp, buf, size_buf);
					if (size_read >0) {
						err = rose_zipWriteInFileInZip(zf, buf, size_read);
					}
				} while ((err == ZIP_OK) && (size_read>0));

				if (err < 0) {
					err = ZIP_ERRNO;
				} else {
					err = rose_zipCloseFileInZip(zf);
				}
			}
        }
        errclose = rose_zipClose(zf,NULL);
    }

    free(buf);
    return err == ZIP_OK;
}

} // namespace minizip