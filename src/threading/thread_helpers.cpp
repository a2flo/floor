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

#include <floor/threading/thread_helpers.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/cpp_ext.hpp>

#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(__APPLE__)
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#elif defined(__linux__)
#include <floor/core/core.hpp>
#include <pthread.h>
#include <sys/sysinfo.h>
#elif defined(__FreeBSD__)
#include <pthread.h>
#include <pthread_np.h>
#elif defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <strsafe.h>
#include <floor/core/essentials.hpp> // cleanup
#endif

namespace fl {

uint32_t get_logical_core_count() {
	uint32_t hw_thread_count = 1; // default to 1
#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
	size_t size = sizeof(hw_thread_count);
#if !defined(__OpenBSD__)
	sysctlbyname("hw.ncpu", &hw_thread_count, &size, nullptr, 0);
#else
	static const int sysctl_cmd[2] { CTL_HW, HW_NCPU };
	sysctl(sysctl_cmd, 2, &hw_thread_count, &size, nullptr, 0);
#endif
#elif defined(__linux__)
	hw_thread_count = (uint32_t)sysconf(_SC_NPROCESSORS_CONF);
#elif defined(__WINDOWS__)
	constexpr const uint32_t buffer_size { 16u };
	uint32_t size = buffer_size - 1;
	char output[buffer_size];
	GetEnvironmentVariable("NUMBER_OF_PROCESSORS", output, size);
	output[buffer_size - 1] = 0;
	hw_thread_count = stou(output);
#else // other platforms?
#endif
	return hw_thread_count;
}

uint32_t get_physical_core_count() {
	uint32_t core_count = 1; // default to 1
#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
	size_t size = sizeof(core_count);
#if !defined(__OpenBSD__)
	sysctlbyname("hw.physicalcpu", &core_count, &size, nullptr, 0);
#else
	static const int sysctl_cmd[2] { CTL_HW, HW_NCPU }; // TODO: is there a way to get the physical count?
	sysctl(sysctl_cmd, 2, &core_count, &size, nullptr, 0);
#endif
#elif defined(__linux__)
	std::string cpuinfo_output;
	core::system("grep \"cpu cores\" /proc/cpuinfo -m 1", cpuinfo_output);
	const auto rspace = cpuinfo_output.rfind(' ');
	if (rspace == std::string::npos) {
		return get_logical_core_count();
	}
	core_count = stou(cpuinfo_output.substr(rspace + 1));
#elif defined(__WINDOWS__)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align)
	uint32_t len = 0;
	GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, (PDWORD)&len);
	auto info_data = std::make_unique<uint8_t[]>(len);
	if (!GetLogicalProcessorInformationEx(RelationProcessorCore, (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)info_data.get(), (PDWORD)&len)) {
		return get_logical_core_count();
	}
	
	core_count = 0;
	auto info_ptr = (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)info_data.get();
	for (uint64_t cur_len = 0; cur_len < len; ++core_count) {
		assert(info_ptr->Relationship == RelationProcessorCore);
		cur_len += info_ptr->Size;
		info_ptr = (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(((const uint8_t*)info_ptr) + info_ptr->Size);
	}
FLOOR_POP_WARNINGS()
#else
#error "unhandled platform"
#endif
	return core_count;
}

void set_thread_affinity(const uint32_t affinity) {
#if defined(__APPLE__)
	thread_port_t thread_port = pthread_mach_thread_np(pthread_self());
	thread_affinity_policy thread_affinity { int(affinity) };
	thread_policy_set(thread_port, THREAD_AFFINITY_POLICY, (thread_policy_t)&thread_affinity, THREAD_AFFINITY_POLICY_COUNT);
#elif defined(__linux__) || defined(__FreeBSD__)
	// use gnu extension
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(affinity - 1, &cpu_set);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
#elif defined(__OpenBSD__)
	// TODO: pthread gnu extension not available here
#elif defined(__WINDOWS__)
	SetThreadAffinityMask(GetCurrentThread(), 1u << (affinity - 1u));
#else
#error "unhandled platform"
#endif
}

std::string get_current_thread_name() {
#if defined(_PTHREAD_H)
	char thread_name[16];
	pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
	thread_name[15] = 0;
	return thread_name;
#else
	return "<unknown>";
#endif
}

void set_current_thread_name([[maybe_unused]] const std::string& thread_name) {
#if defined(_PTHREAD_H)
	// pthreads restricts name sizes to 15 characters (+one \0)
	if (thread_name.size() > 15) {
		log_error("thread name is too long: $", thread_name);
	}
	const std::string name = (thread_name.size() > 15 ? thread_name.substr(0, 15) : thread_name);
	const auto err = pthread_setname_np(
#if !defined(__APPLE__)
										pthread_self(),
#endif
										name.c_str());
	 if (err != 0) {
		 log_error("failed to set thread name: $", err);
	 }
#endif
}

} // namespace fl
