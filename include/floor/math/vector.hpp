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

#if !defined(FLOOR_VECTOR_WIDTH)
#error "don't include this header file - include vector_lib.hpp"
#endif

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align) // NOTE: except for "long double" this is always aligned correctly

#define FLOOR_VECNAME_CONCAT(vec_width) vector##vec_width
#define FLOOR_VECNAME_EVAL(vec_width) FLOOR_VECNAME_CONCAT(vec_width)
#define FLOOR_VECNAME FLOOR_VECNAME_EVAL(FLOOR_VECTOR_WIDTH)
#define FLOOR_VECNAME_STR_STRINGIFY(vec_name) #vec_name
#define FLOOR_VECNAME_STR_EVAL(vec_name) FLOOR_VECNAME_STR_STRINGIFY(vec_name)
#define FLOOR_VECNAME_STR FLOOR_VECNAME_STR_EVAL(FLOOR_VECNAME)

#include <floor/math/vector_ops.hpp>
#include <floor/math/matrix4.hpp>
#include <type_traits>
#include <tuple>
#include <functional>
#include <compare>

#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
#include <array>
#include <random>

#if !defined(FLOOR_VECTOR_RAND)
#define FLOOR_VECTOR_RAND
//! for internal use only
namespace fl::vector_rng {
	std::mt19937& get_vec_gen();
} // namespace fl::vector_rng
#endif
#endif

// for compute and graphics: signal that this vector type can be converted to the corresponding clang/LLVM vector type
#if defined(FLOOR_DEVICE) && (!defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
#define FLOOR_CLANG_VECTOR_COMPAT __attribute__((vector_compat))
#else
#define FLOOR_CLANG_VECTOR_COMPAT
#endif

namespace fl {

//! This is the general vector class that is backed by N scalar variables, with N = { 1, 2, 3, 4 },
//! and is instantiated for all float types (float, double, long double), all unsigned types from
//! 8-bit to 64-bit and all signed types from 8-bit to 64-bit.
//! This class provides the most vector functionality, almost all of them also usable with constexpr
//! at compile-time (with the exception of obviously non-compile-time things like I/O, string, stream
//! and RNG code). This class however lacks the ability to be used atomically.
//! -- "one vector class to rule them all, ... and in the darkness bind them."
template <typename scalar_type> class FLOOR_CLANG_VECTOR_COMPAT FLOOR_VECNAME {
public:
	// the underlying type of the vector
	// NOTE: only .xyzw are usable with constexpr
	union {
		// main scalar accessors (.xyzw)
		struct {
			scalar_type x;
#if FLOOR_VECTOR_WIDTH >= 2
			scalar_type y;
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			scalar_type z;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			scalar_type w;
#endif
		};
		
		// accessors that are directly usable (.xy, .zw, .yz, .xyz, .yzw)
		// other kinds must be made via a function call
#if FLOOR_VECTOR_WIDTH >= 3
		struct {
			vector2<scalar_type> xy;
#if FLOOR_VECTOR_WIDTH >= 4
			vector2<scalar_type> zw;
#endif
		};
#endif
		
#if FLOOR_VECTOR_WIDTH >= 3
		struct {
			scalar_type __dont_use_0;
			vector2<scalar_type> yz;
		};
#endif
		
#if FLOOR_VECTOR_WIDTH >= 4
		struct {
			vector3<scalar_type> xyz;
		};
		
		struct {
			scalar_type __dont_use_1;
			vector3<scalar_type> yzw;
		};
#endif
		
		// lo/hi accessors
#if FLOOR_VECTOR_WIDTH == 2
		struct {
			vector1<scalar_type> lo;
			vector1<scalar_type> hi;
		};
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		struct {
			vector2<scalar_type> lo;
#if FLOOR_VECTOR_WIDTH == 3
			vector1<scalar_type> hi;
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			vector2<scalar_type> hi;
#endif
		};
#endif
	};
	
	//! "this" vector type
	using vector_type = FLOOR_VECNAME<scalar_type>;
	//! this scalar type
	using this_scalar_type = scalar_type;
	//! decayed scalar type (removes refs/etc.)
#if !defined(FLOOR_DEVICE) || defined(FLOOR_DEVICE_HOST_COMPUTE)
	using decayed_scalar_type = std::decay_t<scalar_type>;
#else // also remove address space for compute
	using decayed_scalar_type = decay_as_t<scalar_type>;
#endif
	//! signed vector type corresponding to this type
	using signed_vector_type = FLOOR_VECNAME<typename vector_helper<decayed_scalar_type>::signed_type>;
	//! dimensionality of this vector type
	static constexpr uint32_t dim() { return FLOOR_VECTOR_WIDTH; }
	
	//! returns true if the vector type has a corresponding clang vector type
	static constexpr bool has_clang_vector_type() {
		return !std::is_same_v<decayed_scalar_type, bool>;
	}
	template <typename vec_type> struct clang_vector_type_spec {};
	template <typename vec_type> requires(vec_type::has_clang_vector_type()) struct clang_vector_type_spec<vec_type> {
		// special handling for half/fp16 types
#if !defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
		using clang_scalar_type = scalar_type;
#else
		using clang_scalar_type = std::conditional_t<!std::is_same_v<scalar_type, half>, scalar_type,
#if FLOOR_HAS_NATIVE_FP16 == 1
												__fp16
#else
												uint16_t
#endif
												>;
#endif
		
		using clang_vector_type = __attribute__((ext_vector_type(FLOOR_VECTOR_WIDTH))) clang_scalar_type;
	};
	template <typename vec_type> requires(!vec_type::has_clang_vector_type()) struct clang_vector_type_spec<vec_type> {
		// would usually use void here, but this makes certain use cases impossible (no void&), even if it is disabled
		// + redirect to a floor vector for better compat (i.e. not having to deal with sema ...)
		using clang_vector_type = FLOOR_VECNAME<int>;
	};
	
	//! corresponding clang vector type
	using clang_vector_type = typename clang_vector_type_spec<vector_type>::clang_vector_type;
	
	//! type of the scalar interpolator when doing linear/cubic/catmull-rom interpolation
	//! NOTE: defaults to float for all non-floating-point types
	using interpolation_scalar_type = std::conditional_t<ext::is_floating_point_v<decayed_scalar_type>, decayed_scalar_type, float>;
	//! type of the vector interpolator when doing linear/cubic/catmull-rom interpolation
	using interpolation_vector_type = FLOOR_VECNAME<interpolation_scalar_type>;
	
	//////////////////////////////////////////
	// constructors and assignment operators
#pragma mark constructors and assignment operators
	
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

	//! default copy-construct from same vector_type
	constexpr FLOOR_VECNAME(const vector_type& vec) noexcept = default;
	
	//! copy-construct from same vector_type (pointer)
	constexpr FLOOR_VECNAME(const vector_type* vec) noexcept :
	FLOOR_VEC_EXPAND_DUAL(vec->, FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT FLOOR_COMMA, FLOOR_PAREN_RIGHT) {}
	
	//! default move-construct from same vector_type (rvalue)
	constexpr FLOOR_VECNAME(vector_type&& vec) noexcept = default;
	
	//! construct by specifying each component
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
	
	//! construct by setting all components to the same scalar value
	constexpr FLOOR_VECNAME(const scalar_type& val) noexcept :
	FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , (val)) {}
	
	//! construct with type conversion from identically-sized vector
	template <typename from_scalar_type>
	constexpr FLOOR_VECNAME(const FLOOR_VECNAME<from_scalar_type>& vec) noexcept :
	FLOOR_VEC_EXPAND_DUAL(vec., FLOOR_PAREN_LEFT (scalar_type), FLOOR_PAREN_RIGHT FLOOR_COMMA, FLOOR_PAREN_RIGHT) {}
	
	// construction from lower types
#if FLOOR_VECTOR_WIDTH == 2
	//! construction from <vector1, scalar>
	constexpr FLOOR_VECNAME(const vector1<scalar_type>& vec,
							const scalar_type val = (scalar_type)0) noexcept :
	x(vec.x), y(val) {}
#endif
#if FLOOR_VECTOR_WIDTH == 3
	//! construction from <vector1, scalar, scalar>
	constexpr FLOOR_VECNAME(const vector1<scalar_type>& vec,
							const scalar_type val_y = (scalar_type)0,
							const scalar_type val_z = (scalar_type)0) noexcept :
	x(vec.x), y(val_y), z(val_z) {}
	//! construction from <vector2, scalar>
	constexpr FLOOR_VECNAME(const vector2<scalar_type>& vec,
							const scalar_type val = (scalar_type)0) noexcept :
	x(vec.x), y(vec.y), z(val) {}
	//! construction from <scalar, vector2>
	constexpr FLOOR_VECNAME(const scalar_type& val,
							const vector2<scalar_type>& vec) noexcept :
	x(val), y(vec.x), z(vec.y) {}
#endif
#if FLOOR_VECTOR_WIDTH == 4
	//! construction from <vector1, scalar, scalar, scalar>
	constexpr FLOOR_VECNAME(const vector1<scalar_type>& vec,
							const scalar_type val_y = (scalar_type)0,
							const scalar_type val_z = (scalar_type)0,
							const scalar_type val_w = (scalar_type)0) noexcept :
	x(vec.x), y(val_y), z(val_z), w(val_w) {}
	//! construction from <vector2, scalar, scalar>
	constexpr FLOOR_VECNAME(const vector2<scalar_type>& vec,
							const scalar_type val_z = (scalar_type)0,
							const scalar_type val_w = (scalar_type)0) noexcept :
	x(vec.x), y(vec.y), z(val_z), w(val_w) {}
	//! construction from <scalar, vector2, scalar>
	constexpr FLOOR_VECNAME(const scalar_type& val_x,
							const vector2<scalar_type>& vec,
							const scalar_type& val_w) noexcept :
	x(val_x), y(vec.x), z(vec.y), w(val_w) {}
	//! construction from <scalar, scalar, vector2>
	constexpr FLOOR_VECNAME(const scalar_type& val_x,
							const scalar_type& val_y,
							const vector2<scalar_type>& vec) noexcept :
	x(val_x), y(val_y), z(vec.x), w(vec.y) {}
	//! construction from <vector2, vector2>
	constexpr FLOOR_VECNAME(const vector2<scalar_type>& vec_lo,
							const vector2<scalar_type>& vec_hi) noexcept :
	x(vec_lo.x), y(vec_lo.y), z(vec_hi.x), w(vec_hi.y) {}
	
	//! construction from <vector3, scalar>
	constexpr FLOOR_VECNAME(const vector3<scalar_type>& vec,
							const scalar_type val_w = (scalar_type)0) noexcept :
	x(vec.x), y(vec.y), z(vec.z), w(val_w) {}
	//! construction from <scalar, vector3>
	constexpr FLOOR_VECNAME(const scalar_type& val_x,
							const vector3<scalar_type>& vec) noexcept :
	x(val_x), y(vec.x), z(vec.y), w(vec.z) {}
#endif
	
	//! construction from an equally sized array
	constexpr FLOOR_VECNAME(const scalar_type (&arr)[FLOOR_VECTOR_WIDTH]) noexcept :
	x(arr[0])
#if FLOOR_VECTOR_WIDTH >= 2
	, y(arr[1])
#endif
#if FLOOR_VECTOR_WIDTH >= 3
	, z(arr[2])
#endif
#if FLOOR_VECTOR_WIDTH >= 4
	, w(arr[3])
#endif
	{}
	
	//! construction from an equally sized std::array
	constexpr FLOOR_VECNAME(const std::array<scalar_type, FLOOR_VECTOR_WIDTH>& arr) noexcept :
	x(arr[0])
#if FLOOR_VECTOR_WIDTH >= 2
	, y(arr[1])
#endif
#if FLOOR_VECTOR_WIDTH >= 3
	, z(arr[2])
#endif
#if FLOOR_VECTOR_WIDTH >= 4
	, w(arr[3])
#endif
	{}

	//! default copy assignment
	constexpr vector_type& operator=(const vector_type& vec) noexcept = default;
	
	//! default move assignment
	constexpr vector_type& operator=(vector_type&& vec) noexcept = default;
	
	//! copy assignment with decayed_scalar_type != scalar_type (e.g. scalar_type is a reference)
	floor_inline_always constexpr vector_type& operator=(const FLOOR_VECNAME<decayed_scalar_type>& vec) noexcept
	requires(!std::is_same_v<FLOOR_VECNAME<decayed_scalar_type>, vector_type>) {
		FLOOR_VEC_EXPAND_DUAL(vec., =, FLOOR_SEMICOLON, FLOOR_SEMICOLON)
		return *this;
	}
	
	//! move assignment with decayed_scalar_type != scalar_type (e.g. scalar_type is a reference)
	floor_inline_always constexpr vector_type& operator=(FLOOR_VECNAME<decayed_scalar_type>&& vec) noexcept
	requires(!std::is_same_v<FLOOR_VECNAME<decayed_scalar_type>, vector_type>) {
		FLOOR_VEC_EXPAND_DUAL(vec., =, FLOOR_SEMICOLON, FLOOR_SEMICOLON)
		return *this;
	}

	//! assignment from scalar value -> set all components
	floor_inline_always constexpr vector_type& operator=(const decayed_scalar_type& val) noexcept {
		FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_SEMICOLON, , = val, FLOOR_SEMICOLON)
		return *this;
	}
	
	//! assignment from lower vector types
#if FLOOR_VECTOR_WIDTH >= 3
	floor_inline_always constexpr vector_type& operator=(const vector2<decayed_scalar_type>& vec) noexcept {
		x = vec.x;
		y = vec.y;
		return *this;
	}
#endif
#if FLOOR_VECTOR_WIDTH >= 4
	floor_inline_always constexpr vector_type& operator=(const vector3<decayed_scalar_type>& vec) noexcept {
		x = vec.x;
		y = vec.y;
		z = vec.z;
		return *this;
	}
#endif
	
	//////////////////////////////////////////
	// constants
	
#if FLOOR_VECTOR_WIDTH == 3
	
	//! +X right vector
	static constexpr vector3<decayed_scalar_type> right() {
		return { decayed_scalar_type(1), decayed_scalar_type(0), decayed_scalar_type(0) };
	}
	//! -X left vector
	static constexpr vector3<decayed_scalar_type> left() {
		return { decayed_scalar_type(-1), decayed_scalar_type(0), decayed_scalar_type(0) };
	}
	//! +Y up vector
	static constexpr vector3<decayed_scalar_type> up() {
		return { decayed_scalar_type(0), decayed_scalar_type(1), decayed_scalar_type(0) };
	}
	//! -Y down vector
	static constexpr vector3<decayed_scalar_type> down() {
		return { decayed_scalar_type(0), decayed_scalar_type(-1), decayed_scalar_type(0) };
	}
	//! -Z forward vector
	static constexpr vector3<decayed_scalar_type> forward() {
		return { decayed_scalar_type(0), decayed_scalar_type(0), decayed_scalar_type(-1) };
	}
	//! +Z backward vector
	static constexpr vector3<decayed_scalar_type> backward() {
		return { decayed_scalar_type(0), decayed_scalar_type(0), decayed_scalar_type(1) };
	}
	
#endif
	
	//////////////////////////////////////////
	// access
#pragma mark access
	
	//! const subscript access, with index in [0, #components - 1]
	//! NOTE: prefer using the named accessors (these don't require a reinterpret_cast)
	//! NOTE: not constexpr if index is not const due to the reinterpret_cast
	constexpr const scalar_type& operator[](const uint32_t& index) const {
		__builtin_assume(index < FLOOR_VECTOR_WIDTH);
		if consteval {
			std::array arr { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, std::cref FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT) };
			return arr[index];
		}
		return ((const scalar_type*)this)[index];
	}
	
	//! subscript access, with index in [0, #components - 1]
	//! NOTE: prefer using the named accessors (these don't require a reinterpret_cast)
	//! NOTE: not constexpr if index is not const due to the reinterpret_cast
	constexpr scalar_type& operator[](const uint32_t& index) {
		__builtin_assume(index < FLOOR_VECTOR_WIDTH);
		if consteval {
			std::array arr { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, std::ref FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT) };
			return arr[index];
		}
		return ((scalar_type*)this)[index];
	}
	
	//! constexpr subscript access, with index out of bounds
	constexpr const scalar_type& operator[](const uint32_t& index) const
	__attribute__((enable_if(index >= FLOOR_VECTOR_WIDTH, "index out of bounds"), unavailable("index out of bounds")));
	//! constexpr subscript access, with index out of bounds
	constexpr scalar_type& operator[](const uint32_t& index)
	__attribute__((enable_if(index >= FLOOR_VECTOR_WIDTH, "index out of bounds"), unavailable("index out of bounds")));
	
	//! c array style access (not enabled if scalar_type is a reference)
	template <typename ptr_base_type = scalar_type> requires(!std::is_reference_v<ptr_base_type>)
	const ptr_base_type* data() const {
		return (const ptr_base_type*)this;
	}
	
	//! c array style access (not enabled if scalar_type is a reference)
	template <typename ptr_base_type = scalar_type> requires(!std::is_reference_v<ptr_base_type>)
	ptr_base_type* data() {
		return (ptr_base_type*)this;
	}
	
	//! constexpr helper function to get a const& to a vector component from an index
	template <uint32_t index>
	static constexpr const scalar_type& component_select(const vector_type& vec) {
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
	
	//! uses ternary select rather than array access to select the vector component identified by "index" in [0, vector-dim)
	//! NOTE: out-of-bounds indices will return the last component
	constexpr scalar_type& component_select(const uint32_t index) {
#if FLOOR_VECTOR_WIDTH == 1
		(void)index;
		return x;
#elif FLOOR_VECTOR_WIDTH == 2
		return (index == 0 ? x : y);
#elif FLOOR_VECTOR_WIDTH == 3
		return (index == 0 ? x : (index == 1 ? y : z));
#elif FLOOR_VECTOR_WIDTH == 4
		return (index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w)));
#endif
	}
	
	//! uses ternary select rather than array access to select the vector component identified by "index" in [0, vector-dim)
	//! NOTE: out-of-bounds indices will return the last component
	constexpr const scalar_type& component_select(const uint32_t index) const {
#if FLOOR_VECTOR_WIDTH == 1
		(void)index;
		return x;
#elif FLOOR_VECTOR_WIDTH == 2
		return (index == 0 ? x : y);
#elif FLOOR_VECTOR_WIDTH == 3
		return (index == 0 ? x : (index == 1 ? y : z));
#elif FLOOR_VECTOR_WIDTH == 4
		return (index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w)));
#endif
	}
	
	//! swizzles this vector, according to the specified component indices
	template <uint32_t c0
#if FLOOR_VECTOR_WIDTH >= 2
			  , uint32_t c1
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			  , uint32_t c2
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			  , uint32_t c3
#endif
			 >
	constexpr vector_type& swizzle() {
#if FLOOR_VECTOR_WIDTH >= 2 // rather pointless for vector1
		const auto tmp = *this;
		x = component_select<c0>(tmp);
		y = component_select<c1>(tmp);
#if FLOOR_VECTOR_WIDTH >= 3
		z = component_select<c2>(tmp);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = component_select<c3>(tmp);
#endif
#endif
		return *this;
	}
	
	//! returns a swizzled version of this vector, according to the specified component indices
	template <uint32_t c0
#if FLOOR_VECTOR_WIDTH >= 2
			  , uint32_t c1
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			  , uint32_t c2
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			  , uint32_t c3
#endif
			 >
	constexpr vector_type swizzled() const {
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
	
	//! returns the corresponding index to the specified component char/name
	template <char c> static constexpr uint32_t char_to_index() {
		switch(c) {
			case 'x':
			case 'r':
			case 's':
			case '0':
				return 0;
			case 'y':
			case 'g':
			case 't':
			case '1':
				return 1;
			case 'z':
			case 'b':
			case 'p':
			case '2':
				return 2;
			case 'w':
			case 'a':
			case 'q':
			case '3':
				return 3;
			default: break;
		}
		return ~0u;
	}
	
	//! depending on how many indices are valid in correct order (!), this returns the corresponding vector type
	//! e.g.: <0, 1, 2, ~0u> -> vector3; <0, ~0u, 1, 2> -> vector1; ...
	//! this will also fail when trying to access a component/index that is invalid for the vector width of this type
	//! -> trying to use index 2 / component .z if this is a vector2 or vector1 will fail
	template <uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3>
	struct vector_ref_type_from_indices {
		static_assert(i0 < FLOOR_VECTOR_WIDTH, "invalid index - first component must be valid!");
		static_assert(i1 < FLOOR_VECTOR_WIDTH || i1 == ~0u, "invalid index");
		static_assert(i2 < FLOOR_VECTOR_WIDTH || i2 == ~0u, "invalid index");
		static_assert(i3 < FLOOR_VECTOR_WIDTH || i3 == ~0u, "invalid index");
		
		static constexpr uint32_t valid_components() {
			if(i0 >= FLOOR_VECTOR_WIDTH) return 0; // shouldn't happen, but just in case (static_assert should trigger first)
			if(i1 >= FLOOR_VECTOR_WIDTH) return 1;
			if(i2 >= FLOOR_VECTOR_WIDTH) return 2;
			if(i3 >= FLOOR_VECTOR_WIDTH) return 3;
			return 4;
		}
		
		using type = std::conditional_t<valid_components() == 1, vector1<decayed_scalar_type&>,
					 std::conditional_t<valid_components() == 2, vector2<decayed_scalar_type&>,
					 std::conditional_t<valid_components() == 3, vector3<decayed_scalar_type&>,
					 std::conditional_t<valid_components() == 4, vector4<decayed_scalar_type&>, void>>>>;
	};
	
	//! don't use this, use ref or ref_idx instead!
	template <uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3, uint32_t valid_components> requires(valid_components == 1)
	constexpr vector1<decayed_scalar_type&> _create_vec_ref() {
		return { (*this)[i0] };
	}
	//! don't use this, use ref or ref_idx instead!
	template <uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3, uint32_t valid_components> requires(valid_components == 2)
	constexpr vector2<decayed_scalar_type&> _create_vec_ref() {
		return { (*this)[i0], (*this)[i1] };
	}
	//! don't use this, use ref or ref_idx instead!
	template <uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3, uint32_t valid_components> requires(valid_components == 3)
	constexpr vector3<decayed_scalar_type&> _create_vec_ref() {
		return { (*this)[i0], (*this)[i1], (*this)[i2] };
	}
	//! don't use this, use ref or ref_idx instead!
	template <uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3, uint32_t valid_components> requires(valid_components == 4)
	constexpr vector4<decayed_scalar_type&> _create_vec_ref() {
		return { (*this)[i0], (*this)[i1], (*this)[i2], (*this)[i3] };
	}
	
	//! creates a vector with components referencing the components of this vector in an arbitrary order.
	//! components are specified via their name ('x', 'w', 'r', etc.)
	//! NOTE: opencl/glsl style vectors referencing the same component multiple types can be created!
	template <char c0, char c1 = '_', char c2 = '_', char c3 = '_',
			  uint32_t i0 = char_to_index<c0>(), uint32_t i1 = char_to_index<c1>(),
			  uint32_t i2 = char_to_index<c2>(), uint32_t i3 = char_to_index<c3>(),
			  class vec_return_type = typename vector_ref_type_from_indices<i0, i1, i2, i3>::type,
			  uint32_t valid_components = vector_ref_type_from_indices<i0, i1, i2, i3>::valid_components()>
	constexpr vec_return_type ref() {
		static_assert(!std::is_same_v<vec_return_type, void>, "invalid index");
		return _create_vec_ref<i0, i1, i2, i3, valid_components>();
	}
	
	//! creates a vector with components referencing the components of this vector in an arbitrary order
	//! components are specified via their index (0, 1, 2 or 3)
	//! NOTE: opencl/glsl style vectors referencing the same component multiple types can be created!
	template <uint32_t i0, uint32_t i1 = ~0u, uint32_t i2 = ~0u, uint32_t i3 = ~0u,
			  class vec_return_type = typename vector_ref_type_from_indices<i0, i1, i2, i3>::type,
			  uint32_t valid_components = vector_ref_type_from_indices<i0, i1, i2, i3>::valid_components()>
	constexpr vec_return_type ref_idx() {
		static_assert(!std::is_same_v<vec_return_type, void>, "invalid index");
		return _create_vec_ref<i0, i1, i2, i3, valid_components>();
	}
	
	//! returns a copy of this vector that is trimmed to "target_dim" components
	template <uint32_t target_dim>
	floor_inline_always constexpr auto trim() const {
		static_assert(target_dim != 0, "target dim can't be 0");
		static_assert(target_dim <= FLOOR_VECTOR_WIDTH, "specified target dim must be <= dim of this vector");
		if constexpr(target_dim == 1) {
			return vector1<scalar_type> { x };
		}
#if FLOOR_VECTOR_WIDTH >= 2
		else if constexpr(target_dim == 2) {
			return vector2<scalar_type> { x, y };
		}
#if FLOOR_VECTOR_WIDTH >= 3
		else if constexpr(target_dim == 3) {
			return vector3<scalar_type> { x, y, z };
		}
#if FLOOR_VECTOR_WIDTH >= 4
		else {
			return vector4<scalar_type> { x, y, z, w };
		}
#endif
#endif
#endif
	}
	
#if FLOOR_VECTOR_WIDTH >= 3
	//! additional combined X and Z component accessor (copy), since they are not directly accessible together
	constexpr vector2<scalar_type> xz() const {
		return { x, z };
	}
#endif
	
#if FLOOR_VECTOR_WIDTH >= 4
	//! additional combined X and W component accessor (copy), since they are not directly accessible together
	constexpr vector2<scalar_type> xw() const {
		return { x, w };
	}
	
	//! additional combined Y and W component accessor (copy), since they are not directly accessible together
	constexpr vector2<scalar_type> yw() const {
		return { y, w };
	}
#endif
	
	//////////////////////////////////////////
	// basic ops
#pragma mark basic ops
	
	//! component-wise addition
	FLOOR_VEC_OP(+)
	//! component-wise subtraction
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
	FLOOR_VEC_UNARY_OP_FUNC(!, unary_not)
	
#if FLOOR_VECTOR_WIDTH == 1
	//! M * v, 4x4 matrix * vector1 multiplication
	//! -> implicit vector4 with .y = 0, .z = 0 and .w = 1 and dropping the bottom three rows of the matrix
	constexpr vector_type operator*(const matrix4<decayed_scalar_type>& mat) const {
		return {
			mat.data[0] * x + mat.data[12],
		};
	}
#endif
#if FLOOR_VECTOR_WIDTH == 2
	//! M * v, 4x4 matrix * vector2 multiplication
	//! -> implicit vector4 with .z = 0 and .w = 1 and dropping the bottom two rows of the matrix
	constexpr vector_type operator*(const matrix4<decayed_scalar_type>& mat) const {
		return {
			mat.data[0] * x + mat.data[4] * y + mat.data[12],
			mat.data[1] * x + mat.data[5] * y + mat.data[13],
		};
	}
#endif
#if FLOOR_VECTOR_WIDTH == 3
	//! M * v, 4x4 matrix * vector3 multiplication
	//! -> implicit vector4 with .w = 1 and dropping the bottom row of the matrix
	constexpr vector_type operator*(const matrix4<decayed_scalar_type>& mat) const {
		return {
			mat.data[0] * x + mat.data[4] * y + mat.data[8] * z + mat.data[12],
			mat.data[1] * x + mat.data[5] * y + mat.data[9] * z + mat.data[13],
			mat.data[2] * x + mat.data[6] * y + mat.data[10] * z + mat.data[14],
		};
	}
#endif
#if FLOOR_VECTOR_WIDTH == 4
	//! M * v, 4x4 matrix * vector4 multiplication
	constexpr vector_type operator*(const matrix4<decayed_scalar_type>& mat) const {
		return {
			mat.data[0] * x + mat.data[4] * y + mat.data[8] * z + mat.data[12] * w,
			mat.data[1] * x + mat.data[5] * y + mat.data[9] * z + mat.data[13] * w,
			mat.data[2] * x + mat.data[6] * y + mat.data[10] * z + mat.data[14] * w,
			mat.data[3] * x + mat.data[7] * y + mat.data[11] * z + mat.data[15] * w,
		};
	}
#endif
	//! v = M * v, with M = 4x4 matrix
	constexpr vector_type& operator*=(const matrix4<decayed_scalar_type>& mat) {
		*this = *this * mat;
		return *this;
	}
	
	//! fused-multiply-add assign of "this = (this * b_vec) + c_vec"
	constexpr vector_type& fma(const vector_type& b_vec, const vector_type& c_vec) {
		x = vector_helper<decayed_scalar_type>::fma(x, b_vec.x, c_vec.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = vector_helper<decayed_scalar_type>::fma(y, b_vec.y, c_vec.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = vector_helper<decayed_scalar_type>::fma(z, b_vec.z, c_vec.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = vector_helper<decayed_scalar_type>::fma(w, b_vec.w, c_vec.w);
#endif
		return *this;
	}
	//! fused-multiply-add copy of "(this * b_vec) + c_vec"
	constexpr vector_type fmaed(const vector_type& b_vec, const vector_type& c_vec) const {
		return {
			vector_helper<decayed_scalar_type>::fma(x, b_vec.x, c_vec.x)
#if FLOOR_VECTOR_WIDTH >= 2
			, vector_helper<decayed_scalar_type>::fma(y, b_vec.y, c_vec.y)
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			, vector_helper<decayed_scalar_type>::fma(z, b_vec.z, c_vec.z)
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			, vector_helper<decayed_scalar_type>::fma(w, b_vec.w, c_vec.w)
#endif
		};
	}
	
	//////////////////////////////////////////
	// bit ops
#pragma mark bit ops
	
	//! component-wise bit-wise OR
	//! NOTE: if this is a floating point type vector, the second argument is the unsigned integral equivalent type
	//! (e.g. float -> uint32_t) and this function is _not_ constexpr due to the necessary reinterpret_cast
	FLOOR_VEC_OP_FUNC_SPEC_ARG_TYPE(|, bit_or, ext::integral_eqv_t<decayed_scalar_type>)
	//! component-wise bit-wise AND
	//! NOTE: if this is a floating point type vector, the second argument is the unsigned integral equivalent type
	//! (e.g. float -> uint32_t) and this function is _not_ constexpr due to the necessary reinterpret_cast
	FLOOR_VEC_OP_FUNC_SPEC_ARG_TYPE(&, bit_and, ext::integral_eqv_t<decayed_scalar_type>)
	//! component-wise bit-wise XOR
	//! NOTE: if this is a floating point type vector, the second argument is the unsigned integral equivalent type
	//! (e.g. float -> uint32_t) and this function is _not_ constexpr due to the necessary reinterpret_cast
	FLOOR_VEC_OP_FUNC_SPEC_ARG_TYPE(^, bit_xor, ext::integral_eqv_t<decayed_scalar_type>)
	//! component-wise left shift
	//! NOTE: if this is a floating point type vector, the second argument is the unsigned integral equivalent type
	//! (e.g. float -> uint32_t) and this function is _not_ constexpr due to the necessary reinterpret_cast
	FLOOR_VEC_OP_FUNC_SPEC_ARG_TYPE(<<, bit_left_shift, ext::integral_eqv_t<decayed_scalar_type>)
	//! component-wise right shift
	//! NOTE: if this is a floating point type vector, the second argument is the unsigned integral equivalent type
	//! (e.g. float -> uint32_t) and this function is _not_ constexpr due to the necessary reinterpret_cast
	FLOOR_VEC_OP_FUNC_SPEC_ARG_TYPE(>>, bit_right_shift, ext::integral_eqv_t<decayed_scalar_type>)
	//! component-wise bit-wise complement
	FLOOR_VEC_UNARY_OP_FUNC(~, unary_complement)
	
	//////////////////////////////////////////
	// testing
#pragma mark testing
	
	//! returns true if all components are 0
	constexpr bool is_null() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(&&, FLOOR_PAREN_LEFT, == (scalar_type)0 FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if all components are finite values
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	constexpr bool is_finite() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(&&, __builtin_isfinite FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if any component is NaN
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	constexpr bool is_nan() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(||, __builtin_isnan FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if any component is inf
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	constexpr bool is_inf() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(||, __builtin_isinf FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//! returns true if all components are normal
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	constexpr bool is_normal() const {
		return FLOOR_VEC_EXPAND_ENCLOSED(&&, __builtin_isnormal FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT);
	}
	
	//////////////////////////////////////////
	// rounding / clamping / wrapping
#pragma mark rounding / clamping / wrapping
	
	//! rounds towards nearest integer value in fp format and halfway cases away from 0
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::round, round, rounded)
	//! rounds downwards, to the largest integer value in fp format that is not greater than the current value
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::floor, floor, floored)
	//! rounds upwards, to the smallest integer value in fp format that is not less than the current value
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::ceil, ceil, ceiled)
	//! computes the nearest integer value in fp format that is not greater in magnitude than the current value
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::trunc, trunc, truncated)
	//! rounds to an integer value in fp format using the current rounding mode
	//! NOTE: if constexpr, this will return the same as floor(...)
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::rint, rint, rinted)
	
	//! clamps all components of this vector to [min, max]
	FLOOR_VEC_FUNC_ARGS(math::clamp, clamp, clamped,
						(const scalar_type& min, const scalar_type& max),
						min, max)
	//! clamps all components of this vector to [0, max]
	FLOOR_VEC_FUNC_ARGS(math::clamp, clamp, clamped,
						(const scalar_type& max),
						max)
	
	//! clamps all components of this vector to [min, max]
	constexpr vector_type& clamp(const vector_type& min, const vector_type& max) {
		x = math::clamp(x, min.x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = math::clamp(y, min.y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = math::clamp(z, min.z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = math::clamp(w, min.w, max.w);
#endif
		return *this;
	}
	//! clamps all components of this vector to [min, max]
	constexpr vector_type clamped(const vector_type& min, const vector_type& max) const {
		return vector_type(*this).clamp(min, max);
	}
	//! clamps all components of this vector to [0, max]
	constexpr vector_type& clamp(const vector_type& max) {
		x = math::clamp(x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = math::clamp(y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = math::clamp(z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = math::clamp(w, max.w);
#endif
		return *this;
	}
	//! clamps all components of this vector to [0, max]
	constexpr vector_type clamped(const vector_type& max) const {
		return vector_type(*this).clamp(max);
	}
	
	//! wraps all components of this vector around/to [0, max)
	FLOOR_VEC_FUNC_ARGS(math::wrap, wrap, wrapped,
						(const scalar_type& max),
						max)
	
	//! signed wrapping of all components of this vector around/to [-max, max)
	//! NOTE: for unsigned types, this wraps around/to [0, max)
	FLOOR_VEC_FUNC_ARGS(math::swrap, swrap, swrapped,
						(const scalar_type& max),
						max)
	
	//! mirrored/alternating wrapping of all components of this vector around/to [0, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	FLOOR_VEC_FUNC_ARGS(math::mwrap, mwrap, mwrapped,
						(const scalar_type& max),
						max)
	
	//! signed mirrored/alternating wrapping of all components of this vector around/to [-max, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	//! NOTE: for unsigned types, this wraps around/to [0, max]
	FLOOR_VEC_FUNC_ARGS(math::mswrap, mswrap, mswrapped,
						(const scalar_type& max),
						max)
	
	//! shingled mirrored/alternating wrapping of all components of this vector around/to [0, max),
	//! i.e. creating a triangle/zigzag signal from a linear input with Y-gaps in repeats
	//! ref: https://www.desmos.com/calculator/b4xbbrsqmy
	FLOOR_VEC_FUNC_ARGS(math::shmwrap, shmwrap, shmwrapped,
						(const scalar_type& max, const scalar_type& gap),
						max, gap)
	
	//! wraps all components of this vector around/to [0, max)
	constexpr vector_type& wrap(const vector_type& max) {
		x = math::wrap(x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = math::wrap(y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = math::wrap(z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = math::wrap(w, max.w);
#endif
		return *this;
	}
	//! wraps all components of this vector around/to [0, max)
	constexpr vector_type wrapped(const vector_type& max) const {
		return vector_type(*this).wrap(max);
	}
	
	//! signed wrapping of all components of this vector around/to [-max, max)
	//! NOTE: for unsigned types, this wraps around/to [0, max)
	constexpr vector_type& swrap(const vector_type& max) {
		x = math::swrap(x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = math::swrap(y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = math::swrap(z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = math::swrap(w, max.w);
#endif
		return *this;
	}
	//! signed wrapping of all components of this vector around/to [-max, max)
	//! NOTE: for unsigned types, this wraps around/to [0, max]
	constexpr vector_type swrapped(const vector_type& max) const {
		return vector_type(*this).swrap(max);
	}
	
	//! mirrored/alternating wrapping of all components of this vector around/to [0, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	constexpr vector_type& mwrap(const vector_type& max) {
		x = math::mwrap(x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = math::mwrap(y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = math::mwrap(z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = math::mwrap(w, max.w);
#endif
		return *this;
	}
	//! signed mirrored/alternating wrapping of all components of this vector around/to [0, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	constexpr vector_type mwrapped(const vector_type& max) const {
		return vector_type(*this).mwrap(max);
	}
	
	//! signed mirrored/alternating wrapping of all components of this vector around/to [-max, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	//! NOTE: for unsigned types, this wraps around/to [0, max]
	constexpr vector_type& mswrap(const vector_type& max) {
		x = math::mswrap(x, max.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = math::mswrap(y, max.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = math::mswrap(z, max.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = math::mswrap(w, max.w);
#endif
		return *this;
	}
	//! signed mirrored/alternating wrapping of all components of this vector around/to [-max, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	//! NOTE: for unsigned types, this wraps around/to [0, max]
	constexpr vector_type mswrapped(const vector_type& max) const {
		return vector_type(*this).mswrap(max);
	}
	
	//! shingled mirrored/alternating wrapping of all components of this vector around/to [0, max),
	//! i.e. creating a triangle/zigzag signal from a linear input with Y-gaps in repeats
	constexpr vector_type& shmwrap(const vector_type& max, const vector_type& gap) {
		x = math::shmwrap(x, max.x, gap.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = math::shmwrap(y, max.y, gap.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = math::shmwrap(z, max.z, gap.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = math::shmwrap(w, max.w, gap.w);
#endif
		return *this;
	}
	//! shingled signed mirrored/alternating wrapping of all components of this vector around/to [0, max),
	//! i.e. creating a triangle/zigzag signal from a linear input with Y-gaps in repeats
	constexpr vector_type shmwrapped(const vector_type& max, const vector_type& gap) const {
		return vector_type(*this).shmwrap(max, gap);
	}
	
	//! rounds the components of this vector to next multiple of "multiple"
	//! (e.g. { 17, 32, 33 }.round_next_multiple(32) == { 32, 32, 64 })
	constexpr vector_type rounded_next_multiple(const scalar_type& multiple) const {
		const auto mod = *this % multiple;
		return {
			mod.x == (scalar_type)0 ? (x == (scalar_type)0 ? multiple : x) : x - mod.x + multiple,
#if FLOOR_VECTOR_WIDTH >= 2
			mod.y == (scalar_type)0 ? (y == (scalar_type)0 ? multiple : y) : y - mod.y + multiple,
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			mod.z == (scalar_type)0 ? (z == (scalar_type)0 ? multiple : z) : z - mod.z + multiple,
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			mod.w == (scalar_type)0 ? (w == (scalar_type)0 ? multiple : w) : w - mod.w + multiple,
#endif
		};
	}
	//! rounds the components of this vector to next multiple of "multiple"
	constexpr vector_type& round_next_multiple(const scalar_type& multiple) {
		*this = rounded_next_multiple(multiple);
		return *this;
	}
	
	//! rounds each component of this vector to next multiple of each component in "multiple_vec"
	constexpr vector_type rounded_next_multiple(const vector_type& multiple_vec) const {
		const auto mod = *this % multiple_vec;
		return {
			mod.x == (scalar_type)0 ? (x == (scalar_type)0 ? multiple_vec.x : x) : x - mod.x + multiple_vec.x,
#if FLOOR_VECTOR_WIDTH >= 2
			mod.y == (scalar_type)0 ? (y == (scalar_type)0 ? multiple_vec.y : y) : y - mod.y + multiple_vec.y,
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			mod.z == (scalar_type)0 ? (z == (scalar_type)0 ? multiple_vec.z : z) : z - mod.z + multiple_vec.z,
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			mod.w == (scalar_type)0 ? (w == (scalar_type)0 ? multiple_vec.w : w) : w - mod.w + multiple_vec.w,
#endif
		};
	}
	//! rounds each component of this vector to next multiple of each component in "multiple_vec"
	constexpr vector_type& round_next_multiple(const vector_type& multiple_vec) {
		*this = rounded_next_multiple(multiple_vec);
		return *this;
	}
	
	//! sets the components of this vector to their fractional part
	//! (e.g. { 1.2f, 3.8f } -> { 0.2f, 0.8f }
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	constexpr vector_type& fractional() {
		x = vector_helper<decayed_scalar_type>::fractional(x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = vector_helper<decayed_scalar_type>::fractional(y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = vector_helper<decayed_scalar_type>::fractional(z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = vector_helper<decayed_scalar_type>::fractional(w);
#endif
		return *this;
	}
	
	//! returns a vector which components are set to the fractional part of the components of this vector
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	constexpr vector_type fractionaled() const {
		return {
			vector_helper<decayed_scalar_type>::fractional(x),
#if FLOOR_VECTOR_WIDTH >= 2
			vector_helper<decayed_scalar_type>::fractional(y),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			vector_helper<decayed_scalar_type>::fractional(z),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			vector_helper<decayed_scalar_type>::fractional(w),
#endif
		};
	}
	
	//////////////////////////////////////////
	// geometric
#pragma mark geometric
	
	//! dot product of this vector with itself
	constexpr scalar_type dot() const {
		return FLOOR_VEC_EXPAND_DUAL(, *, +);
	}
	//! dot product of this vector with another vector
	constexpr scalar_type dot(const vector_type& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., *, +);
	}
	
#if FLOOR_VECTOR_WIDTH == 2
	//! sets this vector to a vector perpendicular to this vector (-90° rotation)
	constexpr vector_type& perpendicular() {
		const scalar_type tmp = x;
		x = -y;
		y = tmp;
		return *this;
	}
	//! returns a vector perpendicular to this vector (-90° rotation)
	constexpr vector_type perpendiculared() const {
		return { -y, x };
	}
#endif
#if FLOOR_VECTOR_WIDTH == 3
	//! sets this vector to the cross product of this vector with another vector
	constexpr vector_type& cross(const vector_type& vec) {
		*this = crossed(vec);
		return *this;
	}
	//! computes the cross product of this vector with another vector
	constexpr vector_type crossed(const vector_type& vec) const {
		return {
			y * vec.z - z * vec.y,
			z * vec.x - x * vec.z,
			x * vec.y - y * vec.x
		};
	}
#endif
	
	//! returns the length of this vector
	constexpr scalar_type length() const {
		return vector_helper<decayed_scalar_type>::sqrt(dot());
	}
	
	//! returns the distance between this vector and another vector
	constexpr scalar_type distance(const vector_type& vec) const {
		return (vec - *this).length();
	}
	
	//! returns the squared distance between this vector and another vector
	constexpr scalar_type distance_squared(const vector_type& vec) const {
		return (vec - *this).dot();
	}
	
	//! returns the angle (in [0, pi]) between this vector and another vector
	constexpr scalar_type angle(const vector_type& vec) const {
		// if either vector is 0, there is no angle -> return 0
		if (is_null() || vec.is_null()) {
			return (scalar_type)0;
		}
		// acos(<x, y> / ||x||*||y||)
		return vector_helper<decayed_scalar_type>::acos(this->dot(vec) / (length() * vec.length()));
	}
	
	//! returns the angle (in [0, pi]) between this vector and another vector
	//! ref: https://people.eecs.berkeley.edu/~wkahan/Mindless.pdf
	//! NOTE: according to Kahan, this has better numeric stability than the acos variant used in angle(), but has a higher computational cost
	constexpr scalar_type angle_kahan(const vector_type& vec) const {
		const auto this_len = length();
		const auto vec_len = vec.length();
		return scalar_type(2) * vector_helper<decayed_scalar_type>::atan2((vec_len * *this - this_len * vec).length(),
																		  (vec_len * *this + this_len * vec).length());
	}
	
#if FLOOR_VECTOR_WIDTH == 3
	//! returns the angle (in [-pi, pi]) between this vector and another vector around the specified axis
	//! NOTE: axis must be normalized!
	constexpr scalar_type angle_around_axis(const vector_type& vec, const vector_type& axis) const {
		return vector_helper<decayed_scalar_type>::atan2(this->crossed(vec).dot(axis), this->dot(vec));
	}
#endif
	
	//! normalizes this vector / returns a normalized vector of this vector
	FLOOR_VEC_FUNC_EXT(// multiply each component with "1 / ||vec||"
					   inv_length * ,
					   normalize, normalized,
					   // compute "1 / ||vec||"
					   const scalar_type inv_length = vector_helper<decayed_scalar_type>::rsqrt(dot());
					   )
	
	//! returns N if Nref.dot(I) < 0, else -N
	static constexpr vector_type faceforward(const vector_type& N,
											 const vector_type& I,
											 const vector_type& Nref) {
		return (Nref.dot(I) < ((scalar_type)0) ? N : -N);
	}
	
	//! sets this vector to N if Nref.dot(this) < 0, else to -N
	constexpr vector_type& faceforward(const vector_type& N, const vector_type& Nref) {
		*this = vector_type::faceforward(N, *this, Nref);
		return *this;
	}
	//! returns N if Nref.dot(this) < 0, else -N
	constexpr vector_type faceforwarded(const vector_type& N, const vector_type& Nref) const {
		return vector_type::faceforward(N, *this, Nref);
	}
	
	//! reflection of the incident vector I according to the normal N, N must be normalized
	static constexpr vector_type reflect(const vector_type& N,
										 const vector_type& I) {
		return I - ((scalar_type)2) * N.dot(I) * N;
	}
	
	//! reflects this vector according to the normal N, N must be normalized
	constexpr vector_type& reflect(const vector_type& N) {
		*this = vector_type::reflect(N, *this);
		return *this;
	}
	//! returns the reflected vector of *this according to the normal N, N must be normalized
	constexpr vector_type reflected(const vector_type& N) const {
		return vector_type::reflect(N, *this);
	}
	
	//! N and I must be normalized
	static constexpr vector_type refract(const vector_type& N,
										 const vector_type& I,
										 const scalar_type& eta) {
		const scalar_type dNI = N.dot(I);
		const scalar_type k = ((scalar_type)1) - (eta * eta) * (((scalar_type)1) - dNI * dNI);
		return (k < ((scalar_type)0) ?
				vector_type { (scalar_type)0 } :
				(eta * I - (eta * dNI + vector_helper<decayed_scalar_type>::sqrt(k)) * N));
	}
	
	//! refract this vector (the incident vector) according to the normal N and the refraction index eta,
	//! *this and N must be normalized
	constexpr vector_type& refract(const vector_type& N, const scalar_type& eta) {
		*this = vector_type::refract(N, *this, eta);
		return *this;
	}
	//! returns the refracted vector of *this (the incident vector) according to the normal N and
	//! the refraction index eta, *this and N must be normalized
	constexpr vector_type refracted(const vector_type& N, const scalar_type& eta) const {
		return vector_type::refract(N, *this, eta);
	}
	
	//! tests each component if it is < edge, resulting in 0 if true, else 1,
	//! and returns that result in the resp. component of the return vector
	constexpr vector_type step(const scalar_type& edge) const {
		// x < edge ? 0 : 1
		return {
			FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , < edge ? (scalar_type)0 : (scalar_type)1)
		};
	}
	
	//! tests each component if it is < edge vector component, resulting in 0 if true, else 1,
	//! and returns that result in the resp. component of the return vector
	constexpr vector_type step(const vector_type& edge_vec) const {
		return {
			FLOOR_VEC_EXPAND_DUAL_ENCLOSED(edge_vec., <, , ? (scalar_type)0 : (scalar_type)1, FLOOR_COMMA)
		};
	}
	
	//! for each component: results in 0 if component <= edge_0, 1 if component >= edge_1 and
	//! performs smooth hermite interpolation between 0 and 1 if: edge_0 < component < edge_1
	constexpr vector_type smoothstep(const scalar_type& edge_0, const scalar_type& edge_1) const {
		// t = clamp((x - edge_0) / (edge_1 - edge_0), 0, 1)
		const vector_type t_vec { vector_type {
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
	constexpr vector_type smoothstep(const vector_type& edge_vec_0, const vector_type& edge_vec_1) const {
		// t = clamp((x - edge_0) / (edge_1 - edge_0), 0, 1)
		const vector_type t_vec { vector_type {
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
	
#if FLOOR_VECTOR_WIDTH == 2
	//! rotates this vector counter-clockwise according to angle (in degrees)
	constexpr vector_type& rotate(const scalar_type& angle) {
		return rotate_rad(const_math::deg_to_rad(angle));
	}
	
	//! rotates this vector counter-clockwise according to angle (in rad)
	constexpr vector_type& rotate_rad(const scalar_type& rad_angle) {
		const auto sin_val = vector_helper<decayed_scalar_type>::sin(rad_angle);
		const auto cos_val = vector_helper<decayed_scalar_type>::cos(rad_angle);
		const auto x_tmp = x;
		x = x * cos_val - y * sin_val;
		y = x_tmp * sin_val + y * cos_val;
		return *this;
	}
	
	//! returns a rotated version of this vector, rotation is counter-clockwise according to angle (in degrees)
	constexpr vector_type rotated(const scalar_type& angle) const {
		return rotated_rad(const_math::deg_to_rad(angle));
	}
	
	//! returns a rotated version of this vector, rotation is counter-clockwise according to angle (in rad)
	constexpr vector_type rotated_rad(const scalar_type& rad_angle) const {
		const auto sin_val = vector_helper<decayed_scalar_type>::sin(rad_angle);
		const auto cos_val = vector_helper<decayed_scalar_type>::cos(rad_angle);
		return { x * cos_val - y * sin_val, x * sin_val + y * cos_val };
	}
#endif
#if FLOOR_VECTOR_WIDTH == 3
	//! rotates this vector around the specified rotation axis vector according to angle (in degrees)
	//! ref: https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
	//! NOTE: axis must be normalized
	constexpr vector_type& rotate(const vector_type& axis, const scalar_type& angle) {
		*this = rotated_rad(axis, const_math::deg_to_rad(angle));
		return *this;
	}
	
	//! rotates this vector around the specified rotation axis vector according to angle (in rad)
	//! ref: https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
	//! NOTE: axis must be normalized
	constexpr vector_type& rotate_rad(const vector_type& axis, const scalar_type& rad_angle) {
		*this = rotated_rad(axis, rad_angle);
		return *this;
	}
	
	//! return a rotated version of this vector around the specified rotation axis vector according to angle (in degrees)
	//! ref: https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
	//! NOTE: axis must be normalized
	constexpr vector_type rotated(const vector_type& axis, const scalar_type& angle) const {
		return rotated_rad(axis, const_math::deg_to_rad(angle));
	}
	
	//! return a rotated version of this vector around the specified rotation axis vector according to angle (in rad)
	//! ref: https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
	//! NOTE: axis must be normalized
	constexpr vector_type rotated_rad(const vector_type& axis, const scalar_type& rad_angle) const {
		const auto sin_val = vector_helper<decayed_scalar_type>::sin(rad_angle);
		const auto cos_val = vector_helper<decayed_scalar_type>::cos(rad_angle);
		return { *this * cos_val + crossed(axis) * sin_val + axis * (dot(axis) * (scalar_type(1) - cos_val)) };
	}
#endif
	
	//! assumes that this vector contains angles in degrees and converts them to radians
	FLOOR_VEC_FUNC(const_math::deg_to_rad, to_rad, to_raded)
	
	//! assumes that this vector contains angles in radians and converts them to degrees
	FLOOR_VEC_FUNC(const_math::rad_to_deg, to_deg, to_deged)
	
	//////////////////////////////////////////
	// relational
	// NOTE: for logic && and || use the bit-wise operators, which have the same effect on bool#
#pragma mark relational
	
	// comparisons returning component-wise results
	//! component-wise equal comparison
	constexpr FLOOR_VECNAME<bool> operator==(const vector_type& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., ==, FLOOR_COMMA) };
	}
	//! component-wise unequal comparison
	constexpr FLOOR_VECNAME<bool> operator!=(const vector_type& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., !=, FLOOR_COMMA) };
	}
	//! component-wise less-than comparison (this < another vector)
	constexpr FLOOR_VECNAME<bool> operator<(const vector_type& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., <, FLOOR_COMMA) };
	}
	//! component-wise less-or-equal comparison (this <= another vector)
	constexpr FLOOR_VECNAME<bool> operator<=(const vector_type& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., <=, FLOOR_COMMA) };
	}
	//! component-wise greater-than comparison (this > another vector)
	constexpr FLOOR_VECNAME<bool> operator>(const vector_type& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., >, FLOOR_COMMA) };
	}
	//! component-wise greater-or-equal comparison (this >= another vector)
	constexpr FLOOR_VECNAME<bool> operator>=(const vector_type& vec) const {
		return { FLOOR_VEC_EXPAND_DUAL(vec., >=, FLOOR_COMMA) };
	}
	
	//! three-way comparison
	constexpr std::partial_ordering operator<=>(const vector_type& vec) const {
		if (is_less(vec)) {
			return std::partial_ordering::less;
		} else if (is_greater(vec)) {
			return std::partial_ordering::greater;
		} else if (is_equal(vec)) {
			return std::partial_ordering::equivalent;
		}
		return std::partial_ordering::unordered;
	}
	
	// comparisons returning ANDed component-wise results
	//! returns true if all components are equal to the corresponding components of another vector
	constexpr bool is_equal(const vector_type& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., ==, &&);
	}
	//! returns true if all components are unequal to the corresponding components of another vector
	constexpr bool is_unequal(const vector_type& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., !=, &&);
	}
	//! returns true if all components are less than to the corresponding components of another vector
	constexpr bool is_less(const vector_type& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., <, &&);
	}
	//! returns true if all components are less than or equal to the corresponding components of another vector
	constexpr bool is_less_or_equal(const vector_type& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., <=, &&);
	}
	//! returns true if all components are greater than to the corresponding components of another vector
	constexpr bool is_greater(const vector_type& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., >, &&);
	}
	//! returns true if all components are greater than or equal to the corresponding components of another vector
	constexpr bool is_greater_or_equal(const vector_type& vec) const {
		return FLOOR_VEC_EXPAND_DUAL(vec., >=, &&);
	}
	
	// comparisons with an +/- epsilon returning ANDed component-wise results
	//! returns true if all components are equal to the corresponding components of another vector +/- epsilon
	constexpr bool is_equal(const vector_type& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_equal, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	//! returns true if all components are unequal to the corresponding components of another vector +/- epsilon
	constexpr bool is_unequal(const vector_type& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_unequal, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	//! returns true if all components are less than to the corresponding components of another vector +/- epsilon
	constexpr bool is_less(const vector_type& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_less, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	//! returns true if all components are less than or equal to the corresponding components of another vector +/- epsilon
	constexpr bool is_less_or_equal(const vector_type& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_less_or_equal, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	//! returns true if all components are greater than to the corresponding components of another vector +/- epsilon
	constexpr bool is_greater(const vector_type& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_greater, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	//! returns true if all components are greater than or equal to the corresponding components of another vector +/- epsilon
	constexpr bool is_greater_or_equal(const vector_type& vec, const scalar_type& epsilon) const {
		return FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_greater_or_equal, vec., &&, FLOOR_VEC_RHS_VEC,
										FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon);
	}
	
	//! component-wise equal comparison +/- epsilon
	constexpr FLOOR_VECNAME<bool> is_equal_vec(const vector_type& vec, const scalar_type& epsilon) const {
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_equal, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC,
										  FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon) };
	}
	//! component-wise unequal comparison +/- epsilon
	constexpr FLOOR_VECNAME<bool> is_unequal_vec(const vector_type& vec, const scalar_type& epsilon) const {
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_unequal, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC,
										  FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon) };
	}
	//! component-wise less-than comparison (this < another vector) +/- epsilon
	constexpr FLOOR_VECNAME<bool> is_less_vec(const vector_type& vec, const scalar_type& epsilon) const {
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_less, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC,
										  FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon) };
	}
	//! component-wise less-or-equal comparison (this <= another vector) +/- epsilon
	constexpr FLOOR_VECNAME<bool> is_less_or_equal_vec(const vector_type& vec, const scalar_type& epsilon) const {
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_less_or_equal, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC,
										  FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon) };
	}
	//! component-wise greater-than comparison (this > another vector) +/- epsilon
	constexpr FLOOR_VECNAME<bool> is_greater_vec(const vector_type& vec, const scalar_type& epsilon) const {
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_greater, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC,
										  FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon) };
	}
	//! component-wise greater-or-equal comparison (this >= another vector) +/- epsilon
	constexpr FLOOR_VECNAME<bool> is_greater_or_equal_vec(const vector_type& vec, const scalar_type& epsilon) const {
		return { FLOOR_VEC_FUNC_OP_EXPAND(this->, const_math::is_greater_or_equal, vec., FLOOR_COMMA, FLOOR_VEC_RHS_VEC,
										  FLOOR_COMMA, FLOOR_VEC_ASSIGN_NOP, epsilon) };
	}
	
	//! returns true if any component is true
	template <class B = scalar_type> requires(std::is_same_v<B, bool>)
	constexpr bool any() const {
		return FLOOR_VEC_EXPAND(||);
	}
	
	//! returns true if all components are true
	template <class B = scalar_type> requires(std::is_same_v<B, bool>)
	constexpr bool all() const {
		return FLOOR_VEC_EXPAND(&&);
	}
	
	//! returns true if all components are false
	template <class B = scalar_type> requires(std::is_same_v<B, bool>)
	constexpr bool none() const {
		return !(FLOOR_VEC_EXPAND(||));
	}
	
	//! explicit bool conversion/eval for vector*<bool>: returns true if all components are true
	template <class B = scalar_type> requires(std::is_same_v<B, bool>)
	explicit constexpr operator bool() const {
		return FLOOR_VEC_EXPAND(&&);
	}
	
	//////////////////////////////////////////
	// functional / algorithm
#pragma mark functional / algorithm
	
	//! same as operator=(vec)
	constexpr vector_type& set(const vector_type& vec) {
		*this = vec;
		return *this;
	}
	
	//! same as operator=(scalar)
	constexpr vector_type& set(const scalar_type& val) {
		*this = val;
		return *this;
	}
	
#if FLOOR_VECTOR_WIDTH >= 2
	//! same as operator=(vector# { val_x, ... })
	constexpr vector_type& set(const scalar_type& vec_x
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
	
	//! sets the components of this vector to the components of another vector if the corresponding component
	//! in the condition bool vector is set to true
	constexpr vector_type& set_if(const FLOOR_VECNAME<bool>& cond_vec,
								  const vector_type& vec) {
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
	
	//! sets the components of this vector to a scalar value if the corresponding component in the condition
	//! bool vector is set to true
	constexpr vector_type& set_if(const FLOOR_VECNAME<bool>& cond_vec,
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
	//! sets the components of this vector to the corresponding scalar value if the corresponding component
	//! in the condition bool vector is set to true
	constexpr vector_type& set_if(const FLOOR_VECNAME<bool>& cond_vec,
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
	template <typename unary_function> constexpr vector_type& apply(unary_function uf) {
		FLOOR_VEC_FUNC_OP_EXPAND(this->, uf, FLOOR_NOP, FLOOR_SEMICOLON, FLOOR_VEC_RHS_NOP,
								 FLOOR_NOP, FLOOR_VEC_ASSIGN_SET);
		return *this;
	}
	
	//! if the corresponding component in cond_vec is true, this will apply / set the component
	//! to the result of the function call with the resp. component
	template <typename unary_function> constexpr vector_type& apply_if(const FLOOR_VECNAME<bool>& cond_vec,
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
	
	//! ternary / component-wise select between "true_vec" and "false_vec" based on the value of "cond_vec"
	static constexpr vector_type select(const FLOOR_VECNAME<bool>& cond_vec,
										const vector_type& true_vec,
										const vector_type& false_vec) {
		return {
			cond_vec.x ? true_vec.x : false_vec.x,
#if FLOOR_VECTOR_WIDTH >= 2
			cond_vec.y ? true_vec.y : false_vec.y,
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			cond_vec.z ? true_vec.z : false_vec.z,
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			cond_vec.w ? true_vec.w : false_vec.w,
#endif
		};
	}
	
	//! returns the number of components that are equal to value
	constexpr uint32_t count(const scalar_type& value) const {
		uint32_t ret = 0;
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
	template <typename unary_function> constexpr uint32_t count(unary_function uf) const {
		uint32_t ret = 0;
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
	
	//! sum of all components / #components
	constexpr scalar_type average() const {
		return (scalar_type)((FLOOR_VEC_EXPAND(+)) / (scalar_type)FLOOR_VECTOR_WIDTH);
	}
	
	//! sum of all components
	constexpr scalar_type accumulate() const {
		return (scalar_type)(FLOOR_VEC_EXPAND(+));
	}
	//! sum of all components
	constexpr scalar_type sum() const {
		return accumulate();
	}
	
	//////////////////////////////////////////
	// I/O
#pragma mark I/O
	
#if !defined(FLOOR_NO_MATH_STR)
	
	//! ostream output of this vector
	template <typename nonchar_type = scalar_type> requires(!std::is_same_v<nonchar_type, int8_t> && !std::is_same_v<nonchar_type, uint8_t>)
	friend std::ostream& operator<<(std::ostream& output, const vector_type& vec) {
		output << "(" << FLOOR_VEC_EXPAND_ENCLOSED(<< ", " <<, vec., ) << ")";
		return output;
	}
	//! ostream output of this vector (char version)
	template <typename nonchar_type = scalar_type> requires(std::is_same_v<nonchar_type, int8_t>)
	friend std::ostream& operator<<(std::ostream& output, const vector_type& vec) {
		output << "(" << FLOOR_VEC_EXPAND_ENCLOSED(<< ", " <<, (int32_t)vec., ) << ")";
		return output;
	}
	//! ostream output of this vector (uchar version)
	template <typename nonchar_type = scalar_type> requires(std::is_same_v<nonchar_type, uint8_t>)
	friend std::ostream& operator<<(std::ostream& output, const vector_type& vec) {
		output << "(" << FLOOR_VEC_EXPAND_ENCLOSED(<< ", " <<, (uint32_t)vec., ) << ")";
		return output;
	}
	//! returns a string representation of this vector
	std::string to_string() const {
		std::stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
	
#endif
	
	//////////////////////////////////////////
	// misc
#pragma mark misc
	
	//! returns the extent of this vector (components multiplied with each other)
	constexpr decayed_scalar_type extent() const {
		return FLOOR_VEC_EXPAND(*);
	}
	
	//! assigns the minimum between each component of this vector and the corresponding component of another vector to this vector
	constexpr vector_type& min(const vector_type& vec) {
		*this = this->minned(vec);
		return *this;
	}
	//! assigns the maximum between each component of this vector and the corresponding component of another vector to this vector
	constexpr vector_type& max(const vector_type& vec) {
		*this = this->maxed(vec);
		return *this;
	}
	//! returns a vector of the minimum between each component of this vector and the corresponding component of another vector
	constexpr vector_type minned(const vector_type& vec) const {
		return {
			FLOOR_VEC_EXPAND_DUAL_ENCLOSED(vec., FLOOR_COMMA, vector_helper<decayed_scalar_type>::min, , FLOOR_COMMA)
		};
	}
	//! returns a vector of the maximum between each component of this vector and the corresponding component of another vector
	constexpr vector_type maxed(const vector_type& vec) const {
		return {
			FLOOR_VEC_EXPAND_DUAL_ENCLOSED(vec., FLOOR_COMMA, vector_helper<decayed_scalar_type>::max, , FLOOR_COMMA)
		};
	}
	//! returns the minimum value and maximum value vector of the components of this vector and another vector
	constexpr std::pair<vector_type, vector_type> minmax(const vector_type& vec) const {
		vector_type min_vec, max_vec;
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
	
	//! returns the smallest component
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
	//! returns the largest component
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
#if FLOOR_VECTOR_WIDTH >= 2
	//! returns <minimal component, maximal component>
	constexpr vector2<decayed_scalar_type> minmax_element() const {
		return { min_element(), max_element() };
	}
#endif
	
	//! returns the index of the smallest component
	constexpr uint32_t min_element_index() const {
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
	//! returns the index of the largest component
	constexpr uint32_t max_element_index() const {
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
#if FLOOR_VECTOR_WIDTH >= 2
	//! returns <index minimal component, index maximal component>
	constexpr vector2<uint32_t> minmax_element_index() const {
		return { min_element_index(), max_element_index() };
	}
#endif
	
	//! absolute value/vector
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::abs, abs, absed)
	
	//! linearly interpolates this vector with another vector according to interp
	constexpr vector_type& interpolate(const vector_type& vec, const interpolation_scalar_type& interp) {
		x = const_math::interpolate(x, vec.x, interp);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::interpolate(y, vec.y, interp);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::interpolate(z, vec.z, interp);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::interpolate(w, vec.w, interp);
#endif
		return *this;
	}
	//! linearly interpolates this vector with another vector according to interp
	constexpr vector_type& interpolate(const vector_type& vec, const interpolation_vector_type& interp) {
		x = const_math::interpolate(x, vec.x, interp.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::interpolate(y, vec.y, interp.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::interpolate(z, vec.z, interp.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::interpolate(w, vec.w, interp.w);
#endif
		return *this;
	}
	//! returns the linear interpolation between this vector and another vector according to interp
	constexpr vector_type interpolated(const vector_type& vec, const interpolation_scalar_type& interp) const {
		return {
			const_math::interpolate(x, vec.x, interp),
#if FLOOR_VECTOR_WIDTH >= 2
			const_math::interpolate(y, vec.y, interp),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			const_math::interpolate(z, vec.z, interp),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			const_math::interpolate(w, vec.w, interp)
#endif
		};
	}
	//! returns the linear interpolation between this vector and another vector according to interp
	constexpr vector_type interpolated(const vector_type& vec, const interpolation_vector_type& interp) const {
		return {
			const_math::interpolate(x, vec.x, interp.x),
#if FLOOR_VECTOR_WIDTH >= 2
			const_math::interpolate(y, vec.y, interp.y),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			const_math::interpolate(z, vec.z, interp.z),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			const_math::interpolate(w, vec.w, interp.w)
#endif
		};
	}
	
	//! cubic interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type& cubic_interpolate(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
											 const interpolation_vector_type& interp) {
		x = const_math::cubic_interpolate(a_prev.x, x, b.x, b_next.x, interp.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::cubic_interpolate(a_prev.y, y, b.y, b_next.y, interp.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::cubic_interpolate(a_prev.z, z, b.z, b_next.z, interp.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::cubic_interpolate(a_prev.w, w, b.w, b_next.w, interp.w);
#endif
		return *this;
	}
	//! cubic interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type cubic_interpolated(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
											 const interpolation_vector_type& interp) const {
		return {
			const_math::cubic_interpolate(a_prev.x, x, b.x, b_next.x, interp.x),
#if FLOOR_VECTOR_WIDTH >= 2
			const_math::cubic_interpolate(a_prev.y, y, b.y, b_next.y, interp.y),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			const_math::cubic_interpolate(a_prev.z, z, b.z, b_next.z, interp.z),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			const_math::cubic_interpolate(a_prev.w, w, b.w, b_next.w, interp.w)
#endif
		};
	}
	//! cubic interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type& cubic_interpolate(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
											 const interpolation_scalar_type& interp) {
		x = const_math::cubic_interpolate(a_prev.x, x, b.x, b_next.x, interp);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::cubic_interpolate(a_prev.y, y, b.y, b_next.y, interp);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::cubic_interpolate(a_prev.z, z, b.z, b_next.z, interp);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::cubic_interpolate(a_prev.w, w, b.w, b_next.w, interp);
#endif
		return *this;
	}
	//! cubic interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type cubic_interpolated(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
											 const interpolation_scalar_type& interp) const {
		return {
			const_math::cubic_interpolate(a_prev.x, x, b.x, b_next.x, interp),
#if FLOOR_VECTOR_WIDTH >= 2
			const_math::cubic_interpolate(a_prev.y, y, b.y, b_next.y, interp),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			const_math::cubic_interpolate(a_prev.z, z, b.z, b_next.z, interp),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			const_math::cubic_interpolate(a_prev.w, w, b.w, b_next.w, interp)
#endif
		};
	}
	
	//! cubic catmull-rom interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type& catmull_rom_interpolate(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
												   const interpolation_vector_type& interp) {
		x = const_math::catmull_rom_interpolate(a_prev.x, x, b.x, b_next.x, interp.x);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::catmull_rom_interpolate(a_prev.y, y, b.y, b_next.y, interp.y);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::catmull_rom_interpolate(a_prev.z, z, b.z, b_next.z, interp.z);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::catmull_rom_interpolate(a_prev.w, w, b.w, b_next.w, interp.w);
#endif
		return *this;
	}
	//! cubic catmull-rom interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type catmull_rom_interpolated(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
												   const interpolation_vector_type& interp) const {
		return {
			const_math::catmull_rom_interpolate(a_prev.x, x, b.x, b_next.x, interp.x),
#if FLOOR_VECTOR_WIDTH >= 2
			const_math::catmull_rom_interpolate(a_prev.y, y, b.y, b_next.y, interp.y),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			const_math::catmull_rom_interpolate(a_prev.z, z, b.z, b_next.z, interp.z),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			const_math::catmull_rom_interpolate(a_prev.w, w, b.w, b_next.w, interp.w)
#endif
		};
	}
	//! cubic catmull-rom interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type& catmull_rom_interpolate(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
												   const interpolation_scalar_type& interp) {
		x = const_math::catmull_rom_interpolate(a_prev.x, x, b.x, b_next.x, interp);
#if FLOOR_VECTOR_WIDTH >= 2
		y = const_math::catmull_rom_interpolate(a_prev.y, y, b.y, b_next.y, interp);
#endif
#if FLOOR_VECTOR_WIDTH >= 3
		z = const_math::catmull_rom_interpolate(a_prev.z, z, b.z, b_next.z, interp);
#endif
#if FLOOR_VECTOR_WIDTH >= 4
		w = const_math::catmull_rom_interpolate(a_prev.w, w, b.w, b_next.w, interp);
#endif
		return *this;
	}
	//! cubic catmull-rom interpolation (with *this = a, interpolation between a and b, and values in order [a_prev, a, b, b_next])
	constexpr vector_type catmull_rom_interpolated(const vector_type& b, const vector_type& a_prev, const vector_type& b_next,
												   const interpolation_scalar_type& interp) const {
		return {
			const_math::catmull_rom_interpolate(a_prev.x, x, b.x, b_next.x, interp),
#if FLOOR_VECTOR_WIDTH >= 2
			const_math::catmull_rom_interpolate(a_prev.y, y, b.y, b_next.y, interp),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			const_math::catmull_rom_interpolate(a_prev.z, z, b.z, b_next.z, interp),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			const_math::catmull_rom_interpolate(a_prev.w, w, b.w, b_next.w, interp)
#endif
		};
	}
	
	//! returns an int vector with each component representing the sign of the corresponding component in this vector:
	//! sign: -1, no sign: 1
	template <typename signed_type = decayed_scalar_type> requires(ext::is_signed_v<signed_type>)
	constexpr signed_vector_type sign() const {
		// signed version
		return {
			FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , < (scalar_type)0 ? (signed_type)-1 : (signed_type)1)
		};
	}
	//! returns an int vector with each component representing the sign of the corresponding component in this vector:
	//! uint -> all 1
	template <typename signed_type = decayed_scalar_type> requires(!ext::is_signed_v<signed_type>)
	constexpr signed_vector_type sign() const {
		// unsigned version
		return { (signed_type)1 };
	}
	
	//! returns a bool vector with each component representing the sign of the corresponding component in this vector:
	//! true: sign, false: no sign
	template <typename signed_type = decayed_scalar_type> requires(ext::is_signed_v<signed_type>)
	constexpr FLOOR_VECNAME<bool> sign_bit() const {
		// signed version
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, , < (scalar_type)0) };
	}
	//! returns an int vector with each component representing the sign of the corresponding component in this vector:
	//! uint -> all false
	template <typename signed_type = decayed_scalar_type> requires(!ext::is_signed_v<signed_type>)
	constexpr FLOOR_VECNAME<bool> sign_bit() const {
		// unsigned version
		return { false };
	}
	
	//! returns an int vector containing the number of leading 0-bits of each component in this vector
	constexpr FLOOR_VECNAME<int> clz() const {
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, vector_helper<decayed_scalar_type>::clz FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT) };
	}
	
	//! returns an int vector containing the number of trailing 0-bits of each component in this vector
	constexpr FLOOR_VECNAME<int> ctz() const {
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, vector_helper<decayed_scalar_type>::ctz FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT) };
	}
	
	//! returns an int vector containing the number of 1-bits of each component in this vector
	constexpr FLOOR_VECNAME<int> popcount() const {
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, vector_helper<decayed_scalar_type>::popcount FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT) };
	}
	
	//! returns an int vector containing 1 + the index of the least significant 1-bit of each component in this vector,
	//! or 0 if the component is 0
	constexpr FLOOR_VECNAME<int> ffs() const {
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, vector_helper<decayed_scalar_type>::ffs FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT) };
	}
	
	//! returns an int vector containing the parity of each component in this vector (1 if odd number of 1-bits set, 0 else)
	constexpr FLOOR_VECNAME<int> parity() const {
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, vector_helper<decayed_scalar_type>::parity FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT) };
	}
	
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
	//! returns a randomized vector using a uniform distribution with each component in [0, max]
	template <typename int_type = scalar_type> requires(ext::is_integral_v<int_type>)
	static vector_type random(const scalar_type max = std::numeric_limits<int_type>::max()) {
		std::uniform_int_distribution<int_type> dist((int_type)0, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(vector_rng::get_vec_gen()) FLOOR_COMMA) };
	}
	//! returns a randomized vector using a uniform distribution with each component in [min, max]
	template <typename int_type = scalar_type> requires(ext::is_integral_v<int_type>)
	static vector_type random(const scalar_type min, const scalar_type max) {
		std::uniform_int_distribution<int_type> dist(min, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(vector_rng::get_vec_gen()) FLOOR_COMMA) };
	}
	//! returns a randomized vector using a uniform distribution with each component in [0, max)
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	static vector_type random(const scalar_type max = (scalar_type)1) {
		std::uniform_real_distribution<fp_type> dist((fp_type)0, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(vector_rng::get_vec_gen()) FLOOR_COMMA) };
	}
	//! returns a randomized vector using a uniform distribution with each component in [min, max)
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	static vector_type random(const scalar_type min, const scalar_type max) {
		std::uniform_real_distribution<fp_type> dist(min, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(vector_rng::get_vec_gen()) FLOOR_COMMA) };
	}
	
	//! returns a randomized vector using the specified generator, with a uniform distribution with each component in [0, max]
	template <typename int_type = scalar_type> requires(ext::is_integral_v<int_type>)
	static vector_type random(std::mt19937& gen, const scalar_type max = std::numeric_limits<int_type>::max()) {
		std::uniform_int_distribution<int_type> dist((int_type)0, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(gen) FLOOR_COMMA) };
	}
	//! returns a randomized vector using the specified generator, with a uniform distribution with each component in [min, max]
	template <typename int_type = scalar_type> requires(ext::is_integral_v<int_type>)
	static vector_type random(std::mt19937& gen, const scalar_type min, const scalar_type max) {
		std::uniform_int_distribution<int_type> dist(min, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(gen) FLOOR_COMMA) };
	}
	//! returns a randomized vector using the specified generator, with a uniform distribution with each component in [0, max)
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	static vector_type random(std::mt19937& gen, const scalar_type max = (scalar_type)1) {
		std::uniform_real_distribution<fp_type> dist((fp_type)0, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(gen) FLOOR_COMMA) };
	}
	//! returns a randomized vector using the specified generator, with a uniform distribution with each component in [min, max)
	template <typename fp_type = scalar_type> requires(ext::is_floating_point_v<fp_type>)
	static vector_type random(std::mt19937& gen, const scalar_type min, const scalar_type max) {
		std::uniform_real_distribution<fp_type> dist(min, max);
		return { FLOOR_VEC_EXPAND_NO_ELEMS(dist(gen) FLOOR_COMMA) };
	}
#endif
	
	//! returns an integer value representing the number of components of this vector (-> equivalent to vec_step in opencl)
	constexpr int vector_step() const {
		return FLOOR_VECTOR_WIDTH;
	}
	
	//////////////////////////////////////////
	// misc math
#pragma mark misc math
	
	//! applies the sqrt function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::sqrt, sqrt, sqrted)
	//! applies the rsqrt function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::rsqrt, rsqrt, rsqrted)
	//! applies the sin function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::sin, sin, sined)
	//! applies the cos function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::cos, cos, cosed)
	//! applies the tan function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::tan, tan, taned)
	//! applies the asin function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::asin, asin, asined)
	//! applies the acos function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::acos, acos, acosed)
	//! applies the atan function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::atan, atan, ataned)
	//! applies the atan2 function on all components (scalar x)
	FLOOR_VEC_FUNC_ARGS(vector_helper<decayed_scalar_type>::atan2, atan2, atan2ed,
						(const scalar_type& rhs),
						rhs)
	//! applies the atan2 function on all components (vector x)
	FLOOR_VEC_FUNC_ARGS_VEC(vector_helper<decayed_scalar_type>::atan2, atan2, atan2ed,
							(const vector_type& rhs),
							rhs.)
	//! applies the sinh function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::sinh, sinh, sinhed)
	//! applies the cosh function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::cosh, cosh, coshed)
	//! applies the tanh function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::tanh, tanh, tanhed)
	//! applies the asinh function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::asinh, asinh, asinhed)
	//! applies the acosh function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::acosh, acosh, acoshed)
	//! applies the atanh function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::atanh, atanh, atanhed)
	//! applies the exp function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::exp, exp, exped)
	//! applies the exp2 function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::exp2, exp2, exp2ed)
	//! applies the log function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::log, log, loged)
	//! applies the log2 function on all components
	FLOOR_VEC_FUNC(vector_helper<decayed_scalar_type>::log2, log2, log2ed)
	//! applies the pow function on all components (scalar exponent)
	FLOOR_VEC_FUNC_ARGS(vector_helper<decayed_scalar_type>::pow, pow, powed,
						(const scalar_type& exponent),
						exponent)
	//! applies the pow function on all components (vector exponent)
	FLOOR_VEC_FUNC_ARGS_VEC(vector_helper<decayed_scalar_type>::pow, pow, powed,
							(const vector_type& exponent),
							exponent.)
	
	//////////////////////////////////////////
	// type conversion
#pragma mark type conversion
	
#if FLOOR_VECTOR_WIDTH == 1
	//! vector1<scalar_type> is directly convertible to its scalar_type
	explicit operator scalar_type() const {
		return x;
	}
#endif
	
	//! converts this vector to the corresponding clang vector type,
	//! e.g. float4 to "float __attribute__((ext_vector_type(4)))"
	template <typename vec_type = vector_type> requires(vec_type::has_clang_vector_type())
	constexpr operator clang_vector_type() const {
		return (clang_vector_type){ FLOOR_VEC_EXPAND(FLOOR_COMMA) };
	}
	
	//! explicitly converts this vector to the corresponding clang vector type,
	//! e.g. float4 to "float __attribute__((ext_vector_type(4)))"
	template <typename vec_type = vector_type> requires(vec_type::has_clang_vector_type())
	constexpr clang_vector_type to_clang_vector() const {
		return (clang_vector_type){ FLOOR_VEC_EXPAND(FLOOR_COMMA) };
	}
	
	//! converts the corresponding clang vector type to this vector type
	template <typename vec_type = vector_type> requires(vec_type::has_clang_vector_type())
	static constexpr vector_type from_clang_vector(const clang_vector_type& vec) {
		return { FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, vec., ) };
	}
	
	//! returns a C++/std array with the elements of this vector
	constexpr auto to_array() const {
		return std::array<scalar_type, FLOOR_VECTOR_WIDTH> { FLOOR_VEC_EXPAND(FLOOR_COMMA) };
	}
	
	//! returns this vector as a tuple
	constexpr auto as_tuple() const {
		return std::make_tuple(FLOOR_VEC_EXPAND(FLOOR_COMMA));
	}
	
	//! returns this vector as a tuple, with each tuple element referencing its corresponding vector component
	constexpr auto as_tuple_ref() {
		return std::make_tuple(FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, std::ref FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT));
	}
	
	//! returns this vector as a tuple, with each tuple element referencing its corresponding vector component
	constexpr auto as_tuple_ref() const {
		return std::make_tuple(FLOOR_VEC_EXPAND_ENCLOSED(FLOOR_COMMA, std::cref FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT));
	}
	
	//! explicitly casts this vector (its components) to "dst_scalar_type"
	template <typename dst_scalar_type>
	constexpr auto cast() const {
		return FLOOR_VECNAME<dst_scalar_type> {
			dst_scalar_type(x),
#if FLOOR_VECTOR_WIDTH >= 2
			dst_scalar_type(y),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			dst_scalar_type(z),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			dst_scalar_type(w),
#endif
		};
	}
	
	//! explicitly reinterprets this vector (its components) as "dst_scalar_type"
	template <typename dst_scalar_type>
	constexpr auto reinterpret() const {
		static_assert(sizeof(dst_scalar_type) <= sizeof(scalar_type),
					  "reinterpret type size must <= the current type size");
		return FLOOR_VECNAME<dst_scalar_type> {
			*((const dst_scalar_type*)&x),
#if FLOOR_VECTOR_WIDTH >= 2
			*((const dst_scalar_type*)&y),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			*((const dst_scalar_type*)&z),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			*((const dst_scalar_type*)&w),
#endif
		};
	}
	
	//! explicitly bitcasts this vector (its components) to "dst_scalar_type"
	template <typename dst_scalar_type>
	constexpr auto bitcast() const {
		static_assert(sizeof(dst_scalar_type) == sizeof(scalar_type),
					  "bitcast type size must == the current type size");
		return FLOOR_VECNAME<dst_scalar_type> {
			std::bit_cast<dst_scalar_type>(x),
#if FLOOR_VECTOR_WIDTH >= 2
			std::bit_cast<dst_scalar_type>(y),
#endif
#if FLOOR_VECTOR_WIDTH >= 3
			std::bit_cast<dst_scalar_type>(z),
#endif
#if FLOOR_VECTOR_WIDTH >= 4
			std::bit_cast<dst_scalar_type>(w),
#endif
		};
	}
	
};
	
} // namespace fl

// cleanup macros
#include <floor/math/vector_ops_cleanup.hpp>

#undef FLOOR_CLANG_VECTOR_COMPAT

#undef FLOOR_VECNAME_CONCAT
#undef FLOOR_VECNAME_EVAL
#undef FLOOR_VECNAME
#undef FLOOR_VECNAME_STR_STRINGIFY
#undef FLOOR_VECNAME_STR_EVAL
#undef FLOOR_VECNAME_STR

FLOOR_POP_WARNINGS()
