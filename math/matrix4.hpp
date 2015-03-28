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

#ifndef __FLOOR_MATRIX4_HPP__
#define __FLOOR_MATRIX4_HPP__

#include <utility>
#if !defined(FLOOR_COMPUTE)
#include <array>
#endif
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif
#include <floor/constexpr/const_math.hpp>
#include <floor/math/vector_helper.hpp>

template <typename T> class matrix4;
typedef matrix4<float> matrix4f;
typedef matrix4<double> matrix4d;
typedef matrix4<int> matrix4i;
typedef matrix4<unsigned int> matrix4ui;

template <typename T> class vector4;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpacked"
#endif

template <typename T> class __attribute__((packed, aligned(16))) matrix4 {
public:
	template <typename elem_type> struct cexpr_array {
		elem_type data[16];
		
		constexpr elem_type& operator[](const size_t& index) __attribute__((enable_if(index < 16, "index is const"))) {
			return data[index];
		}
		constexpr elem_type& operator[](const size_t& index) __attribute__((enable_if(index >= 16, "index is invalid"), unavailable("index is invalid")));
		constexpr elem_type& operator[](const size_t& index) {
			return data[index];
		}
		
		constexpr const elem_type& operator[](const size_t& index) const __attribute__((enable_if(index < 16, "index is const"))) {
			return data[index];
		}
		constexpr const elem_type& operator[](const size_t& index) const __attribute__((enable_if(index >= 16, "index is invalid"), unavailable("index is invalid")));
		constexpr const elem_type& operator[](const size_t& index) const {
			return data[index];
		}
	};
	cexpr_array<T> data;
	
	constexpr matrix4() noexcept :
	data {{
		(T)1, (T)0, (T)0, (T)0,
		(T)0, (T)1, (T)0, (T)0,
		(T)0, (T)0, (T)1, (T)0,
		(T)0, (T)0, (T)0, (T)1 }} {}
	constexpr matrix4(const T& val) noexcept :
	data {{
		val, val, val, val,
		val, val, val, val,
		val, val, val, val,
		val, val, val, val }} {}
	constexpr matrix4(matrix4&& m4) noexcept : data(m4.data) {}
	constexpr matrix4(const matrix4<T>& m4) noexcept : data(m4.data) {}
	constexpr matrix4(const matrix4<T>* m4) noexcept : data(m4->data) {}
	constexpr matrix4(const T& m0, const T& m1, const T& m2, const T& m3,
					  const T& m4, const T& m5, const T& m6, const T& m7,
					  const T& m8, const T& m9, const T& m10, const T& m11,
					  const T& m12, const T& m13, const T& m14, const T& m15) noexcept :
	data {{ m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15 }} {}
	constexpr matrix4(const vector4<T>& col_0,
					  const vector4<T>& col_1,
					  const vector4<T>& col_2,
					  const vector4<T>& col_3) noexcept :
	data {{
		((T*)&col_0)[0], ((T*)&col_0)[1], ((T*)&col_0)[2], ((T*)&col_0)[3],
		((T*)&col_1)[0], ((T*)&col_1)[1], ((T*)&col_1)[2], ((T*)&col_1)[3],
		((T*)&col_2)[0], ((T*)&col_2)[1], ((T*)&col_2)[2], ((T*)&col_2)[3],
		((T*)&col_3)[0], ((T*)&col_3)[1], ((T*)&col_3)[2], ((T*)&col_3)[3] }} {}
	template <typename U> constexpr matrix4(const matrix4<U>& mat4) noexcept :
	data {{
		(T)mat4.data[0], (T)mat4.data[1], (T)mat4.data[2], (T)mat4.data[3],
		(T)mat4.data[4], (T)mat4.data[5], (T)mat4.data[6], (T)mat4.data[7],
		(T)mat4.data[8], (T)mat4.data[9], (T)mat4.data[10], (T)mat4.data[11],
		(T)mat4.data[12], (T)mat4.data[13], (T)mat4.data[14], (T)mat4.data[15] }} {}
	
	constexpr matrix4& operator=(const matrix4& mat) noexcept {
		for(size_t i = 0; i < 16; ++i) {
			data[i] = mat.data[i];
		}
		return *this;
	}
	
#if !defined(FLOOR_NO_MATH_STR)
	friend ostream& operator<<(ostream& output, const matrix4<T>& m4) {
		const auto cur_flags(output.flags());
		output << fixed;
		output << "/" << m4.data[0] << "\t" << m4.data[4] << "\t" << m4.data[8] << "\t" << m4.data[12] << "\\" << endl;
		output << "|" << m4.data[1] << "\t" << m4.data[5] << "\t" << m4.data[9] << "\t" << m4.data[13] << "|" << endl;
		output << "|" << m4.data[2] << "\t" << m4.data[6] << "\t" << m4.data[10] << "\t" << m4.data[14] << "|" << endl;
		output << "\\" << m4.data[3] << "\t" << m4.data[7] << "\t" << m4.data[11] << "\t" << m4.data[15] << "/" << endl;
		output.flags(cur_flags);
		return output;
	}
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
	constexpr T& operator[](const size_t index) { return data[index]; }
	constexpr const T& operator[](const size_t index) const { return data[index]; }
	
	constexpr matrix4 operator*(const matrix4& mat) const;
	constexpr matrix4& operator*=(const matrix4& mat);
	
	// basic functions
	constexpr matrix4& invert();
	constexpr matrix4& identity();
	constexpr matrix4& transpose();
	
	// transformations
	constexpr matrix4& translate(const T x, const T y, const T z);
	constexpr matrix4& set_translation(const T x, const T y, const T z);
	constexpr matrix4& scale(const T x, const T y, const T z);
	
	constexpr matrix4& rotate_x(const T x);
	constexpr matrix4& rotate_y(const T y);
	constexpr matrix4& rotate_z(const T z);
	
	//
	template<size_t col> constexpr vector4<T> column() const {
		return vector4<T>(data[col*4],
						  data[(col*4) + 1],
						  data[(col*4) + 2],
						  data[(col*4) + 3]);
	}
	
	// projection
	constexpr matrix4& perspective(const T fov, const T aspect, const T z_near, const T z_far);
	constexpr matrix4& ortho(const T left, const T right, const T bottom, const T top, const T z_near, const T z_far);
};

template<typename T> constexpr matrix4<T> matrix4<T>::operator*(const matrix4<T>& mat) const {
	matrix4 mul_mat((T)0);
	for(size_t mi = 0; mi < 4; mi++) { // column
		for(size_t mj = 0; mj < 4; mj++) { // row
			for(size_t mk = 0; mk < 4; mk++) { // mul iteration
				mul_mat.data[(mi*4) + mj] += data[(mi*4) + mk] * mat.data[(mk*4) + mj];
			}
		}
	}
	return mul_mat;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::operator*=(const matrix4<T>& mat) {
	*this = *this * mat;
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::invert() {
	matrix4<T> mat;
	
	const T p00(data[10] * data[15]);
	const T p01(data[14] * data[11]);
	const T p02(data[6] * data[15]);
	const T p03(data[14] * data[7]);
	const T p04(data[6] * data[11]);
	const T p05(data[10] * data[7]);
	const T p06(data[2] * data[15]);
	const T p07(data[14] * data[3]);
	const T p08(data[2] * data[11]);
	const T p09(data[10] * data[3]);
	const T p10(data[2] * data[7]);
	const T p11(data[6] * data[3]);
	
	mat.data[0] = (p00 * data[5] + p03 * data[9] + p04 * data[13]) - (p01 * data[5] + p02 * data[9] + p05 * data[13]);
	mat.data[1] = (p01 * data[1] + p06 * data[9] + p09 * data[13]) - (p00 * data[1] + p07 * data[9] + p08 * data[13]);
	mat.data[2] = (p02 * data[1] + p07 * data[5] + p10 * data[13]) - (p03 * data[1] + p06 * data[5] + p11 * data[13]);
	mat.data[3] = (p05 * data[1] + p08 * data[5] + p11 * data[9]) - (p04 * data[1] + p09 * data[5] + p10 * data[9]);
	mat.data[4] = (p01 * data[4] + p02 * data[8] + p05 * data[12]) - (p00 * data[4] + p03 * data[8] + p04 * data[12]);
	mat.data[5] = (p00 * data[0] + p07 * data[8] + p08 * data[12]) - (p01 * data[0] + p06 * data[8] + p09 * data[12]);
	mat.data[6] = (p03 * data[0] + p06 * data[4] + p11 * data[12]) - (p02 * data[0] + p07 * data[4] + p10 * data[12]);
	mat.data[7] = (p04 * data[0] + p09 * data[4] + p10 * data[8]) - (p05 * data[0] + p08 * data[4] + p11 * data[8]);
	
	const T q00(data[8] * data[13]);
	const T q01(data[12] * data[9]);
	const T q02(data[4] * data[13]);
	const T q03(data[12] * data[5]);
	const T q04(data[4] * data[9]);
	const T q05(data[8] * data[5]);
	const T q06(data[0] * data[13]);
	const T q07(data[12] * data[1]);
	const T q08(data[0] * data[9]);
	const T q09(data[8] * data[1]);
	const T q10(data[0] * data[5]);
	const T q11(data[4] * data[1]);
	
	mat.data[8] = (q00 * data[7] + q03 * data[11] + q04 * data[15]) - (q01 * data[7] + q02 * data[11] + q05 * data[15]);
	mat.data[9] = (q01 * data[3] + q06 * data[11] + q09 * data[15]) - (q00 * data[3] + q07 * data[11] + q08 * data[15]);
	mat.data[10] = (q02 * data[3] + q07 * data[7] + q10 * data[15]) - (q03 * data[3] + q06 * data[7] + q11 * data[15]);
	mat.data[11] = (q05 * data[3] + q08 * data[7] + q11 * data[11]) - (q04 * data[3] + q09 * data[7] + q10 * data[11]);
	mat.data[12] = (q02 * data[10] + q05 * data[14] + q01 * data[6]) - (q04 * data[14] + q00 * data[6] + q03 * data[10]);
	mat.data[13] = (q08 * data[14] + q00 * data[2] + q07 * data[10]) - (q06 * data[10] + q09 * data[14] + q01 * data[2]);
	mat.data[14] = (q06 * data[6] + q11 * data[14] + q03 * data[2]) - (q10 * data[14] + q02 * data[2] + q07 * data[6]);
	mat.data[15] = (q10 * data[10] + q04 * data[2] + q09 * data[6]) - (q08 * data[6] + q11 * data[10] + q05 * data[2]);
	
	const T mx(((T)1) / (data[0] * mat.data[0] + data[4] * mat.data[1] + data[8] * mat.data[2] + data[12] * mat.data[3]));
	for(size_t mi = 0; mi < 4; mi++) {
		for(size_t mj = 0; mj < 4; mj++) {
			mat.data[(mi*4) + mj] *= mx;
		}
	}
	
	*this = mat;
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::identity() {
	data[0] = ((T)1);
	data[1] = ((T)0);
	data[2] = ((T)0);
	data[3] = ((T)0);
	
	data[4] = ((T)0);
	data[5] = ((T)1);
	data[6] = ((T)0);
	data[7] = ((T)0);
	
	data[8] = ((T)0);
	data[9] = ((T)0);
	data[10] = ((T)1);
	data[11] = ((T)0);
	
	data[12] = ((T)0);
	data[13] = ((T)0);
	data[14] = ((T)0);
	data[15] = ((T)1);
	
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::transpose() {
	std::swap(data[1], data[4]);
	std::swap(data[2], data[8]);
	std::swap(data[3], data[12]);
	std::swap(data[6], data[9]);
	std::swap(data[7], data[13]);
	std::swap(data[11], data[14]);
	return *this;
}

// transformations
template<typename T> constexpr matrix4<T>& matrix4<T>::translate(const T x, const T y, const T z) {
	matrix4 trans_mat;
	trans_mat[12] = x;
	trans_mat[13] = y;
	trans_mat[14] = z;
	
	*this *= trans_mat;
	
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::set_translation(const T x, const T y, const T z) {
	data[12] = x;
	data[13] = y;
	data[14] = z;
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::scale(const T x, const T y, const T z) {
	matrix4 scale_mat;
	scale_mat[0] = x;
	scale_mat[5] = y;
	scale_mat[10] = z;
	
	*this *= scale_mat;
	
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::rotate_x(const T x) {
	const T angle { const_math::deg_to_rad(x) };
	const T sinx { vector_helper<T>::sin(angle) };
	const T cosx { vector_helper<T>::cos(angle) };
	
	data[0] = ((T)1);
	data[1] = ((T)0);
	data[2] = ((T)0);
	data[3] = ((T)0);
	
	data[4] = ((T)0);
	data[5] = cosx;
	data[6] = sinx;
	data[7] = ((T)0);
	
	data[8] = ((T)0);
	data[9] = -sinx;
	data[10] = cosx;
	data[11] = ((T)0);
	
	data[12] = ((T)0);
	data[13] = ((T)0);
	data[14] = ((T)0);
	data[15] = ((T)1);
	
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::rotate_y(const T y) {
	const T angle { const_math::deg_to_rad(y) };
	const T siny { vector_helper<T>::sin(angle) };
	const T cosy { vector_helper<T>::cos(angle) };
	
	data[0] = cosy;
	data[1] = ((T)0);
	data[2] = -siny;
	data[3] = ((T)0);
	
	data[4] = ((T)0);
	data[5] = ((T)1);
	data[6] = ((T)0);
	data[7] = ((T)0);
	
	data[8] = siny;
	data[9] = ((T)0);
	data[10] = cosy;
	data[11] = ((T)0);
	
	data[12] = ((T)0);
	data[13] = ((T)0);
	data[14] = ((T)0);
	data[15] = ((T)1);
	
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::rotate_z(const T z) {
	const T angle { const_math::deg_to_rad(z) };
	const T sinz { vector_helper<T>::sin(angle) };
	const T cosz { vector_helper<T>::cos(angle) };
	
	data[0] = cosz;
	data[1] = sinz;
	data[2] = ((T)0);
	data[3] = ((T)0);
	
	data[4] = -sinz;
	data[5] = cosz;
	data[6] = ((T)0);
	data[7] = ((T)0);
	
	data[8] = ((T)0);
	data[9] = ((T)0);
	data[10] = ((T)1);
	data[11] = ((T)0);
	
	data[12] = ((T)0);
	data[13] = ((T)0);
	data[14] = ((T)0);
	data[15] = ((T)1);
	
	return *this;
}

// projection
template<typename T> constexpr matrix4<T>& matrix4<T>::perspective(const T fov, const T aspect, const T z_near, const T z_far) {
	typedef typename conditional<is_floating_point<T>::value, T, float>::type fp_type;
	const fp_type f {
		(fp_type)1 / vector_helper<fp_type>::tan(fp_type(fov) * const_math::PI_DIV_360<fp_type>)
	};
	
	////
	data[0] = (T)f / (T)aspect;
	data[1] = (T)0;
	data[2] = (T)0;
	data[3] = (T)0;
	
	data[4] = (T)0;
	data[5] = (T)f;
	data[6] = (T)0;
	data[7] = (T)0;
	
	data[8] = (T)0;
	data[9] = (T)0;
	data[10] = (z_far + z_near) / (z_near - z_far);
	data[11] = (T)-1;
	
	data[12] = (T)0;
	data[13] = (T)0;
	data[14] = (((T)2) * z_far * z_near) / (z_near - z_far);
	data[15] = (T)0;
	
	return *this;
}

template<typename T> constexpr matrix4<T>& matrix4<T>::ortho(const T left, const T right, const T bottom, const T top, const T z_near, const T z_far) {
	const T r_l(right - left);
	const T t_b(top - bottom);
	const T f_n(z_far - z_near);
	
	data[0] = ((T)2) / r_l;
	data[1] = ((T)0);
	data[2] = ((T)0);
	data[3] = ((T)0);
	
	data[4] = ((T)0);
	data[5] = ((T)2) / t_b;
	data[6] = ((T)0);
	data[7] = ((T)0);
	
	data[8] = ((T)0);
	data[9] = ((T)0);
	data[10] = -((T)2) / f_n;
	data[11] = ((T)0);
	
	data[12] = -(right + left) / (right - left);
	data[13] = -(top + bottom) / (top - bottom);
	data[14] = -(z_far + z_near) / (z_far - z_near);
	data[15] = ((T)1);
	
	return *this;
}

#if defined(FLOOR_EXPORT)
// only instantiate this in the matrix4.cpp
extern template class matrix4<float>;
extern template class matrix4<double>;
extern template class matrix4<int>;
extern template class matrix4<unsigned int>;
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
