/*
   miniunz.c
   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#include "unzip.h"
#include "util.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "serialization/string_utils.hpp"

// MiniUnz 1.01b, demo of zLib + Unz package written by Gilles Vollant
// more info at http://www.winimage.com/zLibDll/unzip.html

#define CASESENSITIVITY (0)
#define WRITEBUFFERSIZE (8192)

namespace minizip {

static void change_file_date(const char *filename, uLong dosdate, tm_unz tmu_date)
{
	// impletement in future.
}

static int do_list(unzFile uf)
{
    uLong i;
    unz_global_info gi;
    int err;

    err = unzGetGlobalInfo (uf,&gi);
    if (err!=UNZ_OK)
        printf("error %d with zipfile in unzGetGlobalInfo \n",err);
    printf(" Length  Method   Size  Ratio   Date    Time   CRC-32     Name\n");
    printf(" ------  ------   ----  -----   ----    ----   ------     ----\n");
    for (i=0;i<gi.number_entry;i++)
    {
        char filename_inzip[256];
        unz_file_info file_info;
        uLong ratio=0;
        const char *string_method;
        char charCrypt=' ';
        err = rose_unzGetCurrentFileInfo(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);
        if (err!=UNZ_OK)
        {
            printf("error %d with zipfile in rose_unzGetCurrentFileInfo\n",err);
            break;
        }
        if (file_info.uncompressed_size>0)
            ratio = (file_info.compressed_size*100)/file_info.uncompressed_size;

        /* display a '*' if the file is crypted */
        if ((file_info.flag & 1) != 0)
            charCrypt='*';

        if (file_info.compression_method==0)
            string_method="Stored";
        else
        if (file_info.compression_method==Z_DEFLATED)
        {
            uInt iLevel=(uInt)((file_info.flag & 0x6)/2);
            if (iLevel==0)
              string_method="Defl:N";
            else if (iLevel==1)
              string_method="Defl:X";
            else if ((iLevel==2) || (iLevel==3))
              string_method="Defl:F"; /* 2:fast , 3 : extra fast*/
        }
        else
            string_method="Unkn. ";

        printf("%7lu  %6s%c%7lu %3lu%%  %2.2lu-%2.2lu-%2.2lu  %2.2lu:%2.2lu  %8.8lx   %s\n",
                file_info.uncompressed_size,string_method,
                charCrypt,
                file_info.compressed_size,
                ratio,
                (uLong)file_info.tmu_date.tm_mon + 1,
                (uLong)file_info.tmu_date.tm_mday,
                (uLong)file_info.tmu_date.tm_year % 100,
                (uLong)file_info.tmu_date.tm_hour,(uLong)file_info.tmu_date.tm_min,
                (uLong)file_info.crc,filename_inzip);
        if ((i+1)<gi.number_entry)
        {
            err = unzGoToNextFile(uf);
            if (err!=UNZ_OK)
            {
                printf("error %d with zipfile in unzGoToNextFile\n",err);
                break;
            }
        }
    }

    return 0;
}


static int do_extract_currentfile(unzFile uf,
    const std::string& outpath,
    const char* password)
{
    char filename_inzip[256];
    int err=UNZ_OK;
    FILE *fout=NULL;
    void* buf;
    uInt size_buf;

    unz_file_info file_info;
    uLong ratio=0;
    err = rose_unzGetCurrentFileInfo(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

    if (err!=UNZ_OK) {
        // "error %d with zipfile in rose_unzGetCurrentFileInfo
        return err;
    }

    size_buf = WRITEBUFFERSIZE;
    buf = (void*)malloc(size_buf);
    if (buf==NULL)
    {
        // Error allocating memory
        return UNZ_INTERNALERROR;
    }

	std::string fullfilename_inzip = outpath + "/" + filename_inzip;

	bool isdir = SDL_IsDirectory(fullfilename_inzip.c_str());
	std::string directory = isdir? fullfilename_inzip: extract_directory(fullfilename_inzip);
    SDL_MakeDirectory(directory.c_str());

    if (!isdir) {
        const char* write_filename = fullfilename_inzip.c_str();

        err = unzOpenCurrentFilePassword(uf, password);
        if (err!=UNZ_OK)
        {
            // error %d with zipfile in unzOpenCurrentFilePassword
        }

		tfile fout(fullfilename_inzip, GENERIC_WRITE, CREATE_ALWAYS);
        if (fout.valid()) {
            do {
                err = unzReadCurrentFile(uf,buf,size_buf);
                if (err<0) {
                    // error %d with zipfile in unzReadCurrentFile
                    break;
                }
                if (err > 0) {
                    posix_fwrite(fout.fp, buf, err);
				}
            } while (err > 0);
            fout.close();

            if (err==0) {
                // change_file_date(write_filename,file_info.dosDate, file_info.tmu_date);
			}
        }

        if (err==UNZ_OK) {
            err = unzCloseCurrentFile(uf);
        } else {
			unzCloseCurrentFile(uf);
		}
    }

    free(buf);
    return err;
}


static bool do_extract(unzFile uf, const std::string& outpath, const char* password)
{
    uLong i;
    unz_global_info gi;
    int err = UNZ_OK;

    err = unzGetGlobalInfo(uf,&gi);
    if (err != UNZ_OK) {
        // error %d with zipfile in unzGetGlobalInfo
		return false;
	}

    for (i=0;i<gi.number_entry;i++) {
        if (do_extract_currentfile(uf, outpath, password) != UNZ_OK) {
            break;
		}

        if ((i+1)<gi.number_entry) {
            err = unzGoToNextFile(uf);
            if (err!=UNZ_OK) {
                // error %d with zipfile in unzGoToNextFile
                break;
            }
        }
    }

    return err == UNZ_OK;
}

static int do_extract_onefile(unzFile uf,
    const char* filename,
    const std::string& outpath,
    const char* password)
{
    int err = UNZ_OK;
    if (unzLocateFile(uf,filename,CASESENSITIVITY)!=UNZ_OK) {
        // file %s not found in the zipfile
        return false;
    }

    err = do_extract_currentfile(uf, outpath, password);
    return err == UNZ_OK;
}

struct tunzip_close_file_lock
{
    tunzip_close_file_lock(SDL_RWops* fp)
        : fp(fp)
    {}

    ~tunzip_close_file_lock()
    {
        unzClose(fp);
    }

    SDL_RWops* fp;
};

bool unzip_file(const std::string& zipfile, const std::string& outpath2, const std::string& file_to_extract, const std::string& password)
{
	VALIDATE(!zipfile.empty(), null_str);
	const std::string outpath = outpath2.empty()? extract_directory(zipfile): outpath2;

    bool opt_do_list = false;
	bool ret = false;

    unzFile uf = unzOpen2(zipfile.c_str());
	if (uf == nullptr) {
		return false;
	}

    tunzip_close_file_lock lock(uf);

    if (opt_do_list) {
        return do_list(uf);

	} else {
        if (file_to_extract.empty()) {
            ret = do_extract(uf, outpath, password.empty()? nullptr: password.c_str());
		} else {
            ret = do_extract_onefile(uf, file_to_extract.c_str(),
                                      outpath, password.empty()? nullptr: password.c_str());
		}
    }
    unzCloseCurrentFile(uf);

    return true;
}

} // namespace minizip