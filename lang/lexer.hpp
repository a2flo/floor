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

#ifndef __FLOOR_LEXER_HPP__
#define __FLOOR_LEXER_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_LANG)

#include <string>
#include <floor/core/util.hpp>
#include <floor/lang/source_types.hpp>
#include <floor/lang/lang_context.hpp>
using namespace std;

class lexer final {
public:
	//! phase 1: replaces \r occurrences and creates a newline iterator set
	static void map_characters(translation_unit& tu);
	//! phase 3+: the actual lexing (and all the other necessary stuff)
	static void lex(translation_unit& tu);
	
	//! for debugging and assignment purposes
	static void print_tokens(const translation_unit& tu);
	
	//! returns the iters corresponding <line number, column number>
	static pair<uint32_t, uint32_t> get_line_and_column_from_iter(const translation_unit& tu,
																  const source_iterator& iter);
	
	//! assigns the resp. FLOOR_KEYWORD and FLOOR_PUNCTUATOR enums/sub-type to the token type
	static void assign_token_sub_types(translation_unit& tu);
	
protected:
	// NOTE: these are all the functions to lex any token
	// NOTE: every lex_* function has to return an iterator to the character following the lexed token (or .end())
	// and must also set the iter parameter to this iterator position!
	
	static source_iterator lex_keyword_or_identifier(source_iterator& iter, const source_iterator& source_end);
	static source_iterator lex_decimal_constant(source_iterator& iter, const source_iterator& source_end);
	static source_iterator lex_character_constant(source_iterator& iter, const source_iterator& source_end);
	static source_iterator lex_string_literal(source_iterator& iter, const source_iterator& source_end);
	static source_iterator lex_punctuator(source_iterator& iter, const source_iterator& source_end);
	static source_iterator lex_comment(source_iterator& iter, const source_iterator& source_end);
	
	//! checks whether a single character is part of an escape sequence (char after '\')
	static bool is_escape_sequence_char(const source_iterator& iter);
	
	//! checks whether a single character is part of the source character set
	static bool is_char_in_character_set(const source_iterator& iter); // TODO: utf-8
	
	//! checks whether a single character is printable
	static bool is_printable_char(const source_iterator& iter); // TODO: utf-8
	
	// static class
	lexer(const lexer&) = delete;
	~lexer() = delete;
	lexer& operator=(const lexer&) = delete;
	
};

#endif

#endif
