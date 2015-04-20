/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#if !defined(FLOOR_NO_LANG)

#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <atomic>
#include <floor/core/util.hpp>
#include <floor/lang/source_types.hpp>
#include <floor/lang/ast_base.hpp>
using namespace std;

//! translation unit ("source file"), managed by a lang_context
struct FLOOR_API translation_unit {
	//! the file name (or <stdin>) corresponding to this TU
	const string file_name;
	//! the full source code of the TU
	source_type source;
	
	//! the lexed tokens
	token_container tokens;
	//! ordered set of iterators to all newlines in source
	set<source_iterator> lines;
	
	//! the constructed abstract syntax tree (points to the root node)
	unique_ptr<node_base> ast;
	
	translation_unit(const string& file_name_) : file_name(file_name_) {}
};

//! the compiler context contains all translation units, original file names and the compile mode that
//! have been set via command line or other means
class FLOOR_API lang_context {
public:
	//! the processing/tool mode the compiler is in
	enum class COMPILER_MODE {
		TOKENIZE,	//!< only tokenize the input
		PARSE,		//!< only tokenize + parse the input
	};
	
	// this should just work
	lang_context() = default;
	lang_context(lang_context&&) = default;
	lang_context& operator=(lang_context&&) = default;
	// prevent copy construction
	lang_context(const lang_context&) = delete;
	lang_context& operator=(const lang_context&) = delete;
	
	//! processes all files that have been added according to the mode that has been set.
	//! returns the amount of fully processed files.
	//! NOTE: exceptions must be handled externally, since lang_context doesn't know your intent!
	uint32_t process_files();
	//! kills all translation units (but keeps the compile mode and files), so process_files() can be
	//! called again and will actually process all files again
	void clean();
	
	//! sets the compile mode
	void set_mode(const COMPILER_MODE mode);
	//! returns the current compile mode
	const COMPILER_MODE& get_mode() const;
	
	//! adds a file that should be processed (move)
	void add_file(string&& file_name);
	//! adds a file that should be processed
	void add_file(const string& file_name);
	
	//! sets the "dump_ast" flag (the AST is dumped to cout)
	void set_dump_ast(const bool state);
	//! returns the "dump_ast" flag
	const bool& get_dump_ast() const;
	
	//! prints true if there have been any new errors since the last call to "has_new_errors".
	//! this will then reset the flag to false.
	bool has_new_errors();
	
	//! returns the translation unit corresponding to the given file name.
	//! NOTE: returns nullptr if the TU doesn't exist and the returned pointer is only valid
	//! as long as no new translation unit is being added.
	const translation_unit* get_translation_unit(const string& file_name) const;
	
protected:
	COMPILER_MODE mode { COMPILER_MODE::PARSE };
	vector<string> files;
	bool dump_ast { false };
	
	// per-context error/warning/note counters
	atomic<bool> new_errors { false };
	
	//! file name -> translation unit
	unordered_map<string, unique_ptr<translation_unit>> translation_units;
	
};

#endif

#endif
