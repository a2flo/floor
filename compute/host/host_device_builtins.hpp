/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_HOST_HOST_DEVICE_BUILTINS_HPP__
#define __FLOOR_HOST_HOST_DEVICE_BUILTINS_HPP__

#include <floor/core/essentials.hpp>
#include <unordered_map>
#include <string>

#if !defined(FLOOR_NO_HOST_COMPUTE)

//! function name prefix that is used for all builtin floor host device functions
#define FLOOR_HOST_DEVICE_FUNC_PREFIX libfloor_host_dev_

//! gets the map of supported "C function name" -> "builtin floor host device function name"
const std::unordered_map<std::string, std::string>& floor_get_c_to_floor_builtin_map();

#endif

#endif
