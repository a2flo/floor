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

#ifndef __FLOOR_AST_BASE_HPP__
#define __FLOOR_AST_BASE_HPP__

#include <array>
#include <vector>
#include <memory>
#include <type_traits>
#include <string>
#include <unordered_map>
#include "core/platform.hpp"
#include "lang/source_types.hpp"
using namespace std;

class lang_context;
struct translation_unit;
class ast_printer;

//! common base class of all AST nodes
class node_base {
public:
	enum class NODE_TYPE : uint32_t {
		INVALID = 0u,
		TRANSITIONAL = 1u, //!< only used during AST construction
		
		// primary types (use the upper 8 bits)
		__PRIMARY_TYPE			= (1u << 31u),
		__PRIMARY_TYPE_SHIFT 	= (24u),
		__PRIMARY_TYPE_MASK		= (0xFFu << __PRIMARY_TYPE_SHIFT),
		
		UNARY_EXPRESSION		= __PRIMARY_TYPE + (1u << __PRIMARY_TYPE_SHIFT),
		POSTFIX_EXPRESSION		= __PRIMARY_TYPE + (2u << __PRIMARY_TYPE_SHIFT),
		BINARY_EXPRESSION		= __PRIMARY_TYPE + (3u << __PRIMARY_TYPE_SHIFT),
		LIST					= __PRIMARY_TYPE + (4u << __PRIMARY_TYPE_SHIFT),
		DECLARATION				= __PRIMARY_TYPE + (5u << __PRIMARY_TYPE_SHIFT),
		STATEMENT				= __PRIMARY_TYPE + (6u << __PRIMARY_TYPE_SHIFT),
		IDENTIFIER				= __PRIMARY_TYPE + (7u << __PRIMARY_TYPE_SHIFT),
		FUNCTION				= __PRIMARY_TYPE + (8u << __PRIMARY_TYPE_SHIFT),
		INTEGER_CONSTANT		= __PRIMARY_TYPE + (9u << __PRIMARY_TYPE_SHIFT),
		CHARACTER_CONSTANT		= __PRIMARY_TYPE + (10u << __PRIMARY_TYPE_SHIFT),
		STRING_LITERAL			= __PRIMARY_TYPE + (11u << __PRIMARY_TYPE_SHIFT),
		
		// NOTE: sub-types: can use the lower 24 bits
	};
	enum_class_bitwise_and(NODE_TYPE)
	enum_class_bitwise_or(NODE_TYPE)
	NODE_TYPE type;
	
	//! returns the primary node type
	NODE_TYPE get_type() const;
	
	//! the token range in the source code (if applicable)
	token_range range;
	
	//
	node_base(const NODE_TYPE& type_, token_range&& range_) noexcept : type(type_), range(move(range_)) {}
	virtual ~node_base() noexcept = default;
	
	bool is_primary_type(const NODE_TYPE& test_type) const {
		return ((type & NODE_TYPE::__PRIMARY_TYPE_MASK) == (test_type & NODE_TYPE::__PRIMARY_TYPE_MASK));
	}
	
	//! prints the node according to the printer arg
	virtual void print(ast_printer& printer) const;
	//! prints the node and creates an ast_printer object (-> calls print(printer))
	void print() const;
	
	//! prints the node type and its contained nodes
	virtual void dump(const size_t level = 0) const;
	
};

//! ir_gen_error exception
class ir_gen_error : public exception {
protected:
	const node_base* node;
	string error_msg;
public:
	ir_gen_error(const node_base* node_, const string& error_msg_) : node(node_), error_msg(error_msg_) {}
	const char* what() const noexcept override;
	const node_base* get_node() const noexcept;
};

//! helper function to cast an unique_ptr<node_base> object to a derived node type:
//! as_node<derived>(unique_ptr<node_base>& node)
template <class as_node_type, class = typename enable_if<is_base_of<node_base, as_node_type>::value>::type>
inline as_node_type* as_node(unique_ptr<node_base>& node) {
#if defined(FLOOR_DEBUG)
	if(node == nullptr) {
		throw floor_exception("as_node: node must not be nullptr");
	}
#endif
	return (as_node_type*)node.get();
}

//! helper function to cast an unique_ptr<node_base> object to a derived node type (const version)
template <class as_node_type, class = typename enable_if<is_base_of<node_base, as_node_type>::value>::type>
inline const as_node_type* as_node(const unique_ptr<node_base>& node) {
#if defined(FLOOR_DEBUG)
	if(node == nullptr) {
		throw floor_exception("as_node: node must not be nullptr");
	}
#endif
	return (const as_node_type*)node.get();
}

//!
class identifier_node : public node_base {
public:
	//! name of the identifier
	const string name;
	
	identifier_node(string&& name_, token_range&& range_) noexcept :
	node_base(node_base::NODE_TYPE::IDENTIFIER, move(range_)), name(name_) {}
	identifier_node(const string& name_, token_range&& range_) noexcept :
	node_base(node_base::NODE_TYPE::IDENTIFIER, move(range_)), name(name_) {}
	virtual ~identifier_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//! arbitrary list node
class list_node : public node_base {
public:
	enum class LIST_TYPE : uint16_t {
		EXPRESSIONS,
		ARGUMENT_EXPRESSIONS,
		PARAMETERS,
		STRUCT_DECLARATIONS,
		STRUCT_DECLARATORS,
		BLOCK,
		TRANSLATION_UNIT,
		__LIST_TYPE_MAX,
		__LIST_TYPE_MASK = (0xFFFFu)
	};
	enum_class_bitwise_and(LIST_TYPE)
	enum_class_bitwise_or(LIST_TYPE)
	
	LIST_TYPE get_type() const;
	
	vector<unique_ptr<node_base>> nodes;
	
	list_node(const LIST_TYPE sub_type, token_range&& range_) noexcept :
	node_base(NODE_TYPE::LIST | (NODE_TYPE)sub_type, move(range_)) {}
	list_node(vector<unique_ptr<node_base>>&& nodes_, const LIST_TYPE sub_type, token_range&& range_) noexcept :
	node_base(NODE_TYPE::LIST | (NODE_TYPE)sub_type, move(range_)), nodes(move(nodes_)) {}
	virtual ~list_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//!
class unary_node : public node_base {
public:
	enum class UNARY_TYPE : uint16_t {
		NOT,				//!< !
		MINUS,				//!< -
		REFERENCE,			//!< &
		DEREFERENCE,		//!< *
		INCREMENT,			//!< ++
		DECREMENT,			//!< --
		SIZEOF_EXPRESSION,	//!< sizeof unary-expression
		SIZEOF_TYPE,		//!< sizeof ( type-name )
		__UNARY_TYPE_MASK	= (0xFFFFu),
	};
	enum_class_bitwise_and(UNARY_TYPE)
	enum_class_bitwise_or(UNARY_TYPE)
	
	UNARY_TYPE get_type() const;
	
	//! operand (*expression, type-name, ...)
	unique_ptr<node_base> node;

	unary_node(const UNARY_TYPE sub_type, unique_ptr<node_base>&& node_, token_range&& range_) noexcept :
	node_base(NODE_TYPE::UNARY_EXPRESSION | (NODE_TYPE)sub_type, move(range_)),
	node(move(node_)) {}
	virtual ~unary_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//!
class postfix_node : public node_base {
public:
	enum class POSTFIX_TYPE : uint16_t {
		START_NODE,		//!< primary-expression and start node of a postfix chain
		SUBSCRIPT,		//!< [...]
		ARG_LIST,		//!< (...)
		ACCESS,			//!< .
		DEREF_ACCESS,	//!< ->
		INCREMENT,		//!< ++
		DECREMENT,		//!< --
		__POSTFIX_TYPE_MASK = (0xFFFFu)
	};
	enum_class_bitwise_and(POSTFIX_TYPE)
	enum_class_bitwise_or(POSTFIX_TYPE)
	
	POSTFIX_TYPE get_type() const;
	
	//! operand (*expression, type-name, ...) in the postfix node chain
	unique_ptr<node_base> node;
	
	//! potential next node in the postfix chain (nullptr if this is the end)
	//! NOTE: these are linked in left-to-right order
	unique_ptr<postfix_node> next;
	
	postfix_node(const POSTFIX_TYPE sub_type,
				 unique_ptr<node_base>&& node_, unique_ptr<postfix_node>&& next_,
				 token_range&& range_) noexcept :
	node_base(NODE_TYPE::POSTFIX_EXPRESSION | (NODE_TYPE)sub_type, move(range_)),
	node(move(node_)), next(move(next_)) {}
	virtual ~postfix_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
	
protected:
	bool ends_on_inc_or_dec() const;
};

//!
class binary_node : public node_base {
public:
	enum class BINARY_TYPE : uint16_t {
		MULTIPLY,			//!< *
		DIVIDE,				//!< /
		MODULO,				//!< %
		ADD,				//!< +
		SUBSTRACT,			//!< -
		LESS_THAN,			//!< <
		GREATER_THAN,		//!< >
		EQUAL,				//!< ==
		UNEQUAL,			//!< !=
		BIT_OR,				//!< |
		BIT_AND,			//!< &
		BIT_XOR,			//!< ^
		LOGIC_OR,			//!< ||
		LOGIC_AND,			//!< &&
		ASSIGNMENT,			//!< =
		CAST_EXPRESSION,	//!< ( type-name ) cast-expression
		__BINARY_TYPE_MASK = (0xFFFFu)
	};
	enum_class_bitwise_and(BINARY_TYPE)
	enum_class_bitwise_or(BINARY_TYPE)
	
	BINARY_TYPE get_type() const;
	
	//! left-hand side
	unique_ptr<node_base> lhs;
	//! right-hand side
	unique_ptr<node_base> rhs;
	
	binary_node(const BINARY_TYPE sub_type,
				unique_ptr<node_base>&& lhs_, unique_ptr<node_base>&& rhs_,
				token_range&& range_) noexcept :
	node_base(NODE_TYPE::BINARY_EXPRESSION | (NODE_TYPE)sub_type, move(range_)),
	lhs(move(lhs_)), rhs(move(rhs_)) {}
	virtual ~binary_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//!
class declaration_node : public node_base {
public:
	//!
	enum class DECLARATION_TYPE : uint16_t {
		DECLARATION,						//!< declaration (with an identifier)
		ABSTRACT_DECLARATION,				//!< abstract declaration (might be a type name)
		STRUCTURE_DEFINITION,				//!< the actual structure decl (e.g. struct id { ... };)
		STRUCTURE_DECLARATION,				//!< declaration inside a structure
		STRUCTURE_DECLARATION_DECLARATOR,	//!< declarator inside a declaration inside a structure
		STRUCTURE_DECLARATOR,				//!< usage declarator of a structure
		__DECLARATION_TYPE_MASK = (0xFFFFu)
	};
	enum_class_bitwise_and(DECLARATION_TYPE)
	enum_class_bitwise_or(DECLARATION_TYPE)
	
	DECLARATION_TYPE get_type() const;
	
	//!
	enum class TYPE_QUALIFIER {
		CONST,
		RESTRICT,
		VOLATILE,
		ATOMIC
	};
	//! (TODO: unique these at some point: always manually after insertion or find a way to do this automatically / find a specific decl insertion point where this can be called)
	vector<TYPE_QUALIFIER> type_qualifiers {};
	//! cleanup methods that calls sort(...) and unique(...) on the qualifiers vector
	void make_unique_qualifiers();
	//! returns true if this type is const-qualified
	bool is_const() const;
	
	//!
	enum class TYPE_SPECIFIER {
		INVALID,	//!< initial state, must be set later or decl is incomplete
		VOID,		//!< void
		CHAR,		//!< char
		INT,		//!< int
		STRUCT,		//!< struct
		UNION		//!< union
	};
	//!
	TYPE_SPECIFIER type_specifier { TYPE_SPECIFIER::INVALID };
	//! if the type specifier is a struct or union, this points to the corresponding node.
	//! note that this has to be a shared_ptr in order to support multiple declarators with
	//! the same shared struct/union type (so to avoid copies).
	shared_ptr<declaration_node> type_specifier_node;
	//! amount of inner pointers/stars (type itself "is" a pointer)
	uint32_t inner_ptr_count { 0 };
	//! amount of outer pointers/stars (pointer to this type)
	uint32_t outer_ptr_count { 0 };
	//! flag if this is a function or function pointer declaration
	bool is_function { false };
	
	//! actual declaration identifier
	unique_ptr<identifier_node> name;
	
	//! encapsulated declarations (parameter list, declarator list, ...)
	unique_ptr<list_node> declarations;
	
	declaration_node(const DECLARATION_TYPE sub_type, token_range&& range_) noexcept :
	node_base(NODE_TYPE::DECLARATION | (node_base::NODE_TYPE)sub_type, move(range_)) {}
	virtual ~declaration_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

class structure_node : public declaration_node {
public:
	structure_node(const TYPE_SPECIFIER spec, token_range&& range_) noexcept :
	declaration_node(DECLARATION_TYPE::STRUCTURE_DEFINITION, move(range_)) {
		assert(spec == TYPE_SPECIFIER::STRUCT || spec == TYPE_SPECIFIER::UNION);
		type_specifier = spec;
	}
	virtual ~structure_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//!
class statement_node : public node_base {
public:
	enum class STATEMENT_TYPE : uint16_t {
		__STATEMENT_TYPE		= (1u << 15u),
		__STATEMENT_TYPE_SHIFT 	= (8u),
		__STATEMENT_TYPE_MASK	= (0xFFu << __STATEMENT_TYPE_SHIFT),
		
		CONTROL_STATEMENT		= __STATEMENT_TYPE + (1u << __STATEMENT_TYPE_SHIFT),
		JUMP_STATEMENT			= __STATEMENT_TYPE + (2u << __STATEMENT_TYPE_SHIFT),
		LABELED_STATEMENT		= __STATEMENT_TYPE + (3u << __STATEMENT_TYPE_SHIFT),
		COMPOUND_STATEMENT		= __STATEMENT_TYPE + (4u << __STATEMENT_TYPE_SHIFT),
		EXPRESSION_STATEMENT	= __STATEMENT_TYPE + (5u << __STATEMENT_TYPE_SHIFT),
	};
	enum_class_bitwise_and(STATEMENT_TYPE)
	enum_class_bitwise_or(STATEMENT_TYPE)
	
	STATEMENT_TYPE get_type() const;
	
	//! node of the statement(s) or otherwise named node of this statement
	unique_ptr<node_base> statement;
	
	statement_node(const STATEMENT_TYPE sub_type, unique_ptr<node_base>&& stmnt, token_range&& range_) noexcept :
	node_base(NODE_TYPE::STATEMENT | (NODE_TYPE)sub_type, move(range_)), statement(move(stmnt)) {}
	virtual ~statement_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

class control_statement_node : public statement_node {
public:
	enum class CONTROL_STATEMENT_TYPE : uint8_t {
		WHILE,					//!< while ( expression) statement
		DO_WHILE,				//!< do statement while ( expression )
		IF,						//!< if ( expression ) statement
		IF_ELSE,				//!< if ( expression ) statement else next_statement
		CONDITIONAL_EXPRESSION,	//!< ternary: statement ? expression : next_statement
		__CONTROL_STATEMENT_TYPE_MASK = (0xFFu)
	};
	enum_class_bitwise_and(CONTROL_STATEMENT_TYPE)
	enum_class_bitwise_or(CONTROL_STATEMENT_TYPE)
	
	CONTROL_STATEMENT_TYPE get_type() const;
	
	//! node of the expression(s)
	unique_ptr<node_base> expression;
	//! node of the next statement (if there is any)
	unique_ptr<node_base> next_statement;
	
	control_statement_node(const CONTROL_STATEMENT_TYPE sub_type,
						   unique_ptr<node_base>&& stmnt,
						   unique_ptr<node_base>&& expr,
						   unique_ptr<node_base>&& next_stmnt,
						   token_range&& range_) noexcept :
	statement_node(STATEMENT_TYPE::CONTROL_STATEMENT | (STATEMENT_TYPE)sub_type, move(stmnt), move(range_)),
	expression(move(expr)), next_statement(move(next_stmnt)) {}
	virtual ~control_statement_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

class jump_statement_node : public statement_node {
public:
	enum class JUMP_STATEMENT_TYPE : uint8_t {
		GOTO,
		CONTINUE,
		BREAK,
		RETURN,
		__JUMP_STATEMENT_TYPE_MASK = (0xFFu)
	};
	enum_class_bitwise_and(JUMP_STATEMENT_TYPE)
	enum_class_bitwise_or(JUMP_STATEMENT_TYPE)
	
	JUMP_STATEMENT_TYPE get_type() const;
	
	jump_statement_node(const JUMP_STATEMENT_TYPE sub_type,
						unique_ptr<node_base>&& jump_stmnt,
						token_range&& range_) noexcept :
	statement_node(STATEMENT_TYPE::JUMP_STATEMENT | (STATEMENT_TYPE)sub_type,
				   move(jump_stmnt), move(range_)) {}
	virtual ~jump_statement_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

class labeled_statement_node : public statement_node {
public:
	//! the identifier node (the label)
	unique_ptr<node_base> identifier;
	
	labeled_statement_node(unique_ptr<node_base>&& id, unique_ptr<node_base>&& stmnt, token_range&& range_) noexcept :
	statement_node(STATEMENT_TYPE::LABELED_STATEMENT, move(stmnt), move(range_)), identifier(move(id)) {}
	virtual ~labeled_statement_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//!
class function_node : public node_base {
public:
	//! specifiers and declarator
	unique_ptr<declaration_node> declaration;
	
	//! function body (compound statement)
	unique_ptr<statement_node> body;
	
	function_node(unique_ptr<declaration_node>&& declaration_, unique_ptr<statement_node>&& body_, token_range&& range_) noexcept :
	node_base(NODE_TYPE::FUNCTION, move(range_)), declaration(move(declaration_)), body(move(body_)) {}
	virtual ~function_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//!
class integer_constant_node : public node_base {
public:
	//! int32 value of the constant (other types/widths are not supported as per requirements)
	const int32_t value;
	
	integer_constant_node(const int32_t& _value, token_range&& range_) noexcept :
	node_base(node_base::NODE_TYPE::INTEGER_CONSTANT, move(range_)), value(_value) {}
	virtual ~integer_constant_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

class character_constant_node : public node_base {
public:
	//! string representation of the constant (TODO: this is still the token version -> transform)
	const string value;
	
	character_constant_node(string&& value_, token_range&& range_) noexcept :
	node_base(node_base::NODE_TYPE::CHARACTER_CONSTANT, move(range_)), value(value_) {}
	virtual ~character_constant_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

//!
class string_literal_node : public node_base {
public:
	//! string representation of the constant (TODO: this is still the token version -> transform)
	const string value;
	
	string_literal_node(string&& _value, token_range&& range_) noexcept :
	node_base(node_base::NODE_TYPE::STRING_LITERAL, move(range_)), value(_value) {}
	virtual ~string_literal_node() noexcept = default;
	
	void print(ast_printer& printer) const override;
	void dump(const size_t level) const override;
};

#endif
