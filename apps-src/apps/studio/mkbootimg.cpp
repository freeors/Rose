/* tools/mkbootimg/mkbootimg.c
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "serialization/string_utils.hpp"

// #include "mincrypt/sha.h"
#include <openssl/sha.h>
#include "bootimg.hpp"
/*
static void *load_file(const char *fn, unsigned *_sz)
{
    char *data;
    int sz;
    int fd;

    data = 0;
    fd = open(fn, O_RDONLY);
    if(fd < 0) return 0;

    sz = lseek(fd, 0, SEEK_END);
    if(sz < 0) goto oops;

    if(lseek(fd, 0, SEEK_SET) != 0) goto oops;

    data = (char*) malloc(sz);
    if(data == 0) goto oops;

    if(read(fd, data, sz) != sz) goto oops;
    close(fd);

    if(_sz) *_sz = sz;
    return data;

oops:
    close(fd);
    if(data != 0) free(data);
    return 0;
}
*/

static tfile* load_file(const char *fn, unsigned *_sz)
{
	tfile* fp = new tfile(fn, GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = fp->read_2_data();
	VALIDATE(fsize > 0, null_str);

	*_sz = fsize;
	return fp;
}

static int usage2(void)
{
    fprintf(stderr,"usage: mkbootimg\n"
            "       --kernel <filename>\n"
            "       --ramdisk <filename>\n"
            "       [ --second <2ndbootloader-filename> ]\n"
            "       [ --cmdline <kernel-commandline> ]\n"
            "       [ --board <boardname> ]\n"
            "       [ --base <address> ]\n"
            "       [ --pagesize <pagesize> ]\n"
            "       -o|--output <filename>\n"
            );
    return 1;
}



static unsigned char padding[16384] = { 0, };

int write_padding(posix_file_t fd, unsigned pagesize, unsigned itemsize)
{
    unsigned pagemask = pagesize - 1;
    size_t count;

    if ((itemsize & pagemask) == 0) {
        return 0;
    }

    count = pagesize - (itemsize & pagemask);

    if (posix_fwrite(fd, padding, count) != count) {
        return -1;
    } else {
        return 0;
    }
}

bool mkbootimg(const std::string& kernel_fn, const std::string& ramdisk_fn, const std::string& bootimg)
{
	VALIDATE(!kernel_fn.empty() && !ramdisk_fn.empty() && !bootimg.empty(), null_str);
    boot_img_hdr hdr;

    void *kernel_data = 0;
    void *ramdisk_data = 0;
    char *second_fn = 0;
    void *second_data = 0;

	std::unique_ptr<tfile> kernel_fp;
	std::unique_ptr<tfile> ramdisk_fp;
	std::unique_ptr<tfile> second_fp;

    char *cmdline = "";
    char *board = "";
    unsigned pagesize = 2048;
    SHA_CTX ctx;
    uint8_t sha[SHA_DIGEST_LENGTH];
    unsigned base           = 0x10000000;
    unsigned kernel_offset  = 0x00008000;
    unsigned ramdisk_offset = 0x01000000;
    unsigned second_offset  = 0x00f00000;
    unsigned tags_offset    = 0x00000100;
    size_t cmdlen;

    memset(&hdr, 0, sizeof(hdr));
/*
    while(argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        if(argc < 2) {
            return usage2();
        }
        argc -= 2;
        argv += 2;
        if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            bootimg = val;
        } else if(!strcmp(arg, "--second")) {
            second_fn = val;
        } else if(!strcmp(arg, "--cmdline")) {
            cmdline = val;
        } else if(!strcmp(arg, "--base")) {
            base = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--kernel_offset")) {
            kernel_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--ramdisk_offset")) {
            ramdisk_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--second_offset")) {
            second_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--tags_offset")) {
            tags_offset = strtoul(val, 0, 16);
        } else if(!strcmp(arg, "--board")) {
            board = val;
        } else if(!strcmp(arg,"--pagesize")) {
            pagesize = strtoul(val, 0, 10);
            if ((pagesize != 2048) && (pagesize != 4096)
                && (pagesize != 8192) && (pagesize != 16384)) {
                fprintf(stderr,"error: unsupported page size %d\n", pagesize);
                return -1;
            }
        } else {
            return usage2();
        }
    }
*/
    hdr.page_size = pagesize;

    hdr.kernel_addr =  base + kernel_offset;
    hdr.ramdisk_addr = base + ramdisk_offset;
    hdr.second_addr =  base + second_offset;
    hdr.tags_addr =    base + tags_offset;

    if(strlen(board) >= BOOT_NAME_SIZE) {
        fprintf(stderr,"error: board name too large\n");
        return false;
    }

    strcpy((char *) hdr.name, board);

    memcpy(hdr.magic, BOOT_MAGIC, BOOT_MAGIC_SIZE);

    cmdlen = strlen(cmdline);
    if(cmdlen > (BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE - 2)) {
        fprintf(stderr,"error: kernel commandline too large\n");
        return false;
    }
    /* Even if we need to use the supplemental field, ensure we
     * are still NULL-terminated */
    strncpy((char *)hdr.cmdline, cmdline, BOOT_ARGS_SIZE - 1);
    hdr.cmdline[BOOT_ARGS_SIZE - 1] = '\0';
    if (cmdlen >= (BOOT_ARGS_SIZE - 1)) {
        cmdline += (BOOT_ARGS_SIZE - 1);
        strncpy((char *)hdr.extra_cmdline, cmdline, BOOT_EXTRA_ARGS_SIZE);
    }

    kernel_fp.reset(load_file(kernel_fn.c_str(), &hdr.kernel_size));
	kernel_data = kernel_fp.get()->data;
    
	ramdisk_fp.reset(load_file(ramdisk_fn.c_str(), &hdr.ramdisk_size));
	ramdisk_data = ramdisk_fp.get()->data;

    if (second_fn) {
		second_fp.reset(load_file(second_fn, &hdr.second_size));
		second_data = second_fp.get()->data;
        if(second_data == 0) {
            fprintf(stderr,"error: could not load secondstage '%s'\n", second_fn);
            return false;
        }
    }

    /* put a hash of the contents in the header so boot images can be
     * differentiated based on their first 2k.
     */
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, kernel_data, hdr.kernel_size);
    SHA1_Update(&ctx, &hdr.kernel_size, sizeof(hdr.kernel_size));
    SHA1_Update(&ctx, ramdisk_data, hdr.ramdisk_size);
    SHA1_Update(&ctx, &hdr.ramdisk_size, sizeof(hdr.ramdisk_size));
    SHA1_Update(&ctx, second_data, hdr.second_size);
    SHA1_Update(&ctx, &hdr.second_size, sizeof(hdr.second_size));
    SHA1_Final(sha, &ctx);
    memcpy(hdr.id, sha,
           SHA_DIGEST_LENGTH > sizeof(hdr.id) ? sizeof(hdr.id) : SHA_DIGEST_LENGTH);

	tfile fd(bootimg, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(fd.valid(), null_str);

    if (posix_fwrite(fd.fp, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		goto fail;
	}
    if (write_padding(fd.fp, pagesize, sizeof(hdr))) {
		goto fail;
	}

    if (posix_fwrite(fd.fp, kernel_data, hdr.kernel_size) != (size_t) hdr.kernel_size) {
		goto fail;
	}
    if (write_padding(fd.fp, pagesize, hdr.kernel_size)) {
		goto fail;
	}

    if (posix_fwrite(fd.fp, ramdisk_data, hdr.ramdisk_size) != (size_t) hdr.ramdisk_size) {
		goto fail;
	}
    if (write_padding(fd.fp, pagesize, hdr.ramdisk_size)) {
		goto fail;
	}

    if (second_data) {
        if(posix_fwrite(fd.fp, second_data, hdr.second_size) != (size_t) hdr.second_size) {
			goto fail;
		}
        if(write_padding(fd.fp, pagesize, hdr.second_size)) {
			goto fail;
		}
    }

    return true;

fail:
    return false;
}
