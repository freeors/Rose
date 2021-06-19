/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2018 Live Networks, Inc.  All rights reserved.
// Common routines for opening/closing named input files
// Implementation

#include "liveMedia/include/InputFile.hh"
#include <string.h>
#include "base/logging.h"

#include "filesystem.hpp"
#include "rose_config.hpp"
#include "SDL_log.h"

tfile* OpenInputFile(UsageEnvironment& env, char const* fileName) {
  // Check for a special case file name: "stdin"
  CHECK(strcmp(fileName, "stdin") != 0);
    
    const std::string fullname = game_config::preferences_dir + "/" + fileName;
    tfile* file = new tfile(fullname, GENERIC_READ, OPEN_EXISTING);
    if (!file->valid()) {
      env.setResultMsg("unable to open file \"",fileName, "\"");
    }
	SDL_Log("tfile(%s, GENERIC_READ, OPEN_EXISTING), %s", fullname.c_str(), file->valid()? "true": "false");

  return file;
}

void CloseInputFile(tfile* fid) {
  CHECK(fid != nullptr);
  // Don't close 'stdin', in case we want to use it again later.
  if (fid->valid()) fid->close();
  delete fid;
}

u_int64_t GetFileSize(char const* fileName, tfile* fid) {
  CHECK(fid != nullptr && fid->valid());
  u_int64_t fileSize = posix_fsize(fid->fp);
  return fileSize;
}

int64_t SeekFile64(tfile *fid, int64_t offset, int whence) {
  CHECK(whence == SEEK_SET);
  if (fid == nullptr || !fid->valid()) return -1;

  return posix_fseek(fid->fp, offset);
}

int64_t TellFile64(FILE *fid) {
  CHECK(false);
  if (fid == NULL) return -1;

  clearerr(fid);
  fflush(fid);
#if (defined(__WIN32__) || defined(_WIN32)) && !defined(_WIN32_WCE)
  return _telli64(_fileno(fid));
#else
#if defined(_WIN32_WCE)
  return ftell(fid);
#else
  return ftello(fid);
#endif
#endif
}

Boolean FileIsSeekable() {
  return True;
}
