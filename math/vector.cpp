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

#include <floor/math/vector_lib.hpp>
#include <floor/core/core.hpp>

template <typename scalar_type>
vector1<scalar_type> random(const scalar_type max) {
	return { core::rand(max) };
}
template <typename scalar_type>
vector2<scalar_type> random(const scalar_type max) {
	return { core::rand(max), core::rand(max) };
}
template <typename scalar_type>
vector3<scalar_type> random(const scalar_type max) {
	return { core::rand(max), core::rand(max), core::rand(max) };
}
template <typename scalar_type>
vector4<scalar_type> random(const scalar_type max) {
	return { core::rand(max), core::rand(max), core::rand(max), core::rand(max) };
}

template <typename scalar_type>
vector1<scalar_type> random(const scalar_type min, const scalar_type max) {
	return { core::rand(min, max) };
}
template <typename scalar_type>
vector2<scalar_type> random(const scalar_type min, const scalar_type max) {
	return { core::rand(min, max), core::rand(min, max) };
}
template <typename scalar_type>
vector3<scalar_type> random(const scalar_type min, const scalar_type max) {
	return { core::rand(min, max), core::rand(min, max), core::rand(min, max) };
}
template <typename scalar_type>
vector4<scalar_type> random(const scalar_type min, const scalar_type max) {
	return { core::rand(min, max), core::rand(min, max), core::rand(min, max), core::rand(min, max) };
}
