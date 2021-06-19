/* ioapi.h -- IO base function header for compress/uncompress .zip
   files using zlib + zip or unzip API

   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#ifndef MINIZIP_IOAPI_H
#define MINIZIP_IOAPI_H

#include <SDL_rwops.h>

#define ZLIB_FILEFUNC_SEEK_CUR (RW_SEEK_CUR)
#define ZLIB_FILEFUNC_SEEK_END (RW_SEEK_END)
#define ZLIB_FILEFUNC_SEEK_SET (RW_SEEK_SET)

#ifdef __cplusplus
extern "C" {
#endif

#define ZREAD(filestream,buf,size) (SDL_RWread(filestream, buf, 1, size))
#define ZWRITE(filestream,buf,size) (SDL_RWwrite(filestream, buf, 1, size))
#define ZTELL(filestream) (SDL_RWtell(filestream))
#define ZSEEK(filestream,pos,mode) (SDL_RWseek(filestream, pos, mode))
#define ZCLOSE(filestream) (SDL_RWclose(filestream))


#ifdef __cplusplus
}
#endif

#endif // MINIZIP_IOAPI_H

