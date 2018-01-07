/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#include <floor/lang/lexer.hpp>
#include <iostream>
#include <unordered_set>
#include <cstdio>
#include <floor/core/logger.hpp>

void lexer::map_characters(translation_unit& tu) {
	// -> we will only need to remove \r characters here (replace \r\n by \n and replace single \r chars by \n)
	// also, while we're at it, also build a "lines set" (iterator to each line)
	// TODO: \r actually doesn't have to be replaced (just skip like \n + don't count as newline if \r\n?)
	vector<uint32_t> lines; // NOTE: we can't store iterators yet, so this must be a uint
	for(auto begin_iter = begin(tu.source), end_iter = end(tu.source), iter = begin_iter; iter != end_iter; ++iter) {
		if(*iter == '\n' || *iter == '\r') {
			if(*iter == '\r') {
				auto next_iter = iter + 1;
				if(next_iter != end_iter && *next_iter == '\n') {
					// replace \r\n with single \n (erase \r)
					iter = tu.source.erase(iter); // iter now at '\n'
					// we now have a new end and begin iter
					end_iter = end(tu.source);
					begin_iter = begin(tu.source);
				}
				else {
					// single \r -> \n replace
					*iter = '\n';
				}
			}
			// else: \n
			
			// add newline position
			lines.emplace_back(distance(begin_iter, iter));
		}
	}
	
	// TODO: don't use a set, rather use a vector (it is already sorted!) and binary_search
	// now that we can store iterators, do so (+store it in an actual set<>)
	const auto begin_iter = tu.source.data();
	// add the "character before the first character" as a newline
	// (yes, kind of a hack, but this keeps us from doing +1s/-1s and begin/end checking later on)
	tu.lines.insert(begin_iter - 1);
	// add all newline iterators (offset from the begin iterator)
	for(const auto& line_offset : lines) {
		tu.lines.insert(begin_iter + (int32_t)line_offset);
	}
	// also insert the "<eof> newline" (if it hasn't been added already)
	tu.lines.insert(tu.source.data() + tu.source.size());
	// NOTE: the additional begin+end newline iterators will make sure that there will always be
	// a valid line iterator for each source_iterator (all tokens)
}

lexer::lex_return_type lexer::handle_error(const translation_unit& tu, const source_iterator& iter, const string& error_msg) {
	// print the error (<file>:<line>:<column>: error: <error-text>)
	const auto line_and_column = get_line_and_column_from_iter(tu, iter);
	log_error("%s:%u:%u: error: %s",
			  tu.file_name, line_and_column.first, line_and_column.second, error_msg);
	
	// print erroneous line if possible
	if(line_and_column.first != 0) {
		// find and extract the correct line (without the \n)
		const auto lines_begin = tu.lines.cbegin();
		const auto line_end = tu.lines.lower_bound(iter);
		const auto line_start = prev(line_end);
		const string line_str = tu.source.substr(size_t(distance(*lines_begin, *line_start)), size_t(*line_end - *line_start) - 1u);
		log_undecorated("%s", line_str);
		
		// print '^' at erroneous character if possible
		if(line_and_column.second != 0) {
			const string caret_string(line_and_column.second - 1, ' ');
			log_undecorated("%s^", caret_string);
		}
	}
	
	return { false, {} };
}

void lexer::print_tokens(const translation_unit& tu) {
	// NOTE: for the sake of speed, this uses fwrite instead of cout and string voodoo ...
	// also: doesn't use get_line_and_column_from_iter(...), because this would mean quadratic runtime
	// instead: just keep track of the newline iters and use a line counter
	string tmp;
	tmp.reserve(256);
	tmp = tu.file_name + ":";
	const auto insert_loc = tmp.size();
	
	const auto lines_begin = tu.lines.cbegin();
	auto cur_line = lines_begin, next_line = next(lines_begin);
	uint32_t line_num = 1;
	
	for(const auto& token : tu.tokens) {
		// <file>:<line>:<column>: <token-type> <token-text>
		tmp.erase(insert_loc);
		
		// current token iter
		const auto& tok_begin = token.second.begin;
		
		// get/update current and next line iter and line number
		while(tok_begin >= *next_line) {
			cur_line = next_line;
			++next_line;
			++line_num;
		}
		
		// compute column num (distance between last newline and current token)
		const uint32_t column_num = (uint32_t)distance(*cur_line, tok_begin);
		
		tmp.append(to_string(line_num));
		tmp.append(":");
		tmp.append(to_string(column_num));
		tmp.append(": ");
		tmp.append(token_type_to_string(token.first));
		tmp.append(" ");
		tmp.append(token.second.begin, token.second.end);
		tmp.append("\n");
		fwrite(tmp.data(), 1, tmp.size(), stdout);
	}
}

lexer::lex_return_type lexer::lex_keyword_or_identifier(const translation_unit&, source_iterator& iter, const source_iterator& source_end) {
	for(++iter; iter != source_end; ++iter) {
		switch(*iter) {
			// valid keyword and identifier characters
			case '_':
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9':
				continue;
				
			// anything else -> done, return end iter
			default:
				return { true, iter };
		}
	}
	return { true, iter }; // eof
}

lexer::lex_return_type lexer::lex_decimal_constant(const translation_unit&, source_iterator& iter, const source_iterator& source_end) {
	for(++iter; iter != source_end; ++iter) {
		switch(*iter) {
			// valid decimal constant characters
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9':
				continue;
				
			// anything else -> done, return end iter
			default:
				return { true, iter };
		}
	}
	return { true, iter }; // eof
}

lexer::lex_return_type lexer::lex_character_constant(const translation_unit& tu, source_iterator& iter, const source_iterator& source_end) {
	++iter; // opening '
	if(iter == source_end) {
		return handle_error(tu, iter, "unterminated character-constant (premature EOF)");
	}
	
	// escape sequence
	if(*iter == '\\') {
		++iter;
		if(iter == source_end) {
			return handle_error(tu, iter, "unterminated character-constant (premature EOF)");
		}
		
		if(!is_escape_sequence_char(iter)) {
			const string invalid_seq = (is_printable_char(iter) ? string(1, *iter) : "<"+to_string(0xFFu & (uint32_t)*iter)+">");
			return handle_error(tu, iter, "invalid escape sequence \'\\" + invalid_seq + "\' in character-constant");
		}
	}
	// error: no newline allowed
	else if(*iter == '\n') {
		return handle_error(tu, iter, "invalid new-line inside character-constant");
	}
	// error: no ' allowed (char constant must be non-empty)
	else if(*iter == '\'') {
		return handle_error(tu, iter, "character-constant must be non-empty");
	}
	// error, invalid character (not in the source character set)
	else if(!is_char_in_character_set(iter)) {
		return handle_error(tu, iter, "invalid character inside character-constant (not in the source character set)");
	}
	// else: everything is fine
	
	++iter; // must be closing '
	if(iter == source_end) {
		return handle_error(tu, iter, "unterminated character-constant (premature EOF)");
	}
	if(*iter != '\'') {
		// error: only single-character constants allowed (or no terminator)
		return handle_error(tu, iter, "only single-character constants are allowed (or missing terminator)");
	}
	
	// done
	return { true, ++iter };
}

lexer::lex_return_type lexer::lex_string_literal(const translation_unit& tu, source_iterator& iter, const source_iterator& source_end) {
	for(++iter; iter != source_end; ++iter) {
		// handling of '\"' escape sequence in a string literal
		if(*iter == '\\') {
			++iter;
			if(iter == source_end) {
				// eof error
				break;
			}
			
			if(!is_escape_sequence_char(iter)) {
				const string invalid_seq = (is_printable_char(iter) ? string(1, *iter) : "<"+to_string(0xFFu & (uint32_t)*iter)+">");
				return handle_error(tu, iter, "invalid escape sequence \'\\" + invalid_seq + "\' in string literal");
			}
		}
		// must be the end of the string literal
		else if(*iter == '\"') {
			++iter;
			return { true, iter };
		}
		// newlines are not allowed
		else if(*iter == '\n') {
			return handle_error(tu, iter, "invalid new-line inside string literal");
		}
		// error, invalid character (not in the source character set)
		else if(!is_char_in_character_set(iter)) {
			return handle_error(tu, iter, "invalid character inside string literal (not in the source character set)");
		}
		// else: continue
	}
	// error: eof before end of string literal
	return handle_error(tu, iter, "unterminated string literal (premature EOF)");
}

lexer::lex_return_type lexer::lex_punctuator(const translation_unit&, source_iterator& iter, const source_iterator& source_end) {
	// used to check for double-char punctuators (will probably/hopefully be inlined)
	const auto check_double_char_punctuator = [&iter, &source_end](initializer_list<const char> chars) -> source_iterator {
		auto next_iter = (iter + 1);
		if(next_iter == source_end) {
			return ++iter; // eof
		}
		// check all possible double-character punctuators (i.e. check the second character)
		for(const auto& ch : chars) {
			if(*next_iter == ch) {
				// found the second char -> double-character punctuator
				iter += 2;
				return iter;
			}
		}
		// if nothing was found, this is a single-character punctuator
		return ++iter;
	};
	
	//
	switch(*iter) {
		// single-character punctuators
		case '[': case ']': case '(': case ')':
		case '{': case '}': case '~': case '?':
		case ';': case ',':
			return { true, ++iter };
		
		// ellipses or dot punctuator
		case '.': {
			auto p1_iter = (iter + 1);
			auto p2_iter = (iter + 2);
			if(p1_iter == source_end || p2_iter == source_end ||
			   *p1_iter != '.' || *p2_iter != '.') {
				// eof or no .. found
				return { true, ++iter };
			}
			// ellipses found
			iter += 3;
			return { true, iter };
		}
		
		// <: << <<= <= <: <%
		case '<': {
			auto p1_iter = (iter + 1);
			auto p2_iter = (iter + 2);
			if(p1_iter == source_end) {
				return { true, ++iter }; // eof, single-char
			}
			if(*p1_iter == '<') {
				if(p2_iter == source_end || *p2_iter != '=') {
					// <<, double-char
					iter += 2;
					return { true, iter };
				}
				// <<=, triple-char
				iter += 3;
				return { true, iter };
			}
			else if(*p1_iter == '=' || *p1_iter == ':' || *p1_iter == '%') {
				// <= <: <%, double-char
				iter += 2;
				return { true, iter };
			}
			// single-char
			return { true, ++iter };
		}
		
		// >: >> >>= >=
		case '>': {
			// NOTE: almost copy/paste, but there's no point putting this into a function
			auto p1_iter = (iter + 1);
			auto p2_iter = (iter + 2);
			if(p1_iter == source_end) {
				return { true, ++iter }; // eof, single-char
			}
			if(*p1_iter == '>') {
				if(p2_iter == source_end || *p2_iter != '=') {
					// >>, double-char
					iter += 2;
					return { true, iter };
				}
				// >>=, triple-char
				iter += 3;
				return { true, iter };
			}
			else if(*p1_iter == '=') {
				// <=, double-char
				iter += 2;
				return { true, iter };
			}
			// single-char
			return { true, ++iter };
		}
			
		// %: %= %> %: %:%:
		case '%': {
			auto next_iter = (iter + 1);
			if(next_iter == source_end) {
				return { true, ++iter }; // eof
			}
			else if(*next_iter == ':') {
				auto p2_iter = (iter + 2);
				auto p3_iter = (iter + 3);
				if(p2_iter == source_end || p3_iter == source_end ||
				   *p2_iter != '%' || *p3_iter != ':') {
					// %:, double-char
					iter += 2;
					return { true, iter };
				}
				// %:%:, quadruple-char
				iter += 4;
				return { true, iter };
			}
			else if(*next_iter == '=' || *next_iter == '>') {
				// %= %>, double-char
				iter += 2;
				return { true, iter };
			}
			// single-char
			return { true, ++iter };
		}
		
		// possible double-character punctuators:
		// -: -- -> -=
		case '-': return { true, check_double_char_punctuator({'-', '>', '='}) };
		// &: && &=
		case '&': return { true, check_double_char_punctuator({'&', '='}) };
		// *: *=
		case '*': return { true, check_double_char_punctuator({'='}) };
		// +: ++ +=
		case '+': return { true, check_double_char_punctuator({'+', '='}) };
		// !: !=
		case '!': return { true, check_double_char_punctuator({'='}) };
		// =: ==
		case '=': return { true, check_double_char_punctuator({'='}) };
		// ^: ^=
		case '^': return { true, check_double_char_punctuator({'='}) };
		// |: || |=
		case '|': return { true, check_double_char_punctuator({'|', '='}) };
		// :: :>
		case ':': return { true, check_double_char_punctuator({'>'}) };
		// #: ##
		case '#': return { true, check_double_char_punctuator({'#'}) };
		// /: /=
		case '/': return { true, check_double_char_punctuator({'='}) };
		
		default: break;
	}
	floor_unreachable();
}

lexer::lex_return_type lexer::lex_comment(const translation_unit& tu, source_iterator& iter, const source_iterator& source_end) {
	// NOTE: we already made sure that this must be a comment and the next character is a '/' or '*'
	++iter;
	
	// single-line comment
	if(*iter == '/') {
		for(++iter; iter != source_end; ++iter) {
			// newline signals end of single-line comment
			if(*iter == '\n') {
				return { true, iter };
			}
			// NOTE: as translation phase 2 is omitted, a single '\' character followed by a newline
			// does _not_ signal a "multi-line" single-line comment
		}
		return { true, iter }; // eof is okay in a single-line comment
	}
	// multi-line comment
	else /* (*iter == '*') */ {
		for(++iter; iter != source_end; ++iter) {
			if(*iter == '*') {
				++iter;
				if(iter == source_end) {
					// eof error
					break;
				}
				
				// */ signals end of multi-line comment
				if(*iter == '/') {
					return { true, ++iter };
				}
			}
		}
	}
	
	// error: eof inside comment
	return handle_error(tu, iter, "unterminated /* comment (premature EOF)");
}

bool lexer::is_escape_sequence_char(const source_iterator& iter) {
	switch(*iter) {
		// valid escape sequences
		case '\'': case '\"': case '?': case '\\':
		case 'a': case 'b': case 'f': case 'n':
		case 'r': case 't': case 'v':
			return true;
		// anything else: error
		default: break;
	}
	return false;
}

bool lexer::is_char_in_character_set(const source_iterator& iter) {
	// http://en.wikipedia.org/wiki/ISO/IEC_8859-1#Codepage_layout
	// -> valid characters are in the ranges [0x20, 0x7E] and [0xA0, 0xFF]
	// -> valid control characters are everything in [0x00, 0x1F] except 0x00, 0x0A and 0x0D (terminator, newline, carriage return)
	// NOTE: don't use characters here, since the encoding of *this file is utf-8, not iso-8859-1!
	const uint8_t char_uint = (uint8_t)*iter;
	return ((char_uint >= 0x01 && char_uint <= 0x09) ||
			(char_uint >= 0x0B && char_uint <= 0x0C) ||
			(char_uint >= 0x0E && char_uint <= 0x7E) ||
			(char_uint >= 0xA0 /* && <= 0xFF, always true */));
}

bool lexer::is_printable_char(const source_iterator& iter) {
	// -> see lexer::is_char_in_character_set
	// this will only accept 0x09 (tab), [0x20, 0x7E] and [0xA0, 0xFF]
	const uint8_t char_uint = (uint8_t)*iter;
	return ((char_uint == 0x09) ||
			(char_uint >= 0x20 && char_uint <= 0x7E) ||
			(char_uint >= 0xA0 /* && <= 0xFF, always true */));
}

pair<uint32_t, uint32_t> lexer::get_line_and_column_from_iter(const translation_unit& tu,
															  const source_iterator& iter) {
	// eof check
	if(iter == *tu.lines.cend()) {
		return { 0, 0 };
	}
	
	pair<uint32_t, uint32_t> ret;
	const auto lines_begin = tu.lines.cbegin();
	
	// line num
	const auto line_iter = tu.lines.lower_bound(iter);
	ret.first = (uint32_t)distance(lines_begin, line_iter);
	
	// column num
	const auto prev_newline = *prev(line_iter);
	ret.second = (uint32_t)distance(prev_newline, iter);
	
	return ret;
}
