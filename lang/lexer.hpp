/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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
#include <string>
#include <floor/core/util.hpp>
#include <floor/lang/source_types.hpp>
#include <floor/lang/lang_context.hpp>
using namespace std;

class lexer {
public:
	//! lex the input data specified in the translation unit
	// implement in user class: static void lex(translation_unit& tu);
	
	//! for debugging/development purposes
	static void print_tokens(const translation_unit& tu);
	
	//! returns the iters corresponding <line number, column number>
	static pair<uint32_t, uint32_t> get_line_and_column_from_iter(const translation_unit& tu,
																  const source_iterator& iter);
	
	//! replaces \r occurrences and creates a newline iterator set
	static void map_characters(translation_unit& tu);
	
	//! lex_* function return type to signal if lexing was successful + return an iterator to the next token
	typedef pair<bool, source_iterator> lex_return_type;
	
	//! prints a proper lexer error message with line+column info and returns a signal to stop the lexing
	static lex_return_type handle_error(const translation_unit& tu,
										const source_iterator& iter,
										const string& error_msg);
	
	// NOTE: these are all the functions to lex any token
	// NOTE: every lex_* function has to return an iterator to the character following the lexed token (or .end())
	// and must also set the iter parameter to this iterator position!
	// NOTE: these lexing function are meant to lex a subset of C11, but might be of use for other cases
	
	static lex_return_type lex_keyword_or_identifier(const translation_unit& tu,
													 source_iterator& iter,
													 const source_iterator& source_end);
	static lex_return_type lex_decimal_constant(const translation_unit& tu,
												source_iterator& iter,
												const source_iterator& source_end);
	static lex_return_type lex_character_constant(const translation_unit& tu,
												  source_iterator& iter,
												  const source_iterator& source_end);
	static lex_return_type lex_string_literal(const translation_unit& tu,
											  source_iterator& iter,
											  const source_iterator& source_end);
	static lex_return_type lex_punctuator(const translation_unit& tu,
										  source_iterator& iter,
										  const source_iterator& source_end);
	static lex_return_type lex_comment(const translation_unit& tu,
									   source_iterator& iter,
									   const source_iterator& source_end);
	
	//! checks whether a single character is part of an escape sequence (char after '\')
	static bool is_escape_sequence_char(const source_iterator& iter);
	
	//! checks whether a single character is part of the source character set
	static bool is_char_in_character_set(const source_iterator& iter);
	
	//! checks whether a single character is printable
	static bool is_printable_char(const source_iterator& iter);
	
	//! simple SOURCE_TOKEN_TYPE enum to C11 token name string conversion
	static constexpr const char* token_type_to_string(const SOURCE_TOKEN_TYPE& token_type) {
		switch(get_token_primary_type(token_type)) {
			case SOURCE_TOKEN_TYPE::KEYWORD: return "keyword";
			case SOURCE_TOKEN_TYPE::IDENTIFIER: return "identifier";
			case SOURCE_TOKEN_TYPE::CONSTANT: return "constant";
			case SOURCE_TOKEN_TYPE::STRING_LITERAL: return "string-literal";
			case SOURCE_TOKEN_TYPE::PUNCTUATOR: return "punctuator";
			default: break;
		}
		return "<invalid-token-type>";
	}
	
protected:
	// static class
	lexer(const lexer&) = delete;
	~lexer() = delete;
	lexer& operator=(const lexer&) = delete;
	
};

#endif
