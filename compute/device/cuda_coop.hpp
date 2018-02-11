/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_CUDA_COOP_HPP__
#define __FLOOR_COMPUTE_DEVICE_CUDA_COOP_HPP__

#if defined(FLOOR_COMPUTE_CUDA)
#if FLOOR_COMPUTE_INFO_CUDA_PTX >= 60

#if FLOOR_COMPUTE_INFO_CUDA_SM >= 60
// NOTE: these must be linked by the driver
extern "C" {
	extern uint64_t __cuda_syscall_CGS_get_intrinsic_handle(uint32_t scope); // aka "cudaCGGetIntrinsicHandle"
	extern uint32_t __cuda_syscall_CGS_sync(uint64_t handle, uint32_t flags); // aka "cudaCGSynchronize"
	extern uint32_t __cuda_syscall_CGS_get_size(uint32_t* work_item_count, uint32_t* device_count, uint64_t handle); // aka "cudaCGGetSize"
	extern uint32_t __cuda_syscall_CGS_get_rank(uint32_t* local_rank, uint32_t* global_rank, uint64_t handle); // aka "cudaCGGetRank"
}
#endif

// NOTE: this is _very_ experimental and subject to change
// global-group -> work-group -> sub-group
namespace coop {
	struct group_base {
	};
	
#if FLOOR_COMPUTE_INFO_CUDA_SM >= 60
	struct global_group : group_base {
		void barrier() {
			__cuda_syscall_CGS_sync(__cuda_syscall_CGS_get_intrinsic_handle(1 /* cudaCGScopeGrid */), 0);
		}
		
		static uint32_t size() {
			// product of global_size dims
			return (__nvvm_read_ptx_sreg_nctaid_x() * __nvvm_read_ptx_sreg_ntid_x() *
					__nvvm_read_ptx_sreg_nctaid_y() * __nvvm_read_ptx_sreg_ntid_y() *
					__nvvm_read_ptx_sreg_nctaid_z() * __nvvm_read_ptx_sreg_ntid_z());
		}
	};
#endif
	
	struct work_group : group_base {
		void barrier() {
			asm volatile("barrier.sync 0;");
		}
		void barrier(const uint32_t idx) __attribute__((enable_if(idx <= 15, "idx must be <= 15"))) {
			asm volatile("barrier.sync %0;" : : "r"(idx));
		}
	};
	
	struct sub_group : group_base {
		void barrier() {
			asm volatile("bar.warp.sync 0xFFFFFFFF;");
		}
		
	};
	
}

#endif
#endif

#endif
