/* $Id: map.cpp 47153 2010-10-22 21:11:48Z shadowmaster $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Routines related to game-maps, terrain, locations, directions. etc.
 */

#include "global.hpp"

#include <cassert>

#include "map.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "map_exception.hpp"
#include "serialization/parser.hpp"
#include "util.hpp"
#include "wml_exception.hpp"
#include "image.hpp"

const std::string tmap::default_map_header = "usage=map\nborder_size=1\n\n";
const tmap::tborder tmap::default_border = tmap::SINGLE_TILE_BORDER;

config tmap::terrain_types;

const t_translation::t_list& tmap::underlying_mvt_terrain(t_translation::t_terrain terrain) const
{
	const std::map<t_translation::t_terrain,terrain_type>::const_iterator i =
		tcodeToTerrain_.find(terrain);

	if(i == tcodeToTerrain_.end()) {
		static t_translation::t_list result(1);
		result[0] = terrain;
		return result;
	} else {
		return i->second.mvt_type();
	}
}

const t_translation::t_list& tmap::underlying_def_terrain(t_translation::t_terrain terrain) const
{
	const std::map<t_translation::t_terrain, terrain_type>::const_iterator i =
		tcodeToTerrain_.find(terrain);

	if(i == tcodeToTerrain_.end()) {
		static t_translation::t_list result(1);
		result[0] = terrain;
		return result;
	} else {
		return i->second.def_type();
	}
}

const t_translation::t_list& tmap::underlying_union_terrain(t_translation::t_terrain terrain) const
{
	const std::map<t_translation::t_terrain,terrain_type>::const_iterator i =
		tcodeToTerrain_.find(terrain);

	if(i == tcodeToTerrain_.end()) {
		static t_translation::t_list result(1);
		result[0] = terrain;
		return result;
	} else {
		return i->second.union_type();
	}
}



std::string tmap::get_terrain_string(const t_translation::t_terrain& terrain) const
{
	std::string str =
		get_terrain_info(terrain).description();

	str += get_underlying_terrain_string(terrain);

	return str;
}

std::string tmap::get_terrain_editor_string(const t_translation::t_terrain& terrain) const
{
	std::string str =
		get_terrain_info(terrain).editor_name();
	const std::string desc =
		get_terrain_info(terrain).description();

	if(str != desc) {
		str += "/";
		str += desc;
	}

	str += get_underlying_terrain_string(terrain);

	return str;
}

std::string tmap::get_underlying_terrain_string(const t_translation::t_terrain& terrain) const
{
	std::string str;

	const t_translation::t_list& underlying = underlying_union_terrain(terrain);
	assert(!underlying.empty());

	if(underlying.size() > 1 || underlying[0] != terrain) {
		str += " (";
        t_translation::t_list::const_iterator i = underlying.begin();
        str += get_terrain_info(*i).name();
        while (++i != underlying.end()) {
			str += ", " + get_terrain_info(*i).name();
        }
		str += ")";
	}

	return str;
}

void tmap::write_terrain(const map_location &loc, config& cfg) const
{
	cfg["terrain"] = t_translation::write_terrain_code(get_terrain(loc));
}

tmap::tmap(const std::string& data)
	: tiles_(1)
	, terrainList_()
	, tcodeToTerrain_()
	, villages_()
	, borderCache_()
	, terrainFrequencyCache_()
	, w_(-1)
	, h_(-1)
	, total_width_(0)
	, total_height_(0)
	, border_size_(NO_BORDER)
	, usage_(IS_MAP)
{
	const config::const_child_itors &terrains = terrain_types.child_range("terrain_type");
	create_terrain_maps(terrains, terrainList_, tcodeToTerrain_);

	read(data);
}

tmap::tmap(const surface& surf, int tile_size)
	: tiles_(1)
	, terrainList_()
	, tcodeToTerrain_()
	, villages_()
	, borderCache_()
	, terrainFrequencyCache_()
	, w_(-1)
	, h_(-1)
	, total_width_(0)
	, total_height_(0)
	, border_size_(NO_BORDER)
	, usage_(IS_MAP)
	, surf_(surf)
{
	VALIDATE(surf && tile_size && !(tile_size & 0x3), null_str);
	VALIDATE(!(surf->w % tile_size) && !(surf->h % tile_size), null_str);

	w_ = total_width_ = surf->w / tile_size;
	h_ = total_height_ = surf->h / tile_size;

	tex_ = SDL_CreateTextureFromSurface(get_renderer(), surf);
	tile_size_ = tile_size;
}

void tmap::read(const std::string& data)
{
	// Initial stuff
	tiles_.clear();
	villages_.clear();
	std::fill(startingPositions_, startingPositions_ +
		sizeof(startingPositions_) / sizeof(*startingPositions_), map_location());
	std::map<int, t_translation::coordinate> starting_positions;

	if(data.empty()) {
		w_ = 0;
		h_ = 0;
		return;
	}

	// Test whether there is a header section
	size_t header_offset = data.find("\n\n");
	if(header_offset == std::string::npos) {
		// For some reason Windows will fail to load a file with \r\n
		// lineending properly no problems on Linux with those files.
		// This workaround fixes the problem the copy later will copy
		// the second \r\n to the map, but that's no problem.
		header_offset = data.find("\r\n\r\n");
	}
	const size_t comma_offset = data.find(",");
	// The header shouldn't contain commas, so if the comma is found
	// before the header, we hit a \n\n inside or after a map.
	// This is no header, so don't parse it as it would be.
	VALIDATE(
		!(header_offset == std::string::npos || comma_offset < header_offset),
		_("A map without a header is not supported"));

	std::string header_str(std::string(data, 0, header_offset + 1));
	config header;
	::read(header, header_str);

	border_size_ = header["border_size"];
	const std::string usage = header["usage"];

	utils::string_map symbols;
	symbols["border_size_key"] = "border_size";
	symbols["usage_key"] = "usage";
	symbols["usage_val"] = usage;
	const std::string msg = "'$border_size_key|' should be "
		"'$border_size_val|' when '$usage_key| = $usage_val|'";

	if(usage == "map") {
		usage_ = IS_MAP;
		symbols["border_size_val"] = "1";
		// BUG! to editor, it cannot support vgettext.
	} else if(usage == "mask") {
		usage_ = IS_MASK;
		symbols["border_size_val"] = "0";
		// BUG! to editor, it cannot support vgettext.
	} else if(usage == "") {
		throw incorrect_map_format_error("Map has a header but no usage");
	} else {
		std::string msg = "Map has a header but an unknown usage:" + usage;
		throw incorrect_map_format_error(msg.c_str());
	}

	const std::string& map = std::string(data, header_offset + 2);

	try {
		tiles_ = t_translation::read_game_map(map, starting_positions);

	} catch(t_translation::error& e) {
		// We re-throw the error but as map error.
		// Since all codepaths test for this, it's the least work.
		throw incorrect_map_format_error(e.message);
	}

	// Convert the starting positions to the array
	std::map<int, t_translation::coordinate>::const_iterator itor =
		starting_positions.begin();

	for(; itor != starting_positions.end(); ++itor) {

		// Check for valid position,
		// the first valid position is 1,
		// so the offset 0 in the array is never used.
		if(itor->first < 1 || itor->first >= MAX_PLAYERS+1) {
			std::stringstream ss;
			ss << "Starting position " << itor->first << " out of range\n";
			// ERR_CF << ss.str();
			ss << "The map cannot be loaded.";
			throw incorrect_map_format_error(ss.str().c_str());
		}

		// Add to the starting position array
		startingPositions_[itor->first] = map_location(itor->second.x - 1, itor->second.y - 1);
	}

	// Post processing on the map
	total_width_ = tiles_.size();
	total_height_ = total_width_ > 0 ? tiles_[0].size() : 0;
	w_ = total_width_ - 2 * border_size_;
	h_ = total_height_ - 2 * border_size_;

	for(int x = 0; x < total_width_; ++x) {
		for(int y = 0; y < total_height_; ++y) {

			// Is the terrain valid?
			if(tcodeToTerrain_.count(tiles_[x][y]) == 0) {
				if(!try_merge_terrains(tiles_[x][y])) {
					std::stringstream ss;
					ss << "Illegal tile in map: (" << t_translation::write_terrain_code(tiles_[x][y])
						   << ") '" << tiles_[x][y] << "'\n";
					// ERR_CF << ss.str();
					ss << "The map cannot be loaded.";
					throw incorrect_map_format_error(ss.str().c_str());
				}
			}

			// Is it a village?
			if(x >= border_size_ && y >= border_size_
					&& x < total_width_-border_size_  && y < total_height_-border_size_
					&& is_village(tiles_[x][y])) {
				villages_.push_back(map_location(x-border_size_, y-border_size_));
			}
		}
	}
}

std::string tmap::write() const
{
	// Convert the starting positions to a map
	std::map<int, t_translation::coordinate> starting_positions;
	for (int i = 0; i < MAX_PLAYERS + 1; ++i)
	{
		if (!on_board(startingPositions_[i])) continue;
		t_translation::coordinate position(startingPositions_[i].x + border_size_, startingPositions_[i].y + border_size_);
		starting_positions[i] = position;
	}

	// Let the low level convertor do the conversion
	std::ostringstream s;
	s << "border_size=" << border_size_ << "\nusage="
		<< (usage_ == IS_MAP ? "map" : "mask") << "\n\n"
		<< t_translation::write_game_map(tiles_, starting_positions);
	return s.str();
}

void tmap::overlay(const tmap& m, const config& rules_cfg, int xpos, int ypos, bool border)
{
	const config::const_child_itors &rules = rules_cfg.child_range("rule");
	int actual_border = (m.border_size() == border_size()) && border ? border_size() : 0;

	const int xstart = std::max<int>(-actual_border, -xpos - actual_border);
	const int ystart = std::max<int>(-actual_border, -ypos - actual_border - ((xpos & 1) ? 1 : 0));
	const int xend = std::min<int>(m.w() + actual_border, w() + actual_border - xpos);
	const int yend = std::min<int>(m.h() + actual_border, h() + actual_border - ypos);
	for(int x1 = xstart; x1 < xend; ++x1) {
		for(int y1 = ystart; y1 < yend; ++y1) {
			const int x2 = x1 + xpos;
			const int y2 = y1 + ypos +
				((xpos & 1) && (x1 & 1) ? 1 : 0);

			const t_translation::t_terrain t = m[x1][y1 + m.border_size_];
			const t_translation::t_terrain current = (*this)[x2][y2 + border_size_];

			if(t == t_translation::FOGGED || t == t_translation::VOID_TERRAIN) {
				continue;
			}

			// See if there is a matching rule
			config::const_child_iterator rule = rules.first;
			for( ; rule != rules.second; ++rule)
			{
				static const std::string src_key = "old", src_not_key = "old_not",
				                         dst_key = "new", dst_not_key = "new_not";
				const config &cfg = *rule;
				const t_translation::t_list& src = t_translation::read_list(cfg[src_key]);

				if(!src.empty() && t_translation::terrain_matches(current, src) == false) {
					continue;
				}

				const t_translation::t_list& src_not = t_translation::read_list(cfg[src_not_key]);

				if(!src_not.empty() && t_translation::terrain_matches(current, src_not)) {
					continue;
				}

				const t_translation::t_list& dst = t_translation::read_list(cfg[dst_key]);

				if(!dst.empty() && t_translation::terrain_matches(t, dst) == false) {
					continue;
				}

				const t_translation::t_list& dst_not = t_translation::read_list(cfg[dst_not_key]);

				if(!dst_not.empty() && t_translation::terrain_matches(t, dst_not)) {
					continue;
				}

				break;
			}


			if (rule != rules.second)
			{
				const config &cfg = *rule;
				const t_translation::t_list& terrain = t_translation::read_list(cfg["terrain"]);

				tmerge_mode mode = BOTH;
				if (cfg["layer"] == "base") {
					mode = BASE;
				}
				else if (cfg["layer"] == "overlay") {
					mode = OVERLAY;
				}

				t_translation::t_terrain new_terrain = t;
				if(!terrain.empty()) {
					new_terrain = terrain[0];
				}

				if (!cfg["use_old"].to_bool()) {
					set_terrain(map_location(x2, y2), new_terrain, mode, cfg["replace_if_failed"].to_bool());
				}

			} else {
				set_terrain(map_location(x2,y2),t);
			}
		}
	}

	for(const map_location* pos = m.startingPositions_;
			pos != m.startingPositions_ + sizeof(m.startingPositions_)/sizeof(*m.startingPositions_);
			++pos) {

		if(pos->valid()) {
			startingPositions_[pos - m.startingPositions_] = *pos;
		}
	}
}

t_translation::t_terrain tmap::get_terrain(const map_location& loc) const
{
	if (tex_.get()) {
		if (on_board(loc)) {
			return t_translation::IMG_MAP;
		}
		return t_translation::OFF_MAP_USER;
	}

	if (on_board_with_border(loc)) {
		return tiles_[loc.x + border_size_][loc.y + border_size_];
	}

	const std::map<map_location, t_translation::t_terrain>::const_iterator itor = borderCache_.find(loc);
	if(itor != borderCache_.end())
		return itor->second;

	// If not on the board, decide based on what surrounding terrain is
	t_translation::t_terrain items[6];
	int nitems = 0;

	map_location adj[6];
	get_adjacent_tiles(loc,adj);
	for(int n = 0; n != 6; ++n) {
		if(on_board(adj[n])) {
			items[nitems] = tiles_[adj[n].x][adj[n].y];
			++nitems;
		} else {
			// If the terrain is off map but already in the border cache,
			// this will be used to determine the terrain.
			// This avoids glitches
			// * on map with an even width in the top right corner
			// * on map with an odd height in the bottom left corner.
			// It might also change the result on other map and become random,
			// but the border tiles will be determined in the future, so then
			// this will no longer be used in the game
			// (The editor will use this feature to expand maps in a better way).
			std::map<map_location, t_translation::t_terrain>::const_iterator itor =
				borderCache_.find(adj[n]);

			// Only add if it is in the cache and a valid terrain
			if(itor != borderCache_.end() &&
					itor->second != t_translation::NONE_TERRAIN)  {

				items[nitems] = itor->second;
				++nitems;
			}
		}

	}

	// Count all the terrain types found,
	// and see which one is the most common, and use it.
	t_translation::t_terrain used_terrain;
	int terrain_count = 0;
	for(int i = 0; i != nitems; ++i) {
		if(items[i] != used_terrain && !is_village(items[i]) && !is_keep(items[i])) {
			const int c = std::count(items+i+1,items+nitems,items[i]) + 1;
			if(c > terrain_count) {
				used_terrain = items[i];
				terrain_count = c;
			}
		}
	}

	borderCache_.insert(std::pair<map_location, t_translation::t_terrain>(loc,used_terrain));
	return used_terrain;

}

const map_location& tmap::starting_position(int n) const
{
	if(size_t(n) < sizeof(startingPositions_)/sizeof(*startingPositions_)) {
		return startingPositions_[n];
	} else {
		static const map_location null_loc;
		return null_loc;
	}
}

int tmap::num_valid_starting_positions() const
{
	const int res = is_starting_position(map_location());
	if(res == -1)
		return num_starting_positions()-1;
	else
		return res;
}

int tmap::is_starting_position(const map_location& loc) const
{
	const map_location* const beg = startingPositions_+1;
	const map_location* const end = startingPositions_+num_starting_positions();
	const map_location* const pos = std::find(beg,end,loc);

	return pos == end ? -1 : pos - beg;
}

void tmap::set_starting_position(int side, const map_location& loc)
{
	if(side >= 0 && side < num_starting_positions()) {
		startingPositions_[side] = loc;
	}
}

bool tmap::on_board(const map_location& loc) const
{
	return loc.valid() && loc.x < w_ && loc.y < h_;
}

bool tmap::on_board_with_border(const map_location& loc) const
{
	if(tiles_.empty()) {
		return false;
	} else {
		return loc.x >= (0 - border_size_) && loc.x < (w_ + border_size_) &&
			loc.y >= (0 - border_size_) && loc.y < (h_ + border_size_);
	}
}

const terrain_type& tmap::get_terrain_info(const t_translation::t_terrain terrain) const
{
	static const terrain_type default_terrain;
	const std::map<t_translation::t_terrain,terrain_type>::const_iterator i =
		tcodeToTerrain_.find(terrain);

	if(i != tcodeToTerrain_.end())
		return i->second;
	else
		return default_terrain;
}

void tmap::set_terrain(const map_location& loc, const t_translation::t_terrain terrain, const tmerge_mode mode, bool replace_if_failed) {
	if(!on_board_with_border(loc)) {
		// off the map: ignore request
		return;
	}

	t_translation::t_terrain new_terrain = merge_terrains(get_terrain(loc), terrain, mode, replace_if_failed);

	if(new_terrain == t_translation::NONE_TERRAIN) {
		return;
	}

	if(on_board(loc)) {
		const bool old_village = is_village(loc);
		const bool new_village = is_village(new_terrain);

		if(old_village && !new_village) {
			villages_.erase(std::remove(villages_.begin(),villages_.end(),loc),villages_.end());
		} else if(!old_village && new_village) {
			villages_.push_back(loc);
		}
	}

	tiles_[loc.x + border_size_][loc.y + border_size_] = new_terrain;

	// Update the off-map autogenerated tiles
	map_location adj[6];
	get_adjacent_tiles(loc,adj);

	for(int n = 0; n < 6; ++n) {
		remove_from_border_cache(adj[n]);
	}
}

const std::map<t_translation::t_terrain, size_t>& tmap::get_weighted_terrain_frequencies() const
{
	if(terrainFrequencyCache_.empty() == false) {
		return terrainFrequencyCache_;
	}

	const map_location center(w()/2,h()/2);

	const size_t furthest_distance = distance_between(map_location(0,0),center);

	const size_t weight_at_edge = 100;
	const size_t additional_weight_at_center = 200;

	for(size_t i = 0; i != size_t(w()); ++i) {
		for(size_t j = 0; j != size_t(h()); ++j) {
			const size_t distance = distance_between(map_location(i,j),center);
			terrainFrequencyCache_[(*this)[i][j]] += weight_at_edge +
			    (furthest_distance-distance)*additional_weight_at_center;
		}
	}

	return terrainFrequencyCache_;
}

bool tmap::try_merge_terrains(const t_translation::t_terrain terrain) {

	if(tcodeToTerrain_.count(terrain) == 0) {
		const std::map<t_translation::t_terrain, terrain_type>::const_iterator base_iter =
			tcodeToTerrain_.find(t_translation::t_terrain(terrain.base, t_translation::NO_LAYER));
		const std::map<t_translation::t_terrain, terrain_type>::const_iterator overlay_iter =
			tcodeToTerrain_.find(t_translation::t_terrain(t_translation::NO_LAYER, terrain.overlay));

		if(base_iter == tcodeToTerrain_.end() || overlay_iter == tcodeToTerrain_.end()) {
			return false;
		}

		terrain_type new_terrain(base_iter->second, overlay_iter->second);
		terrainList_.push_back(new_terrain.number());
		tcodeToTerrain_.insert(std::pair<t_translation::t_terrain, terrain_type>(
								   new_terrain.number(), new_terrain));
		return true;
	}
	return true; // Terrain already exists, nothing to do
}

t_translation::t_terrain tmap::merge_terrains(const t_translation::t_terrain old_t, const t_translation::t_terrain new_t, const tmerge_mode mode, bool replace_if_failed) {
	t_translation::t_terrain result = t_translation::NONE_TERRAIN;

	if(mode == OVERLAY) {
		const t_translation::t_terrain t = t_translation::t_terrain(old_t.base, new_t.overlay);
		if (try_merge_terrains(t)) {
			result = t;
		}
	}
	else if(mode == BASE) {
		const t_translation::t_terrain t = t_translation::t_terrain(new_t.base, old_t.overlay);
		if (try_merge_terrains(t)) {
			result = t;
		}
	}
	else if(mode == BOTH && new_t.base != t_translation::NO_LAYER) {
		// We need to merge here, too, because the dest terrain might be a combined one.
		if (try_merge_terrains(new_t)) {
			result = new_t;
		}
	}

	// if merging of overlay and base failed, and replace_if_failed is set,
	// replace the terrain with the complete new terrain (if given)
	// or with (default base)^(new overlay)
	if(result == t_translation::NONE_TERRAIN && replace_if_failed && tcodeToTerrain_.count(new_t) > 0) {
		if(new_t.base != t_translation::NO_LAYER) {
			// Same as above
			if (try_merge_terrains(new_t)) {
				result = new_t;
			}
		}
		else if (get_terrain_info(new_t).default_base() != t_translation::NONE_TERRAIN) {
			result = get_terrain_info(new_t).terrain_with_default_base();
		}
	}
	return result;
}

void form_map_str_rectangle(const std::string& data, std::vector<std::vector<std::string> >& result)
{
	//
	// refrence tmap::read
	//
	result.clear();

	// Test whether there is a header section
	size_t header_offset = data.find("\n\n");
	if(header_offset == std::string::npos) {
		// For some reason Windows will fail to load a file with \r\n
		// lineending properly no problems on Linux with those files.
		// This workaround fixes the problem the copy later will copy
		// the second \r\n to the map, but that's no problem.
		header_offset = data.find("\r\n\r\n");
	}
	const size_t comma_offset = data.find(",");
	// The header shouldn't contain commas, so if the comma is found
	// before the header, we hit a \n\n inside or after a map.
	// This is no header, so don't parse it as it would be.
	VALIDATE(
		!(header_offset == std::string::npos || comma_offset < header_offset),
		_("A map without a header is not supported"));

	std::string header_str(std::string(data, 0, header_offset + 1));
	config header;
	::read(header, header_str);

	int border_size = header["border_size"];
	const std::string usage = header["usage"];

	if (usage != "map" || border_size != 1) {
		return;
	}
	const std::string& map = std::string(data, header_offset + 2);
	const std::string& str = map;

	size_t offset = 0;
	size_t x = 0, y = 0, width = 0;

	// Skip the leading newlines
	while (offset < str.length() && utils::isnewline(str[offset])) {
		++offset;
	}

	// Did we get an empty map?
	if ((offset + 1) >= str.length()) {
		return;
	}

	while (offset < str.length()) {

		// Get a terrain chunk
		const std::string separators = ",\n\r";
		const size_t pos_separator = str.find_first_of(separators, offset);
		std::string terrain = str.substr(offset, pos_separator - offset);
        utils::strip(terrain);

		// Make space for the new item
		// NOTE we increase the vector every loop for every x and y.
		// Profiling with an increase of y with 256 items didn't show
		// an significant speed increase.
		// So didn't rework the system to allocate larger vectors at once.
		if (result.size() <= x) {
			result.resize(x + 1);
		}
		if(result[x].size() <= y) {
			result[x].resize(y + 1);
		}

		// Add the resulting terrain number
		result[x][y] = terrain;

		// Evaluate the separator
		if(pos_separator == std::string::npos || utils::isnewline(str[pos_separator])) {
			// the first line we set the with the other lines we check the width
			if(y == 0) {
				// x contains the offset in the map
				width = x + 1;
			} else {
				if ((x + 1) != width ) {
					// ERR_G << "Map not a rectangle error occurred at line offset " << y << " position offset " << x << "\n";
					// throw error("Map not a rectangle.");
					return;
				}
			}

			// Prepare next iteration
			++y;
			x = 0;

			// Avoid in infinite loop if the last line ends without an EOL
			if(pos_separator == std::string::npos) {
				offset = str.length();

			} else {

				offset = pos_separator + 1;
				// Skip the following newlines
				while(offset < str.length() && utils::isnewline(str[offset])) {
					++offset;
				}
			}

		} else {
			++x;
			offset = pos_separator + 1;
		}
	}
}

std::string combine_map(const std::vector<std::vector<std::string> >& left_res, const std::vector<std::vector<std::string> >& right_res, bool rflip)
{
	size_t boarder_size = 1;

	// Let the low level convertor do the conversion
	std::ostringstream s;
	s << "border_size=" << boarder_size << "\nusage=map\n\n";

	size_t min_size = 12;
	for (size_t y = 0; y < left_res[0].size(); ++ y) {
		std::string result;
		for (size_t x = 0; x < left_res.size() - boarder_size; ++ x) {
			// Add the separator
			if (x != 0) {
				s << ", ";
			}

			result = left_res[x][y];
			if (result.size() < min_size) {
				result.resize(min_size, ' ');
			}
			s << result;
		}

		// transition column
		s << ", ";
		result = "Gg";
		if (result.size() < min_size) {
			result.resize(min_size, ' ');
		}
		s << result;

		int x = rflip? (int)(right_res.size() - boarder_size) - 1: 0;
		do { 
			// Add the separator
			s << ", ";

			result = right_res[x][y];
			if (rflip) {
				std::string::size_type pos;
				if ((pos = result.find("\\")) != std::string::npos) {
					result.replace(pos, 1, "/");
				} else if ((pos = result.find("/")) != std::string::npos) {
					result.replace(pos, 1, "\\");
				}
			}
			if (result.size() < min_size) {
				result.resize(min_size, ' ');
			}
			s << result;

		} while ((rflip && -- x >= 0) || (!rflip && ++ x < (int)(right_res.size() - boarder_size)));

		s << "\n";
	}

	return s.str();

}

std::string combine_map(const std::string& left, const std::vector<std::vector<std::string> >& right_res, bool rflip)
{
	std::vector<std::vector<std::string> > left_res;
	form_map_str_rectangle(left, left_res);

	return combine_map(left_res, right_res, rflip);
}

std::string combine_map(const std::string& left, const std::string& right, bool rflip)
{
	std::vector<std::vector<std::string> > left_res, right_res;

	form_map_str_rectangle(left, left_res);
	form_map_str_rectangle(right, right_res);

	return combine_map(left_res, right_res, rflip);
}