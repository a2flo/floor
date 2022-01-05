/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

// NOTE: this file only exists to instantiate the vector class (split across multiple files to speed up compilation)

#include <floor/math/vector_lib.hpp>

// extern template instantiation
#if defined(FLOOR_EXPORT)
#define FLOOR_VECTOR_TMPL_INST(pod_type, prefix, vec_width) \
template class vector##vec_width<pod_type>;

FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_TMPL_INST, 4)
#endif
