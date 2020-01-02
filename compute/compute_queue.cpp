/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#include <floor/compute/compute_queue.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/compute_kernel.hpp>

void compute_queue::start_profiling() {
	finish();
	us_prof_start = core::unix_timestamp_us();
}

uint64_t compute_queue::stop_profiling() {
	finish();
	return core::unix_timestamp_us() - us_prof_start;
}

void compute_queue::kernel_execute_forwarder(shared_ptr<compute_kernel> kernel,
											 const bool is_cooperative,
											 const uint1& global_size, const uint1& local_size,
											 const vector<compute_kernel_arg>& args) const {
	kernel->execute(*this, is_cooperative, 1, uint3 { global_size }, uint3 { local_size }, args);
}

void compute_queue::kernel_execute_forwarder(shared_ptr<compute_kernel> kernel,
											 const bool is_cooperative,
											 const uint2& global_size, const uint2& local_size,
											 const vector<compute_kernel_arg>& args) const {
	kernel->execute(*this, is_cooperative, 2, uint3 { global_size }, uint3 { local_size }, args);
}

void compute_queue::kernel_execute_forwarder(shared_ptr<compute_kernel> kernel,
											 const bool is_cooperative,
											 const uint3& global_size, const uint3& local_size,
											 const vector<compute_kernel_arg>& args) const {
	kernel->execute(*this, is_cooperative, 3, global_size, local_size, args);
}
