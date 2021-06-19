/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2015 by Guillaume Melquiond <guillaume.melquiond@gmail.com>


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
 * Routines related to configuration-files / WML.
 */

#include "global.hpp"

#include "config.hpp"
#include "serialization/string_utils.hpp"
#include "util.hpp"
#include "const_clone.tpp"
#include "wml_exception.hpp"

#include <cstdlib>
#include <cstring>
#include <deque>

#include <boost/foreach.hpp>

struct tconfig_implementation
{
	/**
	 * Implementation for the wrappers for
	 * [const] config& child(const std::string& key, const std::string& parent);
	 *
	 * @tparam T                  A pointer to the config.
	 */
	template<class T>
	static typename utils::tconst_clone<config, T>::reference
	child(
			  T config
			, const std::string& key
			, const std::string& parent)
	{
		config->check_valid();

		VALIDATE(!parent.empty(), null_str);
		VALIDATE(parent[0] == '[', null_str);
		VALIDATE(parent[parent.size() - 1] == ']', null_str);

		if(config->has_child(key)) {
			return *(config->children_.find(key)->second.front());
		}

		/**
		 * @todo Implement a proper wml_exception here.
		 *
		 * at the moment there seem to be dependency issues, which i don't want
		 * to fix right now.
		 */
//		FAIL(missing_mandatory_wml_section(parent, key));

		std::stringstream sstr;
		sstr << "Mandatory WML child »[" << key << "]« missing in »"
				<< parent << "«. Please report this bug.";

		throw config::error(sstr.str());
	}
};


/* ** Attribute value implementation ** */


// Special string values.
const std::string config::attribute_value::s_yes("yes");
const std::string config::attribute_value::s_no("no");
const std::string config::attribute_value::s_true("true");
const std::string config::attribute_value::s_false("false");


/** Default implementation, but defined out-of-line for efficiency reasons. */
config::attribute_value::attribute_value()
	: int_value_(0)
	, str_value_(nullptr)
	, tstr_value_(nullptr)
	, type_(type_blank)
#ifdef VERBOSE_CONFIG
	, verbose_()
#endif
{
}

/** Default implementation, but defined out-of-line for efficiency reasons. */
config::attribute_value::~attribute_value()
{
	if (str_value_ != nullptr) {
		delete str_value_;
		str_value_ = nullptr;
	}
	if (tstr_value_ != nullptr) {
		delete tstr_value_;
		tstr_value_ = nullptr;
	}
}

/** Default implementation, but defined out-of-line for efficiency reasons. */
config::attribute_value::attribute_value(const config::attribute_value &that)
	: int_value_(that.int_value_)
	, str_value_(nullptr)
	, tstr_value_(nullptr)
	, type_(that.type_)
#ifdef VERBOSE_CONFIG
	, verbose_(that.verbose_)
#endif
{
	evaluate_value(that);
}

void config::attribute_value::evaluate_value(const config::attribute_value& that)
{
	type_ = that.type_;
	if (type_ == type_bool) {
		bool_value_ = that.bool_value_;

	} else if (type_ == type_integer) {
		int_value_ = that.int_value_;

	} else if (type_ == type_double) {
		double_value_ = that.double_value_;

	} else if (type_ == type_string) {
		VALIDATE(that.str_value_ != nullptr, null_str);
		evaluate_str(*that.str_value_);

	} else if (type_ == type_tstring) {
		VALIDATE(that.tstr_value_ != nullptr, null_str);
		evaluate_tstr(*that.tstr_value_);
	}
}

void config::attribute_value::evaluate_str(const std::string& that)
{
	VALIDATE(type_ = type_string, null_str);
	if (tstr_value_ != nullptr) {
		delete tstr_value_;
		tstr_value_ = nullptr;
	}

	if (str_value_ == nullptr) {
		str_value_ = new std::string(that);
	} else {
		*str_value_ = that;
	}
}

void config::attribute_value::evaluate_tstr(const t_string& that)
{
	VALIDATE(type_ = type_tstring, null_str);
	if (str_value_ != nullptr) {
		delete str_value_;
		str_value_ = nullptr;
	}

	if (tstr_value_ == nullptr) {
		tstr_value_ = new t_string(that);
	} else {
		*tstr_value_ = that;
	}
}

/** Default implementation, but defined out-of-line for efficiency reasons. */
config::attribute_value &config::attribute_value::operator=(const config::attribute_value &that)
{
	evaluate_value(that);

#ifdef VERBOSE_CONFIG
	verbose_ = that.verbose_;
#endif
	return *this;
}

config::attribute_value &config::attribute_value::operator=(bool v)
{
	bool_value_ = v;
	type_ = type_bool;

#ifdef VERBOSE_CONFIG
	verbose_ = v? s_yes: s_no;
#endif
	return *this;
}

config::attribute_value &config::attribute_value::operator=(int v)
{
	int_value_ = v;
	type_ = type_integer;
#ifdef VERBOSE_CONFIG
	verbose_ = str_cast(v);
#endif
	return *this;
}

config::attribute_value &config::attribute_value::operator=(long long v)
{
	int_value_ = v;
	type_ = type_integer;

#ifdef VERBOSE_CONFIG
	verbose_ = str_cast(v);
#endif
	return *this;
}

config::attribute_value &config::attribute_value::operator=(unsigned long long v)
{
	int_value_ = v;
	type_ = type_integer;
#ifdef VERBOSE_CONFIG
	verbose_ = str_cast(v);
#endif
	return *this;
}

void config::attribute_value::evaluate_double(double v)
{
	double_value_ = v;
	type_ = type_double;

	double integred = SDL_ceil(v);
	if (integred != v) {
		// has decimal
		return;
	}
	// must has no decimal.
	if (v > 0.0) {
		if (v <= ULLONG_MAX) {
			type_ = type_integer;
			int_value_ = (uint64_t)v;
		}
	} else {
		if (v >= LLONG_MIN) {
			type_ = type_integer;
			int_value_ = (int64_t)v;
		}
	}
}

config::attribute_value &config::attribute_value::operator=(double v)
{
	evaluate_double(v);
#ifdef VERBOSE_CONFIG
	verbose_ = str_cast(v);
#endif

	return *this;
}

config::attribute_value& config::attribute_value::from_string(const std::string &v, bool keep_str)
{
	type_ = type_string;
#ifdef VERBOSE_CONFIG
	verbose_ = v;
#endif

	// Handle some special strings.
	if (keep_str || v.empty()) {
		evaluate_str(v);
		return *this; 
	}
	if (v == s_yes || v == s_true) {
		type_ = type_bool;
		bool_value_ = true;
		return *this;
	}
	if (v == s_no || v == s_false) {
		type_ = type_bool;
		bool_value_ = false;
		return *this;
	}

	// Attempt to convert to a number.
	char *eptr;

	// must not use SDL_strtod(v.c_str(), &eptr)
	// -9223372036854775807 ==> -4294967295.0
	// double d = SDL_strtod(v.c_str(), &eptr);
	double d = strtod(v.c_str(), &eptr);
	if ( *eptr == '\0' ) {
		evaluate_double(d);
		return *this;
	}

	// No conversion possible. Store the string.
	evaluate_str(v);
	return *this;
}

config::attribute_value &config::attribute_value::operator=(const std::string &v)
{
	return from_string(v, false);
}

config::attribute_value &config::attribute_value::operator=(const t_string &v)
{
	if (!v.translatable()) return *this = v.str();
	type_ = type_tstring;
	evaluate_tstr(v);
#ifdef VERBOSE_CONFIG
	verbose_ = v;
#endif
	return *this;
}


bool config::attribute_value::to_bool(bool def) const
{
	if (type_ == type_bool) {
		return bool_value_;
	}

	// No other types are ever recognized as boolean.
	return def;
}

int config::attribute_value::to_int(int def) const
{
	if (type_ == type_integer) {
		return (int)int_value_;
	}
	return def;
}

long long config::attribute_value::to_long_long(long long def) const
{
	if (type_ == type_integer) {
		return int_value_;
	}
	return def;
}

unsigned config::attribute_value::to_unsigned(unsigned def) const
{
	if (type_ == type_integer) {
		return (uint32_t)int_value_;
	}
	return def;
}

size_t config::attribute_value::to_size_t(size_t def) const
{
	if (type_ == type_integer) {
		return (size_t)int_value_;
	}
	return def;
}

time_t config::attribute_value::to_time_t(time_t def) const
{
	if (type_ == type_integer) {
		return (time_t)int_value_;
	}
	return def;
}

double config::attribute_value::to_double(double def) const
{
	if (type_ == type_integer) {
		return int_value_;
	} else if (type_ == type_double) {
		return double_value_;
	}
	return def;
}

std::string config::attribute_value::str() const
{
	if (type_ == type_bool) {
		return bool_value_? s_yes: s_no;

	} else if (type_ == type_integer) {
		return str_cast(int_value_);

	} else if (type_ == type_double) {
		return utils::from_double(double_value_);
		// return str_cast(int_value_);

	} else if (type_ == type_string) {
		return *str_value_;

	} else if (type_ == type_tstring) {
		return tstr_value_->str();
	}

	VALIDATE(type_ == type_blank, null_str);
	return null_str;
}

const std::string& config::attribute_value::str_ref() const
{
	VALIDATE(type_ == type_string, null_str);
	return *str_value_;
}

t_string config::attribute_value::t_str() const
{
	if (type_ == type_tstring) {
		return *tstr_value_;
	}
	return str();
}

const t_string& config::attribute_value::t_str_ref() const
{
	VALIDATE(type_ == type_tstring, null_str);
	return *tstr_value_;
}

/**
 * Tests for an attribute that was never set.
 */
bool config::attribute_value::blank() const
{
	return type_ == type_blank;
}

/**
 * Tests for an attribute that either was never set or was set to "".
 */
bool config::attribute_value::empty() const
{
	if (type_ == type_blank) {
		return true;
	} else if (type_ == type_string) {
		return str_value_->empty();
	}
	return false;
}

/**
 * Checks for equality of the attribute values when viewed as strings.
 * Exception: Boolean synonyms can be equal ("yes" == "true").
 * Note: Blanks have no string representation, so do not equal "" (an empty string).
 */
bool config::attribute_value::operator==(const config::attribute_value &other) const
{
	if (type_ == type_bool) {
		return other.type_ == type_bool && other.bool_value_ == bool_value_;

	} else if (type_ == type_integer) {
		return other.type_ == type_integer && other.int_value_ == int_value_;

	} else if (type_ == type_double) {
		return other.type_ == type_double && other.double_value_ == double_value_;

	} else if (type_ == type_string) {
		return other.type_ == type_string && *other.str_value_ == *str_value_;

	} else if (type_ == type_tstring) {
		return other.type_ == type_tstring && *other.tstr_value_ == *tstr_value_;
	}

	VALIDATE(type_ == type_blank, null_str);
	return (other.type_ == type_blank) || (other.type_ == type_string && other.str_value_->empty());
}

std::ostream &operator<<(std::ostream &os, const config::attribute_value &v)
{
	// Simple implementation, but defined out-of-line because of the templating
	// involved.
	return os << v.str();
}


/* ** config implementation ** */


config config::invalid;
const config null_cfg;

const char* config::diff_track_attribute = "__diff_track";

void config::check_valid() const
{
	VALIDATE(*this, "Mandatory WML child missing yet untested for. Please report.");
}

void config::check_valid(const config &cfg) const
{
	VALIDATE(*this && cfg, "Mandatory WML child missing yet untested for. Please report.");
}

config::config()
	: values_()
	, children_()
	, ordered_children()
{
}

config::config(const config& cfg)
	: values_(cfg.values_)
	, children_()
	, ordered_children()
{
	append_children(cfg);
}

config::config(const std::string& child)
	: values_()
	, children_(), ordered_children()
{
	add_child(child);
}

config::~config()
{
	clear();
}

config& config::operator=(const config& cfg)
{
	if(this == &cfg) {
		return *this;
	}

	clear();
	append_children(cfg);
	values_.insert(cfg.values_.begin(), cfg.values_.end());
	return *this;
}

#ifdef HAVE_CXX11
config::config(config &&cfg):
	values(std::move(cfg.values)),
	children(std::move(cfg.children)),
	ordered_children(std::move(cfg.ordered_children))
{
}

config &config::operator=(config &&cfg)
{
	clear();
	swap(cfg);
	return *this;
}
#endif

bool config::valid_tag(const std::string& name)
{
	if(name == "") {
		// Empty strings not allowed
		return false;
	} else if(name == "_") {
		// A lone underscore isn't a valid tag name
		return false;
	} else {
/*
		return std::all_of(name.begin(), name.end(), [](const char& c)
		{
			// Only alphanumeric ASCII characters and underscores are allowed
			return std::isalnum(c, std::locale::classic()) || c == '_';
		});
*/
		return true;
	}
}

bool config::valid_attribute(const std::string& name)
{
	VALIDATE(false, null_str);
/*
	if(name.empty()) {
		return false;
	}

	for(char c : name) {
		if(!std::isalnum(c, std::locale::classic()) && c != '_') {
			return false;
		}
	}
*/
	return true;
}

bool config::has_attribute(const std::string &key) const
{
	check_valid();
	return values_.find(key) != values_.end();
}

bool config::has_old_attribute(const std::string &key, const std::string &old_key, const std::string& msg) const
{
	check_valid();
	if (values_.find(key) != values_.end()) {
		return true;
	} else if (values_.find(old_key) != values_.end()) {
		if (!msg.empty()) {
			// $msg;
		}
		return true;
	}
	return false;
}


void config::remove_attribute(const std::string &key)
{
	check_valid();
	values_.erase(key);
}

void config::append_children(const config &cfg)
{
	check_valid(cfg);

	BOOST_FOREACH (const any_child &value, cfg.all_children_range()) {
		add_child(value.key, value.cfg);
	}
}

void config::append(const config &cfg)
{
	append_children(cfg);
	for (const attribute& v : cfg.values_) {
		values_[v.first] = v.second;
	}
}

void config::merge_children(const std::string& key)
{
	check_valid();

	if (child_count(key) < 2) return;

	config merged_children;
	BOOST_FOREACH (const config &cfg, child_range(key)) {
		merged_children.append(cfg);
	}

	clear_children(key);
	add_child(key,merged_children);
}

void config::merge_children_by_attribute(const std::string& key, const std::string& attribute)
{
	check_valid();

	if (child_count(key) < 2) return;

	typedef std::map<std::string, config> config_map;
	config_map merged_children_map;
	BOOST_FOREACH (const config &cfg, child_range(key)) {
		const std::string &value = cfg[attribute];
		config_map::iterator m = merged_children_map.find(value);
		if ( m!=merged_children_map.end() ) {
			m->second.append(cfg);
		} else {
			merged_children_map.insert(make_pair(value, cfg));
		}
	}

	clear_children(key);
	for (const config_map::value_type &i: merged_children_map) {
		add_child(key,i.second);
	}
}

config::child_itors config::child_range(const std::string& key)
{
	check_valid();

	child_map::iterator i = children_.find(key);
	static child_list dummy;
	child_list *p = &dummy;
	if (i != children_.end()) {
		p = &i->second;
	}
	return child_itors(child_iterator(p->begin()), child_iterator(p->end()));
}

config::const_child_itors config::child_range(const std::string& key) const
{
	check_valid();

	child_map::const_iterator i = children_.find(key);
	static child_list dummy;
	const child_list *p = &dummy;
	if (i != children_.end()) {
		p = &i->second;
	}
	return const_child_itors(const_child_iterator(p->begin()), const_child_iterator(p->end()));
}

unsigned config::child_count(const std::string &key) const
{
	check_valid();

	child_map::const_iterator i = children_.find(key);
	if(i != children_.end()) {
		return i->second.size();
	}
	return 0;
}

bool config::has_child(const std::string &key) const
{
	check_valid();

	return children_.find(key) != children_.end();
}

config &config::child(const std::string& key, int n)
{
	check_valid();

	const child_map::const_iterator i = children_.find(key);
	if (i == children_.end()) {
		// The config object has no child named $key.

		return invalid;
	}

	if (n < 0) n = i->second.size() + n;
	if(size_t(n) < i->second.size()) {
		return *i->second[n];
	} else {
		// The config object has only $i->second.size() children named $key; request for the index $n cannot be honored.

		return invalid;
	}
}

config& config::child(const std::string& key, const std::string& parent)
{
	return tconfig_implementation::child(this, key, parent);
}

const config& config::child(
		  const std::string& key
		, const std::string& parent) const
{
	return tconfig_implementation::child(this, key, parent);
}

const config & config::child_or_empty(const std::string& key) const
{
	static const config empty_cfg;
	check_valid();

	child_map::const_iterator i = children_.find(key);
	if (i != children_.end() && !i->second.empty())
		return *i->second.front();

	return empty_cfg;
}

config &config::child_or_add(const std::string &key)
{
	child_map::const_iterator i = children_.find(key);
	if (i != children_.end() && !i->second.empty())
		return *i->second.front();

	return add_child(key);
}

config& config::add_child(const std::string& key)
{
	check_valid();

	child_list& v = children_[key];
	v.push_back(new config());
	ordered_children.push_back(child_pos(children_.find(key),v.size()-1));
	return *v.back();
}

config& config::add_child(const std::string& key, const config& val)
{
	check_valid(val);

	child_list& v = children_[key];
	v.push_back(new config(val));
	ordered_children.push_back(child_pos(children_.find(key),v.size()-1));
	return *v.back();
}

#ifdef HAVE_CXX11
config &config::add_child(const std::string &key, config &&val)
{
	check_valid(val);

	child_list &v = children[key];
	v.push_back(new config(std::move(val)));
	ordered_children.push_back(child_pos(children.find(key), v.size() - 1));
	return *v.back();
}
#endif

config &config::add_child_at(const std::string &key, const config &val, unsigned index)
{
	check_valid(val);

	child_list& v = children_[key];
	if(index > v.size()) {
		throw error("illegal index to add child at");
	}

	v.insert(v.begin()+index,new config(val));

	bool inserted = false;

	const child_pos value(children_.find(key),index);

	std::vector<child_pos>::iterator ord = ordered_children.begin();
	for(; ord != ordered_children.end(); ++ord) {
		if (ord->pos != value.pos) continue;
		if (!inserted && ord->index == index) {
			ord = ordered_children.insert(ord,value);
			inserted = true;
		} else if (ord->index >= index) {
			ord->index++;
		}
	}

	if(!inserted) {
		ordered_children.push_back(value);
	}

	return *v[index];
}

namespace {

struct remove_ordered
{
	remove_ordered(const config::child_map::iterator &iter) : iter_(iter) {}

	bool operator()(const config::child_pos &pos) const
	{ return pos.pos == iter_; }
private:
	config::child_map::iterator iter_;
};

}

void config::clear_children(const std::string& key)
{
	check_valid();

	child_map::iterator i = children_.find(key);
	if (i == children_.end()) return;

	ordered_children.erase(std::remove_if(ordered_children.begin(),
		ordered_children.end(), remove_ordered(i)), ordered_children.end());

	for (config *c: i->second) {
		delete c;
	}

	children_.erase(i);
}

void config::splice_children(config &src, const std::string &key)
{
	check_valid(src);

	child_map::iterator i_src = src.children_.find(key);
	if (i_src == src.children_.end()) return;

	src.ordered_children.erase(std::remove_if(src.ordered_children.begin(),
		src.ordered_children.end(), remove_ordered(i_src)),
		src.ordered_children.end());

	child_list &dst = children_[key];
	child_map::iterator i_dst = children_.find(key);
	unsigned before = dst.size();
	dst.insert(dst.end(), i_src->second.begin(), i_src->second.end());
	src.children_.erase(i_src);
	// key might be a reference to i_src->first, so it is no longer usable.

	for (unsigned j = before; j < dst.size(); ++j) {
		ordered_children.push_back(child_pos(i_dst, j));
	}
}

void config::recursive_clear_value(const std::string& key)
{
	check_valid();

	values_.erase(key);

	BOOST_FOREACH(const any_child &value, all_children_range()) {
		const_cast<config *>(&value.cfg)->recursive_clear_value(key);
	}
}

std::vector<config::child_pos>::iterator config::remove_child(
	const child_map::iterator &pos, unsigned index)
{
	/* Find the position with the correct index and decrement all the
	   indices in the ordering that are above this index. */
	unsigned found = 0;
	for (child_pos &p: ordered_children)
	{
		if (p.pos != pos) continue;
		if (p.index == index)
			found = &p - &ordered_children.front();
		else if (p.index > index)
			--p.index;
	}

	// Remove from the child map.
	delete pos->second[index];
	pos->second.erase(pos->second.begin() + index);

	// Erase from the ordering and return the next position.
	return ordered_children.erase(ordered_children.begin() + found);
}

config::all_children_iterator config::erase(const config::all_children_iterator& i)
{
	return all_children_iterator(remove_child(i.i_->pos, i.i_->index));
}

void config::remove_child(const std::string &key, unsigned index)
{
	check_valid();

	child_map::iterator i = children_.find(key);
	if (i == children_.end() || index >= i->second.size()) {
		// Error: attempting to delete non-existing child: $key [$index]
		return;
	}

	remove_child(i, index);
}

const config::attribute_value &config::operator[](const std::string &key) const
{
	check_valid();

	const attribute_map::const_iterator i = values_.find(key);
	if (i != values_.end()) return i->second;
	static const attribute_value empty_attribute;
	return empty_attribute;
}

const config::attribute_value *config::get(const std::string &key) const
{
	check_valid();
	attribute_map::const_iterator i = values_.find(key);
	return i != values_.end() ? &i->second : NULL;
}

config::attribute_value &config::operator[](const std::string &key)
{
	check_valid();
	return values_[key];
}

const config::attribute_value &config::get_old_attribute(const std::string &key, const std::string &old_key, const std::string &msg) const
{
	check_valid();

	attribute_map::const_iterator i = values_.find(key);
	if (i != values_.end())
		return i->second;

	i = values_.find(old_key);
	if (i != values_.end()) {
		if (!msg.empty()) {
			// $msg;
		}
		return i->second;
	}

	static const attribute_value empty_attribute;
	return empty_attribute;
}


void config::merge_attributes(const config &cfg)
{
	check_valid(cfg);

	VALIDATE(this != &cfg, null_str);
	for (const attribute& v : cfg.values_) {

		std::string key = v.first;
		if (key.substr(0,7) == "add_to_") {
			std::string add_to = key.substr(7);
			values_[add_to] = values_[add_to].to_int() + v.second.to_int();
		} else
			values_[v.first] = v.second;
	}
}

config::const_attr_itors config::attribute_range() const
{
	check_valid();

	const_attr_itors range(const_attribute_iterator(values_.begin()), const_attribute_iterator(values_.end()));

	// Ensure the first element is not blank, as a few places assume this
	while(range.begin() != range.end() && range.begin()->second.blank()) {
		range.pop_front();
	}

	return range;
}

namespace {

struct config_has_value {
	config_has_value(const std::string& name, const std::string& value)
		: name_(name), value_()
	{
		value_ = value;
	}

	bool operator()(const config* cfg) const { return (*cfg)[name_] == value_; }

private:
	std::string name_;
	config::attribute_value value_;
};

} // end namespace

config &config::find_child(const std::string &key, const std::string &name,
	const std::string &value)
{
	check_valid();

	const child_map::iterator i = children_.find(key);
	if(i == children_.end()) {
		// Key $name value $value pair not found as child of key $key.

		return invalid;
	}

	const child_list::iterator j = std::find_if(i->second.begin(),
	                                            i->second.end(),
	                                            config_has_value(name,value));
	if(j != i->second.end()) {
		return **j;
	} else {
		// Key $name value $value pair not found as child of key $key.

		return invalid;
	}
}

namespace {
	/**
	 * Helper struct for iterative config clearing.
	 */
	struct config_clear_state
	{
		config_clear_state()
			: c(NULL)
			, mi()
			, vi(0)
		{
		}

		config* c; //the config being inspected
		config::child_map::iterator mi; //current child map entry
		size_t vi; //index into the child map item vector
	};
}

void config::clear()
{
	// No validity check for this function.

	if (!children_.empty()) {
		//start with this node, the first entry in the child map,
		//zeroeth element in that entry
		config_clear_state init;
		init.c = this;
		init.mi = children_.begin();
		init.vi = 0;
		std::deque<config_clear_state> l;
		l.push_back(init);

		while (!l.empty()) {
			config_clear_state& state = l.back();
			if (state.mi != state.c->children_.end()) {
				std::vector<config*>& v = state.mi->second;
				if (state.vi < v.size()) {
					config* c = v[state.vi];
					++state.vi;
					if (c->children_.empty()) {
						delete c; //special case for a slight speed increase?
					} else {
						//descend to the next level
						config_clear_state next;
						next.c = c;
						next.mi = c->children_.begin();
						next.vi = 0;
						l.push_back(next);
					}
				} else {
					state.vi = 0;
					++state.mi;
				}
			} else {
				//reached end of child map for this element - all child nodes
				//have been deleted, so it's safe to clear the map, delete the
				//node and move up one level
				state.c->children_.clear();
				if (state.c != this) delete state.c;
				l.pop_back();
			}
		}
	}

	values_.clear();
	ordered_children.clear();
}

bool config::empty() const
{
	check_valid();

	return children_.empty() && values_.empty();
}

void config::to_json(Json::Value& node) const
{
	for (attribute_map::const_iterator it = values_.begin(); it != values_.end(); ++ it) {
		const attribute_value& val = it->second;
		if (val.type_ == attribute_value::type_bool) {
			node[it->first] = val.bool_value_;
		} else if (val.type_ == attribute_value::type_integer) {
			node[it->first] = val.int_value_;
		} else if (val.type_ == attribute_value::type_double) {
			node[it->first] = val.double_value_;
		} else if (val.type_ == attribute_value::type_string) {
			node[it->first] = *val.str_value_;
		} else if (val.type_ == attribute_value::type_tstring) {
			node[it->first] = val.str();
		}
	}

	for (child_map::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		const std::string& key = it->first;
		if (key.empty()) {
			continue;
		}
		const child_list& list = it->second;
		Json::Value json_children;
		for (child_list::const_iterator it2 = list.begin(); it2 != list.end(); ++ it2) {
			const config& cfg = **it2;
			Json::Value json_child;
			cfg.to_json(json_child);
			json_children.append(json_child);
		}
		node[key] = json_children;
	}
}

config::all_children_iterator::reference config::all_children_iterator::operator*() const
{
	return any_child(&i_->pos->first, i_->pos->second[i_->index]);
}

config::all_children_iterator config::ordered_begin() const
{
	return all_children_iterator(ordered_children.begin());
}

config::all_children_iterator config::ordered_end() const
{
	return all_children_iterator(ordered_children.end());
}

config::all_children_itors config::all_children_range() const
{
	return all_children_itors(
		all_children_iterator(ordered_children.begin()),
		all_children_iterator(ordered_children.end())
	);
}

config config::get_diff(const config& c) const
{
	check_valid(c);

	config res;
	get_diff(c, res);
	return res;
}

void config::get_diff(const config& c, config& res) const
{
	check_valid(c);
	check_valid(res);

	config* inserts = NULL;

	attribute_map::const_iterator i;
	for(i = values_.begin(); i != values_.end(); ++i) {
		const attribute_map::const_iterator j = c.values_.find(i->first);
		if(j == c.values_.end() || (i->second != j->second && i->second != "")) {
			if(inserts == NULL) {
				inserts = &res.add_child("insert");
			}

			(*inserts)[i->first] = i->second;
		}
	}

	config* deletes = NULL;

	for(i = c.values_.begin(); i != c.values_.end(); ++i) {
		const attribute_map::const_iterator itor = values_.find(i->first);
		if(itor == values_.end() || itor->second == "") {
			if(deletes == NULL) {
				deletes = &res.add_child("delete");
			}

			(*deletes)[i->first] = "x";
		}
	}

	std::vector<std::string> entities;

	child_map::const_iterator ci;
	for(ci = children_.begin(); ci != children_.end(); ++ci) {
		entities.push_back(ci->first);
	}

	for(ci = c.children_.begin(); ci != c.children_.end(); ++ci) {
		if(children_.count(ci->first) == 0) {
			entities.push_back(ci->first);
		}
	}

	for(std::vector<std::string>::const_iterator itor = entities.begin(); itor != entities.end(); ++itor) {

		const child_map::const_iterator itor_a = children_.find(*itor);
		const child_map::const_iterator itor_b = c.children_.find(*itor);

		static const child_list dummy;

		// Get the two child lists. 'b' has to be modified to look like 'a'.
		const child_list& a = itor_a != children_.end() ? itor_a->second : dummy;
		const child_list& b = itor_b != c.children_.end() ? itor_b->second : dummy;

		size_t ndeletes = 0;
		size_t ai = 0, bi = 0;
		while(ai != a.size() || bi != b.size()) {
			// If the two elements are the same, nothing needs to be done.
			if(ai < a.size() && bi < b.size() && *a[ai] == *b[bi]) {
				++ai;
				++bi;
			} else {
				// We have to work out what the most appropriate operation --
				// delete, insert, or change is the best to get b[bi] looking like a[ai].
				std::stringstream buf;

				// If b has more elements than a, then we assume this element
				// is an element that needs deleting.
				if(b.size() - bi > a.size() - ai) {
					config& new_delete = res.add_child("delete_child");
					buf << bi - ndeletes;
					new_delete.values_["index"] = buf.str();
					new_delete.add_child(*itor);

					++ndeletes;
					++bi;
				}

				// If b has less elements than a, then we assume this element
				// is an element that needs inserting.
				else if(b.size() - bi < a.size() - ai) {
					config& new_insert = res.add_child("insert_child");
					buf << ai;
					new_insert.values_["index"] = buf.str();
					new_insert.add_child(*itor,*a[ai]);

					++ai;
				}

				// Otherwise, they have the same number of elements,
				// so try just changing this element to match.
				else {
					config& new_change = res.add_child("change_child");
					buf << bi;
					new_change.values_["index"] = buf.str();
					new_change.add_child(*itor,a[ai]->get_diff(*b[bi]));

					++ai;
					++bi;
				}
			}
		}
	}
}

void config::apply_diff(const config& diff, bool track /* = false */)
{
	check_valid(diff);

	if (track) values_[diff_track_attribute] = "modified";

	if (const config &inserts = diff.child("insert")) {
		for (const attribute &v: inserts.attribute_range()) {
			values_[v.first] = v.second;
		}
	}

	if (const config &deletes = diff.child("delete")) {
		for (const attribute &v: deletes.attribute_range()) {
			values_.erase(v.first);
		}
	}

	BOOST_FOREACH(const config &i, diff.child_range("change_child"))
	{
		const size_t index = lexical_cast<size_t>(i["index"].str());
		BOOST_FOREACH(const any_child &item, i.all_children_range())
		{
			if (item.key.empty()) {
				continue;
			}

			const child_map::iterator itor = children_.find(item.key);
			if(itor == children_.end() || index >= itor->second.size()) {
				throw error("error in diff: could not find element '" + item.key + "'");
			}

			itor->second[index]->apply_diff(item.cfg, track);
		}
	}

	BOOST_FOREACH(const config &i, diff.child_range("insert_child"))
	{
		const size_t index = lexical_cast<size_t>(i["index"].str());
		BOOST_FOREACH(const any_child &item, i.all_children_range()) {
			config& inserted = add_child_at(item.key, item.cfg, index);
			if (track) inserted[diff_track_attribute] = "new";
		}
	}

	BOOST_FOREACH(const config &i, diff.child_range("delete_child"))
	{
		const size_t index = lexical_cast<size_t>(i["index"].str());
		BOOST_FOREACH(const any_child &item, i.all_children_range()) {
			if (!track) {
				remove_child(item.key, index);
			} else {
				const child_map::iterator itor = children_.find(item.key);
				if(itor == children_.end() || index >= itor->second.size()) {
					throw error("error in diff: could not find element '" + item.key + "'");
				}
				itor->second[index]->values_[diff_track_attribute] = "deleted";
			}
		}
	}
}

void config::clear_diff_track(const config& diff)
{
	remove_attribute(diff_track_attribute);
	BOOST_FOREACH(const config &i, diff.child_range("delete_child"))
	{
		const size_t index = lexical_cast<size_t>(i["index"].str());
		BOOST_FOREACH(const any_child &item, i.all_children_range()) {
			remove_child(item.key, index);
		}
	}

	BOOST_FOREACH(const config &i, diff.child_range("change_child"))
	{
		const size_t index = lexical_cast<size_t>(i["index"].str());
		BOOST_FOREACH(const any_child &item, i.all_children_range())
		{
			if (item.key.empty()) {
				continue;
			}

			const child_map::iterator itor = children_.find(item.key);
			if(itor == children_.end() || index >= itor->second.size()) {
				throw error("error in diff: could not find element '" + item.key + "'");
			}

			itor->second[index]->clear_diff_track(item.cfg);
		}
	}
	BOOST_FOREACH(const any_child &value, all_children_range()) {
		const_cast<config *>(&value.cfg)->remove_attribute(diff_track_attribute);
	}
}

/**
 * Merge config 'c' into this config, overwriting this config's values.
 */
void config::merge_with(const config& c)
{
	check_valid(c);

	std::vector<child_pos> to_remove;
	std::map<std::string, unsigned> visitations;

	// Merge attributes first
	merge_attributes(c);

	// Now merge shared tags
	all_children_iterator::Itor i, i_end = ordered_children.end();
	for(i = ordered_children.begin(); i != i_end; ++i) {
		const std::string& tag = i->pos->first;
		const child_map::const_iterator j = c.children_.find(tag);
		if (j != c.children_.end()) {
			unsigned &visits = visitations[tag];
			if(visits < j->second.size()) {
				// Get a const config so we do not add attributes.
				const config & merge_child = *j->second[visits++];

				if ( merge_child["__remove"].to_bool() ) {
					to_remove.push_back(*i);
				} else
					(i->pos->second[i->index])->merge_with(merge_child);
			}
		}
	}

	// Now add any unvisited tags
	for(child_map::const_iterator j = c.children_.begin(); j != c.children_.end(); ++j) {
		const std::string& tag = j->first;
		unsigned &visits = visitations[tag];
		while(visits < j->second.size()) {
			add_child(tag, *j->second[visits++]);
		}
	}

	// Remove those marked so
	std::map<std::string, unsigned> removals;
	for (const child_pos& pos: to_remove) {
		const std::string& tag = pos.pos->first;
		unsigned &removes = removals[tag];
		remove_child(tag, pos.index - removes++);
	}

}

/**
 * Merge config 'c' into this config, preserving this config's values.
 */
void config::inherit_from(const config& c)
{
	// Using a scratch config and merge_with() seems to execute about as fast
	// as direct coding of this merge.
	config scratch(c);
	scratch.merge_with(*this);
	swap(scratch);
}

bool config::matches(const config &filter) const
{
	check_valid(filter);

	for (const attribute &i: filter.attribute_range())
	{
		const attribute_value *v = get(i.first);
		if (!v || *v != i.second) return false;
	}

	BOOST_FOREACH(const any_child &i, filter.all_children_range())
	{
		if (i.key == "not") {
			if (matches(i.cfg)) return false;
			continue;
		}
		bool found = false;
		BOOST_FOREACH(const config &j, child_range(i.key)) {
			if (j.matches(i.cfg)) {
				found = true;
				break;
			}
		}
		if(!found) return false;
	}
	return true;
}

std::string config::debug() const
{
	check_valid();

	std::ostringstream outstream;
	outstream << *this;
	return outstream.str();
}

std::ostream& operator << (std::ostream& outstream, const config& cfg)
{
	static int i = 0;
	i++;
	for (const config::attribute &val: cfg.attribute_range()) {
		for (int j = 0; j < i-1; j++){ outstream << char(9); }
		outstream << val.first << " = " << val.second << '\n';
	}
	BOOST_FOREACH(const config::any_child &child, cfg.all_children_range())
	{
		for (int j = 0; j < i - 1; ++j) outstream << char(9);
		outstream << "[" << child.key << "]\n";
		outstream << child.cfg;
		for (int j = 0; j < i - 1; ++j) outstream << char(9);
		outstream << "[/" << child.key << "]\n";
	}
	i--;
    return outstream;
}

std::string config::hash() const
{
	check_valid();

	static const unsigned int hash_length = 128;
	static const char hash_string[] =
		"+-,.<>0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char hash_str[hash_length + 1];
	std::string::const_iterator c;

	unsigned int i;
	for(i = 0; i != hash_length; ++i) {
		hash_str[i] = 'a';
	}
	hash_str[hash_length] = 0;

	i = 0;
	for (const attribute &val: values_)
	{
		for (c = val.first.begin(); c != val.first.end(); ++c) {
			hash_str[i] ^= *c;
			if (++i == hash_length) i = 0;
		}
		std::string base_str = val.second.t_str().base_str();
		for (c = base_str.begin(); c != base_str.end(); ++c) {
			hash_str[i] ^= *c;
			if (++i == hash_length) i = 0;
		}
	}

	BOOST_FOREACH(const any_child &ch, all_children_range())
	{
		std::string child_hash = ch.cfg.hash();
		for (char c: child_hash) {
			hash_str[i] ^= c;
			++i;
			if(i == hash_length) {
				i = 0;
			}
		}
	}

	for(i = 0; i != hash_length; ++i) {
		hash_str[i] = hash_string[
			static_cast<unsigned>(hash_str[i]) % strlen(hash_string)];
	}

	return std::string(hash_str);
}

void config::swap(config& cfg)
{
	check_valid(cfg);

	values_.swap(cfg.values_);
	children_.swap(cfg.children_);
	ordered_children.swap(cfg.ordered_children);
}

bool operator==(const config& a, const config& b)
{
	a.check_valid(b);

	if (a.values_ != b.values_)
		return false;

	config::all_children_itors x = a.all_children_range(), y = b.all_children_range();
	for (; x.first != x.second && y.first != y.second; ++x.first, ++y.first) {
		if (x.first->key != y.first->key || x.first->cfg != y.first->cfg) {
			return false;
		}
	}

	return x.first == x.second && y.first == y.second;
}