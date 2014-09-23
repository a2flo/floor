/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_PARSER_HPP__
#define __FLOOR_PARSER_HPP__

#include <string>
#include <floor/core/util.hpp>
#include <floor/lang/source_types.hpp>
#include <floor/lang/lang_context.hpp>
#include <floor/lang/grammar.hpp>
using namespace std;

class parser {
public:
	//! parse the specified translation unit and construct the AST
	static void parse(lang_context& ctx, translation_unit& tu);
	
protected:
	// static class
	parser(const parser&) = delete;
	~parser() = delete;
	parser& operator=(const parser&) = delete;
	
	//! checks the semantic of an already constructed AST
	static void check_semantic(lang_context& ctx, translation_unit& tu);
	
};

//! c grammar definition: for internal use only
class parser_grammar {
protected:
	// all grammar rule objects
#if !defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_SET_NAMES)
#define FLOOR_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__;
#else
#define FLOOR_GRAMMAR_OBJECTS(...) grammar_rule __VA_ARGS__; \
	/* in debug mode, also set the debug name of each grammar rule object */ \
	void set_debug_names() { \
		string names { #__VA_ARGS__ }; \
		set_debug_name(names, __VA_ARGS__); \
	} \
	void set_debug_name(string& names, grammar_rule& obj) { \
		const auto comma_pos = names.find(","); \
		obj.debug_name = names.substr(0, comma_pos); \
		names.erase(0, comma_pos + 1 + (comma_pos+1 < names.size() && names[comma_pos+1] == ' ' ? 1 : 0)); \
	} \
	template <typename... grammar_objects> void set_debug_name(string& names, grammar_rule& obj, grammar_objects&... objects) { \
		set_debug_name(names, obj); \
		set_debug_name(names, objects...); \
	}
#endif
	
	FLOOR_GRAMMAR_OBJECTS(/* 6.5 */
						  assignment_expression, conditional_expression, primary_expression,
						  expression, postfix_expression, postfix_expression_tail, argument_expression_list,
						  unary_expression, cast_expression, multiplicative_expression, additive_expression,
						  relational_expression, equality_expression, and_expression, exclusive_or_expression,
						  inclusive_or_expression, logical_and_expression, logical_or_expression,
						  /* 6.7 */
						  declaration, declaration_specifiers, declarator, type_specifier,
						  struct_or_union_specifier, struct_declaration_list, struct_declaration, specifier_qualifier_list,
						  struct_declarator_list, type_qualifier, direct_declarator, pointer,
						  type_qualifier_list, parameter_type_list, parameter_declaration, type_name,
						  abstract_declarator, direct_abstract_declarator, direct_declarator_tail, direct_abstract_declarator_tail,
						  /* 6.8 */
						  statement, labeled_statement, compound_statement, expression_statement,
						  selection_statement, iteration_statement, jump_statement, block_item_list,
						  /* 6.9 */
						  translation_unit, function_definition)
	
	parser_grammar();
	
};

//! semantic_error exception
class semantic_error : public exception {
protected:
	token_iterator iter;
	string error_msg;
public:
	semantic_error(token_iterator iter_, const string& error_msg_) : iter(iter_), error_msg(error_msg_) {}
	virtual const char* what() const noexcept override;
	const token_iterator& get_iter() const noexcept;
};

class parser_ast : public parser_grammar {
public:
	//!
	parser_ast();
	
	//! parse, according to the specified grammar and using the specified parser_context (+translation unit)
	unique_ptr<node_base> parse(parser_context& ctx) const;
	
};

#endif
