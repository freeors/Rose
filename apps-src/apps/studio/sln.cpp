#include "global.hpp"
#include "game_config.hpp"
#include "filesystem.hpp"
#include "sln.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"


const std::string apps_sln_guid = "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942";

namespace apps_sln {

std::string sln_from_src2_path(const std::string& src2_path)
{
	const std::string app = extract_file(src2_path);
	bool is_work_kit = app == "apps";

	VALIDATE(!game_config::apps_src_path.empty(), null_str);
	return src2_path + "/projectfiles/vc/" + (is_work_kit? "apps.sln": (app + ".sln"));
}

std::map<std::string, std::string> apps_in(const std::string& src2_path)
{
	std::map<std::string, std::string> apps;
	std::stringstream ss;

	const std::string sln = sln_from_src2_path(src2_path);

	tfile file(sln,  GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return apps;
	}
	// i think length of appended data isn't more than 512 bytes.
	file.resize_data(fsize + 512, fsize);
	file.data[fsize] = '\0';

	const char* start2 = file.data;
	if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
		start2 += BOM_LENGTH;
	}

	// studio
	ss.str("");
	ss << "Project(\"{" << apps_sln_guid << "}\") = \"";
	std::string prefix = ss.str();
	const char* ptr = NULL;
	{
		ptr = strstr(start2, prefix.c_str());
		while (ptr) {
			ptr += prefix.size();
			start2 = strchr(ptr, '\"');
			VALIDATE(start2, null_str);

			const std::string app(ptr, start2 - ptr);

			ptr = strchr(start2, '{');
			VALIDATE(ptr, null_str);
			ptr ++;
			start2 = strchr(ptr, '}');
			VALIDATE(start2, null_str);
			const std::string guid(ptr, start2 - ptr);

			apps.insert(std::make_pair(app, guid));
			ptr = strstr(start2, prefix.c_str());
		}
	}

	return apps;
}

std::set<std::string> include_directories_in(const std::string& app)
{
	std::set<std::string> result;

	const std::string vcxproj = game_config::apps_src_path + "/apps/projectfiles/vc/" + app + ".vcxproj";
	tfile file(vcxproj,  GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return result;
	}

	const char* start2 = file.data;
	if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
		start2 += BOM_LENGTH;
	}

	// studio
	std::string prefix = "<ItemDefinitionGroup Condition=\"'$(Configuration)|$(Platform)'=='Release|Win32'\">";
	const char* ptr = NULL;
	{
		ptr = strstr(start2, prefix.c_str());
		VALIDATE(ptr, null_str);

		ptr += prefix.size();
		prefix = "<AdditionalIncludeDirectories>";
		ptr = strstr(ptr, prefix.c_str());
		VALIDATE(ptr, null_str);
		start2 = ptr + prefix.size();

		prefix = "</AdditionalIncludeDirectories>";
		ptr = strstr(start2, prefix.c_str());

		const std::string str(start2, ptr - start2);
		std::vector<std::string> directories = utils::split(str, ';');

		for (std::vector<std::string>::const_iterator it = directories.begin(); it != directories.end(); ++ it) {
			result.insert(*it);
		}
	}

	return result;
}

bool add_project(const std::string& app, const std::string& guid_str)
{
	std::set<std::string> apps;
	std::stringstream ss;

	VALIDATE(!game_config::apps_src_path.empty(), null_str);
	const std::string sln = game_config::apps_src_path + "/apps/projectfiles/vc/apps.sln";

	tfile file(sln, GENERIC_WRITE, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return false;
	}
	// i think length of appended data isn't more than 512 bytes.
	file.resize_data(fsize + 512, fsize);
	file.data[fsize] = '\0';

	const char* start2 = file.data;
	if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
		start2 += BOM_LENGTH;
	}

	// insert project at end
	std::string prefix = "EndProject";
	std::string postfix = "Global";
	const char* ptr = strstr(start2, prefix.c_str());
	while (ptr) {
		const char* ptr2 = utils::skip_blank_characters(ptr + prefix.size());
		if (!SDL_strncmp(ptr2, postfix.c_str(), postfix.size())) {
			std::stringstream ss;
			ss << "Project(\"{" << apps_sln_guid << "}\") = \"" << app << "\", \"" << app << ".vcxproj\", \"{" << guid_str << "}\"";
			ss << "\r\n";
			ss << "EndProject";
			ss << "\r\n";
			fsize = file.replace_span(ptr2 - file.data, 0, ss.str().c_str(), ss.str().size(), fsize);
			break;
		}
		ptr = strstr(ptr2, prefix.c_str());
	}

	// insert configuration
	prefix = "= postSolution";
	ptr = strstr(start2, prefix.c_str());
	if (ptr) {
		postfix = "EndGlobalSection";
		ptr = strstr(ptr, postfix.c_str());
		if (ptr) {
			ptr -= 1; // \t
		}
	}
	if (ptr) {
		std::stringstream ss;
		ss << "\t\t{" << guid_str << "}.Debug|Win32.ActiveCfg = Debug|Win32" << "\r\n";
		ss << "\t\t{" << guid_str << "}.Debug|Win32.Build.0 = Debug|Win32" << "\r\n";
		ss << "\t\t{" << guid_str << "}.Release|Win32.ActiveCfg = Release|Win32" << "\r\n";
		ss << "\t\t{" << guid_str << "}.Release|Win32.Build.0 = Release|Win32" << "\r\n";
		fsize = file.replace_span(ptr - file.data, 0, ss.str().c_str(), ss.str().size(), fsize);
	}

	// write data to new file
	posix_fseek(file.fp, 0);
	posix_fwrite(file.fp, file.data, fsize);

	return true;
}

bool remove_project(const std::string& app)
{
	std::set<std::string> apps;
	std::stringstream ss;

	VALIDATE(!game_config::apps_src_path.empty(), null_str);
	const std::string sln = game_config::apps_src_path + "/apps/projectfiles/vc/apps.sln";

	// const std::string sln3 = game_config::apps_src_path + "/apps/projectfiles/vc/apps.sln3";
	// SDL_CopyFiles(sln.c_str(), sln3.c_str());

	tfile file(sln,  GENERIC_WRITE, OPEN_EXISTING);
	int fsize = file.read_2_data();
	if (!fsize) {
		return false;
	}
	// i think length of appended data isn't more than 512 bytes.
	file.resize_data(fsize + 512, fsize);
	file.data[fsize] = '\0';

	const char* start2 = file.data;
	if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
		start2 += BOM_LENGTH;
	}

	// studio
	ss.str("");
	ss << "Project(\"{" << apps_sln_guid << "}\") = \"" << app << "\"";
	std::string prefix = ss.str();
	const char* ptr = strstr(start2, prefix.c_str());
	if (!ptr) {
		return false;
	}
	ptr = strchr(ptr + prefix.size(), '{');
	VALIDATE(ptr, null_str);

	start2 = ptr + 1;
	ptr = strchr(start2, '}');

	const std::string guid_str(start2, ptr - start2);
	const int guid_size2 = 36;
	VALIDATE(guid_str.size() == guid_size2, null_str);
	start2 += guid_str.size();
	// move start2 to first char of this line
	while (start2[0] != '\r' && start2[0] != '\n') { start2 --; }
	start2 ++;

	//
	// delete like below lines:
	// Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "hello", "hello.vcxproj", "{43FDDD3E-D26A-4F52-B207-8DC03CB25396}"
	// EndProject
	//

	prefix = "EndProject";
	ptr = strstr(ptr, prefix.c_str());
	if (!ptr) {
		return false;
	}
	ptr += prefix.size();
	// move ptr to first char of next line.
	while (ptr[0] != '\r' && ptr[0] != '\n') { ptr ++; }
	while (ptr[0] == '\r' || ptr[0] == '\n') { ptr ++; }
	fsize = file.replace_span(start2 - file.data, ptr - start2, NULL, 0, fsize);

	// remove items in GlobalSection
	ss.str("");
	ss << "{" << guid_str << "}";
	prefix = ss.str();

	ptr = strstr(start2, prefix.c_str());
	if (!ptr) {
		return false;
	}
	start2 = ptr - 1;
	while (start2[0] == '\t' || start2[0] == ' ') { start2 --; }
	// start indicate postion that from delete.
	const char* start = start2 + 1;

	// move start2 to last line that has guid_str.
	while (ptr) {
		start2 = ptr;
		ptr = strstr(start2 + prefix.size(), prefix.c_str());
	}

	// move ptr to first char of next line.
	ptr = start2 + prefix.size();
	while (ptr[0] != '\r' && ptr[0] != '\n') { ptr ++; }
	while (ptr[0] == '\r' || ptr[0] == '\n') { ptr ++; }

	fsize = file.replace_span(start - file.data, ptr - start, NULL, 0, fsize);

	// write data to new file
	posix_fseek(file.fp, 0);
	posix_fwrite(file.fp, file.data, fsize);

	file.truncate(fsize);

	return true;
}

bool can_new_dialog(const std::string& app)
{
	std::set<std::string> apps;
	std::stringstream ss;

	VALIDATE(!game_config::apps_src_path.empty() && !app.empty(), null_str);

	{
		const std::string vcxproj = game_config::apps_src_path + "/apps/projectfiles/vc/" + app + ".vcxproj";
		tfile file(vcxproj, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		// <ClCompile Include="..\..\studio\main.cpp"
		std::string prefix = "<ClCompile Include=\"..\\..\\" + app + "\\main.cpp\"";
		const char* ptr = strstr(start2, prefix.c_str());
		if (!ptr) {
			return false;
		}
	}

	{
		const std::string filters = game_config::apps_src_path + "/apps/projectfiles/vc/" + app + ".vcxproj.filters";
		tfile file(filters, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		std::vector<std::string> must_filters;
		must_filters.push_back("Source Files");
		must_filters.push_back("Header Files");
		must_filters.push_back("gui\\dialogs");

		for (std::vector<std::string>::const_iterator it = must_filters.begin(); it != must_filters.end(); ++ it) {
			// <Filter Include="gui\dialogs">
			std::string prefix = "<Filter Include=\"" + *it + "\">";
			const char* ptr = strstr(start2, prefix.c_str());
			if (!ptr) {
				return false;
			}
		}
	}
	return true;
}

int append_at_ItemGroup_end(tfile& file, const char* ptr, const int fsize, const std::string& str)
{
	const std::string prefix = "</ItemGroup>";
	const char* start2 = strstr(ptr, prefix.c_str());
	VALIDATE(start2, null_str);

	while (start2[0] != '\r' && start2[0] != '\n') { start2 --; }
	start2 ++;

	return file.replace_span(start2 - file.data, 0, str.c_str(), str.size(), fsize);
}

// @start2: this file's start pointer.
// @ptr: previous pointer.
const char* insert_hpp_ItemGroup(tfile& file, const char* start2, const std::string& app, int& fsize)
{
	std::string prefix = "<ClInclude Include=\"";
	const char* ptr2 = strstr(start2, prefix.c_str());
	if (!ptr2) {
		// there is no ItemGroup save .hpp, create it.
		// always create after the ItemGroup contain main.cpp.
		std::string prefix = "<ClCompile Include=\"..\\..\\" + app + "\\main.cpp\"";
		const char* ptr = strstr(start2, prefix.c_str());
		VALIDATE(ptr, null_str); // sorry, must conatin main.cpp.

		prefix = "</ItemGroup>\r\n";
		ptr = strstr(ptr, prefix.c_str());
		VALIDATE(ptr, null_str);
		ptr += prefix.size();

		std::stringstream ss;
		//  <ItemGroup>
		//  </ItemGroup>
		ss << "  <ItemGroup>\r\n";
		ss << "  </ItemGroup>\r\n";
		fsize = file.replace_span(ptr - file.data, 0, ss.str().c_str(), ss.str().size(), fsize);
		ptr2 = ptr + ss.str().size() - prefix.size();
	}
	return ptr2;
}

static std::pair<std::string, bool> generate_insert_string(const std::string& app, const std::string& file, const bool filters)
{
	bool cpp = true;
	const std::string file2 = utils::normalize_path(file, true);
	VALIDATE(file2.size() > 4, null_str);

	size_t pos = file2.rfind(".hpp");
	if (pos == file2.size() - 4) {
		cpp = false;
	} else {
		// .cpp
		pos = file2.rfind(".cpp");
		VALIDATE(pos + 4 == file2.size(), null_str);
	}

	const std::string dir = extract_directory(file2);
	std::stringstream ss;
	if (!filters) {
		if (cpp) {
			//    <ClCompile Include="..\..\studio\gui\dialogs\control_setting.cpp">
			//      <ObjectFileName Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">$(IntDir)gui\dialogs\</ObjectFileName>
			//      <ObjectFileName Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">$(IntDir)gui\dialogs\</ObjectFileName>
			//    </ClCompile>
			if (dir.empty()) {
				ss << "    <ClCompile Include=\"..\\..\\" << app << "\\" << file2 << "\" />\r\n";
			} else {
				ss << "    <ClCompile Include=\"..\\..\\" << app << "\\" << file2 << "\">\r\n";
				ss << "      <ObjectFileName Condition=\"'$(Configuration)|$(Platform)'=='Debug|Win32'\">$(IntDir)" << dir << "\\</ObjectFileName>\r\n";
				ss << "      <ObjectFileName Condition=\"'$(Configuration)|$(Platform)'=='Release|Win32'\">$(IntDir)" << dir << "\\</ObjectFileName>\r\n";
				ss << "    </ClCompile>\r\n";
			}

		} else {
			//    <ClInclude Include="..\..\studio\gui\dialogs\cell_setting.hpp" />
			ss << "    <ClInclude Include=\"..\\..\\" << app << "\\" << file2 << "\" />\r\n";
		}
		
	} else {
		// const std::string filter = "Source Files";
		std::string filter = dir;
		if (filter.empty()) {
			filter = cpp? "Source Files": "Header Files";
		}

		if (cpp) {
			//    <ClCompile Include="..\..\studio\gui\dialogs\control_setting.cpp">
			//      <Filter>gui\dialogs</Filter>
			//    </ClCompile>
			ss << "    <ClCompile Include=\"..\\..\\" << app << "\\" << file2 << "\">\r\n";
			ss << "      <Filter>" << filter << "</Filter>\r\n";
			ss << "    </ClCompile>\r\n";
		} else {
			// .hpp
			//    <ClInclude Include="..\..\studio\gui\dialogs\cell_setting.hpp">
			//      <Filter>gui\dialogs</Filter>
			//    </ClInclude>
			ss << "    <ClInclude Include=\"..\\..\\" << app << "\\" << file2 << "\">\r\n";
			ss << "      <Filter>" << filter << "</Filter>\r\n";
			ss << "    </ClInclude>\r\n";
		}
	}
	return std::make_pair(ss.str(), cpp);
}

bool new_dialog(const std::string& app, const std::vector<std::string>& files)
{
	std::set<std::string> apps;

	VALIDATE(!game_config::apps_src_path.empty() && !app.empty() && !files.empty(), null_str);

	const std::string vcxproj = game_config::apps_src_path + "/apps/projectfiles/vc/" + app + ".vcxproj";
	const std::string vcxproj_tmp = game_config::preferences_dir + "/__tmp";
	
	{
		SDL_CopyFiles(vcxproj.c_str(), vcxproj_tmp.c_str());
		tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		// <ClCompile Include="..\..\studio\main.cpp"
		std::string prefix = "<ClCompile Include=\"..\\..\\" + app + "\\main.cpp\"";
		const char* cpp_ptr = strstr(start2, prefix.c_str());
		if (!cpp_ptr) {
			return false;
		}

		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			std::pair<std::string, bool> pair = generate_insert_string(app, *it, false);
			const char* ptr = nullptr;
			if (!pair.second) {
				// .hpp
				ptr = insert_hpp_ItemGroup(file, start2, app, fsize);
			} else {
				ptr = cpp_ptr;
			}
			fsize = append_at_ItemGroup_end(file, ptr, fsize, pair.first);
		}
		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
	}

	{
		const std::string filters = game_config::apps_src_path + "/apps/projectfiles/vc/" + app + ".vcxproj.filters";
		tfile file(filters, GENERIC_WRITE, OPEN_EXISTING);

		// SDL_CopyFiles(filters.c_str(), vcxproj_tmp.c_str());
		// tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);

		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		// <ClCompile Include="..\..\studio\main.cpp"
		std::string prefix = "<ClCompile Include=\"..\\..\\" + app + "\\main.cpp\"";
		const char* cpp_ptr = strstr(start2, prefix.c_str());
		if (!cpp_ptr) {
			return false;
		}

		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			std::pair<std::string, bool> pair = generate_insert_string(app, *it, true);
			const char* ptr = nullptr;
			if (!pair.second) {
				// .hpp
				ptr = insert_hpp_ItemGroup(file, start2, app, fsize);
			} else {
				ptr = cpp_ptr;
			}
			fsize = append_at_ItemGroup_end(file, ptr, fsize, pair.first);
		}
		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
	}
	SDL_CopyFiles(vcxproj_tmp.c_str(), vcxproj.c_str());
	SDL_DeleteFiles(vcxproj_tmp.c_str());

	return true;
}


static std::string generate_insert_string_aplt(const char** pstart2, const std::vector<std::string>& files, const bool filters)
{
	const char* start2 = *pstart2;

	std::stringstream ss;
	const std::string prefix = "</ItemGroup>";
	// unitl now, start2 is in first <ItemGroup></ItemGroup> or before. 
	// skip first <ItemGroup></ItemGroup>.
	int times = 2;
	for (int n = 0; n < times; n ++) {
		if (n != 0) {
			start2 += prefix.size();
		}
		start2 = strstr(start2, prefix.c_str());
		if (start2 == nullptr) {
			return null_str;
		}
	}

	// 5. move front, until not '\t' and ' '
	start2 --; // start2 pointer to '<', back one.
	while (start2[0] == '\t' || start2[0] == ' ') { start2 --; }
	start2 ++; // forward one. left point to '\t' or ''.

	if (!filters) {
		// 3. insert files
		// file example: aplt_leagor_blesmart/lua/home.lua
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			const std::string file = utils::normalize_path(*it, true);
			// <None Include="..\..\..\..\apps-res\aplz_kos_blesmart\lua\home.lua" />
			ss << "    <None Include=\"..\\..\\..\\..\\apps-res\\" << file << "\" />\r\n";
		}

	} else {
		// 6. insert files
		// "    <None Include="..\..\..\..\apps-res\aplz_kos_blesmart\settings.cfg">
		// "      <Filter>aplz_kos_blesmart</Filter>
		// "    </None>
		ss.str("");
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			const std::string file = utils::normalize_path(*it, true);
			const std::string dir = extract_directory(file);

			ss << "    <None Include=\"..\\..\\..\\..\\apps-res\\" << file << "\">\r\n";
			ss << "      <Filter>" << dir << "</Filter>\r\n";
			ss << "    </None>\r\n";
		}
	}
	*pstart2 = start2;
	return ss.str();
}

bool add_aplt(const std::string& lua_bundleid, const std::vector<std::string>& files)
{
	VALIDATE(!game_config::apps_src_path.empty() && is_lua_bundleid(lua_bundleid) && !files.empty(), null_str);

	const std::string vcxproj = game_config::apps_src_path + "/apps/projectfiles/vc/applet.vcxproj";
	const std::string vcxproj_tmp = game_config::preferences_dir + "/__tmp";
	
	{
		SDL_CopyFiles(vcxproj.c_str(), vcxproj_tmp.c_str());
		tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

/*
		// 1. find second </ItemGroup>
		int times = 2;
		std::string prefix = "</ItemGroup>";
		for (int n = 0; n < times; n ++) {
			if (n != 0) {
				start2 += prefix.size();
			}
			start2 = strstr(start2, prefix.c_str());
			if (start2 == nullptr) {
				return false;
			}
		}

		// 2. move front, until not '\t' and ' '
		start2 --; // start2 pointer to '<', back one.
		while (start2[0] == '\t' || start2[0] == ' ') { start2 --; }
		start2 ++; // forward one. left point to '\t' or ''.

		// 3. insert files
		std::stringstream ss;
		// file example: aplt_leagor_blesmart/lua/home.lua
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			const std::string file = utils::normalize_path(*it, true);
			// <None Include="..\..\..\..\apps-res\aplz_kos_blesmart\lua\home.lua" />
			ss << "    <None Include=\"..\\..\\..\\..\\apps-res\\" << file << "\" />\r\n";
		}
		fsize = file.replace_span(start2 - file.data, 0, ss.str().c_str(), ss.str().size(), fsize);
*/		
		const std::string msg = generate_insert_string_aplt(&start2, files, false);
		if (msg.empty()) {
			return false;
		}
		fsize = file.replace_span(start2 - file.data, 0, msg.c_str(), msg.size(), fsize);

		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
	}

	{
		const std::string filters = game_config::apps_src_path + "/apps/projectfiles/vc/applet.vcxproj.filters";
		tfile file(filters, GENERIC_WRITE, OPEN_EXISTING);

		// SDL_CopyFiles(filters.c_str(), vcxproj_tmp.c_str());
		// tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);

		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		// 1. find first </ItemGroup>
		const std::string prefix = "</ItemGroup>";
		start2 = strstr(start2, prefix.c_str());
		if (start2 == nullptr) {
			return false;
		}

		// 2. move front, until not '\t' and ' '
		start2 --; // start2 pointer to '<', back one.
		while (start2[0] == '\t' || start2[0] == ' ') { start2 --; }
		start2 ++; // forward one. left point to '\t' or ''.

		// 3. insert directory's guid
		std::set<std::string> dirs;
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			const std::string dir = extract_directory(*it);
			VALIDATE(!dir.empty(), null_str);
			if (dirs.count(dir) == 0) {
				dirs.insert(dir);
			}
		}
		// "    <Filter Include="aplt_leagor_studio">
        // "      <UniqueIdentifier>{df627eef-cc1a-40e0-adf8-890444ae1ffb}</UniqueIdentifier>
		// "    </Filter>
		std::stringstream ss;
		ss.str("");
		for (std::set<std::string>::const_iterator it = dirs.begin(); it != dirs.end(); ++ it) {
			GUID guid;
			CoCreateGuid(&guid);
			const std::string guid_str = utils::format_guid(guid);

			const std::string dir = utils::normalize_path(*it, true);
			ss << "    <Filter Include=\""<< dir << "\">\r\n";
			ss << "      <UniqueIdentifier>{" << guid_str << "}</UniqueIdentifier>\r\n";
			ss << "    </Filter>\r\n";
		}
		fsize = file.replace_span(start2 - file.data, 0, ss.str().c_str(), ss.str().size(), fsize);
/*
		// 4. find second </ItemGroup>
		int times = 2; // unitl now, start2 is in first <ItemGroup></ItemGroup>
		for (int n = 0; n < times; n ++) {
			if (n != 0) {
				start2 += prefix.size();
			}
			start2 = strstr(start2, prefix.c_str());
			if (start2 == nullptr) {
				return false;
			}
		}

		// 5. move front, until not '\t' and ' '
		start2 --; // start2 pointer to '<', back one.
		while (start2[0] == '\t' || start2[0] == ' ') { start2 --; }
		start2 ++; // forward one. left point to '\t' or ''.

		// 6. insert files
		// "    <None Include="..\..\..\..\apps-res\aplz_kos_blesmart\settings.cfg">
		// "      <Filter>aplz_kos_blesmart</Filter>
		// "    </None>
		ss.str("");
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			const std::string file = utils::normalize_path(*it, true);
			const std::string dir = extract_directory(file);

			ss << "    <None Include=\"..\\..\\..\\..\\apps-res\\" << file << "\">\r\n";
			ss << "      <Filter>" << dir << "</Filter>\r\n";
			ss << "    </None>\r\n";
		}
		fsize = file.replace_span(start2 - file.data, 0, ss.str().c_str(), ss.str().size(), fsize);
*/
		const std::string msg = generate_insert_string_aplt(&start2, files, true);
		if (msg.empty()) {
			return false;
		}
		fsize = file.replace_span(start2 - file.data, 0, msg.c_str(), msg.size(), fsize);

		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
	}

	SDL_CopyFiles(vcxproj_tmp.c_str(), vcxproj.c_str());
	SDL_DeleteFiles(vcxproj_tmp.c_str());

	return true;
}

// calculate element lines that contain ptr.
// 1. before must has "\r\n".
const char* element_range(const char* ptr, const std::string& start_element, const std::string& end_element, int& len)
{
	len = 0;

	const char* head = ptr;
	int can_check_lines = 1;
	while (true) {
		// move head to first char of this line.
		while (head[0] != '\r' && head[0] != '\n') { head --; }
		head ++; // skip '\r' or '\n'

		const char* head2 = head;
		while (head2[0] == '\t' || head2[0] == ' ') { head2 ++; }
		if (memcmp(head2, start_element.c_str(), start_element.size()) == 0) {
			break;
		}
		if (can_check_lines == 0) {
			return nullptr;
		}
		can_check_lines --;
		// back, to before line.
		while (head2[0] == '\t' || head2[0] == ' ' || head[0] == '\r' || head[0] == '\n') { head --; }
	}
	// move ptr to first char of next line.
	const char* tail = strstr(ptr, end_element.c_str());
	if (tail == nullptr) {
		return nullptr;
	}
	tail += end_element.size();
	while (tail[0] == '\r' || tail[0] == '\n') { tail ++; }

	len = tail - head;
	return head;
}

bool remove_aplt(const std::string& lua_bundleid)
{
	VALIDATE(!game_config::apps_src_path.empty() && is_lua_bundleid(lua_bundleid), null_str);

	const std::string vcxproj = game_config::apps_src_path + "/apps/projectfiles/vc/applet.vcxproj";
	const std::string vcxproj_tmp = game_config::preferences_dir + "/__tmp";
	
	{
		SDL_CopyFiles(vcxproj.c_str(), vcxproj_tmp.c_str());
		tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		// 1. delete all line that contain <aplz_kos_blesmart>.

		int times = 2;
		std::string prefix = lua_bundleid;
		while (true) {
			const char* ptr = strstr(start2, prefix.c_str());
			if (ptr == nullptr) {
				break;
			}
			// delete this line.
			// move ptr to first char of this line.
			const char* head = ptr;
			while (head[0] != '\r' && head[0] != '\n') { head --; }
			head ++; // skip '\r' or '\n'

			// move ptr to first char of next line.
			while (ptr[0] != '\r' && ptr[0] != '\n') { ptr ++; }
			while (ptr[0] == '\r' || ptr[0] == '\n') { ptr ++; }
			const char* tail = ptr;

			fsize = file.replace_span(head - file.data, tail - head, NULL, 0, fsize);
		}

		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
		file.truncate(fsize);
	}

	{
		const std::string filters = game_config::apps_src_path + "/apps/projectfiles/vc/applet.vcxproj.filters";
		tfile file(filters, GENERIC_WRITE, OPEN_EXISTING);

		// SDL_CopyFiles(filters.c_str(), vcxproj_tmp.c_str());
		// tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);

		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		// 1. delete all elements that contain <aplz_kos_blesmart>.
		// there tow elements:
		// 
		// "    <Filter Include="aplt_leagor_studio">
		// "      <UniqueIdentifier>{91F245E0-21AB-48DA-BA7E-BA1572282DCF}</UniqueIdentifier>
		// "    </Filter>
		std::string prefix = lua_bundleid;
		std::string end_element = "</Filter>";
		const char* end_ptr = strstr(start2, "</ItemGroup>");
		int len;
		while (true) {
			const char* ptr = strstr(start2, prefix.c_str());
			if (ptr == nullptr || (end_ptr != nullptr && ptr > end_ptr)) {
				break;
			}
			const char* head = element_range(ptr, "<Filter ", "</Filter>", len);
			if (head == nullptr) {
				break;
			}
			fsize = file.replace_span(head - file.data, len, NULL, 0, fsize);
		}

		// "    <None Include="..\..\..\..\apps-res\aplt_leagor_studio\settings.cfg">
		// "      <Filter>aplt_leagor_studio</Filter>
		// "    </None>
		end_ptr = nullptr;
		while (true) {
			const char* ptr = strstr(start2, prefix.c_str());
			if (ptr == nullptr || (end_ptr != nullptr && ptr > end_ptr)) {
				break;
			}
			// here, <None> maybe is bug in future. other file type should other <Element>.
			const char* head = element_range(ptr, "<None ", "</None>", len);
			if (head == nullptr) {
				break;
			}
			fsize = file.replace_span(head - file.data, len, NULL, 0, fsize);
		}

		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
		file.truncate(fsize);
	}

	SDL_CopyFiles(vcxproj_tmp.c_str(), vcxproj.c_str());
	SDL_DeleteFiles(vcxproj_tmp.c_str());

	return true;
}

bool new_dialog_aplt(const std::string& lua_bundleid, const std::vector<std::string>& files)
{
	VALIDATE(!game_config::apps_src_path.empty() && is_lua_bundleid(lua_bundleid) && !files.empty(), null_str);

	const std::string vcxproj = game_config::apps_src_path + "/apps/projectfiles/vc/applet.vcxproj";
	const std::string vcxproj_tmp = game_config::preferences_dir + "/__tmp";
	
	{
		SDL_CopyFiles(vcxproj.c_str(), vcxproj_tmp.c_str());
		tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		const std::string msg = generate_insert_string_aplt(&start2, files, false);
		if (msg.empty()) {
			return false;
		}
		fsize = file.replace_span(start2 - file.data, 0, msg.c_str(), msg.size(), fsize);

		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
	}

	{
		const std::string filters = game_config::apps_src_path + "/apps/projectfiles/vc/applet.vcxproj.filters";
		tfile file(filters, GENERIC_WRITE, OPEN_EXISTING);

		// SDL_CopyFiles(filters.c_str(), vcxproj_tmp.c_str());
		// tfile file(vcxproj_tmp, GENERIC_WRITE, OPEN_EXISTING);

		int fsize = file.read_2_data();
		if (!fsize) {
			return false;
		}
		// i think length of appended data isn't more than 512 bytes.
		file.resize_data(fsize + 512, fsize);
		file.data[fsize] = '\0';

		const char* start2 = file.data;
		if (utils::bom_magic_started((const uint8_t*)file.data, fsize)) {
			start2 += BOM_LENGTH;
		}

		const std::string msg = generate_insert_string_aplt(&start2, files, true);
		if (msg.empty()) {
			return false;
		}
		fsize = file.replace_span(start2 - file.data, 0, msg.c_str(), msg.size(), fsize);

		posix_fseek(file.fp, 0);
		posix_fwrite(file.fp, file.data, fsize);
	}
	SDL_CopyFiles(vcxproj_tmp.c_str(), vcxproj.c_str());
	SDL_DeleteFiles(vcxproj_tmp.c_str());

	return true;
}

}
