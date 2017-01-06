/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_MATRIX4_HPP__
#define __FLOOR_MATRIX4_HPP__

#include <utility>
#include <tuple>
#include <functional>
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif
#include <floor/constexpr/const_math.hpp>
#include <floor/constexpr/const_array.hpp>
#include <floor/math/vector_helper.hpp>

template <typename scalar_type> class vector3;
template <typename scalar_type> class vector4;

//! column-major 4x4 matrix
template <typename scalar_type> class matrix4 {
public:
	//! "this" matrix type
	typedef matrix4<scalar_type> matrix_type;
	//! this scalar type
	typedef scalar_type this_scalar_type;
	//! decayed scalar type (removes refs/etc.)
	typedef decay_t<scalar_type> decayed_scalar_type;
	
	//! matrix elements are stored in a const_array so that they can be used with constexpr
	const_array<scalar_type, 16> data;
	
	//////////////////////////////////////////
	// constructors and assignment operators
#pragma mark constructors and assignment operators
	
	//! constructs an identity matrix
	constexpr matrix4() noexcept :
	data {{
		(scalar_type)1, (scalar_type)0, (scalar_type)0, (scalar_type)0,
		(scalar_type)0, (scalar_type)1, (scalar_type)0, (scalar_type)0,
		(scalar_type)0, (scalar_type)0, (scalar_type)1, (scalar_type)0,
		(scalar_type)0, (scalar_type)0, (scalar_type)0, (scalar_type)1 }} {}
	
	//! constructs a matrix with all elements set to 'val'
	constexpr matrix4(const scalar_type& val) noexcept :
	data {{
		val, val, val, val,
		val, val, val, val,
		val, val, val, val,
		val, val, val, val }} {}
	
	//! constructs a matrix with the given 16 elements
	constexpr matrix4(const scalar_type& m0, const scalar_type& m1, const scalar_type& m2, const scalar_type& m3,
					  const scalar_type& m4, const scalar_type& m5, const scalar_type& m6, const scalar_type& m7,
					  const scalar_type& m8, const scalar_type& m9, const scalar_type& m10, const scalar_type& m11,
					  const scalar_type& m12, const scalar_type& m13, const scalar_type& m14, const scalar_type& m15) noexcept :
	data {{ m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15 }} {}
	
	//! constructs a matrix out of the specified 4 column vectors
	constexpr matrix4(const vector4<scalar_type>& col_0,
					  const vector4<scalar_type>& col_1,
					  const vector4<scalar_type>& col_2,
					  const vector4<scalar_type>& col_3) noexcept :
	data {{
		((scalar_type*)&col_0)[0], ((scalar_type*)&col_0)[1], ((scalar_type*)&col_0)[2], ((scalar_type*)&col_0)[3],
		((scalar_type*)&col_1)[0], ((scalar_type*)&col_1)[1], ((scalar_type*)&col_1)[2], ((scalar_type*)&col_1)[3],
		((scalar_type*)&col_2)[0], ((scalar_type*)&col_2)[1], ((scalar_type*)&col_2)[2], ((scalar_type*)&col_2)[3],
		((scalar_type*)&col_3)[0], ((scalar_type*)&col_3)[1], ((scalar_type*)&col_3)[2], ((scalar_type*)&col_3)[3] }} {}
	
	//! copy-construct from same matrix type
	constexpr matrix4(const matrix4<scalar_type>& m4) noexcept : data(m4.data) {}
	
	//! copy-construct from same matrix type (pointer)
	constexpr matrix4(const matrix4<scalar_type>* m4) noexcept : data(m4->data) {}
	
	//! copy-construct from same matrix type (rvalue)
	constexpr matrix4(matrix4&& m4) noexcept : data(m4.data) {}
	
	//! conversion construction from another matrix with a different type
	template <typename U> constexpr matrix4(const matrix4<U>& mat4) noexcept :
	data {{
		(scalar_type)mat4.data[0], (scalar_type)mat4.data[1], (scalar_type)mat4.data[2], (scalar_type)mat4.data[3],
		(scalar_type)mat4.data[4], (scalar_type)mat4.data[5], (scalar_type)mat4.data[6], (scalar_type)mat4.data[7],
		(scalar_type)mat4.data[8], (scalar_type)mat4.data[9], (scalar_type)mat4.data[10], (scalar_type)mat4.data[11],
		(scalar_type)mat4.data[12], (scalar_type)mat4.data[13], (scalar_type)mat4.data[14], (scalar_type)mat4.data[15] }} {}
	
	//! copy assignment
	constexpr matrix4& operator=(const matrix4& mat) noexcept {
		data = mat.data;
		return *this;
	}
	
	//////////////////////////////////////////
	// I/O
#pragma mark I/O
	
#if !defined(FLOOR_NO_MATH_STR)
	//! ostream output of this matrix
	friend ostream& operator<<(ostream& output, const matrix4<scalar_type>& m4) {
		const auto cur_flags(output.flags());
		output << fixed;
		output << "/" << m4.data[0] << "\t" << m4.data[4] << "\t" << m4.data[8] << "\t" << m4.data[12] << "\\" << endl;
		output << "|" << m4.data[1] << "\t" << m4.data[5] << "\t" << m4.data[9] << "\t" << m4.data[13] << "|" << endl;
		output << "|" << m4.data[2] << "\t" << m4.data[6] << "\t" << m4.data[10] << "\t" << m4.data[14] << "|" << endl;
		output << "\\" << m4.data[3] << "\t" << m4.data[7] << "\t" << m4.data[11] << "\t" << m4.data[15] << "/" << endl;
		output.flags(cur_flags);
		return output;
	}
	
	//! returns a string representation of this matrix
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
	//////////////////////////////////////////
	// access
#pragma mark access
	
	//! subscript access (note column-major access)
	constexpr scalar_type& operator[](const size_t index) {
		return data[index];
	}
	
	//! const subscript access (note column-major access)
	constexpr const scalar_type& operator[](const size_t index) const {
		return data[index];
	}
	
	//! constexpr subscript access, with index out of bounds
	constexpr scalar_type& operator[](const size_t& index)
	__attribute__((enable_if(index >= 16, "index out of bounds"), unavailable("index out of bounds")));
	
	//! constexpr subscript access, with index out of bounds
	constexpr const scalar_type& operator[](const size_t& index) const
	__attribute__((enable_if(index >= 16, "index out of bounds"), unavailable("index out of bounds")));
	
	//! returns the specified matrix column as a vector4
	template<size_t col> constexpr vector4<scalar_type> column() const {
		return vector4<scalar_type>(data[col*4],
									data[(col*4) + 1],
									data[(col*4) + 2],
									data[(col*4) + 3]);
	}
	
	//////////////////////////////////////////
	// basic ops
#pragma mark basic ops
	
	//! multiplies this matrix with 'mat' and returns the result
	constexpr matrix4 operator*(const matrix4& mat) const {
		matrix4 mul_mat { (scalar_type)0 };
#pragma unroll
		for(size_t mi = 0; mi < 4; ++mi) { // column
#pragma unroll
			for(size_t mj = 0; mj < 4; ++mj) { // row
#pragma unroll
				for(size_t mk = 0; mk < 4; ++mk) { // mul iteration
					mul_mat.data[(mi * 4) + mj] += data[(mi * 4) + mk] * mat.data[(mk * 4) + mj];
				}
			}
		}
		return mul_mat;
	}
	
	//! multiplies this matrix with 'mat' and sets this matrix to the result
	constexpr matrix4& operator*=(const matrix4& mat) {
		*this = *this * mat;
		return *this;
	}
	
	//! multiplies each element of this matrix with 'val' and returns the result
	constexpr matrix4 operator*(const scalar_type& val) const {
		matrix4 mul_mat { (scalar_type)0 };
#pragma unroll
		for(size_t i = 0; i < 16; ++i) {
			mul_mat[i] = data[i] * val;
		}
		return mul_mat;
	}
	
	//! multiplies each element of this matrix with 'val' and sets this matrix to the result
	constexpr matrix4& operator*=(const scalar_type& val) {
		*this = *this * val;
		return *this;
	}
	
	//! divides each element of this matrix by 'val' and returns the result
	constexpr matrix4 operator/(const scalar_type& val) const {
		matrix4 div_mat { (scalar_type)0 };
#pragma unroll
		for(size_t i = 0; i < 16; ++i) {
			div_mat[i] = data[i] / val;
		}
		return div_mat;
	}
	
	//! divides each element of this matrix by 'val' and sets this matrix to the result
	constexpr matrix4& operator/=(const scalar_type& val) {
		*this = *this / val;
		return *this;
	}
	
	//! adds this matrix to 'mat' and returns the result
	constexpr matrix4 operator+(const matrix4& mat) const {
		return {
			data[0] + mat.data[0], data[1] + mat.data[1], data[2] + mat.data[2], data[3] + mat.data[3],
			data[4] + mat.data[4], data[5] + mat.data[5], data[6] + mat.data[6], data[7] + mat.data[7],
			data[8] + mat.data[8], data[9] + mat.data[9], data[10] + mat.data[10], data[11] + mat.data[11],
			data[12] + mat.data[12], data[13] + mat.data[13], data[14] + mat.data[14], data[15] + mat.data[15]
		};
	}
	
	//! adds this matrix to 'mat' and sets this matrix to the result
	constexpr matrix4& operator+=(const matrix4& mat) {
		*this = *this + mat;
		return *this;
	}
	
	//! subtracts 'mat' from this matrix and returns the result
	constexpr matrix4 operator-(const matrix4& mat) const {
		return {
			data[0] - mat.data[0], data[1] - mat.data[1], data[2] - mat.data[2], data[3] - mat.data[3],
			data[4] - mat.data[4], data[5] - mat.data[5], data[6] - mat.data[6], data[7] - mat.data[7],
			data[8] - mat.data[8], data[9] - mat.data[9], data[10] - mat.data[10], data[11] - mat.data[11],
			data[12] - mat.data[12], data[13] - mat.data[13], data[14] - mat.data[14], data[15] - mat.data[15]
		};
	}
	
	//! subtracts 'mat' from this matrix and sets this matrix to the result
	constexpr matrix4& operator-=(const matrix4& mat) {
		*this = *this - mat;
		return *this;
	}
	
	//! computes the determinant of this matrix
	constexpr scalar_type determinant() const {
		// note that laplace is more efficient than computing 4 3x3 determinants here (47 vs 63 ops)
		return {
			((data[0] * data[5] - data[1] * data[4]) * (data[10] * data[15] - data[11] * data[14])) -
			((data[0] * data[6] - data[2] * data[4]) * (data[9] * data[15] - data[11] * data[13])) +
			((data[0] * data[7] - data[3] * data[4]) * (data[9] * data[14] - data[10] * data[13])) +
			((data[1] * data[6] - data[2] * data[5]) * (data[8] * data[15] - data[11] * data[12])) -
			((data[1] * data[7] - data[3] * data[5]) * (data[8] * data[14] - data[10] * data[12])) +
			((data[2] * data[7] - data[3] * data[6]) * (data[8] * data[13] - data[9] * data[12]))
		};
	}
	
	//! computes the determinant of the top left 3x3 part of this matrix
	constexpr scalar_type determinant_3x3() const {
		return {
			data[0] * (data[5] * data[10] - data[9] * data[6]) +
			data[4] * (data[9] * data[2] - data[1] * data[10]) +
			data[8] * (data[1] * data[6] - data[5] * data[2])
		};
	}
	
	//! computes the determinant of the top left 2x2 part of this matrix
	constexpr scalar_type determinant_2x2() const {
		return {
			data[0] * data[5] - data[4] * data[1]
		};
	}
	
	//! resets this matrix to an identity matrix
	constexpr matrix4& identity() {
		*this = matrix4 {};
		return *this;
	}
	
	//! inverts this matrix
	constexpr matrix4& invert() {
		*this = inverted();
		return *this;
	}
	
	//! returns the inverted form of this matrix
	constexpr matrix4 inverted() const {
		matrix4 mat;
		
		const scalar_type p00(data[10] * data[15]);
		const scalar_type p01(data[14] * data[11]);
		const scalar_type p02(data[6] * data[15]);
		const scalar_type p03(data[14] * data[7]);
		const scalar_type p04(data[6] * data[11]);
		const scalar_type p05(data[10] * data[7]);
		const scalar_type p06(data[2] * data[15]);
		const scalar_type p07(data[14] * data[3]);
		const scalar_type p08(data[2] * data[11]);
		const scalar_type p09(data[10] * data[3]);
		const scalar_type p10(data[2] * data[7]);
		const scalar_type p11(data[6] * data[3]);
		
		mat.data[0] = (p00 * data[5] + p03 * data[9] + p04 * data[13]) - (p01 * data[5] + p02 * data[9] + p05 * data[13]);
		mat.data[1] = (p01 * data[1] + p06 * data[9] + p09 * data[13]) - (p00 * data[1] + p07 * data[9] + p08 * data[13]);
		mat.data[2] = (p02 * data[1] + p07 * data[5] + p10 * data[13]) - (p03 * data[1] + p06 * data[5] + p11 * data[13]);
		mat.data[3] = (p05 * data[1] + p08 * data[5] + p11 * data[9]) - (p04 * data[1] + p09 * data[5] + p10 * data[9]);
		mat.data[4] = (p01 * data[4] + p02 * data[8] + p05 * data[12]) - (p00 * data[4] + p03 * data[8] + p04 * data[12]);
		mat.data[5] = (p00 * data[0] + p07 * data[8] + p08 * data[12]) - (p01 * data[0] + p06 * data[8] + p09 * data[12]);
		mat.data[6] = (p03 * data[0] + p06 * data[4] + p11 * data[12]) - (p02 * data[0] + p07 * data[4] + p10 * data[12]);
		mat.data[7] = (p04 * data[0] + p09 * data[4] + p10 * data[8]) - (p05 * data[0] + p08 * data[4] + p11 * data[8]);
		
		const scalar_type q00(data[8] * data[13]);
		const scalar_type q01(data[12] * data[9]);
		const scalar_type q02(data[4] * data[13]);
		const scalar_type q03(data[12] * data[5]);
		const scalar_type q04(data[4] * data[9]);
		const scalar_type q05(data[8] * data[5]);
		const scalar_type q06(data[0] * data[13]);
		const scalar_type q07(data[12] * data[1]);
		const scalar_type q08(data[0] * data[9]);
		const scalar_type q09(data[8] * data[1]);
		const scalar_type q10(data[0] * data[5]);
		const scalar_type q11(data[4] * data[1]);
		
		mat.data[8] = (q00 * data[7] + q03 * data[11] + q04 * data[15]) - (q01 * data[7] + q02 * data[11] + q05 * data[15]);
		mat.data[9] = (q01 * data[3] + q06 * data[11] + q09 * data[15]) - (q00 * data[3] + q07 * data[11] + q08 * data[15]);
		mat.data[10] = (q02 * data[3] + q07 * data[7] + q10 * data[15]) - (q03 * data[3] + q06 * data[7] + q11 * data[15]);
		mat.data[11] = (q05 * data[3] + q08 * data[7] + q11 * data[11]) - (q04 * data[3] + q09 * data[7] + q10 * data[11]);
		mat.data[12] = (q02 * data[10] + q05 * data[14] + q01 * data[6]) - (q04 * data[14] + q00 * data[6] + q03 * data[10]);
		mat.data[13] = (q08 * data[14] + q00 * data[2] + q07 * data[10]) - (q06 * data[10] + q09 * data[14] + q01 * data[2]);
		mat.data[14] = (q06 * data[6] + q11 * data[14] + q03 * data[2]) - (q10 * data[14] + q02 * data[2] + q07 * data[6]);
		mat.data[15] = (q10 * data[10] + q04 * data[2] + q09 * data[6]) - (q08 * data[6] + q11 * data[10] + q05 * data[2]);
		
		const scalar_type mx(((scalar_type)1) / (data[0] * mat.data[0] + data[4] * mat.data[1] + data[8] * mat.data[2] + data[12] * mat.data[3]));
#pragma unroll
		for(size_t mi = 0; mi < 4; mi++) {
#pragma unroll
			for(size_t mj = 0; mj < 4; mj++) {
				mat.data[(mi*4) + mj] *= mx;
			}
		}
		
		return mat;
	}
	
	//! transposes this matrix
	constexpr matrix4& transpose() {
		std::swap(data[1], data[4]);
		std::swap(data[2], data[8]);
		std::swap(data[3], data[12]);
		std::swap(data[6], data[9]);
		std::swap(data[7], data[13]);
		std::swap(data[11], data[14]);
		return *this;
	}
	
	//! returns the transposed form of this matrix
	constexpr matrix4 transposed() const {
		return {
			data[0], data[4], data[8], data[12],
			data[1], data[5], data[9], data[13],
			data[2], data[6], data[10], data[14],
			data[3], data[7], data[11], data[15]
		};
	}
	
	//! translates this matrix by (x, y, z)
	constexpr matrix4& translate(const scalar_type x, const scalar_type y, const scalar_type z) {
		*this *= translation(x, y, z);
		return *this;
	}
	
	//! translates this matrix by (x, y, z) of the given vector
	constexpr matrix4& translate(const vector3<scalar_type>& vec) {
		*this *= translation(vec);
		return *this;
	}
	
	//! sets the "translation components" of this matrix to (x, y, z)
	constexpr matrix4& set_translation(const scalar_type x, const scalar_type y, const scalar_type z) {
		data[12] = x;
		data[13] = y;
		data[14] = z;
		return *this;
	}
	
	//! scales this matrix by (x, y, z)
	constexpr matrix4& scale(const scalar_type x, const scalar_type y, const scalar_type z) {
		*this *= scaling(x, y, z);
		return *this;
	}
	
	//! scales this matrix by (x, y, z) of the given vector
	constexpr matrix4& scale(const vector3<scalar_type>& vec) {
		*this *= scaling(vec);
		return *this;
	}
	
	//! returns this matrix as a tuple
	constexpr auto as_tuple() const {
		return make_tuple(data[0], data[1], data[2], data[3],
						  data[4], data[5], data[6], data[7],
						  data[8], data[9], data[10], data[11],
						  data[12], data[13], data[14], data[15]);
	}
	
	//! returns this matrix as a tuple, with each tuple element referencing its corresponding matrix element
	constexpr auto as_tuple_ref() {
		return make_tuple(std::ref(data[0]), std::ref(data[1]), std::ref(data[2]), std::ref(data[3]),
						  std::ref(data[4]), std::ref(data[5]), std::ref(data[6]), std::ref(data[7]),
						  std::ref(data[8]), std::ref(data[9]), std::ref(data[10]), std::ref(data[11]),
						  std::ref(data[12]), std::ref(data[13]), std::ref(data[14]), std::ref(data[15]));
	}
	
	//! returns this matrix as a tuple, with each tuple element referencing its corresponding matrix element
	constexpr auto as_tuple_ref() const {
		return make_tuple(std::cref(data[0]), std::cref(data[1]), std::cref(data[2]), std::cref(data[3]),
						  std::cref(data[4]), std::cref(data[5]), std::cref(data[6]), std::cref(data[7]),
						  std::cref(data[8]), std::cref(data[9]), std::cref(data[10]), std::cref(data[11]),
						  std::cref(data[12]), std::cref(data[13]), std::cref(data[14]), std::cref(data[15]));
	}
	
	//////////////////////////////////////////
	// static matrix creation functions
#pragma mark static matrix creation functions
	
	//! returns a translation matrix set to (x, y, z)
	static constexpr matrix4 translation(const scalar_type x, const scalar_type y, const scalar_type z) {
		return {
			scalar_type(1), scalar_type(0), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(1), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(0), scalar_type(1), scalar_type(0),
			x, y, z, scalar_type(1)
		};
	}

	//! returns a translation matrix set to (x, y, z) of the given vector
	static constexpr matrix4 translation(const vector3<scalar_type>& vec) {
		return translation(vec[0], vec[1], vec[2]);
	}
	
	//! returns a scale matrix with the specified scale components
	static constexpr matrix4 scaling(const scalar_type sx, const scalar_type sy, const scalar_type sz) {
		return {
			scalar_type(sx), scalar_type(0), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(sy), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(0), scalar_type(sz), scalar_type(0),
			scalar_type(0), scalar_type(0), scalar_type(0), scalar_type(1)
		};
	}
	
	//! returns a scale matrix with the specified scale components (x, y, z) of the given vector
	static constexpr matrix4 scaling(const vector3<scalar_type>& vec) {
		return scaling(vec[0], vec[1], vec[2]);
	}
	
	//! returns a matrix that is rotated by 'rad_angle' radians on the specified axis (0 = x, 1 = y, 2 = z)
	template <uint32_t axis>
	static constexpr matrix4 rotation(const scalar_type rad_angle) {
		static_assert(axis < 3, "axis must be 0 (x), 1 (y) or 2 (z)");
		
		const scalar_type sin_val { vector_helper<scalar_type>::sin(rad_angle) };
		const scalar_type cos_val { vector_helper<scalar_type>::cos(rad_angle) };
		
		switch(axis) {
			case 0:
				return {
					scalar_type(1), scalar_type(0), scalar_type(0), scalar_type(0),
					scalar_type(0), cos_val, sin_val, scalar_type(0),
					scalar_type(0), -sin_val, cos_val, scalar_type(0),
					scalar_type(0), scalar_type(0), scalar_type(0), scalar_type(1)
				};
			case 1:
				return {
					cos_val, scalar_type(0), -sin_val, scalar_type(0),
					scalar_type(0), scalar_type(1), scalar_type(0), scalar_type(0),
					sin_val, scalar_type(0), cos_val, scalar_type(0),
					scalar_type(0), scalar_type(0), scalar_type(0), scalar_type(1)
				};
			case 2:
				return {
					cos_val, sin_val, scalar_type(0), scalar_type(0),
					-sin_val, cos_val, scalar_type(0), scalar_type(0),
					scalar_type(0), scalar_type(0), scalar_type(1), scalar_type(0),
					scalar_type(0), scalar_type(0), scalar_type(0), scalar_type(1)
				};
			default: floor_unreachable();
		}
	}
	
	//! returns a matrix that is rotated by 'rad_angle' radians on the specified axis ('x', 'y' or 'z')
	template <char axis>
	static constexpr matrix4 rotation_named(const scalar_type rad_angle) {
		static_assert(axis == 'x' || axis == 'y' || axis == 'z', "axis must be x, y or z");
		switch(axis) {
			case 'x': return rotation<0>(rad_angle);
			case 'y': return rotation<1>(rad_angle);
			case 'z': return rotation<2>(rad_angle);
			default: floor_unreachable();
		}
	}
	
	//! returns a matrix that is rotated by 'deg_angle' degrees on the specified axis (0 = x, 1 = y, 2 = z)
	template <uint32_t axis>
	static constexpr matrix4 rotation_deg(const scalar_type deg_angle) {
		return rotation<axis>(const_math::deg_to_rad(deg_angle));
	}
	
	//! returns a matrix that is rotated by 'deg_angle' degrees on the specified axis ('x', 'y' or 'z')
	template <char axis>
	static constexpr matrix4 rotation_deg_named(const scalar_type deg_angle) {
		return rotation_named<axis>(const_math::deg_to_rad(deg_angle));
	}
	
#if !defined(_MSC_VER) // duplicate name mangling issues
	//! returns a perspective projection matrix according to the specified parameters
	//! NOTE: this function will be selected if the fov parameter is constant (this is beneficial, because tan(x) calls are costly)
	static constexpr matrix4 perspective(const scalar_type fov, const scalar_type aspect,
										 const scalar_type z_near, const scalar_type z_far)
	__attribute__((enable_if(fov == fov, "perspective with constant field-of-view"))) {
		typedef conditional_t<ext::is_floating_point_v<scalar_type>, scalar_type, float> fp_type;
		const fp_type f {
			(fp_type)1 / const_math::tan(fp_type(fov) * const_math::PI_DIV_360<fp_type>)
		};
		return {
			scalar_type(f) / scalar_type(aspect), scalar_type(0), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(f), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(0), (z_far + z_near) / (z_near - z_far), scalar_type(-1),
			scalar_type(0), scalar_type(0), (scalar_type(2) * z_far * z_near) / (z_near - z_far), scalar_type(0)
		};
	}
#endif
	
	//! returns a perspective projection matrix according to the specified parameters
	static constexpr matrix4 perspective(const scalar_type fov, const scalar_type aspect,
										 const scalar_type z_near, const scalar_type z_far) {
		typedef conditional_t<ext::is_floating_point_v<scalar_type>, scalar_type, float> fp_type;
		const fp_type f {
			(fp_type)1 / math::tan(fp_type(fov) * const_math::PI_DIV_360<fp_type>)
		};
		return {
			scalar_type(f) / scalar_type(aspect), scalar_type(0), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(f), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(0), (z_far + z_near) / (z_near - z_far), scalar_type(-1),
			scalar_type(0), scalar_type(0), (scalar_type(2) * z_far * z_near) / (z_near - z_far), scalar_type(0)
		};
	}
	
	//! returns an orthographic projection matrix according to the specified parameters
	static constexpr matrix4 orthographic(const scalar_type left, const scalar_type right,
										  const scalar_type bottom, const scalar_type top,
										  const scalar_type z_near, const scalar_type z_far) {
		const scalar_type r_l { right - left };
		const scalar_type t_b { top - bottom };
		const scalar_type f_n { z_far - z_near };
		return {
			scalar_type(2) / r_l, scalar_type(0), scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(2) / t_b, scalar_type(0), scalar_type(0),
			scalar_type(0), scalar_type(0), scalar_type(-2) / f_n, scalar_type(0),
			-(right + left) / (right - left),
			-(top + bottom) / (top - bottom),
			-(z_far + z_near) / (z_far - z_near),
			scalar_type(1)
		};
	}
	
	//! computes the outer product of the two given 4-component vectors, resulting in a matrix4
	static constexpr matrix4 outer_product(const vector4<scalar_type>& v0, const vector4<scalar_type>& v1) {
		return {
			v0[0] * v1[0], v0[1] * v1[0], v0[2] * v1[0], v0[3] * v1[0],
			v0[0] * v1[1], v0[1] * v1[1], v0[2] * v1[1], v0[3] * v1[1],
			v0[0] * v1[2], v0[1] * v1[2], v0[2] * v1[2], v0[3] * v1[2],
			v0[0] * v1[3], v0[1] * v1[3], v0[2] * v1[3], v0[3] * v1[3]
		};
	}
	
};

typedef matrix4<float> matrix4f;
#if !defined(FLOOR_COMPUTE_NO_DOUBLE) // disable double + long double if specified
typedef matrix4<double> matrix4d;
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST) // always disable long double on compute platforms (except host)
typedef matrix4<long double> matrix4l;
#endif
#endif
typedef matrix4<int32_t> matrix4i;
typedef matrix4<uint32_t> matrix4ui;

//! type trait function to determine if a type is a floor matrix4*
template <typename any_type, typename = void> struct is_floor_matrix : public false_type {};
template <typename mat_type>
struct is_floor_matrix<mat_type, enable_if_t<is_same<decay_t<mat_type>, typename decay_t<mat_type>::matrix_type>::value>> : public true_type {};

#if defined(FLOOR_EXPORT)
// only instantiate this in the matrix4.cpp
extern template class matrix4<float>;
extern template class matrix4<double>;
extern template class matrix4<long double>;
extern template class matrix4<int32_t>;
extern template class matrix4<uint32_t>;
#endif

#endif
