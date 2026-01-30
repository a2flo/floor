/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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
#include <cstdint>

namespace fl {

//! returns the number of logical CPU cores (hardware threads)
uint32_t get_logical_core_count();

//! returns the number of physical CPU cores
uint32_t get_physical_core_count();

//! returns the number of physical performance CPU cores
//! NOTE: only implemented on Apple platforms right now
uint32_t get_performance_core_count();

//! returns the number of physical efficiency CPU cores
//! NOTE: only implemented on Apple platforms right now
uint32_t get_efficiency_core_count();

//! sets the current threads affinity to the specified "affinity"
//! NOTE: 0 represents no affinity, 1 is CPU core #0, ...
void set_thread_affinity(const uint32_t affinity);

//! returns the name/label of the current thread (only works with pthreads)
std::string get_current_thread_name();

//! sets the name/label of the current thread to "thread_name" (only works with pthreads)
void set_current_thread_name(const std::string& thread_name);

//! makes the current process a high priority process, returns true on success
bool set_high_process_priority();

} // namespace fl
