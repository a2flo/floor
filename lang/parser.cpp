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

#include "lang/parser.hpp"

void parser::parse(lang_context& ctx, translation_unit& tu) {
	static const parser_ast c_parser;
	
	try {
		parser_context parser_ctx { tu };
		tu.ast = move( c_parser.parse(parser_ctx));
		
		// if the end wasn't reached, we have an error
		if(parser_ctx.iter != parser_ctx.end) {
			string error_msg = "parsing failed: ";
			if(parser_ctx.deepest_iter == tu.tokens.cend()) {
				error_msg += "premature EOF after";
				parser_ctx.deepest_iter = parser_ctx.end - 1; // set iter to token before EOF
			}
			else {
				error_msg += "possibly at";
			}
			error_msg += " \"" + parser_ctx.deepest_iter->second.to_string() + "\"";
			
			const auto line_and_column = lexer::get_line_and_column_from_iter(tu, parser_ctx.deepest_iter->second.begin);
			log_error("%s:%u:%u: error: %s",
					  tu.file_name, line_and_column.first, line_and_column.second, error_msg);
			tu.ast.reset();
		}
		else {
			check_semantic(ctx, tu);
		}
	}
	catch(semantic_error& exc) {
		const auto line_and_column = lexer::get_line_and_column_from_iter(tu, exc.get_iter()->second.begin);
		log_error("%s:%u:%u: semantic error: %s",
				  tu.file_name, line_and_column.first, line_and_column.second, exc.what());
		tu.ast = nullptr;
	}
	catch(exception& exc) {
		log_error("%s: error: %s", tu.file_name, exc.what());
		tu.ast = nullptr;
	}
	catch(...) {
		log_error("unknown parser error");
		tu.ast = nullptr;
	}
}

void parser::check_semantic(lang_context& ctx floor_unused, translation_unit& tu) {
	if(!tu.ast) return;
	
	// TODO: semantic analysis (post-parsing and AST construction)
	for(const auto& node floor_unused : as_node<list_node>(tu.ast)->nodes) {
		//node;
	}
}

parser_grammar::parser_grammar() {
	// all literal objects
	static constexpr literal_matcher<FLOOR_KEYWORD, TOKEN_TYPE::KEYWORD>
	AUTO { FLOOR_KEYWORD::AUTO }, BREAK { FLOOR_KEYWORD::BREAK }, CASE { FLOOR_KEYWORD::CASE }, CHAR { FLOOR_KEYWORD::CHAR },
	CONST { FLOOR_KEYWORD::CONST }, CONTINUE { FLOOR_KEYWORD::CONTINUE }, DEFAULT { FLOOR_KEYWORD::DEFAULT }, DO { FLOOR_KEYWORD::DO },
	DOUBLE { FLOOR_KEYWORD::DOUBLE }, ELSE { FLOOR_KEYWORD::ELSE }, ENUM { FLOOR_KEYWORD::ENUM }, EXTERN { FLOOR_KEYWORD::EXTERN },
	FLOAT { FLOOR_KEYWORD::FLOAT }, FOR { FLOOR_KEYWORD::FOR }, GOTO { FLOOR_KEYWORD::GOTO }, IF { FLOOR_KEYWORD::IF },
	INLINE { FLOOR_KEYWORD::INLINE }, INT { FLOOR_KEYWORD::INT }, LONG { FLOOR_KEYWORD::LONG }, REGISTER { FLOOR_KEYWORD::REGISTER },
	RESTRICT { FLOOR_KEYWORD::RESTRICT }, RETURN { FLOOR_KEYWORD::RETURN }, SHORT { FLOOR_KEYWORD::SHORT }, SIGNED { FLOOR_KEYWORD::SIGNED },
	SIZEOF { FLOOR_KEYWORD::SIZEOF }, STATIC { FLOOR_KEYWORD::STATIC }, STRUCT { FLOOR_KEYWORD::STRUCT }, SWITCH { FLOOR_KEYWORD::SWITCH },
	TYPEDEF { FLOOR_KEYWORD::TYPEDEF }, UNION { FLOOR_KEYWORD::UNION }, UNSIGNED { FLOOR_KEYWORD::UNSIGNED }, VOID { FLOOR_KEYWORD::VOID },
	VOLATILE { FLOOR_KEYWORD::VOLATILE }, WHILE { FLOOR_KEYWORD::WHILE }, ALIGNAS { FLOOR_KEYWORD::ALIGNAS }, ALIGNOF { FLOOR_KEYWORD::ALIGNOF },
	ATOMIC { FLOOR_KEYWORD::ATOMIC }, BOOL { FLOOR_KEYWORD::BOOL }, COMPLEX { FLOOR_KEYWORD::COMPLEX }, GENERIC { FLOOR_KEYWORD::GENERIC },
	IMAGINARY { FLOOR_KEYWORD::IMAGINARY }, NORETURN { FLOOR_KEYWORD::NORETURN }, STATIC_ASSERT { FLOOR_KEYWORD::STATIC_ASSERT },
	THREAD_LOCAL { FLOOR_KEYWORD::THREAD_LOCAL };
	
	static constexpr literal_matcher<FLOOR_PUNCTUATOR, TOKEN_TYPE::PUNCTUATOR>
	LEFT_BRACKET { FLOOR_PUNCTUATOR::LEFT_BRACKET }, RIGHT_BRACKET { FLOOR_PUNCTUATOR::RIGHT_BRACKET }, LEFT_PAREN { FLOOR_PUNCTUATOR::LEFT_PAREN },
	RIGHT_PAREN { FLOOR_PUNCTUATOR::RIGHT_PAREN }, LEFT_BRACE { FLOOR_PUNCTUATOR::LEFT_BRACE }, RIGHT_BRACE { FLOOR_PUNCTUATOR::RIGHT_BRACE },
	DOT { FLOOR_PUNCTUATOR::DOT }, ARROW { FLOOR_PUNCTUATOR::ARROW }, INCREMENT { FLOOR_PUNCTUATOR::INCREMENT },
	DECREMENT { FLOOR_PUNCTUATOR::DECREMENT }, BIT_AND { FLOOR_PUNCTUATOR::AND }, ASTERISK { FLOOR_PUNCTUATOR::ASTERISK },
	PLUS { FLOOR_PUNCTUATOR::PLUS }, MINUS { FLOOR_PUNCTUATOR::MINUS }, TILDE { FLOOR_PUNCTUATOR::TILDE }, NOT { FLOOR_PUNCTUATOR::NOT },
	DIV { FLOOR_PUNCTUATOR::DIV }, MODULO { FLOOR_PUNCTUATOR::MODULO }, LEFT_SHIFT { FLOOR_PUNCTUATOR::LEFT_SHIFT },
	RIGHT_SHIFT { FLOOR_PUNCTUATOR::RIGHT_SHIFT }, LESS_THAN { FLOOR_PUNCTUATOR::LESS_THAN }, GREATER_THAN { FLOOR_PUNCTUATOR::GREATER_THAN },
	LESS_OR_EQUAL { FLOOR_PUNCTUATOR::LESS_OR_EQUAL }, GREATER_OR_EQUAL { FLOOR_PUNCTUATOR::GREATER_OR_EQUAL },
	LOGIC_EQUAL { FLOOR_PUNCTUATOR::EQUAL }, LOGIC_UNEQUAL { FLOOR_PUNCTUATOR::UNEQUAL }, BIT_XOR { FLOOR_PUNCTUATOR::XOR },
	BIT_OR { FLOOR_PUNCTUATOR::OR }, LOGIC_AND { FLOOR_PUNCTUATOR::LOGIC_AND }, LOGIC_OR { FLOOR_PUNCTUATOR::LOGIC_OR },
	TERNARY { FLOOR_PUNCTUATOR::TERNARY }, COLON { FLOOR_PUNCTUATOR::COLON }, SEMICOLON { FLOOR_PUNCTUATOR::SEMICOLON },
	ELLIPSIS { FLOOR_PUNCTUATOR::ELLIPSIS }, ASSIGN { FLOOR_PUNCTUATOR::ASSIGN }, MUL_ASSIGN { FLOOR_PUNCTUATOR::MUL_ASSIGN },
	DIV_ASSIGN { FLOOR_PUNCTUATOR::DIV_ASSIGN }, MODULE_ASSIGN { FLOOR_PUNCTUATOR::MODULE_ASSIGN }, ADD_ASSIGN { FLOOR_PUNCTUATOR::ADD_ASSIGN },
	SUB_ASSIGN { FLOOR_PUNCTUATOR::SUB_ASSIGN }, LEFT_SHIFT_ASSIGN { FLOOR_PUNCTUATOR::LEFT_SHIFT_ASSIGN },
	RIGHT_SHIFT_ASSIGN { FLOOR_PUNCTUATOR::RIGHT_SHIFT_ASSIGN }, AND_ASSIGN { FLOOR_PUNCTUATOR::AND_ASSIGN },
	XOR_ASSIGN { FLOOR_PUNCTUATOR::XOR_ASSIGN }, OR_ASSIGN { FLOOR_PUNCTUATOR::OR_ASSIGN }, COMMA { FLOOR_PUNCTUATOR::COMMA },
	HASH { FLOOR_PUNCTUATOR::HASH }, HASH_HASH { FLOOR_PUNCTUATOR::HASH_HASH };
	
	// fixed token type matchers
	static constexpr literal_matcher<const char*, TOKEN_TYPE::IDENTIFIER> IDENTIFIER {};
	static constexpr literal_matcher<const char*, TOKEN_TYPE::STRING_LITERAL> STRING_LITERAL {};
	static constexpr literal_matcher<const char*, TOKEN_TYPE::CONSTANT> CONSTANT {};
	static constexpr epsilon_matcher EPSILON {};
	
	//
#if defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_SET_NAMES)
	set_debug_names();
#endif
	
	// 6.5 expressions
	primary_expression = IDENTIFIER | CONSTANT | STRING_LITERAL | LEFT_PAREN & expression & RIGHT_PAREN;
	postfix_expression = primary_expression & postfix_expression_tail;
	postfix_expression_tail = ((LEFT_BRACKET & expression & RIGHT_BRACKET
								| LEFT_PAREN & ~argument_expression_list & RIGHT_PAREN
								| DOT & IDENTIFIER
								| ARROW & IDENTIFIER
								| INCREMENT
								| DECREMENT) & postfix_expression_tail
							   | EPSILON);
	unary_expression = (postfix_expression
						| (INCREMENT | DECREMENT) & unary_expression
						| (BIT_AND | ASTERISK | MINUS | NOT) & cast_expression
						| SIZEOF & (unary_expression | LEFT_PAREN & type_name & RIGHT_PAREN));
	cast_expression = unary_expression | LEFT_PAREN & type_name & RIGHT_PAREN & cast_expression;
	multiplicative_expression = cast_expression & *((ASTERISK | DIV | MODULO) & cast_expression);
	additive_expression = multiplicative_expression & *((PLUS | MINUS) & multiplicative_expression);
	relational_expression = additive_expression & *((LESS_THAN | GREATER_THAN) & additive_expression);
	equality_expression = relational_expression & *((LOGIC_EQUAL | LOGIC_UNEQUAL) & relational_expression);
	and_expression = equality_expression & *(BIT_AND & equality_expression);
	exclusive_or_expression = and_expression & *(BIT_XOR & and_expression);
	inclusive_or_expression = exclusive_or_expression & *(BIT_OR & exclusive_or_expression);
	logical_and_expression = inclusive_or_expression & *(LOGIC_AND & inclusive_or_expression);
	logical_or_expression  = logical_and_expression & *(LOGIC_OR & logical_and_expression);
	assignment_expression = unary_expression & ASSIGN & assignment_expression | conditional_expression;
	conditional_expression = logical_or_expression & *(TERNARY & expression & COLON & logical_or_expression);
	argument_expression_list = assignment_expression & *(COMMA & assignment_expression);
	expression = assignment_expression & *(COMMA & assignment_expression);
	
	// 6.7 declarations
	declaration = declaration_specifiers & ~declarator & SEMICOLON;
	declaration_specifiers = +type_specifier;
	specifier_qualifier_list = +(type_specifier | type_qualifier);
	type_specifier = VOID | CHAR | INT | struct_or_union_specifier;
	type_qualifier = CONST | RESTRICT | VOLATILE | ATOMIC;
	type_qualifier_list = +type_qualifier;
	struct_or_union_specifier = ((STRUCT | UNION)
								 & (~IDENTIFIER & LEFT_BRACE
									& struct_declaration_list
									& RIGHT_BRACE | IDENTIFIER));
	struct_declaration_list = +struct_declaration;
	struct_declaration = specifier_qualifier_list & ~struct_declarator_list & SEMICOLON;
	struct_declarator_list = declarator & *(COMMA & declarator);
	declarator = ~pointer & direct_declarator;
	direct_declarator = (IDENTIFIER | LEFT_PAREN & declarator & RIGHT_PAREN) & direct_declarator_tail;
	direct_declarator_tail = LEFT_PAREN & parameter_type_list & RIGHT_PAREN & direct_declarator_tail | EPSILON;
	pointer = +ASTERISK;
	parameter_type_list = parameter_declaration & *(COMMA & parameter_declaration);
	parameter_declaration = declaration_specifiers & (declarator | ~abstract_declarator);
	type_name = specifier_qualifier_list & ~abstract_declarator;
	abstract_declarator = pointer | ~pointer & direct_abstract_declarator;
	direct_abstract_declarator = ((LEFT_PAREN & (abstract_declarator | ~parameter_type_list) & RIGHT_PAREN)
								  & direct_abstract_declarator_tail);
	direct_abstract_declarator_tail = ((LEFT_PAREN & ~parameter_type_list & RIGHT_PAREN)
									   & direct_abstract_declarator_tail | EPSILON);
	
	// 6.8 statements
	statement = (labeled_statement | compound_statement | expression_statement
				 | selection_statement | iteration_statement | jump_statement);
	labeled_statement = IDENTIFIER & COLON & statement;
	compound_statement = LEFT_BRACE & ~block_item_list & RIGHT_BRACE;
	block_item_list = +(declaration | statement);
	expression_statement = ~expression & SEMICOLON;
	selection_statement = IF & LEFT_PAREN & expression & RIGHT_PAREN & statement & ~(ELSE & statement);
	iteration_statement = (WHILE & LEFT_PAREN & expression & RIGHT_PAREN & statement
						   | DO & statement & WHILE & LEFT_PAREN & expression & RIGHT_PAREN & SEMICOLON);
	jump_statement = (GOTO & IDENTIFIER | CONTINUE | BREAK | RETURN & ~expression) & SEMICOLON;
	
	// 6.9 external definitions
	translation_unit = *(function_definition | declaration);
	function_definition = declaration_specifiers & declarator & compound_statement;
}


const char* semantic_error::what() const noexcept {
	return error_msg.c_str();
}
const token_iterator& semantic_error::get_iter() const noexcept {
	return iter;
}

parser_ast::parser_ast() : parser_grammar() {
}

unique_ptr<node_base> parser_ast::parse(parser_context& ctx) const {
	auto tu_match = translation_unit.match(ctx);
	return (tu_match.matches.size() == 0 ? // match size might be 0 if this is an empty file or in error cases
			nullptr : move(tu_match.matches[0].ast_node));
}
