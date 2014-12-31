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

#include <floor/lang/ast_base.hpp>
#include <algorithm>

// misc functions
node_base::NODE_TYPE node_base::get_type() const {
	return ((NODE_TYPE)type) & NODE_TYPE::__PRIMARY_TYPE_MASK;
}

list_node::LIST_TYPE list_node::get_type() const {
	return ((LIST_TYPE)type) & LIST_TYPE::__LIST_TYPE_MASK;
}

unary_node::UNARY_TYPE unary_node::get_type() const {
	return ((UNARY_TYPE)type) & UNARY_TYPE::__UNARY_TYPE_MASK;
}

postfix_node::POSTFIX_TYPE postfix_node::get_type() const {
	return ((POSTFIX_TYPE)type) & POSTFIX_TYPE::__POSTFIX_TYPE_MASK;
}

binary_node::BINARY_TYPE binary_node::get_type() const {
	return ((BINARY_TYPE)type) & BINARY_TYPE::__BINARY_TYPE_MASK;
}

declaration_node::DECLARATION_TYPE declaration_node::get_type() const {
	return ((DECLARATION_TYPE)type) & DECLARATION_TYPE::__DECLARATION_TYPE_MASK;
}

statement_node::STATEMENT_TYPE statement_node::get_type() const {
	return ((STATEMENT_TYPE)type) & STATEMENT_TYPE::__STATEMENT_TYPE_MASK;
}

control_statement_node::CONTROL_STATEMENT_TYPE control_statement_node::get_type() const {
	return ((CONTROL_STATEMENT_TYPE)type) & CONTROL_STATEMENT_TYPE::__CONTROL_STATEMENT_TYPE_MASK;
}

jump_statement_node::JUMP_STATEMENT_TYPE jump_statement_node::get_type() const {
	return ((JUMP_STATEMENT_TYPE)type) & JUMP_STATEMENT_TYPE::__JUMP_STATEMENT_TYPE_MASK;
}

void declaration_node::make_unique_qualifiers() {
	sort(type_qualifiers.begin(), type_qualifiers.end());
	auto erase_iter = unique(type_qualifiers.begin(), type_qualifiers.end());
	if(erase_iter == type_qualifiers.end()) return;
	type_qualifiers.erase(erase_iter, type_qualifiers.end());
}

bool declaration_node::is_const() const {
	for(const auto& qual : type_qualifiers) {
		if(qual == TYPE_QUALIFIER::CONST) return true;
	}
	return false;
}

// AST dumping
static void level_indent(const size_t& level) {
	for(size_t i = 0; i < level; ++i) {
		cout.put(' ');
	}
}

void node_base::dump(const size_t level) const {
	level_indent(level);
}

void list_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "list (" << nodes.size() << ") ";
	
	switch(((LIST_TYPE)type) & LIST_TYPE::__LIST_TYPE_MASK) {
		case LIST_TYPE::EXPRESSIONS:
			cout << "expressions";
			break;
		case LIST_TYPE::ARGUMENT_EXPRESSIONS:
			cout << "argument expressions";
			break;
		case LIST_TYPE::PARAMETERS:
			cout << "parameters";
			break;
		case LIST_TYPE::STRUCT_DECLARATORS:
			cout << "struct declarators";
			break;
		case LIST_TYPE::STRUCT_DECLARATIONS:
			cout << "struct declarations";
			break;
		case LIST_TYPE::BLOCK:
			cout << "block";
			break;
		case LIST_TYPE::TRANSLATION_UNIT:
			cout << "translation unit";
			break;
		case LIST_TYPE::__LIST_TYPE_MAX:
		case LIST_TYPE::__LIST_TYPE_MASK:
			floor_unreachable();
	}
	cout << endl;
	
	for(const auto& elem : nodes) {
		elem->dump(level + 1);
	}
}

void identifier_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "identifier (" << name << ")" << endl;
}

void unary_node::dump(const size_t level) const {
	const UNARY_TYPE unary_type = get_type();
	node_base::dump(level);
	cout << "unary (";
	switch(unary_type) {
		case UNARY_TYPE::NOT:
			cout << "!";
			break;
		case UNARY_TYPE::MINUS:
			cout << "-";
			break;
		case UNARY_TYPE::REFERENCE:
			cout << "&";
			break;
		case UNARY_TYPE::DEREFERENCE:
			cout << "*";
			break;
		case UNARY_TYPE::INCREMENT:
			cout << "++";
			break;
		case UNARY_TYPE::DECREMENT:
			cout << "--";
			break;
		case UNARY_TYPE::SIZEOF_EXPRESSION:
			cout << "sizeof expr";
			break;
		case UNARY_TYPE::SIZEOF_TYPE:
			cout << "sizeof type";
			break;
		case UNARY_TYPE::__UNARY_TYPE_MASK:
			floor_unreachable();
	}
	cout << ")" << endl;
	
	node->dump(level + 1);
}

void postfix_node::dump(const size_t level) const {
	const POSTFIX_TYPE postfix_type = get_type();
	
	node_base::dump(level);
	cout << "postfix (";
	switch(postfix_type) {
		case POSTFIX_TYPE::SUBSCRIPT:
			cout << "subscript []";
			break;
		case POSTFIX_TYPE::ARG_LIST:
			cout << "arg list";
			break;
		case POSTFIX_TYPE::ACCESS:
			cout << ".";
			break;
		case POSTFIX_TYPE::DEREF_ACCESS:
			cout << "->";
			break;
		case POSTFIX_TYPE::INCREMENT:
			cout << "++";
			break;
		case POSTFIX_TYPE::DECREMENT:
			cout << "--";
			break;
		case POSTFIX_TYPE::START_NODE:
			cout << "start";
			break;
		case POSTFIX_TYPE::__POSTFIX_TYPE_MASK:
			floor_unreachable();
	}
	cout << ")" << endl;
	
	if(node) {
		node->dump(level + 1);
	}
	else {
		node_base::dump(level + 1);
		cout << "node: nullptr" << endl;
	}
	
	if(next) {
		next->dump(level);
	}
	else {
		node_base::dump(level + 1);
		cout << "next: nullptr" << endl;
	}
}

void binary_node::dump(const size_t level) const {
	const BINARY_TYPE binary_type = get_type();
	
	node_base::dump(level);
	cout << "binary (";
	switch(binary_type) {
		case BINARY_TYPE::MULTIPLY:
			cout << '*';
			break;
		case BINARY_TYPE::DIVIDE:
			cout << '/';
			break;
		case BINARY_TYPE::MODULO:
			cout << '%';
			break;
		case BINARY_TYPE::ADD:
			cout << '+';
			break;
		case BINARY_TYPE::SUBSTRACT:
			cout << '-';
			break;
		case BINARY_TYPE::LESS_THAN:
			cout << '<';
			break;
		case BINARY_TYPE::GREATER_THAN:
			cout << '>';
			break;
		case BINARY_TYPE::EQUAL:
			cout << "==";
			break;
		case BINARY_TYPE::UNEQUAL:
			cout << "!=";
			break;
		case BINARY_TYPE::BIT_OR:
			cout << '|';
			break;
		case BINARY_TYPE::BIT_AND:
			cout << '&';
			break;
		case BINARY_TYPE::BIT_XOR:
			cout << '^';
			break;
		case BINARY_TYPE::LOGIC_OR:
			cout << "||";
			break;
		case BINARY_TYPE::LOGIC_AND:
			cout << "&&";
			break;
		case BINARY_TYPE::ASSIGNMENT:
			cout << '=';
			break;
		case BINARY_TYPE::CAST_EXPRESSION:
			cout << "cast";
			break;
		case BINARY_TYPE::__BINARY_TYPE_MASK:
			floor_unreachable();
	}
	cout << ")" << endl;
	
	lhs->dump(level + 1);
	rhs->dump(level + 1);
}

void declaration_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "declaration (";
	if(is_function) cout << "function: ";
	
	const auto decl_type = get_type();
	const bool is_struct_decl_declarator = (decl_type == DECLARATION_TYPE::STRUCTURE_DECLARATION_DECLARATOR);
	const bool is_struct_or_union = (type_specifier == TYPE_SPECIFIER::STRUCT || type_specifier == TYPE_SPECIFIER::UNION);
	
	if(!is_struct_decl_declarator) {
		for(const auto& qual : type_qualifiers) {
			switch(qual) {
				case TYPE_QUALIFIER::CONST: cout << "const, "; break;
				case TYPE_QUALIFIER::RESTRICT: cout << "restrict, "; break;
				case TYPE_QUALIFIER::VOLATILE: cout << "volatile, "; break;
				case TYPE_QUALIFIER::ATOMIC: cout << "_Atomic, "; break;
			}
		}
		
		switch(type_specifier) {
			case TYPE_SPECIFIER::VOID: cout << "void, "; break;
			case TYPE_SPECIFIER::CHAR: cout << "char, "; break;
			case TYPE_SPECIFIER::INT: cout << "int, "; break;
			case TYPE_SPECIFIER::STRUCT: cout << "struct, "; break;
			case TYPE_SPECIFIER::UNION: cout << "union, "; break;
			case TYPE_SPECIFIER::INVALID: cout << "<incomplete>, "; break;
		}
	}
	
	if(outer_ptr_count > 0) cout << "outer*: " << outer_ptr_count << ", ";
	if(inner_ptr_count > 0) cout << "inner*: " << inner_ptr_count << ", ";
	
	if(!is_struct_or_union) {
		if(name) cout << "name: " << name->name;
		else cout << "<unnamed>";
	}
	else cout << "...";
	cout << ")" << endl;
	
	if(is_struct_or_union && type_specifier_node) type_specifier_node->dump(level + 1);
	if(declarations) declarations->dump(level + 1);
}

void structure_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << (type_specifier == TYPE_SPECIFIER::STRUCT ? "struct" : "union") << " (";
	
	if(name) cout << name->name;
	else cout << "<unnamed>";
	cout << ")" << endl;
	
	if(get_type() != DECLARATION_TYPE::STRUCTURE_DECLARATOR) {
		if(declarations) {
			declarations->dump(level + 1);
		}
	}
}

void statement_node::dump(const size_t level) const {
	node_base::dump(level);
	
	switch(get_type()) {
		case STATEMENT_TYPE::COMPOUND_STATEMENT:
			cout << "compound statement" << endl;
			if(statement) {
				statement->dump(level + 1);
			}
			break;
		case STATEMENT_TYPE::EXPRESSION_STATEMENT:
			cout << "expression statement" << endl;
			if(statement) {
				statement->dump(level + 1);
			}
			break;
		case STATEMENT_TYPE::CONTROL_STATEMENT:
		case STATEMENT_TYPE::JUMP_STATEMENT:
		case STATEMENT_TYPE::LABELED_STATEMENT:
		case STATEMENT_TYPE::__STATEMENT_TYPE:
		case STATEMENT_TYPE::__STATEMENT_TYPE_SHIFT:
		case STATEMENT_TYPE::__STATEMENT_TYPE_MASK:
			throw floor_exception("can't print other statement types");
	}
}

void control_statement_node::dump(const size_t level) const {
	node_base::dump(level);
	
	switch(get_type()) {
		case CONTROL_STATEMENT_TYPE::WHILE:
			cout << "while statement (expr - stmnt)" << endl;
			expression->dump(level + 1);
			statement->dump(level + 1);
			break;
		case CONTROL_STATEMENT_TYPE::DO_WHILE:
			cout << "do-while statement (stmnt - expr)" << endl;
			statement->dump(level + 1);
			expression->dump(level + 1);
			break;
		case CONTROL_STATEMENT_TYPE::IF_ELSE:
			cout << "if-else statement (expr - stmnt - stmnt)" << endl;
			expression->dump(level + 1);
			statement->dump(level + 1);
			next_statement->dump(level + 1);
			break;
		case CONTROL_STATEMENT_TYPE::IF:
			cout << "if statement (expr - stmnt)" << endl;
			expression->dump(level + 1);
			statement->dump(level + 1);
			break;
		case CONTROL_STATEMENT_TYPE::CONDITIONAL_EXPRESSION:
			cout << "ternary/conditional expr (expr - stmnt - stmnt)" << endl;
			expression->dump(level + 1);
			statement->dump(level + 1);
			next_statement->dump(level + 1);
			break;
		case CONTROL_STATEMENT_TYPE::__CONTROL_STATEMENT_TYPE_MASK:
			floor_unreachable();
	}
}

void jump_statement_node::dump(const size_t level) const {
	node_base::dump(level);
	switch(get_type()) {
		case JUMP_STATEMENT_TYPE::GOTO:
			cout << "goto" << endl;
			statement->dump(level + 1);
			break;
		case JUMP_STATEMENT_TYPE::CONTINUE:
			cout << "continue" << endl;
			break;
		case JUMP_STATEMENT_TYPE::BREAK:
			cout << "break" << endl;
			break;
		case JUMP_STATEMENT_TYPE::RETURN:
			cout << "return" << endl;
			if(statement) {
				statement->dump(level + 1);
			}
			break;
		case JUMP_STATEMENT_TYPE::__JUMP_STATEMENT_TYPE_MASK:
			floor_unreachable();
	}
}

void labeled_statement_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "label (" << as_node<identifier_node>(identifier)->name << ")" << endl;
	statement->dump(level + 1);
}

void function_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "function (decl - body)" << endl;
	
	declaration->dump(level + 1);
	
	if(body) {
		body->dump(level + 1);
	}
}

void integer_constant_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "integer constant (" << value << ")" << endl;
}

void character_constant_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "character constant (" << value << ")" << endl;
}

void string_literal_node::dump(const size_t level) const {
	node_base::dump(level);
	cout << "string literal (" << value << ")" << endl;
}
