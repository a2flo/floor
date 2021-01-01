/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __FLOOR_COMPILER_CONTEXT_HPP__
#define __FLOOR_COMPILER_CONTEXT_HPP__

#include <floor/core/essentials.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <atomic>
#include <floor/core/util.hpp>
#include <floor/lang/source_types.hpp>
using namespace std;

//! translation unit ("source file"), managed by a lang_context
struct translation_unit {
	//! the file name (or <stdin>) corresponding to this TU
	const string file_name;
	//! the full source code of the TU
	source_type source;
	
	//! the lexed tokens
	token_container tokens;
	//! ordered set of iterators to all newlines in source
	set<source_iterator> lines;
	
	translation_unit(const string& file_name_) : file_name(file_name_) {}
};

#endif
