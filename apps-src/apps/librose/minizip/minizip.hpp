#ifndef LIBROSE_MINIZIP_HPP_INCLUDED
#define LIBROSE_MINIZIP_HPP_INCLUDED

#include <SDL_filesystem.h>
#include "util.hpp"
#include "filesystem.hpp"
#include "serialization/string_utils.hpp"

namespace minizip {

bool zip_file(const std::string& zipfile, const std::vector<std::string>& files, const std::string& password);
bool unzip_file(const std::string& zipfile, const std::string& outpath, const std::string& file_to_extract, const std::string& password);

}

#endif
