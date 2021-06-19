#ifndef UNIT_MAP_HPP_INCLUDED
#define UNIT_MAP_HPP_INCLUDED

#include "unit.hpp"
#include "base_map.hpp"

class simple_controller;

class unit_map: public base_map
{
public:
	unit_map(simple_controller& controller, const tmap& gmap, bool consistent);

	void add(const map_location& loc, const base_unit* base_u);

	unit* find_unit(const map_location& loc) const;
	unit* find_unit(const map_location& loc, bool verlay) const;
	unit* find_unit(int i) const { return dynamic_cast<unit*>(map_[i]); }
};

#endif
