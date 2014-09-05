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

#if !defined(FLOOR_VECTOR_WIDTH)
#error "must define FLOOR_VECTOR_WIDTH when including vector_ops.hpp"
#endif

#if (FLOOR_VECTOR_WIDTH < 1) || (FLOOR_VECTOR_WIDTH > 4)
#error "unsupported FLOOR_VECTOR_WIDTH - must be between 1 and 4!"
#endif

// the underlying vector type
// assumes the existance of "scalar_type" and "FLOOR_VECTOR_WIDTH"
#if (FLOOR_VECTOR_WIDTH == 1)
#define FLOOR_UNDERLYING_VECTOR_TYPE() \
	scalar_type x;
#elif (FLOOR_VECTOR_WIDTH == 2)
#define FLOOR_UNDERLYING_VECTOR_TYPE() \
	scalar_type x; \
	scalar_type y;
#elif (FLOOR_VECTOR_WIDTH == 3)
#define FLOOR_UNDERLYING_VECTOR_TYPE() \
	scalar_type x; \
	scalar_type y; \
	scalar_type z;
#elif (FLOOR_VECTOR_WIDTH == 4)
#define FLOOR_UNDERLYING_VECTOR_TYPE() \
	scalar_type x; \
	scalar_type y; \
	scalar_type z; \
	scalar_type w;
#endif

// TODO: more unification

// major macro voodoo to expand vector ops
#if (FLOOR_VECTOR_WIDTH == 1)
#define FLOOR_VEC_EXPAND(sep, ...) x __VA_ARGS__
#define FLOOR_VEC_EXPAND_ENCLOSED(sep, front, back, ...) front x back __VA_ARGS__
#define FLOOR_VEC_EXPAND_NO_ELEMS(code) code
#define FLOOR_VEC_EXPAND_DUAL(prefix_second, sep_one_two, sep, ...) \
	x sep_one_two prefix_second x __VA_ARGS__
#define FLOOR_VEC_EXPAND_DUAL_ENCLOSED(prefix_second, sep_one_two, front, back, sep, ...) \
	front (x sep_one_two prefix_second x) back __VA_ARGS__

#define FLOOR_VEC_UNARY_OP_EXPAND(op, rhs, sep) \
	op rhs x
#define FLOOR_VEC_OP_EXPAND(lhs, op, rhs, sep, rhs_sel) \
	lhs x op rhs_sel(rhs, x)
#define FLOOR_VEC_FUNC_OP_EXPAND(lhs, func, rhs, sep, rhs_sel, func_sep, assign, ...) \
	assign(lhs x) func(lhs x func_sep rhs_sel(rhs, x), ##__VA_ARGS__)

#elif (FLOOR_VECTOR_WIDTH == 2)
#define FLOOR_VEC_EXPAND(sep, ...) x sep y __VA_ARGS__
#define FLOOR_VEC_EXPAND_ENCLOSED(sep, front, back, ...) front x back sep front y back __VA_ARGS__
#define FLOOR_VEC_EXPAND_NO_ELEMS(code) code code
#define FLOOR_VEC_EXPAND_DUAL(prefix_second, sep_one_two, sep, ...) \
	x sep_one_two prefix_second x sep \
	y sep_one_two prefix_second y __VA_ARGS__
#define FLOOR_VEC_EXPAND_DUAL_ENCLOSED(prefix_second, sep_one_two, front, back, sep, ...) \
	front (x sep_one_two prefix_second x) back sep \
	front (y sep_one_two prefix_second y) back __VA_ARGS__

#define FLOOR_VEC_UNARY_OP_EXPAND(op, rhs, sep) \
	op rhs x sep \
	op rhs y
#define FLOOR_VEC_OP_EXPAND(lhs, op, rhs, sep, rhs_sel) \
	lhs x op rhs_sel(rhs, x) sep \
	lhs y op rhs_sel(rhs, y)
#define FLOOR_VEC_FUNC_OP_EXPAND(lhs, func, rhs, sep, rhs_sel, func_sep, assign, ...) \
	assign(lhs x) func(lhs x func_sep rhs_sel(rhs, x), ##__VA_ARGS__) sep \
	assign(lhs y) func(lhs y func_sep rhs_sel(rhs, y), ##__VA_ARGS__)

#elif (FLOOR_VECTOR_WIDTH == 3)
#define FLOOR_VEC_EXPAND(sep, ...) x sep y sep z __VA_ARGS__
#define FLOOR_VEC_EXPAND_ENCLOSED(sep, front, back, ...) front x back sep front y back sep front z back __VA_ARGS__
#define FLOOR_VEC_EXPAND_NO_ELEMS(code) code code code
#define FLOOR_VEC_EXPAND_DUAL(prefix_second, sep_one_two, sep, ...) \
	x sep_one_two prefix_second x sep \
	y sep_one_two prefix_second y sep \
	z sep_one_two prefix_second z __VA_ARGS__
#define FLOOR_VEC_EXPAND_DUAL_ENCLOSED(prefix_second, sep_one_two, front, back, sep, ...) \
	front (x sep_one_two prefix_second x) back sep \
	front (y sep_one_two prefix_second y) back sep \
	front (z sep_one_two prefix_second z) back __VA_ARGS__

#define FLOOR_VEC_UNARY_OP_EXPAND(op, rhs, sep) \
	op rhs x sep \
	op rhs y sep \
	op rhs z
#define FLOOR_VEC_OP_EXPAND(lhs, op, rhs, sep, rhs_sel) \
	lhs x op rhs_sel(rhs, x) sep \
	lhs y op rhs_sel(rhs, y) sep \
	lhs z op rhs_sel(rhs, z)
#define FLOOR_VEC_FUNC_OP_EXPAND(lhs, func, rhs, sep, rhs_sel, func_sep, assign, ...) \
	assign(lhs x) func(lhs x func_sep rhs_sel(rhs, x), ##__VA_ARGS__) sep \
	assign(lhs y) func(lhs y func_sep rhs_sel(rhs, y), ##__VA_ARGS__) sep \
	assign(lhs z) func(lhs z func_sep rhs_sel(rhs, z), ##__VA_ARGS__)

#elif (FLOOR_VECTOR_WIDTH == 4)
#define FLOOR_VEC_EXPAND(sep, ...) x sep y sep z sep w __VA_ARGS__
#define FLOOR_VEC_EXPAND_ENCLOSED(sep, front, back, ...) front x back sep front y back sep front z back sep front w back __VA_ARGS__
#define FLOOR_VEC_EXPAND_NO_ELEMS(code) code code code code
#define FLOOR_VEC_EXPAND_DUAL(prefix_second, sep_one_two, sep, ...) \
	x sep_one_two prefix_second x sep \
	y sep_one_two prefix_second y sep \
	z sep_one_two prefix_second z sep \
	w sep_one_two prefix_second w __VA_ARGS__
#define FLOOR_VEC_EXPAND_DUAL_ENCLOSED(prefix_second, sep_one_two, front, back, sep, ...) \
	front (x sep_one_two prefix_second x) back sep \
	front (y sep_one_two prefix_second y) back sep \
	front (z sep_one_two prefix_second z) back sep \
	front (w sep_one_two prefix_second w) back __VA_ARGS__

#define FLOOR_VEC_UNARY_OP_EXPAND(op, rhs, sep) \
	op rhs x sep \
	op rhs y sep \
	op rhs z sep \
	op rhs w
#define FLOOR_VEC_OP_EXPAND(lhs, op, rhs, sep, rhs_sel) \
	lhs x op rhs_sel(rhs, x) sep \
	lhs y op rhs_sel(rhs, y) sep \
	lhs z op rhs_sel(rhs, z) sep \
	lhs w op rhs_sel(rhs, w)
#define FLOOR_VEC_FUNC_OP_EXPAND(lhs, func, rhs, sep, rhs_sel, func_sep, assign, ...) \
	assign(lhs x) func(lhs x func_sep rhs_sel(rhs, x), ##__VA_ARGS__) sep \
	assign(lhs y) func(lhs y func_sep rhs_sel(rhs, y), ##__VA_ARGS__) sep \
	assign(lhs z) func(lhs z func_sep rhs_sel(rhs, z), ##__VA_ARGS__) sep \
	assign(lhs w) func(lhs w func_sep rhs_sel(rhs, w), ##__VA_ARGS__)
#endif

// vector macros
#define FLOOR_COMMA ,
#define FLOOR_COMMA_FUNC() FLOOR_COMMA
#define FLOOR_SEMICOLON ;
#define FLOOR_SEMICOLON_FUNC() FLOOR_SEMICOLON
#define FLOOR_PAREN_LEFT (
#define FLOOR_PAREN_LEFT_FUNC() FLOOR_PAREN_LEFT
#define FLOOR_PAREN_RIGHT )
#define FLOOR_PAREN_RIGHT_FUNC() FLOOR_PAREN_RIGHT
#define FLOOR_NOP
#define FLOOR_NOP_FUNC()

// for use with FLOOR_VEC_FUNC_OP_VEC_EXPAND(...) (expand to nop or actual assignment)
#define FLOOR_VEC_ASSIGN_NOP(val)
#define FLOOR_VEC_ASSIGN_SET(val) val =

// for use with FLOOR_VEC_OP_VEC_EXPAND(...) and FLOOR_VEC_FUNC_OP_VEC_EXPAND(...) (rhs is a vector or scalar)
#define FLOOR_VEC_RHS_NOP(rhs, comp)
#define FLOOR_VEC_RHS_VEC(rhs, comp) rhs comp
#define FLOOR_VEC_RHS_SCALAR(rhs, comp) rhs

// simple vector operation that either returns a new vector object or is the resp. assignment operation
#define FLOOR_VEC_OP(op) \
	constexpr FLOOR_VECNAME operator op (const scalar_type& val) const { \
		return { FLOOR_VEC_OP_EXPAND(this->, op, val, FLOOR_COMMA, FLOOR_VEC_RHS_SCALAR) }; \
	} \
	constexpr FLOOR_VECNAME operator op (const FLOOR_VECNAME& vec) const { \
		return { FLOOR_VEC_OP_EXPAND(this->, op, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC) }; \
	} \
	constexpr friend FLOOR_VECNAME operator op (const scalar_type& val, const FLOOR_VECNAME& v) { \
		return { FLOOR_VEC_OP_EXPAND(v., op, val, FLOOR_COMMA, FLOOR_VEC_RHS_SCALAR) }; \
	} \
	constexpr FLOOR_VECNAME & operator op##= (const scalar_type& val) { \
		FLOOR_VEC_OP_EXPAND(this->, op##=, val, FLOOR_SEMICOLON, FLOOR_VEC_RHS_SCALAR); \
		return *this; \
	} \
	constexpr FLOOR_VECNAME & operator op##= (const FLOOR_VECNAME& vec) { \
		FLOOR_VEC_OP_EXPAND(this->, op##=, vec., FLOOR_SEMICOLON, FLOOR_VEC_RHS_VEC); \
		return *this; \
	}

// TODO
#define FLOOR_VEC_OP_FUNC(op, func_name) \
	constexpr FLOOR_VECNAME operator op (const scalar_type& val) const { \
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, vector_helper<scalar_type>::func_name, val, FLOOR_COMMA, FLOOR_VEC_RHS_SCALAR, FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP) }; \
	} \
	constexpr FLOOR_VECNAME operator op (const FLOOR_VECNAME& vec) const { \
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, vector_helper<scalar_type>::func_name, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC, FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP) }; \
	} \
	constexpr friend FLOOR_VECNAME operator op (const scalar_type& val, const FLOOR_VECNAME& v) { \
		return { FLOOR_VEC_FUNC_OP_EXPAND(v., vector_helper<scalar_type>::func_name, val, FLOOR_COMMA, FLOOR_VEC_RHS_SCALAR, FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP) }; \
	} \
	constexpr FLOOR_VECNAME & operator op##= (const scalar_type& val) { \
		FLOOR_VEC_FUNC_OP_EXPAND(this->, vector_helper<scalar_type>::func_name, val, FLOOR_SEMICOLON, FLOOR_VEC_RHS_SCALAR, FLOOR_COMMA, FLOOR_VEC_ASSIGN_SET); \
		return *this; \
	} \
	constexpr FLOOR_VECNAME & operator op##= (const FLOOR_VECNAME& vec) { \
		FLOOR_VEC_FUNC_OP_EXPAND(this->, vector_helper<scalar_type>::func_name, vec., FLOOR_SEMICOLON, FLOOR_VEC_RHS_VEC, FLOOR_COMMA, FLOOR_VEC_ASSIGN_SET); \
		return *this; \
	}

// TODO
#define FLOOR_VEC_UNARY_OP(op) \
	constexpr FLOOR_VECNAME operator op () const { \
		return { FLOOR_VEC_UNARY_OP_EXPAND(op, this->, FLOOR_COMMA) }; \
	}

// TODO
#define FLOOR_VEC_UNARY_OP_NON_CONST(op) \
	constexpr FLOOR_VECNAME operator op () { \
		return { FLOOR_VEC_UNARY_OP_EXPAND(op, this->, FLOOR_COMMA) }; \
	}

// TODO
#define FLOOR_VEC_UNARY_POSTFIX_OP(op) \
	constexpr FLOOR_VECNAME operator op (int) { \
		return { FLOOR_VEC_OP_EXPAND(this->, op, FLOOR_NOP, FLOOR_COMMA, FLOOR_VEC_RHS_NOP) }; \
	}

// TODO
#define FLOOR_VEC_FUNC(func_name, func_name_this, func_name_copy) \
FLOOR_VEC_FUNC_EXT_ARGS(func_name, func_name_this, func_name_copy, FLOOR_NOP, (), FLOOR_NOP, FLOOR_NOP_FUNC, FLOOR_VEC_RHS_NOP)

#define FLOOR_VEC_FUNC_EXT(func_name, func_name_this, func_name_copy, func_ext) \
FLOOR_VEC_FUNC_EXT_ARGS(func_name, func_name_this, func_name_copy, func_ext, (), FLOOR_NOP, FLOOR_NOP_FUNC, FLOOR_VEC_RHS_NOP)

#define FLOOR_VEC_FUNC_ARGS(func_name, func_name_this, func_name_copy, func_args, ...) \
FLOOR_VEC_FUNC_EXT_ARGS(func_name, func_name_this, func_name_copy, FLOOR_NOP, func_args, FLOOR_NOP, FLOOR_NOP_FUNC, FLOOR_VEC_RHS_NOP, __VA_ARGS__)

#define FLOOR_VEC_FUNC_ARGS_VEC(func_name, func_name_this, func_name_copy, func_args, rhs, rhs_sel, ...) \
FLOOR_VEC_FUNC_EXT_ARGS(func_name, func_name_this, func_name_copy, FLOOR_NOP, func_args, rhs, FLOOR_COMMA_FUNC, rhs_sel, __VA_ARGS__)

#define FLOOR_VEC_FUNC_EXT_ARGS(func_name, func_name_this, func_name_copy, func_ext, func_args, rhs, rhs_sep, rhs_sel, ...) \
	constexpr FLOOR_VECNAME & func_name_this func_args { \
		func_ext \
		FLOOR_VEC_FUNC_OP_EXPAND(this->, func_name, rhs, FLOOR_SEMICOLON, rhs_sel, rhs_sep(), \
								 FLOOR_VEC_ASSIGN_SET, ##__VA_ARGS__); \
		return *this; \
	} \
	constexpr FLOOR_VECNAME func_name_copy func_args const { \
		func_ext \
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, func_name, rhs, FLOOR_COMMA, rhs_sel, \
										  rhs_sep(), FLOOR_VEC_ASSIGN_NOP, ##__VA_ARGS__) }; \
	}
