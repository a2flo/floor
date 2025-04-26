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

#include <floor/core/sig_handler.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/threading/thread_helpers.hpp>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// there is no support for this on mingw/windows
#if !defined(__WINDOWS__) && __has_include(<execinfo.h>)

#include <cxxabi.h>
#include <dlfcn.h>

// disable recursive macro expansion warnings here, b/c glibc screwed up
#if defined(__GNU_LIBRARY__)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(disabled-macro-expansion)
#endif

#include <signal.h>
#include <execinfo.h>

namespace fl {

static struct sigaction act, act_ill;
static void sighandler(int, siginfo_t*, void*) {
#define STACK_PTR_COUNT 256
	void* stack_ptrs[STACK_PTR_COUNT];
	const int ptr_count = (int)backtrace(stack_ptrs, STACK_PTR_COUNT);
	const auto thread_name = get_current_thread_name();
	log_error("segfault/trap/abort in thread/process \"$\":", thread_name);
	
	// converts a stack and load address of a specified binary into a line number,
	// using atos (on macOS) or addr2line (everywhere else)
	static const auto addr_to_source_info = [](const char* binary_name,
#if defined(__APPLE__)
											   void* load_addr,
#else
											   void*, // unused elsewhere
#endif
											   void* stack_addr) -> std::string {
		if(binary_name == nullptr) return "";
		
		std::stringstream cmd;
#if defined(__APPLE__)
		cmd << "atos -o " << binary_name << " -l 0x" << std::hex << (size_t)load_addr << " " << (size_t)stack_addr << " 2>/dev/null";
#else
		cmd << "addr2line -Cie " << binary_name << " 0x" << std::hex << (size_t)stack_addr << " 2>/dev/null";
#endif
		
		std::string output;
		core::system(cmd.str(), output);
		if(output.empty()) return "";
		
		// line should contain a ':'
		const auto colon_pos = output.rfind(':');
		if(colon_pos == std::string::npos) return "";
		
#if defined(__APPLE__)
		const auto rparen_pos = output.find(')', colon_pos + 1);
		const auto lparen_pos = output.rfind('(', colon_pos);
		if(rparen_pos == std::string::npos || lparen_pos == std::string::npos) return "";
		return "@ " + output.substr(lparen_pos + 1, rparen_pos - lparen_pos - 1);
#else
		// "??" -> no info
		if(output.find("??") != std::string::npos) return "";
		
		// return complete line ("/path/to/some/file.cpp:42")
		return "@ " + output;
#endif
	};
	
	// proper stacktrace via glibc backtrace, dl* functionality, cxxabi demangle and addr2line/atos
	Dl_info dl_info;
	for(int i = 0; i < ptr_count; ++i) {
		if(dladdr(stack_ptrs[i], &dl_info) != 0) {
			const char* nearest_symbol_name = dl_info.dli_sname;
			
			int demangle_status = 0;
			char* demangled_func_name = abi::__cxa_demangle(nearest_symbol_name, nullptr, nullptr, &demangle_status);
			
			log_error("[$][$Y] $ -> $ $",
					  i, stack_ptrs[i],
					  (dl_info.dli_fname != nullptr ? dl_info.dli_fname : "<unknown>"),
					  (demangled_func_name != nullptr ? demangled_func_name :
					   (nearest_symbol_name != nullptr ? nearest_symbol_name : "<unknown>")),
					  core::trim(addr_to_source_info(dl_info.dli_fname, dl_info.dli_fbase, stack_ptrs[i])));
			
			if(demangled_func_name != nullptr) free(demangled_func_name);
		}
		else {
			log_error("[$][$Y] <unknown> -> <unknown>", i, stack_ptrs[i]);
		}
	}

	// kill logger
	logger::destroy();
}

[[noreturn]] static void ILLEGAL_INSTRUCTION_handler(int, siginfo_t*, void*) {
	// after a SIGILL (Illegal Instruction) code is usually continued to be executed (at the same spot however)
	// -> kill it manually (will call the normal sighandler)
	abort();
}

void register_segfault_handler() {
	// sig handler setup
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sighandler;
	// must cast all to int, b/c glibc ...
	act.sa_flags = int(SA_SIGINFO) | int(SA_NODEFER) | int(SA_RESETHAND);
	sigaction(SIGSEGV, &act, nullptr);
	sigaction(SIGTRAP, &act, nullptr);
	sigaction(SIGABRT, &act, nullptr);
	
	// sigill setup
	memset(&act_ill, 0, sizeof(act_ill));
	act_ill.sa_sigaction = ILLEGAL_INSTRUCTION_handler;
	// must cast all to int, b/c glibc ...
	act_ill.sa_flags = int(SA_SIGINFO) | int(SA_NODEFER) | int(SA_RESETHAND);
	sigaction(SIGILL, &act_ill, nullptr);
}

} // namespace fl
	
// reenable warnings that were disabled above, b/c of glibc
#if defined(__GNU_LIBRARY__)
FLOOR_POP_WARNINGS()
#endif

#elif !defined(_MSC_VER) // mingw version:

namespace fl {

void register_segfault_handler() {
	// nop
}

} // namespace fl

#else // msvc/windows version:

namespace fl {

void register_segfault_handler() {
	// nop
}

} // namespace fl

#endif
