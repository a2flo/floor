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

#pragma once

#include <floor/core/essentials.hpp>

// need at least CUDA 12.0 to compile and run
#define FLOOR_CUDA_API_VERSION_MIN 12000

#if !defined(FLOOR_NO_CUDA)

#include <floor/device/cuda/cuda_api.hpp>

// if FLOOR_DEVICE_BREAK_ON_ERROR is set, create a debug breakpoint
#if defined(FLOOR_DEBUG)
#define FLOOR_DEVICE_BREAK_ON_ERROR
#endif
#if defined(__clang__) && defined(FLOOR_DEVICE_BREAK_ON_ERROR)
#define CU_DBG_BREAKPOINT() { fl::logger::flush(); __builtin_debugtrap(); }
#else
#define CU_DBG_BREAKPOINT()
#endif

//
#define CU_CALL_FWD(call, error_msg, line_num, do_stuff) {									\
	CU_RESULT _cu_err = call;																\
	/* check if call was successful, or if CUDA is already shutting down, */				\
	/* in which case we just pretend nothing happened and continue ...    */				\
	if(_cu_err != CU_RESULT::SUCCESS && _cu_err != CU_RESULT::DEINITIALIZED) {				\
		const char* err_name, *err_str;														\
		cu_get_error_name(_cu_err, &err_name);												\
		cu_get_error_string(_cu_err, &err_str);												\
		log_error("$: line $: CUDA error $ (#$): $ (call: $)",								\
				  error_msg, line_num, (err_name != nullptr ? err_name : "INVALID"),		\
				  _cu_err, (err_str != nullptr ? err_str : "INVALID"), #call);				\
		CU_DBG_BREAKPOINT()																	\
		do_stuff																			\
	}																						\
}
#define CU_CALL_RET(call, error_msg, ...) CU_CALL_FWD(call, error_msg, __LINE__, return __VA_ARGS__ ;)
#define CU_CALL_CONT(call, error_msg, ...) CU_CALL_FWD(call, error_msg, __LINE__, continue;)
#define CU_CALL_IGNORE(call, ...) CU_CALL_FWD(call, "CUDA error", __LINE__,)
#define CU_CALL_NO_ACTION(call, error_msg, ...) CU_CALL_FWD(call, error_msg, __LINE__,)
#define CU_CALL_ERROR_EXEC(call, error_msg, error_exec) CU_CALL_FWD(call, error_msg, __LINE__, error_exec)

#endif
