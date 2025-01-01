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

#pragma once

#include <floor/core/essentials.hpp>
#include <string>
#include <utility>
#include <vector>
#include <floor/core/util.hpp>
using namespace std;

//! underlying type of the source code
typedef string source_type;

//! constant iterator on the source code
typedef const char* source_iterator;

//! source code range: [begin, end)
struct source_range {
	//! inclusive begin
	source_iterator begin;
	//! exclusive end
	source_iterator end;
	
	bool operator==(const char*) const;
	bool operator==(const char&) const;
	bool operator!=(const char*) const;
	bool operator!=(const char&) const;
	bool equal(const char*, const size_t&) const;
	bool unequal(const char*, const size_t&) const;
	
	string to_string() const;
	size_t size() const;
};

//! enum representation of all C11 keywords
enum class FLOOR_KEYWORD : uint16_t {
	AUTO,
	BREAK,
	CASE,
	CHAR,
	CONST_KWD,
	CONTINUE,
	DEFAULT,
	DO,
	DOUBLE,
	ELSE,
	ENUM,
	EXTERN,
	FLOAT,
	FOR,
	GOTO,
	IF,
	INLINE,
	INT,
	LONG,
	REGISTER,
	RESTRICT,
	RETURN,
	SHORT,
	SIGNED,
	SIZEOF,
	STATIC,
	STRUCT,
	SWITCH,
	TYPEDEF,
	UNION,
	UNSIGNED,
	VOID_KWD,
	VOLATILE,
	WHILE,
	ALIGNAS,
	ALIGNOF,
	ATOMIC,
	BOOL,
	COMPLEX,
	GENERIC,
	IMAGINARY,
	NORETURN,
	STATIC_ASSERT,
	THREAD_LOCAL,
};

//! enum representation of all C11 punctuators
enum class FLOOR_PUNCTUATOR : uint16_t {
	INVALID = 0,		//! used to signal invalid/uninitialized types
	LEFT_BRACKET,		//!< [ and <:
	RIGHT_BRACKET,		//!< ] and :>
	LEFT_PAREN,			//!< (
	RIGHT_PAREN,		//!< )
	LEFT_BRACE,			//!< { and <%
	RIGHT_BRACE,		//!< } and %>
	DOT,				//!< .
	ARROW,				//!< ->
	INCREMENT,			//!< ++
	DECREMENT,			//!< --
	AND,				//!< &
	ASTERISK,			//!< *
	PLUS,				//!< +
	MINUS,				//!< -
	TILDE,				//!< ~
	NOT,				//!< !
	DIV,				//!< /
	MODULO,				//!< %
	LEFT_SHIFT,			//!< <<
	RIGHT_SHIFT,		//!< >>
	LESS_THAN,			//!< <
	GREATER_THAN,		//!< >
	LESS_OR_EQUAL,		//!< <=
	GREATER_OR_EQUAL,	//!< >=
	EQUAL,				//!< ==
	UNEQUAL,			//!< !=
	XOR,				//!< ^
	OR,					//!< |
	LOGIC_AND,			//!< &&
	LOGIC_OR,			//!< ||
	TERNARY,			//!< ?
	COLON,				//!< :
	SEMICOLON,			//!< ;
	ELLIPSIS,			//!< ...
	ASSIGN,				//!< =
	MUL_ASSIGN,			//!< *=
	DIV_ASSIGN,			//!< /=
	MODULE_ASSIGN,		//!< %=
	ADD_ASSIGN,			//!< +=
	SUB_ASSIGN,			//!< -=
	LEFT_SHIFT_ASSIGN,	//!< <<=
	RIGHT_SHIFT_ASSIGN,	//!< >>=
	AND_ASSIGN,			//!< &=
	XOR_ASSIGN,			//!< ^=
	OR_ASSIGN,			//!< |=
	COMMA,				//!< ,
	HASH,				//!< # and %:
	HASH_HASH			//!< ## and %:%:
};

//! full internal type of the token
enum class SOURCE_TOKEN_TYPE : uint32_t {
	// invalid type (for mask testing and other purposes)
	INVALID						= 0u,
	
	// base type according to (6.4)
	KEYWORD						= (1u << 31u),
	IDENTIFIER					= (1u << 30u),
	CONSTANT					= (1u << 29u),
	STRING_LITERAL				= (1u << 28u),
	PUNCTUATOR					= (1u << 27u),
	__BASE_TYPE_MASK			= (KEYWORD | IDENTIFIER | CONSTANT | STRING_LITERAL | PUNCTUATOR),
	
	// sub-types (for easier differentiation)
	INTEGER_CONSTANT 			= (1u << 0u) | CONSTANT,
	UNSIGNED_INTEGER_CONSTANT 	= (1u << 1u) | CONSTANT,
	CHARACTER_CONSTANT			= (1u << 2u) | CONSTANT,
	__SUB_TYPE_MASK				= (0xFFFFu)
};
constexpr SOURCE_TOKEN_TYPE operator&(const SOURCE_TOKEN_TYPE& e0, const SOURCE_TOKEN_TYPE& e1) {
	return (SOURCE_TOKEN_TYPE)((underlying_type_t<SOURCE_TOKEN_TYPE>)e0 &
							   (underlying_type_t<SOURCE_TOKEN_TYPE>)e1);
}
constexpr SOURCE_TOKEN_TYPE& operator&=(SOURCE_TOKEN_TYPE& e0, const SOURCE_TOKEN_TYPE& e1) {
	e0 = e0 & e1;
	return e0;
}
constexpr SOURCE_TOKEN_TYPE operator|(const SOURCE_TOKEN_TYPE& e0, const SOURCE_TOKEN_TYPE& e1) {
	return (SOURCE_TOKEN_TYPE)((underlying_type_t<SOURCE_TOKEN_TYPE>)e0 |
							   (underlying_type_t<SOURCE_TOKEN_TYPE>)e1);
}
constexpr SOURCE_TOKEN_TYPE& operator|=(SOURCE_TOKEN_TYPE& e0, const SOURCE_TOKEN_TYPE& e1) {
	e0 = e0 | e1;
	return e0;
}

constexpr inline SOURCE_TOKEN_TYPE get_token_primary_type(const SOURCE_TOKEN_TYPE& type) {
	return (type & SOURCE_TOKEN_TYPE::__BASE_TYPE_MASK);
}

template<typename sub_type>
constexpr inline sub_type get_token_sub_type(const SOURCE_TOKEN_TYPE& type) {
	return (sub_type)(type & SOURCE_TOKEN_TYPE::__SUB_TYPE_MASK);
}

//! <type, range in the code>
typedef pair<SOURCE_TOKEN_TYPE, source_range> token;

//! container type for all tokens
typedef vector<token> token_container;

//! const iterator of the token container
typedef token_container::const_iterator token_iterator;

//! inclusive token range in the source code: [first, second]
typedef pair<token_iterator, token_iterator> token_range;
