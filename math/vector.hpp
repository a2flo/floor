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
#error "don't include this header file - include vector_lib.hpp"
#endif

#define FLOOR_VECNAME_CONCAT(vec_width) vector##vec_width
#define FLOOR_VECNAME_EVAL(vec_width) FLOOR_VECNAME_CONCAT(vec_width)
#define FLOOR_VECNAME FLOOR_VECNAME_EVAL(FLOOR_VECTOR_WIDTH)
#define FLOOR_VECNAME_N(vec_width) FLOOR_VECNAME_EVAL(vec_width)<scalar_type>

#include "math/vector_ops.hpp"
#include "core/matrix4.hpp"
#include <type_traits>

//! this is the general vector class that is backed by N scalar variables, with N = { 1, 2, 3, 4 },
//! and is instantiated for all float types (float, double, long double), all unsigned types from
//! 8-bit to 64-bit and all signed types from 8-bit to 64-bit.
//! this class provides the most vector functionality, almost all of them also usable with constexpr
//! at compile-time (with the exception of obviously non-compile-time things like I/O, string, stream
//! and RNG code). this class however lacks the ability to be used atomically, another vector class
//! is (TODO: will be) provided for this and conversion between these two is possible.
//! assume that this class is the fastest at runtime and it should generally be used
//! over the other one, except for the cases where the other one is absolutely necessary.
//! -- "one vector class to rule them all, ... and in the darkness bind them."
template <typename scalar_type> class FLOOR_VECNAME {
public:
	FLOOR_UNDERLYING_VECTOR_TYPE()
	typedef FLOOR_VECNAME<typename vector_helper<scalar_type>::signed_type> signed_vector;
	
	//////////////////////////////////////////
	// constructors and assignment operators
	
	//! zero initialization
	constexpr FLOOR_VECNAME() noexcept :
	x((scalar_type)0)
#if FLOOR_VECTOR_WIDTH >= 2
	, y((scalar_type)0)
#endif
#if FLOOR_VECTOR_WIDTH >= 3
	, z((scalar_type)0)
#endif
#if FLOOR_VECTOR_WIDTH >= 4
	, w((scalar_type)0)
#endif
	{}
	
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME& vec) noexcept :
	FLOOR_VEC_EXPAND_DUAL(vec., FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT FLOOR_COMMA, FLOOR_PAREN_RIGHT) {}
	
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME* vec) noexcept :
	FLOOR_VEC_EXPAND_DUAL(vec->, FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT FLOOR_COMMA, FLOOR_PAREN_RIGHT) {}
	
	constexpr FLOOR_VECNAME(FLOOR_VECNAME&& vec) noexcept :
	FLOOR_VEC_EXPAND_DUAL(vec., FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT FLOOR_COMMA, FLOOR_PAREN_RIGHT) {}
	
#if FLOOR_VECTOR_WIDTH >= 2
	constexpr FLOOR_VECNAME(const scalar_type& vec_x, const scalar_type& vec_y
#if FLOOR_VECTOR_WIDTH >= 3
							, const scalar_type& vec_z
#endif
#if FLOOR_VECTOR_WIDTH >= 4
							, const scalar_type& vec_w
#endif
							) noexcept :
	x(vec_x), y(vec_y)
#if FLOOR_VECTOR_WIDTH >= 3
	, z(vec_z)
#endif
#if FLOOR_VECTOR_WIDTH >= 4
	, w(vec_w)
#endif
	{}
#endif
	
	constexpr FLOOR_VECNAME(const scalar_type& val) noexcept :
	FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , (val)) {}
	
	//! construct with conversion
	template <typename from_scalar_type>
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME<from_scalar_type>& vec) noexcept :
	FLOOR_VEC_EXPAND_DUAL(vec., FLOOR_PAREN_LEFT (scalar_type), FLOOR_PAREN_RIGHT FLOOR_COMMA, FLOOR_PAREN_RIGHT) {}
	
	// construction from lower types
#if FLOOR_VECTOR_WIDTH == 3
	//! construction from <vector2, scalar>
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME_N(2)& vec,
							const scalar_type val = (scalar_type)0) noexcept :
	x(vec.x), y(vec.y), z(val) {}
	//! construction from <scalar, vector2>
	constexpr FLOOR_VECNAME(const scalar_type& val,
							const FLOOR_VECNAME_N(2)& vec) noexcept :
	x(val), y(vec.x), z(vec.y) {}
#endif
#if FLOOR_VECTOR_WIDTH == 4
	//! construction from <vector2, scalar, scalar>
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME_N(2)& vec,
							const scalar_type val_z = (scalar_type)0,
							const scalar_type val_w = (scalar_type)0) noexcept :
	x(vec.x), y(vec.y), z(val_z), w(val_w) {}
	//! construction from <scalar, vector2, scalar>
	constexpr FLOOR_VECNAME(const scalar_type& val_x,
							const FLOOR_VECNAME_N(2)& vec,
							const scalar_type& val_w) noexcept :
	x(val_x), y(vec.x), z(vec.y), w(val_w) {}
	//! construction from <scalar, scalar, vector2>
	constexpr FLOOR_VECNAME(const scalar_type& val_x,
							const scalar_type& val_y,
							const FLOOR_VECNAME_N(2)& vec) noexcept :
	x(val_x), y(val_y), z(vec.x), w(vec.y) {}
	//! construction from <vector2, vector2>
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME_N(2)& vec_lo,
							const FLOOR_VECNAME_N(2)& vec_hi) noexcept :
	x(vec_lo.x), y(vec_lo.y), z(vec_hi.x), w(vec_hi.y) {}
	
	//! construction from <vector3, scalar>
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME_N(3)& vec,
							const scalar_type val_w = (scalar_type)0) noexcept :
	x(vec.x), y(vec.y), z(vec.z), w(val_w) {}
	//! construction from <scalar, vector3>
	constexpr FLOOR_VECNAME(const scalar_type& val_x,
							const FLOOR_VECNAME_N(3)& vec) noexcept :
	x(val_x), y(vec.x), z(vec.y), w(vec.z) {}
#endif
	
	constexpr FLOOR_VECNAME& operator=(const FLOOR_VECNAME& vec) noexcept {
		FLOOR_VEC_EXPAND_DUAL(vec., =, FLOOR_SEMICOLON, FLOOR_SEMICOLON);
		return *this;
	}
	
	constexpr FLOOR_VECNAME& operator=(FLOOR_VECNAME&& vec) noexcept {
		FLOOR_VEC_EXPAND_DUAL(vec., =, FLOOR_SEMICOLON, FLOOR_SEMICOLON);
		return *this;
	}
	
	constexpr FLOOR_VECNAME& operator=(const scalar_type& val) noexcept {
		FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_SEMICOLON, , = val, FLOOR_SEMICOLON);
		return *this;
	}
	
	// assignment from lower types
#if FLOOR_VECTOR_WIDTH >= 3
	constexpr FLOOR_VECNAME& operator=(const FLOOR_VECNAME_N(2)& vec) noexcept {
		x = vec.x;
		y = vec.y;
		return *this;
	}
#endif
#if FLOOR_VECTOR_WIDTH >= 4
	constexpr FLOOR_VECNAME& operator=(const FLOOR_VECNAME_N(3)& vec) noexcept {
		x = vec.x;
		y = vec.y;
		z = vec.z;
		return *this;
	}
#endif
	
	//////////////////////////////////////////
	// access
	constexpr const scalar_type& operator[](const size_t& index) const {
		// TODO: proper constexpr support
		return ((scalar_type*)this)[index];
	}
	constexpr scalar_type& operator[](const size_t& index) {
		// TODO: proper constexpr support
		return ((scalar_type*)this)[index];
	}
	const scalar_type* data() const {
		return (scalar_type*)this;
	}
	scalar_type* data() {
		return (scalar_type*)this;
	}
	
	//! constexpr helper function to get a const& to a vector component from an index
	template <size_t index>
	static constexpr const scalar_type& component_select(const FLOOR_VECNAME& vec) {
		static_assert(index < FLOOR_VECTOR_WIDTH, "invalid index");
		switch(index) {
			case 0: return vec.x;
#if FLOOR_VECTOR_WIDTH >= 2
			case 1: return vec.y;
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			case 2: return vec.z;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			case 3: return vec.w;
#endif
		}
	}
	
	//! vector swizzle via component indices
	template <size_t c0
#if FLOOR_VECTOR_WIDTH >= 2
			  , size_t c1
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			  , size_t c2
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			  , size_t c3
#endif
			 >
	constexpr FLOOR_VECNAME swizzle() const {
		return {
			component_select<c0>(*this)
#if FLOOR_VECTOR_WIDTH >= 2
			, component_select<c1>(*this)
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			, component_select<c2>(*this)
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			, component_select<c3>(*this)
#endif
		};
	}
	
	//////////////////////////////////////////
	// basic ops
	//! component-wise addition
	FLOOR_VEC_OP(+)
	//! component-wise substraction
	FLOOR_VEC_OP(-)
	//! component-wise multiplication
	FLOOR_VEC_OP(*)
	//! component-wise division
	FLOOR_VEC_OP(/)
	//! component-wise modulo
	FLOOR_VEC_OP_FUNC(%, modulo)
	//! component-wise postfix increment
	FLOOR_VEC_UNARY_POSTFIX_OP(++)
	//! component-wise prefix increment
	FLOOR_VEC_UNARY_OP_NON_CONST(++)
	//! component-wise postfix decrement
	FLOOR_VEC_UNARY_POSTFIX_OP(--)
	//! component-wise prefix decrement
	FLOOR_VEC_UNARY_OP_NON_CONST(--)
	//! component-wise unary +
	FLOOR_VEC_UNARY_OP(+)
	//! component-wise unary -
	FLOOR_VEC_UNARY_OP(-)
	//! component-wise unary not
	FLOOR_VEC_UNARY_OP(!)
	
#if FLOOR_VECTOR_WIDTH == 2
	//! 4x4 matrix * vector2 multiplication
	//! -> implicit vector4 with .z = 0 and .w = 1 and dropping the bottom two rows of the matrix
	constexpr FLOOR_VECNAME operator*(const matrix4<scalar_type>& mat) const {
		return {
			mat.data[0] * x + mat.data[4] * y + mat.data[12],
			mat.data[1] * x + mat.data[5] * y + mat.data[13],
		};
	}
#endif
#if FLOOR_VECTOR_WIDTH == 3
	//! 4x4 matrix * vector3 multiplication
	//! -> implicit vector4 with .w = 1 and dropping the bottom row of the matrix
	constexpr FLOOR_VECNAME operator*(const matrix4<scalar_type>& mat) const {
		return {
			mat.data[0] * x + mat.data[4] * y + mat.data[8] * z + mat.data[12],
			mat.data[1] * x + mat.data[5] * y + mat.data[9] * z + mat.data[13],
			mat.data[2] * x + mat.data[6] * y + mat.data[10] * z + mat.data[14],
		};
	}
#endif
#if FLOOR_VECTOR_WIDTH == 4
	//! 4x4 matrix * vector4 multiplication
	constexpr FLOOR_VECNAME operator*(const matrix4<scalar_type>& mat) const {
		return {
			mat.data[0] * x + mat.data[4] * y + mat.data[8] * z + mat.data[12] * w,
			mat.data[1] * x + mat.data[5] * y + mat.data[9] * z + mat.data[13] * w,
			mat.data[2] * x + mat.data[6] * y + mat.data[10] * z + mat.data[14] * w,
			mat.data[3] * x + mat.data[7] * y + mat.data[11] * z + mat.data[15] * w,
		};
	}
#endif
	constexpr FLOOR_VECNAME& operator*=(const matrix4<scalar_type>& mat) {
		*this = *this * mat;
		return *this;
	}
	
	//////////////////////////////////////////
	// bit ops
	//! component-wise bit-wise OR
	FLOOR_VEC_OP(|)
	//! component-wise bit-wise AND
	FLOOR_VEC_OP(&)
	//! component-wise bit-wise XOR
	FLOOR_VEC_OP(^)
	//! component-wise left shift
	FLOOR_VEC_OP(<<)
	//! component-wise right shift
	FLOOR_VEC_OP(>>)
	//! component-wise bit-wise complement
	FLOOR_VEC_UNARY_OP(~)
	
	//////////////////////////////////////////
	// testing
	
	//! returns true if all components are 0
	constexpr bool is_null() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(&&, FLOOR_PAREN_LEFT, == (scalar_type)0 FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if all components are finite values
	template <typename fp_type = scalar_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr bool is_finite() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(&&, __builtin_isfinite FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if any component is NaN
	template <typename fp_type = scalar_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr bool is_nan() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(||, __builtin_isnan FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if any component is inf
	template <typename fp_type = scalar_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr bool is_inf() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(||, __builtin_isinf FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if all components are normal
	template <typename fp_type = scalar_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr bool is_normal() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(&&, __builtin_isnormal FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//////////////////////////////////////////
	// rounding / clamping / wrapping
	FLOOR_VEC_FUNC(vector_helper<scalar_type>::round, round, rounded)
	FLOOR_VEC_FUNC(vector_helper<scalar_type>::floor, floor, floored)
	FLOOR_VEC_FUNC(vector_helper<scalar_type>::ceil, ceil, ceiled)
	FLOOR_VEC_FUNC(vector_helper<scalar_type>::trunc, trunc, truncated)
	FLOOR_VEC_FUNC(vector_helper<scalar_type>::rint, rint, rinted)
	
	//!
	FLOOR_VEC_FUNC_ARGS(const_math::clamp, clamp, clamped,
						(const scalar_type& min, const scalar_type& max),
						min, max)
	FLOOR_VEC_FUNC_ARGS(const_math::clamp, clamp, clamped,
						(const scalar_type& max),
						max)
	
	constexpr FLOOR_VECNAME& clamp(const FLOOR_VECNAME& min, const FLOOR_VECNAME& max) {
		x = const_math::clamp(x, min.x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::clamp(y, min.y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::clamp(z, min.z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::clamp(w, min.w, max.w);
#endif
		return *this;
	}
	constexpr FLOOR_VECNAME clamped(const FLOOR_VECNAME& min, const FLOOR_VECNAME& max) const {
		return FLOOR_VECNAME(*this).clamp(min, max);
	}
	constexpr FLOOR_VECNAME& clamp(const FLOOR_VECNAME& max) {
		x = const_math::clamp(x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::clamp(y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::clamp(z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::clamp(w, max.w);
#endif
		return *this;
	}
	constexpr FLOOR_VECNAME clamped(const FLOOR_VECNAME& max) const {
		return FLOOR_VECNAME(*this).clamp(max);
	}
	
	//!
	FLOOR_VEC_FUNC_ARGS(const_math::wrap, wrap, wrapped,
						(const scalar_type& max),
						max)
	
	constexpr FLOOR_VECNAME& wrap(const FLOOR_VECNAME& max) {
		x = const_math::wrap(x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::wrap(y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::wrap(z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::wrap(w, max.w);
#endif
		return *this;
	}
	constexpr FLOOR_VECNAME wrapped(const FLOOR_VECNAME& max) const {
		return FLOOR_VECNAME(*this).wrap(max);
	}
	
	//////////////////////////////////////////
	// geometric
	constexpr scalar_type dot() const {
		return FLOOR_VEC_EXPAND_DUAL(, *, +);
	}
	constexpr scalar_type dot(const FLOOR_VECNAME& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., *, +);
	}
	
#if FLOOR_VECTOR_WIDTH == 2
	constexpr FLOOR_VECNAME& perpendicular() {
		const scalar_type tmp = x;
		x = -y;
		y = tmp;
		return *this;
	}
	constexpr FLOOR_VECNAME perpendiculared() const {
		return { -y, x };
	}
#endif
#if FLOOR_VECTOR_WIDTH == 3
	constexpr FLOOR_VECNAME cross(const FLOOR_VECNAME& vec) const {
		return {
			y * vec.z - z * vec.y,
			z * vec.x - x * vec.z,
			x * vec.y - y * vec.x
		};
	}
#endif
	
	constexpr scalar_type length() const {
		return vector_helper<scalar_type>::sqrt(dot());
	}
	
	constexpr scalar_type distance(const FLOOR_VECNAME& vec) const {
		return (vec - *this).length();
	}
	
	constexpr scalar_type angle(const FLOOR_VECNAME& vec) const {
		// if either vector is 0, there is no angle -> return 0
		if(is_null() || vec.is_null()) return (scalar_type)0;
		
		// acos(<x, y> / ||x||*||y||)
		return vector_helper<scalar_type>::acos(this->dot(vec) / (length() * vec.length()));
	}
	
	FLOOR_VEC_FUNC_EXT(// multiply each component with "1 / ||vec||"
					   inv_length * ,
					   normalize, normalized,
					   // if this vector is 0, there is nothing to normalize
					   if(is_null()) return *this;
					   // compute "1 / ||vec||"
					   const scalar_type inv_length = vector_helper<scalar_type>::inv_sqrt(dot());
					   )
	
	
	//!
	static constexpr FLOOR_VECNAME faceforward(const FLOOR_VECNAME& N,
											   const FLOOR_VECNAME& I,
											   const FLOOR_VECNAME& Nref) {
		return (Nref.dot(I) < ((scalar_type)0) ? N : -N);
	}
	
	//!
	constexpr FLOOR_VECNAME& faceforward(const FLOOR_VECNAME& N, const FLOOR_VECNAME& Nref) {
		*this = FLOOR_VECNAME::faceforward(N, *this, Nref);
		return *this;
	}
	//!
	constexpr FLOOR_VECNAME faceforwarded(const FLOOR_VECNAME& N, const FLOOR_VECNAME& Nref) const {
		return FLOOR_VECNAME::faceforward(N, *this, Nref);
	}
	
	//! reflection of the incident vector I according to the normal N, N must be normalized
	static constexpr FLOOR_VECNAME reflect(const FLOOR_VECNAME& N,
										   const FLOOR_VECNAME& I) {
		return I - ((scalar_type)2) * N.dot(I) * N;
	}
	
	//! reflects this vector according to the normal N, N must be normalized
	constexpr FLOOR_VECNAME& reflect(const FLOOR_VECNAME& N) {
		*this = FLOOR_VECNAME::reflect(N, *this);
		return *this;
	}
	//! returns the reflected vector of *this according to the normal N, N must be normalized
	constexpr FLOOR_VECNAME reflected(const FLOOR_VECNAME& N) const {
		return FLOOR_VECNAME::reflect(N, *this);
	}
	
	//! N and I must be normalized
	static constexpr FLOOR_VECNAME refract(const FLOOR_VECNAME& N,
										   const FLOOR_VECNAME& I,
										   const scalar_type& eta) {
		const scalar_type dNI = N.dot(I);
		const scalar_type k = ((scalar_type)1) - (eta * eta) * (((scalar_type)1) - dNI * dNI);
		return (k < ((scalar_type)0) ?
				FLOOR_VECNAME { (scalar_type)0 } :
				(eta * I - (eta * dNI + vector_helper<scalar_type>::sqrt(k)) * N));
	}
	
	//! refract this vector (the incident vector) according to the normal N and the refraction index eta,
	//! *this and N must be normalized
	constexpr FLOOR_VECNAME& refract(const FLOOR_VECNAME& N, const scalar_type& eta) {
		*this = FLOOR_VECNAME::refract(N, *this, eta);
		return *this;
	}
	//! returns the refracted vector of *this (the incident vector) according to the normal N and
	//! the refraction index eta, *this and N must be normalized
	constexpr FLOOR_VECNAME refracted(const FLOOR_VECNAME& N, const scalar_type& eta) const {
		return FLOOR_VECNAME::refract(N, *this, eta);
	}
	
	//! tests each component if it is < edge, resulting in 0 if true, else 1,
	//! and returns that result in the resp. component of the return vector
	constexpr FLOOR_VECNAME step(const scalar_type& edge) const {
		// x < edge ? 0 : 1
		return {
			FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , < edge ? (scalar_type)0 : (scalar_type)1)
		};
	}
	
	//! tests each component if it is < edge vector component, resulting in 0 if true, else 1,
	//! and returns that result in the resp. component of the return vector
	constexpr FLOOR_VECNAME step(const FLOOR_VECNAME& edge_vec) const {
		return {
			FLOOR_VEC_EXPAND_DUAL_ENCLOSED(edge_vec., <, , ? (scalar_type)0 : (scalar_type)1, FLOOR_COMMA)
		};
	}
	
	//! for each component: results in 0 if component <= edge_0, 1 if component >= edge_1 and
	//! performs smooth hermite interpolation between 0 and 1 if: edge_0 < component < edge_1
	constexpr FLOOR_VECNAME smoothstep(const scalar_type& edge_0, const scalar_type& edge_1) const {
		// t = clamp((x - edge_0) / (edge_1 - edge_0), 0, 1)
		const FLOOR_VECNAME t_vec { FLOOR_VECNAME {
			FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, FLOOR_PAREN_LEFT,
									  - edge_0 FLOOR_PAREN_RIGHT / (edge_1 - edge_0))
		}.clamped((scalar_type)0, (scalar_type)1) };
		return {
			t_vec.x * t_vec.x * ((scalar_type)3 - (scalar_type)2 * t_vec.x)
#if FLOOR_VECTOR_WIDTH >= 2
			, t_vec.y * t_vec.y * ((scalar_type)3 - (scalar_type)2 * t_vec.y)
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			, t_vec.z * t_vec.z * ((scalar_type)3 - (scalar_type)2 * t_vec.z)
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			, t_vec.w * t_vec.w * ((scalar_type)3 - (scalar_type)2 * t_vec.w)
#endif
		};
	}
	
	//! for each component:
	//! results in 0 if component <= edge_vec_0 component, 1 if component >= edge_vec_1 component and
	//! performs smooth hermite interpolation between 0 and 1 if: edge_vec_0 comp < component < edge_vec_1 comp
	constexpr FLOOR_VECNAME smoothstep(const FLOOR_VECNAME& edge_vec_0, const FLOOR_VECNAME& edge_vec_1) const {
		// t = clamp((x - edge_0) / (edge_1 - edge_0), 0, 1)
		const FLOOR_VECNAME t_vec { FLOOR_VECNAME {
			(x - edge_vec_0.x) / (edge_vec_1.x - edge_vec_0.x)
#if FLOOR_VECTOR_WIDTH >= 2
			, (y - edge_vec_0.y) / (edge_vec_1.y - edge_vec_0.y)
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			, (z - edge_vec_0.z) / (edge_vec_1.z - edge_vec_0.z)
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			, (w - edge_vec_0.w) / (edge_vec_1.w - edge_vec_0.w)
#endif
		}.clamped((scalar_type)0, (scalar_type)1) };
		return {
			t_vec.x * t_vec.x * ((scalar_type)3 - (scalar_type)2 * t_vec.x)
#if FLOOR_VECTOR_WIDTH >= 2
			, t_vec.y * t_vec.y * ((scalar_type)3 - (scalar_type)2 * t_vec.y)
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			, t_vec.z * t_vec.z * ((scalar_type)3 - (scalar_type)2 * t_vec.z)
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			, t_vec.w * t_vec.w * ((scalar_type)3 - (scalar_type)2 * t_vec.w)
#endif
		};
	}
	
	//////////////////////////////////////////
	// relational
	// NOTE: for logic && and || use the bit-wise operators, which have the same effect on bool#
	
	// comparisons returning component-wise results
	constexpr FLOOR_VECNAME<bool> operator==(const FLOOR_VECNAME& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., ==, FLOOR_COMMA) };
	}
	constexpr FLOOR_VECNAME<bool> operator!=(const FLOOR_VECNAME& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., !=, FLOOR_COMMA) };
	}
	constexpr FLOOR_VECNAME<bool> operator<(const FLOOR_VECNAME& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., <, FLOOR_COMMA) };
	}
	constexpr FLOOR_VECNAME<bool> operator<=(const FLOOR_VECNAME& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., <=, FLOOR_COMMA) };
	}
	constexpr FLOOR_VECNAME<bool> operator>(const FLOOR_VECNAME& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., >, FLOOR_COMMA) };
	}
	constexpr FLOOR_VECNAME<bool> operator>=(const FLOOR_VECNAME& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., >=, FLOOR_COMMA) };
	}
	
	// comparisons returning ANDed component-wise results
	constexpr bool is_equal(const FLOOR_VECNAME& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., ==, &&);
	}
	constexpr bool is_unequal(const FLOOR_VECNAME& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., !=, &&);
	}
	constexpr bool is_less(const FLOOR_VECNAME& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., <, &&);
	}
	constexpr bool is_less_or_equal(const FLOOR_VECNAME& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., <=, &&);
	}
	constexpr bool is_greater(const FLOOR_VECNAME& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., >, &&);
	}
	constexpr bool is_greater_or_equal(const FLOOR_VECNAME& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., >=, &&);
	}
	
	// comparisons with an +/- epsilon returning ANDed component-wise results
	constexpr bool is_equal(const FLOOR_VECNAME& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, FLOAT_EQ_EPS, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	constexpr bool is_unequal(const FLOOR_VECNAME& vec, const scalar_type& epsilon) const {
		return !is_equal(vec, epsilon);
	}
	constexpr bool is_less(const FLOOR_VECNAME& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, FLOAT_LT_EPS, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	constexpr bool is_less_or_equal(const FLOOR_VECNAME& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, FLOAT_LE_EPS, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	constexpr bool is_greater(const FLOOR_VECNAME& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, FLOAT_GT_EPS, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	constexpr bool is_greater_or_equal(const FLOOR_VECNAME& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, FLOAT_GE_EPS, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	
	//! returns true if any component is true
	template <class B = scalar_type, class = typename enable_if<is_same<B, bool>::value>::type>
	constexpr bool any() const {
		return FLOOR_VEC_EXPAND(||);
	}
	
	//! returns true if all components are true
	template <class B = scalar_type, class = typename enable_if<is_same<B, bool>::value>::type>
	constexpr bool all() const {
		return FLOOR_VEC_EXPAND(&&);
	}
	
	//! returns true if all components are false
	template <class B = scalar_type, class = typename enable_if<is_same<B, bool>::value>::type>
	constexpr bool none() const {
		return !FLOOR_VEC_EXPAND(||);
	}
	
	//////////////////////////////////////////
	// functional / algorithm
	
	//! same as operator=(vec)
	constexpr FLOOR_VECNAME& set(const FLOOR_VECNAME& vec) {
		*this = vec;
		return *this;
	}
	
	//! same as operator=(scalar)
	constexpr FLOOR_VECNAME& set(const scalar_type& val) {
		*this = val;
		return *this;
	}
	
#if FLOOR_VECTOR_WIDTH >= 2
	//! same as operator=(vector# { val_x, ... })
	constexpr FLOOR_VECNAME& set(const scalar_type& vec_x
								 , const scalar_type& vec_y
#if FLOOR_VECTOR_WIDTH >= 3
								 , const scalar_type& vec_z
#endif
#if FLOOR_VECTOR_WIDTH >= 4
								 , const scalar_type& vec_w
#endif
	) {
		x = vec_x;
		y = vec_y;
#if FLOOR_VECTOR_WIDTH >= 3
		z = vec_z;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = vec_w;
#endif
		return *this;
	}
#endif
	
	//!
	constexpr FLOOR_VECNAME& set_if(const FLOOR_VECNAME<bool>& cond_vec,
									const FLOOR_VECNAME& vec) {
		if(cond_vec.x) x = vec.x;
#if FLOOR_VECTOR_WIDTH >= 2
		if(cond_vec.y) y = vec.y;
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		if(cond_vec.z) z = vec.z;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		if(cond_vec.w) w = vec.w;
#endif
		return *this;
	}
	
	//!
	constexpr FLOOR_VECNAME& set_if(const FLOOR_VECNAME<bool>& cond_vec,
									const scalar_type& val) {
		if(cond_vec.x) x = val;
#if FLOOR_VECTOR_WIDTH >= 2
		if(cond_vec.y) y = val;
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		if(cond_vec.z) z = val;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		if(cond_vec.w) w = val;
#endif
		return *this;
	}
	
#if FLOOR_VECTOR_WIDTH >= 2
	//!
	constexpr FLOOR_VECNAME& set_if(const FLOOR_VECNAME<bool>& cond_vec,
									const scalar_type& vec_x
									, const scalar_type& vec_y
#if FLOOR_VECTOR_WIDTH >= 3
									, const scalar_type& vec_z
#endif
#if FLOOR_VECTOR_WIDTH >= 4
									, const scalar_type& vec_w
#endif
	) {
		if(cond_vec.x) x = vec_x;
		if(cond_vec.y) y = vec_y;
#if FLOOR_VECTOR_WIDTH >= 3
		if(cond_vec.z) z = vec_z;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		if(cond_vec.w) w = vec_w;
#endif
		return *this;
	}
#endif
	
	//! sets each component to the result of the function call with the resp. component (uf(x), ...)
	template <typename unary_function> constexpr FLOOR_VECNAME& apply(unary_function uf) {
		FLOOR_VEC_FUNC_OP_EXPAND(this->, uf, FLOOR_NOP, FLOOR_SEMICOLON, FLOOR_VEC_RHS_NOP,
								 FLOOR_NOP, FLOOR_VEC_ASSIGN_SET);
		return *this;
	}
	
	//! if the corresponding component in cond_vec is true, this will apply / set the component
	//! to the result of the function call with the resp. component
	template <typename unary_function> constexpr FLOOR_VECNAME& apply_if(const FLOOR_VECNAME<bool>& cond_vec,
																		 unary_function uf) {
		if(cond_vec.x) x = uf(x);
#if FLOOR_VECTOR_WIDTH >= 2
		if(cond_vec.y) y = uf(y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		if(cond_vec.z) z = uf(z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		if(cond_vec.w) w = uf(w);
#endif
		return *this;
	}
	
	//! returns the number of components that are equal to value
	constexpr size_t count(const scalar_type& value) const {
		size_t ret = 0;
		if(x == value) ++ret;
#if FLOOR_VECTOR_WIDTH >= 2
		if(y == value) ++ret;
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		if(z == value) ++ret;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		if(w == value) ++ret;
#endif
		return ret;
	}
	
	//! returns the number of components for which the unary function returns true
	template <typename unary_function> constexpr size_t count(unary_function uf) const {
		size_t ret = 0;
		if(uf(x)) ++ret;
#if FLOOR_VECTOR_WIDTH >= 2
		if(uf(y)) ++ret;
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		if(uf(z)) ++ret;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		if(uf(w)) ++ret;
#endif
		return ret;
	}
	
	//!
	constexpr scalar_type average() const {
		return { (FLOOR_VEC_EXPAND(+)) / (scalar_type)FLOOR_VECTOR_WIDTH };
	}
	
	//!
	constexpr scalar_type accumulate() const {
		return { FLOOR_VEC_EXPAND(+) };
	}
	constexpr scalar_type sum() const {
		return accumulate();
	}
	
	//////////////////////////////////////////
	// I/O
	friend ostream& operator<<(ostream& output, const FLOOR_VECNAME& vec) {
		output << "(" << FLOOR_VEC_EXPAND_ENCLOSED(<< ", " <<, vec., ) << ")";
		return output;
	}
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
	
	//////////////////////////////////////////
	// misc
	constexpr FLOOR_VECNAME& min(const FLOOR_VECNAME& vec) {
		*this = this->minned(vec);
		return *this;
	}
	constexpr FLOOR_VECNAME& max(const FLOOR_VECNAME& vec) {
		*this = this->maxed(vec);
		return *this;
	}
	constexpr FLOOR_VECNAME minned(const FLOOR_VECNAME& vec) const {
		return {
			FLOOR_VEC_EXPAND_DUAL_ENCLOSED(vec., FLOOR_COMMA, std::min, , FLOOR_COMMA)
		};
	}
	constexpr FLOOR_VECNAME maxed(const FLOOR_VECNAME& vec) const {
		return {
			FLOOR_VEC_EXPAND_DUAL_ENCLOSED(vec., FLOOR_COMMA, std::max, , FLOOR_COMMA)
		};
	}
	constexpr pair<FLOOR_VECNAME, FLOOR_VECNAME> minmax(const FLOOR_VECNAME& vec) const {
		FLOOR_VECNAME min_vec, max_vec;
		if(x >= vec.x) {
			min_vec[0] = vec.x;
			max_vec[0] = x;
		}
		else {
			min_vec[0] = x;
			max_vec[0] = vec.x;
		}
#if FLOOR_VECTOR_WIDTH >= 2
		if(y >= vec.y) {
			min_vec[1] = vec.y;
			max_vec[1] = y;
		}
		else {
			min_vec[1] = y;
			max_vec[1] = vec.y;
		}
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		if(z >= vec.z) {
			min_vec[2] = vec.z;
			max_vec[2] = z;
		}
		else {
			min_vec[2] = z;
			max_vec[2] = vec.z;
		}
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		if(w >= vec.w) {
			min_vec[3] = vec.w;
			max_vec[3] = w;
		}
		else {
			min_vec[3] = w;
			max_vec[3] = vec.w;
		}
#endif
		return { min_vec, max_vec };
	}
	
	constexpr scalar_type min_element() const {
#if FLOOR_VECTOR_WIDTH == 1
		return x;
#elif FLOOR_VECTOR_WIDTH == 2
		return x <= y ? x : y;
#elif FLOOR_VECTOR_WIDTH == 3
		return x <= y ? (x <= z ? x : z) : (y <= z ? y : z);
#elif FLOOR_VECTOR_WIDTH == 4
		return (x <= y && x <= z && x <= w ? x :
				y <= z ? (y <= w ? y : w) : (z <= w ? z : w));
#endif
	}
	constexpr scalar_type max_element() const {
#if FLOOR_VECTOR_WIDTH == 1
		return x;
#elif FLOOR_VECTOR_WIDTH == 2
		return x >= y ? x : y;
#elif FLOOR_VECTOR_WIDTH == 3
		return x >= y ? (x >= z ? x : z) : (y >= z ? y : z);
#elif FLOOR_VECTOR_WIDTH == 4
		return (x >= y && x >= z && x >= w ? x :
				y >= z ? (y >= w ? y : w) : (z >= w ? z : w));
#endif
	}
	constexpr FLOOR_VECNAME_N(2) minmax_element() const {
		return { min_element(), max_element() };
	}
	
	constexpr size_t min_element_index() const {
#if FLOOR_VECTOR_WIDTH == 1
		return 0u;
#elif FLOOR_VECTOR_WIDTH == 2
		return (x <= y ? 0u : 1u);
#elif FLOOR_VECTOR_WIDTH == 3
		return (x <= y && x <= z) ? 0u : (y <= z ? 1u : 2u);
#elif FLOOR_VECTOR_WIDTH == 4
		return ((x <= y && x <= z && x <= w) ? 0u :
				(y <= z && y <= w ? 1u :
				 (z <= w ? 2u : 3u)));
#endif
	}
	constexpr size_t max_element_index() const {
#if FLOOR_VECTOR_WIDTH == 1
		return 0u;
#elif FLOOR_VECTOR_WIDTH == 2
		return (x >= y ? 0u : 1u);
#elif FLOOR_VECTOR_WIDTH == 3
		return (x >= y && x >= z) ? 0u : (y >= z ? 1u : 2u);
#elif FLOOR_VECTOR_WIDTH == 4
		return ((x >= y && x >= z && x >= w) ? 0u :
				(y >= z && y >= w ? 1u :
				 (z >= w ? 2u : 3u)));
#endif
	}
	constexpr FLOOR_VECNAME_N(2) minmax_element_index() const {
		return { min_element_index(), max_element_index() };
	}
	
	//!
	FLOOR_VEC_FUNC(vector_helper<scalar_type>::abs, abs, absed)
	
	//! linear interpolation
	FLOOR_VEC_FUNC_ARGS_VEC(const_math::interpolate, interpolate, interpolated,
							(const FLOOR_VECNAME& vec, const scalar_type& interp),
							vec., FLOOR_VEC_RHS_VEC,
							interp)
	
	//!
	template <typename signed_type = typename vector_helper<scalar_type>::signed_type,
			  typename enable_if<is_same<scalar_type, signed_type>::value, int>::type = 0>
	constexpr signed_vector sign() const {
		// signed version
		return {
			FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , < (scalar_type)0 ? (signed_type)-1 : (signed_type)1)
		};
	}
	template <typename signed_type = typename vector_helper<scalar_type>::signed_type,
			  typename enable_if<!is_same<scalar_type, signed_type>::value, int>::type = 0>
	constexpr signed_vector sign() const {
		// unsigned version
		return { (signed_type)1 };
	}
	
	//!
	template <typename signed_type = typename vector_helper<scalar_type>::signed_type,
			  typename enable_if<is_same<scalar_type, signed_type>::value, int>::type = 0>
	constexpr FLOOR_VECNAME<bool> signbit() const {
		// signed version
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , < (scalar_type)0) };
	}
	template <typename signed_type = typename vector_helper<scalar_type>::signed_type,
			  typename enable_if<!is_same<scalar_type, signed_type>::value, int>::type = 0>
	constexpr FLOOR_VECNAME<bool> signbit() const {
		// unsigned version
		return { false };
	}
	
	//!
	static FLOOR_VECNAME random(const scalar_type max = (scalar_type)1);
	//!
	static FLOOR_VECNAME random(const scalar_type min, const scalar_type max);
	
	//////////////////////////////////////////
	// TODO: type conversion
	
};

// cleanup macros
#include "math/vector_ops_cleanup.hpp"

#undef FLOOR_VECNAME_CONCAT
#undef FLOOR_VECNAME_EVAL
#undef FLOOR_VECNAME
#undef FLOOR_VECNAME_N
