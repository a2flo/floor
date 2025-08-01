/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#define FLOOR_JSON_EXTERN_TEMPLATE_INSTANTIATION 1
#include <floor/core/json.hpp>
#include <floor/core/unicode.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/core.hpp>
#include <floor/core/cpp_ext.hpp>
#include <cassert>

//#define FLOOR_DEBUG_PARSER 1
//#define FLOOR_DEBUG_PARSER_SET_NAMES 1
#include <floor/lang/source_types.hpp>
#include <floor/lang/lang_context.hpp>
#include <floor/lang/grammar.hpp>

namespace fl::json {
using namespace std::literals;

void json_value::print(std::ostream& stream, const uint32_t depth) const {
	std::visit([&stream, &depth](const auto& arg) {
		using value_type = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<value_type, std::nullptr_t>) {
			stream << "null";
		} else if constexpr (std::is_same_v<value_type, int64_t>) {
			stream << arg;
		} else if constexpr (std::is_same_v<value_type, double>) {
			stream << arg;
		} else if constexpr (std::is_same_v<value_type, std::string>) {
			stream << '\"' << arg << '\"';
		} else if constexpr (std::is_same_v<value_type, bool>) {
			stream << (arg ? "true" : "false");
		} else if constexpr (std::is_same_v<value_type, json_object>) {
			const std::string space_string((depth + 1) * 4, ' ');
			stream << "{" << std::endl;
			size_t i = 0, count = size(arg);
			for (const auto& entry : arg) {
				stream << space_string << '\"' << entry.first << "\": ";
				entry.second.print(stream, depth + 1);
				if (i < count - 1) {
					stream << ",";
				}
				stream << std::endl;
				++i;
			}
			stream << std::string(depth * 4, ' ') << "}";
		} else if constexpr (std::is_same_v<value_type, json_array>) {
			const std::string space_string((depth + 1) * 4, ' ');
			stream << "[" << std::endl;
			for (size_t i = 0, count = size(arg); i < count; ++i) {
				stream << space_string;
				arg[i].print(stream, depth + 1);
				if (i < count - 1) {
					stream << ",";
				}
				stream << std::endl;
			}
			stream << std::string(depth * 4, ' ') << "]";
		} else {
			instantiation_trap_dependent_type(value_type, "unhandled value type");
		}
	}, value);
	
	// one last newline if this is @ depth 0
	if (depth == 0) {
		stream << std::endl;
	}
}

class json_lexer final : public lexer {
public:
	static bool lex(translation_unit& tu);
	
	//! assigns the resp. FLOOR_KEYWORD and FLOOR_PUNCTUATOR enums/sub-type to the token type
	static void assign_token_sub_types(translation_unit& tu);
	
protected:
	static lex_return_type lex_keyword(const translation_unit& tu,
									   source_iterator& iter,
									   const source_iterator& source_end);
	static lex_return_type lex_decimal_constant(const translation_unit& tu,
												source_iterator& iter,
												const source_iterator& source_end);
	static lex_return_type lex_string_literal(const translation_unit& tu,
											  source_iterator& iter,
											  const source_iterator& source_end);
	static lex_return_type lex_escape_sequence(const translation_unit& tu,
											   source_iterator& iter,
											   const source_iterator& source_end);
	static lex_return_type lex_comment(const translation_unit& tu,
									   source_iterator& iter,
									   const source_iterator& source_end);
	
	// static class
	json_lexer(const json_lexer&) = delete;
	~json_lexer() = delete;
	json_lexer& operator=(const json_lexer&) = delete;
	
};

bool json_lexer::lex(translation_unit& tu) {
	// tokens reserve strategy: "4 chars : 1 token" seems like a good ratio for now
	tu.tokens.reserve(tu.source.size() / 4);
	
	// lex
	for(const char* src_begin = tu.source.data(), *src_end = tu.source.data() + tu.source.size(), *char_iter = src_begin;
		char_iter != src_end;
		/* NOTE: char_iter is incremented in the individual lex_* functions or whitespace case: */) {
		switch(*char_iter) {
			// keyword
			case 'n': case 't': case 'f': {
				source_range range { char_iter, char_iter };
				const auto ret = lex_keyword(tu, char_iter, src_end);
				if(!ret.first) return false;
				range.end = ret.second;
				tu.tokens.emplace_back(SOURCE_TOKEN_TYPE::IDENTIFIER, range);
				break;
			}
				
			// decimal constant
			// NOTE: json explicitly doesn't allow ".123", it must be "0.123"
			// NOTE: we will notice invalid things like "00123" when checking the grammar (these are two 0 constants and one 123 constant)
			case '-':
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
			case '8': case '9': {
				source_range range { char_iter, char_iter };
				const auto ret = lex_decimal_constant(tu, char_iter, src_end);
				if(!ret.first) return false;
				range.end = ret.second;
				tu.tokens.emplace_back(SOURCE_TOKEN_TYPE::CONSTANT, range);
				break;
			}
			
			// string literal
			case '\"': {
				source_range range { char_iter, char_iter };
				const auto ret = lex_string_literal(tu, char_iter, src_end);
				if(!ret.first) return false;
				range.end = ret.second;
				tu.tokens.emplace_back(SOURCE_TOKEN_TYPE::STRING_LITERAL, range);
				break;
			}
			
			// punctuator
			case '[': case ']': case '{': case '}':
			case ':': case ',': {
				tu.tokens.emplace_back(SOURCE_TOKEN_TYPE::PUNCTUATOR, source_range { char_iter, char_iter + 1 });
				++char_iter;
				break;
			}
			
			// '#' or '/' -> comment
			case '#': case '/': {
				// comment
				const auto ret = lex_comment(tu, char_iter, src_end);
				if(!ret.first) return false;
				break;
			}
				
			// whitespace
			// "space, horizontal tab, new-line"
			// NOTE: already handled/replaced \r with \n
			case ' ': case '\t': case '\n':
				// continue
				++char_iter;
				break;
				
			// invalid char
			default: {
				// extract 32-bit unicode char and check if this is valid and printable somehow
				const auto char_start_iter = char_iter;
				const auto utf32_char = unicode::decode_utf8_char(char_iter, src_end);
				std::string invalid_char;
				if(utf32_char.first) {
					// valid utf-8 char, can print it
					if(utf32_char.second <= 0x7F) {
						if(utf32_char.second >= 0x20 && utf32_char.second < 0x7F) {
							invalid_char = "'"s + (char)utf32_char.second + "' ";
						}
						// else: unprintable
						else invalid_char = "";
					}
					else {
						invalid_char = "'" + tu.source.substr(size_t(char_start_iter - src_begin),
															  size_t(char_iter - char_start_iter) + 1u) + "' ";
					}
					invalid_char += "<0d" + std::to_string(utf32_char.second) + ">";
				}
				else invalid_char = "<invalid utf-8 code point>";
				
				handle_error(tu, char_start_iter, "invalid character " + invalid_char);
				return false;
			}
		}
	}
	return true;
}

lexer::lex_return_type json_lexer::lex_keyword(const translation_unit& tu,
											   source_iterator& iter,
											   const source_iterator& source_end) {
	const auto check_len = [&tu, &iter, &source_end](const size_t& expected_len) -> lexer::lex_return_type {
		const auto end_iter = iter + (ssize_t)(expected_len - 1u);
		if(end_iter >= source_end) {
			return handle_error(tu, iter, "premature EOF while lexing keyword");
		}
		return { true, iter };
	};
	
	switch(*iter) {
		case 'n': { // null
			const auto len_ret = check_len(4);
			if (!len_ret.first) {
				floor_return_no_nrvo(len_ret);
			}
			
			static constexpr const char* error_msg { "invalid keyword - expected 'null'!" };
			if(*++iter != 'u') return handle_error(tu, iter, error_msg);
			if(*++iter != 'l') return handle_error(tu, iter, error_msg);
			if(*++iter != 'l') return handle_error(tu, iter, error_msg);
			break;
		}
		
		case 't': { // true
			const auto len_ret = check_len(4);
			if (!len_ret.first) {
				floor_return_no_nrvo(len_ret);
			}
			
			static constexpr const char* error_msg { "invalid keyword - expected 'true'!" };
			if(*++iter != 'r') return handle_error(tu, iter, error_msg);
			if(*++iter != 'u') return handle_error(tu, iter, error_msg);
			if(*++iter != 'e') return handle_error(tu, iter, error_msg);
			break;
		}
		
		case 'f': { // false
			const auto len_ret = check_len(5);
			if (!len_ret.first) {
				floor_return_no_nrvo(len_ret);
			}
			
			static constexpr const char* error_msg { "invalid keyword - expected 'false'!" };
			if(*++iter != 'a') return handle_error(tu, iter, error_msg);
			if(*++iter != 'l') return handle_error(tu, iter, error_msg);
			if(*++iter != 's') return handle_error(tu, iter, error_msg);
			if(*++iter != 'e') return handle_error(tu, iter, error_msg);
			break;
		}
		
		default: // already handled
			floor_unreachable();
	}
	return { true, ++iter };
}

lexer::lex_return_type json_lexer::lex_decimal_constant(const translation_unit& tu,
														source_iterator& iter,
														const source_iterator& source_end) {
	// lexes [0 - 9]+
	const auto lex_digit = [&tu, &iter, &source_end]() {
		size_t len = 0;
		for(bool lexed = false; iter != source_end; ++iter, ++len) {
			switch(*iter) {
				// valid decimal constant characters
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
				case '8': case '9':
					break;
					
				// anything else -> done
				default:
					lexed = true;
					break;
			}
			if(lexed) break;
		}
		
		// if len == 0, then this wasn't a digit
		if(len == 0) {
			handle_error(tu, iter, "expected a digit");
			return false;
		}
		return true;
	};
	
	// optional minus sign
	if(*iter == '-') {
		++iter;
	}
	
	// if '0' is the first digit, stop lexing "int" and continue with "frac exp"
	if(*iter == '0') {
		++iter;
	}
	else {
		// NOTE: this lexes the initial digit again
		if(!lex_digit()) {
			return { false, iter };
		}
	}
	
	// frac
	if(*iter == '.') {
		++iter;
		
		// frac digit(s)
		if(!lex_digit()) {
			return { false, iter };
		}
	}
	
	// exp
	if(*iter == 'e' || *iter == 'E') {
		++iter;
		
		// optional "minus / plus"
		if(*iter == '-' || *iter == '+') {
			++iter;
		}
		
		// exp digit(s)
		if(!lex_digit()) {
			return { false, iter };
		}
	}
	
	
	return { true, iter };
}

lexer::lex_return_type json_lexer::lex_string_literal(const translation_unit& tu,
													  source_iterator& iter,
													  const source_iterator& source_end) {
	for(++iter; iter != source_end; ++iter) {
		// handling of escape sequence in a string literal
		if(*iter == '\\') {
			++iter;
			if(iter == source_end) {
				// eof error
				break;
			}
			
			const auto es_ret = lex_escape_sequence(tu, iter, source_end);
			if(!es_ret.first) {
				return handle_error(tu, iter, "invalid escape sequence in string literal");
			}
		}
		// must be the end of the string literal
		else if(*iter == '\"') {
			return { true, ++iter };
		}
		// control characters are not allowed
		else if(uint8_t(*iter) < 0x20u) {
			return handle_error(tu, iter, "invalid control character inside string literal");
		}
		// else: must be a utf-8 char
		else {
			const auto start_iter = iter;
			const auto utf32_char = unicode::decode_utf8_char(iter, source_end);
			if(!utf32_char.first) {
				return handle_error(tu, start_iter, "invalid utf-8 code point inside string literal");
			}
			// else: all well, continue
		}
	}
	// error: eof before end of string literal
	return handle_error(tu, iter, "unterminated string literal (premature EOF)");
}

lexer::lex_return_type json_lexer::lex_escape_sequence(const translation_unit& tu,
													   source_iterator& iter,
													   const source_iterator& source_end) {
	switch(*iter) {
		// valid escape sequences
		case '\"':
		case '\\':
		case '/':
		case 'b':
		case 'f':
		case 'n':
		case 'r':
		case 't':
			return { true, iter };
		
		// \uXXXX
		case 'u': {
			auto hex_digit_iter = ++iter;
			for(size_t len = 0; len < 4; ++len, ++hex_digit_iter) {
				if(hex_digit_iter == source_end) {
					return handle_error(tu, iter, "premature EOF while lexing unicode escape sequence");
				}
				else if(!(*hex_digit_iter >= '0' && *hex_digit_iter <= '9') &&
						!(*hex_digit_iter >= 'A' && *hex_digit_iter <= 'F') &&
						!(*hex_digit_iter >= 'a' && *hex_digit_iter <= 'f')) {
					return handle_error(tu, iter, "invalid unicode escape sequence");
				}
				// else: all okay
			}
			iter += 3; // point to last byte
			return { true, iter };
		}
		
		// anything else: error
		default: break;
	}
	return { false, iter };
}

lexer::lex_return_type json_lexer::lex_comment(const translation_unit& tu,
											   source_iterator& iter,
											   const source_iterator& source_end) {
	// check if this is a single line comment
	bool is_single_line = true;
	if(*iter == '/') {
		const auto next_iter = iter + 1;
		if(next_iter != source_end) {
			if(*next_iter == '/') {
				// nop
			}
			else if(*next_iter == '*') {
				is_single_line = false;
			}
			else {
				return handle_error(tu, iter, "invalid '/' character - expected a comment?");
			}
			// advance to first comment char
			iter += 2;
		}
		else {
			return handle_error(tu, iter, "invalid '/' at EOF");
		}
	}
	// else: == '#' -> also single line
	else ++iter;
	
	if(is_single_line) {
		// single-line comment
		for(; iter != source_end; ++iter) {
			// newline signals end of single-line comment
			if(*iter == '\n') {
				return { true, iter };
			}
		}
		return { true, iter }; // eof is okay in a single-line comment
	}
	// multi-line comment
	else {
		for(; iter != source_end; ++iter) {
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

void json_lexer::assign_token_sub_types(translation_unit& tu) {
	//! contains all valid punctuators
	static const std::unordered_map<std::string, FLOOR_PUNCTUATOR> json_punctuator_tokens {
		{ "[", FLOOR_PUNCTUATOR::LEFT_BRACKET },
		{ "]", FLOOR_PUNCTUATOR::RIGHT_BRACKET },
		{ "{", FLOOR_PUNCTUATOR::LEFT_BRACE },
		{ "}", FLOOR_PUNCTUATOR::RIGHT_BRACE },
		{ ":", FLOOR_PUNCTUATOR::COLON },
		{ ",", FLOOR_PUNCTUATOR::COMMA },
	};

	for(auto& token : tu.tokens) {
		// skip non-punctuators
		if(token.first != SOURCE_TOKEN_TYPE::PUNCTUATOR) {
			continue;
		}
		
		// handle punctuators
		token.first |= (SOURCE_TOKEN_TYPE)json_punctuator_tokens.find(token.second.to_string())->second;
	}
}

struct json_keyword_matcher : public parser_node_base<json_keyword_matcher> {
	const char* keyword;
	const size_t keyword_len;
	
	template <size_t len> constexpr json_keyword_matcher(const char (&keyword_)[len]) noexcept :
	keyword(keyword_), keyword_len(len - 1 /* -1, b/c of \0 */) {}
	
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching KEYWORD");
#endif
		if(!ctx.at_end() &&
		   ctx.iter->first == SOURCE_TOKEN_TYPE::IDENTIFIER &&
		   ctx.iter->second.equal(keyword, keyword_len)) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};

struct json_grammar {
	// all grammar rule objects
#if !defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_SET_NAMES)
#define FLOOR_JSON_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__;
#else
#define FLOOR_JSON_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__; \
	/* in debug mode, also set the debug name of each grammar rule object */ \
	void set_debug_names() { \
		std::string names { #__VA_ARGS__ }; \
		set_debug_name(names, __VA_ARGS__); \
	} \
	void set_debug_name(std::string& names, grammar_rule& obj) { \
		const auto comma_pos = names.find(","); \
		obj.debug_name = names.substr(0, comma_pos); \
		names.erase(0, comma_pos + 1 + (comma_pos+1 < names.size() && names[comma_pos+1] == ' ' ? 1 : 0)); \
	} \
	template <typename... grammar_objects> void set_debug_name(std::string& names, grammar_rule& obj, grammar_objects&... objects) { \
		set_debug_name(names, obj); \
		set_debug_name(names, objects...); \
	}
#endif
	
	FLOOR_JSON_GRAMMAR_OBJECTS(json_text, value_matcher, object_matcher, array_matcher, member_list, member, element_list)
	
	// ast nodes used to build the final json document
	struct json_node : ast_node_base {
		enum class JSON_NODE_TYPE {
			INVALID,
			VALUE,
			MEMBER,
			OBJECT,
			ARRAY,
		} type;
		json_node(const JSON_NODE_TYPE& type_) : type(type_) {}
	};
	struct value_node : json_node {
		json_value value;
		value_node(json_value&& value_) : json_node(JSON_NODE_TYPE::VALUE), value(std::forward<json_value>(value_)) {}
		value_node(std::nullptr_t) : json_node(JSON_NODE_TYPE::VALUE), value(nullptr) {}
	};
	struct member_node : json_node {
		std::string name;
		std::shared_ptr<ast_node_base> value;
		member_node(std::string&& name_, std::shared_ptr<ast_node_base>&& value_) :
		json_node(JSON_NODE_TYPE::MEMBER), name(std::forward<std::string>(name_)), value(std::forward<std::shared_ptr<ast_node_base>>(value_)) {}
		member_node(std::nullptr_t) : json_node(JSON_NODE_TYPE::MEMBER), name("INVALID"), value(nullptr) {}
	};
	struct object_node : json_node {
		std::vector<std::shared_ptr<ast_node_base>> objects;
		object_node(std::vector<std::shared_ptr<ast_node_base>>&& objects_) : json_node(JSON_NODE_TYPE::OBJECT),
		objects(std::forward<std::vector<std::shared_ptr<ast_node_base>>>(objects_)) {}
		object_node(std::nullptr_t) : json_node(JSON_NODE_TYPE::OBJECT), objects() {}
	};
	struct array_node : json_node {
		std::vector<std::shared_ptr<ast_node_base>> values;
		array_node(std::vector<std::shared_ptr<ast_node_base>>&& values_) : json_node(JSON_NODE_TYPE::ARRAY),
		values(std::forward<std::vector<std::shared_ptr<ast_node_base>>>(values_)) {}
		array_node(std::nullptr_t) : json_node(JSON_NODE_TYPE::ARRAY), values() {}
	};
	
	//! current document that is being constructed
	document* doc { nullptr };
	
	json_grammar() {
		// fixed token type matchers:
		static constexpr literal_matcher<const char*, SOURCE_TOKEN_TYPE::CONSTANT> NUMBER {};
		static constexpr json_keyword_matcher
		VALUE_NULL { "null" },
		VALUE_TRUE { "true" },
		VALUE_FALSE { "false" };
		static constexpr literal_matcher<const char*, SOURCE_TOKEN_TYPE::STRING_LITERAL> STRING_LITERAL {};
		static constexpr literal_matcher<FLOOR_PUNCTUATOR, SOURCE_TOKEN_TYPE::PUNCTUATOR>
		LEFT_BRACKET { FLOOR_PUNCTUATOR::LEFT_BRACKET },
		RIGHT_BRACKET { FLOOR_PUNCTUATOR::RIGHT_BRACKET },
		LEFT_BRACE { FLOOR_PUNCTUATOR::LEFT_BRACE },
		RIGHT_BRACE { FLOOR_PUNCTUATOR::RIGHT_BRACE },
		COLON { FLOOR_PUNCTUATOR::COLON },
		COMMA { FLOOR_PUNCTUATOR::COMMA };
		
#if defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_SET_NAMES)
		set_debug_names();
#endif
		
		// grammar:
		// ref: https://tools.ietf.org/rfc/rfc7159.txt
		json_text = value_matcher;
		value_matcher = (VALUE_NULL | VALUE_TRUE | VALUE_FALSE | object_matcher | array_matcher | NUMBER | STRING_LITERAL);
		object_matcher = LEFT_BRACE & ~member_list & RIGHT_BRACE;
		member_list = member & *(COMMA & member);
		member = STRING_LITERAL & COLON & value_matcher;
		array_matcher = LEFT_BRACKET & ~element_list & RIGHT_BRACKET;
		element_list = value_matcher & *(COMMA & value_matcher);
		
		//! pushes/moves the even indexed matches to the parent matcher (#0, #2, #4, ...)
		const auto push_to_parent_even = [](auto& matches) -> parser_context::match_list {
			std::vector<parser_context::match> ret;
			for(size_t i = 0, count = matches.size(); i < count; i += 2) {
				ret.emplace_back(std::move(matches[i].ast_node));
			}
			return ret;
		};
		
		json_text.on_match([this](auto& matches) -> parser_context::match_list {
			// done when called
			if(matches.empty()) {
				log_error("no matches in json-text!");
				return {};
			}
			
			if(doc != nullptr) {
				doc->root = std::move(((value_node*)matches[0].ast_node.get())->value);
			}
			return {};
		});
		value_matcher.on_match([](auto& matches) -> parser_context::match_list {
			if(matches.empty()) {
				log_error("value match list should not be empty!");
				return { std::make_shared<value_node>(nullptr) };
			}
			else {
				if(matches[0].type == parser_context::MATCH_TYPE::TOKEN) {
					const auto& token = matches[0].token;
					const auto token_type = get_token_primary_type(token->first);
					switch(token_type) {
						case SOURCE_TOKEN_TYPE::CONSTANT: {
							const auto token_str = token->second.to_string();
							if (token_str.find('.') != std::string::npos ||
								token_str.find('e') != std::string::npos ||
								token_str.find('E') != std::string::npos) {
								// floating point value
								return { std::make_shared<value_node>(json_value { stod(token_str) }) };
							} else {
								// integer value
								return { std::make_shared<value_node>(json_value { int64_t(stoll(token_str)) }) };
							}
						}
						case SOURCE_TOKEN_TYPE::IDENTIFIER: {
							const auto token_str = token->second.to_string();
							if (token_str == "null") {
								return { std::make_shared<value_node>(json_value { nullptr }) };
							} else if (token_str == "true") {
								return { std::make_shared<value_node>(json_value { true }) };
							} else if (token_str == "false") {
								return { std::make_shared<value_node>(json_value { false }) };
							} else {
								log_error("invalid IDENTIFIER: $", token_str);
								return { std::make_shared<value_node>(nullptr) };
							}
						}
						case SOURCE_TOKEN_TYPE::STRING_LITERAL: {
							auto str = token->second.to_string();
							// remove " from front and back
							str.erase(str.begin());
							str.pop_back();
							return { std::make_shared<value_node>(json_value { std::move(str) }) };
						}
						default:
							log_error("invalid token type: $X!", token_type);
							return { std::make_shared<value_node>(nullptr) };
					}
				}
				else { // ast node
					auto jnode = (json_node*)matches[0].ast_node.get();
					switch(jnode->type) {
						case json_node::JSON_NODE_TYPE::OBJECT: {
							auto onode = (object_node*)jnode;
							json_object obj;
							for (auto& object : onode->objects) {
								obj.emplace(std::move(((member_node*)object.get())->name),
											std::move(((value_node*)((member_node*)object.get())->value.get())->value));
							}
							onode->objects.clear();
							return { std::make_shared<value_node>(json_value { std::move(obj) }) };
						}
						case json_node::JSON_NODE_TYPE::ARRAY: {
							auto anode = (array_node*)jnode;
							json_array arr;
							arr.reserve(anode->values.size());
							for (auto& elem : anode->values) {
								arr.emplace_back(std::move(((value_node*)elem.get())->value));
							}
							anode->values.clear();
							return { std::make_shared<value_node>(json_value { std::move(arr) }) };
						}
						case json_node::JSON_NODE_TYPE::MEMBER:
							log_error("value matched a MEMBER json node (not allowed)!");
							break;
						case json_node::JSON_NODE_TYPE::VALUE:
							log_error("value matched another VALUE json node (not allowed)!");
							break;
						case json_node::JSON_NODE_TYPE::INVALID:
							log_error("value matched INVALID json node!");
							break;
					}
				}
				return { std::make_shared<value_node>(nullptr) }; // in case of an error
			}
		});
		object_matcher.on_match([](auto& matches) -> parser_context::match_list {
			if(matches.size() >= 3) {
				// non-empty
				std::vector<std::shared_ptr<ast_node_base>> nodes;
				for(size_t i = 1, count = matches.size() - 1; i < count; ++i) {
					nodes.emplace_back(std::move(matches[i].ast_node));
				}
				return { std::make_shared<object_node>(std::move(nodes)) };
			}
			else if(matches.size() == 2) {
				// empty
				return { std::make_shared<object_node>(nullptr) };
			}
			log_error("invalid object match size: $!", matches.size());
			return { std::make_shared<object_node>(nullptr) };
		});
		member_list.on_match(push_to_parent_even);
		member.on_match([](auto& matches) -> parser_context::match_list {
			if(matches.size() == 3) {
				// remove " from front and back of key
				auto key = matches[0].token->second.to_string();
				key.erase(key.begin());
				key.pop_back();
				return { std::make_shared<member_node>(std::move(key), std::move(matches[2].ast_node)) };
			}
			log_error("invalid member match size: $!", matches.size());
			return { std::make_shared<member_node>(nullptr) };
		});
		array_matcher.on_match([](auto& matches) -> parser_context::match_list {
			if(matches.size() >= 3) {
				// non-empty
				std::vector<std::shared_ptr<ast_node_base>> nodes;
				for(size_t i = 1, count = matches.size() - 1; i < count; ++i) {
					nodes.emplace_back(std::move(matches[i].ast_node));
				}
				return { std::make_shared<array_node>(std::move(nodes)) };
			}
			else if(matches.size() == 2) {
				// empty
				return { std::make_shared<array_node>(nullptr) };
			}
			log_error("invalid array match size: $!", matches.size());
			return { std::make_shared<array_node>(nullptr) };
		});
		element_list.on_match(push_to_parent_even);
	}
	
	bool parse(parser_context& ctx, document& doc_) {
		// parse
		doc = &doc_;
		json_text.match(ctx);
		
		// if the end hasn't been reached, we have an error
		if(ctx.iter != ctx.end) {
			std::string error_msg = "parsing failed: ";
			if(ctx.deepest_iter == ctx.tu.tokens.cend()) {
				error_msg += "premature EOF after";
				ctx.deepest_iter = ctx.end - 1; // set iter to token before EOF
			}
			else {
				error_msg += "possibly at";
			}
			error_msg += " \"" + ctx.deepest_iter->second.to_string() + "\"";
			
			const auto line_and_column = json_lexer::get_line_and_column_from_iter(ctx.tu, ctx.deepest_iter->second.begin);
			log_error("$:$:$: $",
					  ctx.tu.file_name, line_and_column.first, line_and_column.second, error_msg);
			return false;
		}
		
		// done
		doc->valid = true;
		return true;
	}
};

document create_document(const std::string& filename) {
	std::string json_data;
	if(!file_io::file_to_string(filename, json_data)) {
		log_error("failed to read json file \"$\"!", filename);
		return {};
	}
	return create_document_from_string(json_data, filename);
}

document create_document_from_string(const std::string& json_data, const std::string identifier) {
	const auto is_valid_utf8 = unicode::validate_utf8_string(json_data);
	if(!is_valid_utf8.first) {
		log_error("JSON data \"$\" is not UTF-8 encoded or contains invalid UTF-8 code points!",
				  identifier);
		return {};
	}
	
	// create translation unit
	auto json_tu = std::make_unique<translation_unit>(identifier);
	json_tu->source.insert(0, json_data.c_str(), json_data.size());
	
	// lexing
	json_lexer::map_characters(*json_tu);
	if(!json_lexer::lex(*json_tu)) {
		log_error("lexing of JSON data \"$\" failed!", identifier);
		return {};
	}
	json_lexer::assign_token_sub_types(*json_tu);
	
	// parsing and document construction
	document doc;
	parser_context json_parser_ctx { *json_tu };
	json_grammar json_grammar_parser;
	if(!json_grammar_parser.parse(json_parser_ctx, doc)) {
		log_error("parsing of JSON data \"$\" failed!", identifier);
		return {};
	}
	floor_return_no_nrvo(doc);
}

template <typename T> static std::pair<bool, T> extract_value(const document& doc, const std::string& path) {
	// empty path -> return root value
	if(path.empty()) {
		const auto ret = doc.root.get<T>();
		if(!ret.first) log_error("specified type doesn't match type of value (root)!");
		return ret;
	}
	
	// check if root is actually an object that we can traverse
	if (doc.root.value.index() != (size_t)json_value::VALUE_TYPE::OBJECT) {
		log_error("root value is not an object!");
		return { false, T {} };
	}
	
	// tokenize input path, then traverse
	const auto path_stack = core::tokenize(path, '.');
	const json_value* cur_node = &doc.root;
	for (size_t i = 0, count = path_stack.size(); i < count; ++i) {
		const auto& key = path_stack[i];
		bool found = false;
		const auto object_ptr = std::get_if<json_object>(&cur_node->value);
		assert(object_ptr);
		if (!object_ptr) { // should never happen if we get here, but just in case ...
			return { false, T {} };
		}
		for (auto&& member : *object_ptr) {
			if(member.first == key) {
				// is leaf?
				if(i == count - 1) {
					const auto ret = member.second.get<T>();
					if(!ret.first) {
						log_error("type mismatch: value of \"$\" is not of the requested type!", path);
					}
					return ret;
				}
				// set to next child node
				cur_node = &member.second;
				found = true;
				break;
			}
		}
		
		// didn't find it -> abort
		if(!found) {
			return { false, T {} };
		}
		
		// check if child node is actually a json object
		if (cur_node->value.index() != (size_t)json_value::VALUE_TYPE::OBJECT) {
			log_error("found child node ($) is not a json object (path: $)!", key, path);
			return { false, T {} };
		}
	}
	return { false, T {} };
}

template<> std::string document::get<std::string>(const std::string& path, const std::string default_value) const {
	const auto ret = extract_value<std::string>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> float document::get<float>(const std::string& path, const float default_value) const {
	const auto ret = extract_value<float>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> double document::get<double>(const std::string& path, const double default_value) const {
	const auto ret = extract_value<double>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> uint64_t document::get<uint64_t>(const std::string& path, const uint64_t default_value) const {
	const auto ret = extract_value<uint64_t>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> int64_t document::get<int64_t>(const std::string& path, const int64_t default_value) const {
	const auto ret = extract_value<int64_t>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> uint32_t document::get<uint32_t>(const std::string& path, const uint32_t default_value) const {
	const auto ret = extract_value<uint32_t>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> int32_t document::get<int32_t>(const std::string& path, const int32_t default_value) const {
	const auto ret = extract_value<int32_t>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> bool document::get<bool>(const std::string& path, const bool default_value) const {
	const auto ret = extract_value<bool>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> json_object document::get<json_object>(const std::string& path, const json_object default_value) const {
	const auto ret = extract_value<json_object>(*this, path);
	return (ret.first ? ret.second : default_value);
}
template<> json_array document::get<std::vector<json_value>>(const std::string& path, const json_array default_value) const {
	const auto ret = extract_value<json_array>(*this, path);
	return (ret.first ? ret.second : default_value);
}

} // namespace fl::json
