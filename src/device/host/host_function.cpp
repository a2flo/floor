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

#if !defined(_WIN32)
// need to include this before all else
#if defined(__APPLE__) && !defined(_XOPEN_SOURCE) && !defined(_STRUCT_UCONTEXT)
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>
#endif

#include <floor/device/host/host_function.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/device/device_queue.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/host/host_buffer.hpp>
#include <floor/device/host/host_image.hpp>
#include <floor/device/host/host_queue.hpp>
#include <floor/device/host/elf_binary.hpp>
#include <floor/device/host/host_argument_buffer.hpp>
#include <floor/device/backend/host_limits.hpp>
#include <floor/device/backend/host_id.hpp>
#include <floor/device/soft_printf.hpp>
#include <floor/threading/thread_helpers.hpp>
#include <floor/core/logger.hpp>
#include <floor/floor.hpp>

//#define FLOOR_HOST_FUNCTION_ENABLE_TIMING 1
#if defined(FLOOR_HOST_FUNCTION_ENABLE_TIMING)
#include <floor/core/timer.hpp>
#endif

#if !defined(_WIN32)
// sanity check (mostly necessary on macOS where some fool had the idea to make the size of ucontext_t define dependent)
static_assert(sizeof(ucontext_t) > 64, "ucontext_t should not be this small, something is wrong!");
#endif

#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp> // cleanup

// ignore warnings about deprecated functions
FLOOR_IGNORE_WARNING(deprecated-declarations)

// for targets that don't have the same calling convention as the device code, we must ensure that inlining is prevented,
// otherwise the inlining will circumvent the calling convention change/attribute
// NOTE: on all other targets we do of course want inlining to happen if it's beneficial
#if defined(__WINDOWS__)
#define FLOOR_HOST_COMPUTE_CC_MAYBE_NOINLINE FLOOR_HOST_COMPUTE_CC __attribute__((noinline))
#else
#define FLOOR_HOST_COMPUTE_CC_MAYBE_NOINLINE FLOOR_HOST_COMPUTE_CC
#endif

//
namespace fl {
struct floor_fiber_context;
} // namespace fl
extern "C" void run_host_group_item(const uint32_t local_linear_idx) FLOOR_HOST_COMPUTE_CC;
extern "C" void run_host_device_group_item(const uint32_t local_linear_idx) FLOOR_HOST_COMPUTE_CC;
extern "C" void run_exec(fl::floor_fiber_context& main_ctx, fl::floor_fiber_context& first_item) FLOOR_HOST_COMPUTE_CC_MAYBE_NOINLINE;

// NOTE: due to rather fragile stack handling (rsp), this is completely done in asm, so that the compiler can't do anything wrong
#if defined(__x86_64__)
#if defined(__AVX512F__) && defined(__AVX512DQ__)
asm("floor_get_context_sysv_x86_64:"
	// store all registers in floor_fiber_context*
	"movq %rbp, %xmm1;"
	"pinsrq $1, %rbx, %xmm1;"
	"vinserti64x2 $0, %xmm1, %zmm0, %zmm0;"
	
	"movq %r12, %xmm2;"
	"pinsrq $1, %r13, %xmm2;"
	"vinserti64x2 $1, %xmm2, %zmm0, %zmm0;"
	
	"movq %r14, %xmm3;"
	"pinsrq $1, %r15, %xmm3;"
	"vinserti64x2 $2, %xmm3, %zmm0, %zmm0;"
	
	"movq %rsp, %rcx;"
	"addq $0x8, %rcx;"
	"movq %rcx, %xmm4;" // rsp
	"pinsrq $1, (%rsp), %xmm4;"
	"vinserti64x2 $3, %xmm4, %zmm0, %zmm0;"
	"vmovdqa64 %zmm0, (%rdi);" // rip
	
	"retq;");
asm("floor_set_context_sysv_x86_64:"
	// restore all registers from floor_fiber_context*
	"vmovdqu64 (%rdi), %zmm0;"
	"vextracti64x2 $3, %zmm0, %xmm4;"
	"vextracti64x2 $2, %zmm0, %xmm3;"
	"vextracti64x2 $1, %zmm0, %xmm2;"
	"vextracti64x2 $0, %zmm0, %xmm1;"
	
	"vmovq %xmm4, %rsp;"
	"vpextrq $1, %xmm4, %rcx;" // rip
	
	"vmovq %xmm3, %r14;"
	"vpextrq $1, %xmm3, %r15;"
	
	"vmovq %xmm2, %r12;"
	"vpextrq $1, %xmm2, %r13;"
	
	"vmovq %xmm1, %rbp;"
	"vpextrq $1, %xmm1, %rbx;"
	
	// and jump to rip (rcx)
	"jmp *%rcx;");
#elif defined(__AVX__)
asm("floor_get_context_sysv_x86_64:"
	// store all registers in floor_fiber_context*
	"prefetchw (%rdi);"
	"movq %rbp, %xmm0;"
	"pinsrq $1, %rbx, %xmm0;"
	"vmovdqa %xmm0, (%rdi);"
	
	"movq %r12, %xmm1;"
	"pinsrq $1, %r13, %xmm1;"
	"vmovdqa %xmm1, 0x10(%rdi);"
	
	"movq %r14, %xmm2;"
	"pinsrq $1, %r15, %xmm2;"
	"vmovdqa %xmm2, 0x20(%rdi);"
	
	"movq %rsp, %rcx;"
	"addq $0x8, %rcx;"
	"movq %rcx, %xmm3;" // rsp
	"pinsrq $1, (%rsp), %xmm3;"
	"vmovdqa %xmm3, 0x30(%rdi);" // rip
	
	"retq;");
asm("floor_set_context_sysv_x86_64:"
	// restore all registers from floor_fiber_context*
	"prefetchnta (%rdi);"
	
	"vmovdqa 0x30(%rdi), %xmm3;"
	"vmovq %xmm3, %rsp;"
	"vpextrq $1, %xmm3, %rcx;" // rip
	
	"vmovdqa 0x20(%rdi), %xmm2;"
	"vmovq %xmm2, %r14;"
	"vpextrq $1, %xmm2, %r15;"
	
	"vmovdqa 0x10(%rdi), %xmm1;"
	"vmovq %xmm1, %r12;"
	"vpextrq $1, %xmm1, %r13;"
	
	"vmovdqa (%rdi), %xmm0;"
	"vmovq %xmm0, %rbp;"
	"vpextrq $1, %xmm0, %rbx;"
	
	// and jump to rip (rcx)
	"jmp *%rcx;");
#else
asm("floor_get_context_sysv_x86_64:"
	// store all registers in floor_fiber_context*
	"movq %rbp, 0x0(%rdi);"
	"movq %rbx, 0x8(%rdi);"
	"movq %r12, 0x10(%rdi);"
	"movq %r13, 0x18(%rdi);"
	"movq %r14, 0x20(%rdi);"
	"movq %r15, 0x28(%rdi);"
	"movq %rsp, %rcx;"
	"addq $0x8, %rcx;"
	"movq %rcx, 0x30(%rdi);" // rsp
	"movq (%rsp), %rcx;"
	"movq %rcx, 0x38(%rdi);" // rip
	"retq;");
asm("floor_set_context_sysv_x86_64:"
	// restore all registers from floor_fiber_context*
	"movq 0x38(%rdi), %rcx;"
	"movq 0x30(%rdi), %rsp;"
	"movq 0x28(%rdi), %r15;"
	"movq 0x20(%rdi), %r14;"
	"movq 0x18(%rdi), %r13;"
	"movq 0x10(%rdi), %r12;"
	"movq 0x8(%rdi), %rbx;"
	"movq 0x0(%rdi), %rbp;"
	// and jump to rip (rcx)
	"jmp *%rcx;");
#endif
asm(".extern exit;"
	"floor_enter_context_sysv_x86_64:"
	// retrieve floor_fiber_context*
	"movq 0x8(%rsp), %rax;"
	// floor_fiber_context->init_func
	"movq 0x50(%rax), %rcx;"
	// floor_fiber_context->init_arg
	"movl 0x68(%rax), %edi;"
	// call init_func(init_arg)
	"callq *%rcx;"
	// context is done, -> exit to set exit context, or exit(0)
	// retrieve floor_fiber_context* again
	"movq 0x8(%rsp), %rax;"
	// exit floor_fiber_context*
	"movq 0x58(%rax), %rdi;"
	// TODO: cmp 0, -> exit(0)
	// set_context(exit_context)
	"callq floor_set_context_sysv_x86_64;"
	// it's a trap!
	"ud2;");
extern "C" void floor_get_context(void* ctx) asm("floor_get_context_sysv_x86_64");
extern "C" void floor_set_context(void* ctx) asm("floor_set_context_sysv_x86_64");
extern "C" void floor_enter_context() asm("floor_enter_context_sysv_x86_64");

#elif defined(__aarch64__)
asm("floor_get_context_aarch64:"
	// store all registers in floor_fiber_context*
	"stp x19, x20, [x0]\n"
	"stp x21, x22, [x0, #16]\n"
	"stp x23, x24, [x0, #32]\n"
	"stp x25, x26, [x0, #48]\n"
	"stp x27, x28, [x0, #64]\n"
	"str x29, [x0, #80]\n" // fp
	"stp d8, d9, [x0, #88]\n"
	"stp d10, d11, [x0, #104]\n"
	"stp d12, d13, [x0, #120]\n"
	"stp d14, d15, [x0, #136]\n"
	"mov x9, sp\n" // must move sp to usable register first
	"stp x9, x30, [x0, #152]\n" // sp and lr -> ip (assume this was called with "bl")
	"ret;\n");
asm("floor_set_context_aarch64:"
	// restore all registers from floor_fiber_context*
	"ldp x9, x30, [x0, #152]\n" // sp
	"ldp d14, d15, [x0, #136]\n"
	"ldp d12, d13, [x0, #120]\n"
	"ldp d10, d11, [x0, #104]\n"
	"ldp d8, d9, [x0, #88]\n"
	"ldp x28, x29, [x0, #72]\n" // fp
	"ldp x26, x27, [x0, #56]\n"
	"ldp x24, x25, [x0, #40]\n"
	"ldp x22, x23, [x0, #24]\n"
	"ldp x20, x21, [x0, #8]\n"
	"ldr x19, [x0]\n"
	"mov sp, x9\n"
	// and jump to ip
	"br x30;\n");
asm("floor_enter_context_aarch64:"
	// retrieve floor_fiber_context*
	"ldr x9, [sp, #8]\n"
	// floor_fiber_context->init_func
	"ldr x10, [x9, #184]\n"
	// floor_fiber_context->init_arg
	"ldr w0, [x9, #208]\n"
	// call init_func(init_arg)
	"blr x10\n"
	// context is done, -> exit to set exit context, or exit(0)
	// retrieve floor_fiber_context* again
	"ldr x9, [sp, #8]\n"
	// exit floor_fiber_context*
	"ldr x0, [x9, #192]\n"
	// TODO: cmp 0, -> exit(0)
	// set_context(exit_context)
	"bl floor_set_context_aarch64\n"
	// it's a trap!
	"udf #0;\n");
extern "C" void floor_get_context(void* ctx) asm("floor_get_context_aarch64");
extern "C" void floor_set_context(void* ctx) asm("floor_set_context_aarch64");
extern "C" void floor_enter_context() asm("floor_enter_context_aarch64");

#endif

namespace fl {

static constexpr const size_t floor_fiber_context_alignment {
#if defined(__x86_64__)
	128u
#elif defined(__aarch64__)
	256u
#else
#error "unhandled arch"
#endif
};

struct alignas(floor_fiber_context_alignment) floor_fiber_context {
	using init_func_type = void (FLOOR_HOST_COMPUTE_CC *)(const uint32_t);
	
#if defined(__x86_64__) || defined(__aarch64__)
	// SysV x86-64 ABI / ARM AArch64 AAPCS64 compliant implementation
	static constexpr const size_t min_stack_size { std::max(size_t(8192u), size_t(aligned_ptr<int>::page_size)) };
	static_assert(min_stack_size % 16ull == 0, "stack must be 16-byte aligned");
	
	// callee-saved registers
#if defined(__x86_64__)
	uint64_t rbp { 0 };
	uint64_t rbx { 0 };
	uint64_t r12 { 0 };
	uint64_t r13 { 0 };
	uint64_t r14 { 0 };
	uint64_t r15 { 0 };
#elif defined(__aarch64__)
	uint64_t r19 { 0 };
	uint64_t r20 { 0 };
	uint64_t r21 { 0 };
	uint64_t r22 { 0 };
	uint64_t r23 { 0 };
	uint64_t r24 { 0 };
	uint64_t r25 { 0 };
	uint64_t r26 { 0 };
	uint64_t r27 { 0 };
	uint64_t r28 { 0 };
	uint64_t fp { 0 }; // aka r29
	// lower 64-bit of SIMD registers
	uint64_t d8 { 0 };
	uint64_t d9 { 0 };
	uint64_t d10 { 0 };
	uint64_t d11 { 0 };
	uint64_t d12 { 0 };
	uint64_t d13 { 0 };
	uint64_t d14 { 0 };
	uint64_t d15 { 0 };
#endif
	// stack pointer
	uint64_t sp { 0 };
	// return address / instruction pointer
	uint64_t ip { 0 };
	
	void init(void* stack_ptr_, const size_t& stack_size_,
			  init_func_type init_func_, const uint32_t& init_arg_,
			  floor_fiber_context* exit_ctx_, floor_fiber_context* main_ctx_) noexcept {
		init_common(stack_ptr_, stack_size_, init_func_, init_arg_, exit_ctx_, main_ctx_);
		
		if (size_t(this) % floor_fiber_context_alignment != 0u) {
			log_error("floor_fiber_context must be $-byte aligned!", floor_fiber_context_alignment); logger::flush();
			return;
		}
		
		if (stack_ptr != nullptr) {
			// check stack pointer validity (must be 16-byte aligned)
			if (size_t(stack_ptr) % 16u != 0u) {
				log_error("stack must be 16-byte aligned!"); logger::flush();
				return;
			}
			
			// set the first 64-bit value on the stack to this context and the second value to a canary value
			// note that this is only done once, not on every reset, because:
			//  a) it isn't necessary (if everything goes well)
			//  b) if the user function code does overwrite this (stack overflow), this will certainly crash (as it should!)
			auto stack_addr = (uint64_t*)stack_ptr + stack_size / 8ull;
			*(stack_addr - 1u) = (uint64_t)this;
#if defined(FLOOR_DEBUG)
			// for stack protection (well, corruption detection ...) purposes
			// TODO: check this on exit (in debug mode or when manually enabled)
			*(stack_addr - 2u) = 0x0123456789ABCDEFull;
#endif
		}
	}
	
	void reset() noexcept {
		// reset registers, set ip to enter_context and reset sp
#if defined(FLOOR_DEBUG) // this isn't actually necessary
#if defined(__x86_64__)
		rbp = 0;
		rbx = 0;
		r12 = 0;
		r13 = 0;
		r14 = 0;
		r15 = 0;
#elif defined(__aarch64__)
		r19 = 0;
		r20 = 0;
		r21 = 0;
		r22 = 0;
		r23 = 0;
		r24 = 0;
		r25 = 0;
		r26 = 0;
		r27 = 0;
		r28 = 0;
		fp = 0;
		d8 = 0;
		d9 = 0;
		d10 = 0;
		d11 = 0;
		d12 = 0;
		d13 = 0;
		d14 = 0;
		d15 = 0;
#endif
#endif
		
		// we've pushed two 64-bit values here + needs to be 16-byte aligned
		sp = ((size_t)stack_ptr) + stack_size - 16u;
		ip = (uint64_t)floor_enter_context;
		*(uint64_t*)(sp + 8u) = (uint64_t)this;
#if defined(FLOOR_DEBUG)
		*(uint64_t*)(sp) = 0x0123456789ABCDEFull;
#endif
	}
	
	void get_context() noexcept FLOOR_HOST_COMPUTE_CC_MAYBE_NOINLINE {
		floor_get_context(this);
	}
	
	// ignore the "missing noreturn" warning here, because actually setting [[noreturn]] leads to unwanted codegen
	// (ud2 insertion at a point we don't want this to happen -> we already have a ud2 trap in enter_context)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(missing-noreturn)
	void set_context() noexcept FLOOR_HOST_COMPUTE_CC_MAYBE_NOINLINE {
		floor_set_context(this);
		floor_unreachable();
	}
FLOOR_POP_WARNINGS()
	
	void swap_context(floor_fiber_context* next_ctx) noexcept FLOOR_HOST_COMPUTE_CC_MAYBE_NOINLINE {
		// NOTE: order of operation in here:
		// * fiber #1 enters
		// * set swapped to false
		// * get_context() saves the current point of execution for later resume ("store pc", etc.)
		// * swapped is still false here -> execute the if block
		// * set swapped to true
		// * switch to fiber #2 (next_ctx)
		// * some other fiber -> resume fiber #1 after "get_context()"
		// * swapped is now true -> don't execute the if block -> return
		volatile bool swapped = false;
		get_context();
		if (!swapped) {
			swapped = true;
#if defined(__clang_analyzer__)
			(void)swapped; // -> mark as used (will be read again)
#endif
			next_ctx->set_context();
		}
	}
	
#else
	// fallback to posix ucontext_t/etc
	static constexpr const size_t min_stack_size { 32768 };
	
	ucontext_t ctx;
	
	void init(void* stack_ptr_, const size_t& stack_size_,
			  init_func_type init_func_, const uint32_t& init_arg_,
			  floor_fiber_context* exit_ctx_, floor_fiber_context* main_ctx_) noexcept {
		init_common(stack_ptr_, stack_size_, init_func_, init_arg_, exit_ctx_, main_ctx_);
		
		memset(&ctx, 0, sizeof(ucontext_t));
		getcontext(&ctx);
		
		// unknown context vars -> query external
		if (stack_ptr == nullptr) {
			stack_ptr = ctx.uc_stack.ss_sp;
			stack_size = ctx.uc_stack.ss_size;
		}
	}
	
	void reset() noexcept {
		if (exit_ctx != nullptr) {
			ctx.uc_link = &exit_ctx->ctx;
		}
		else ctx.uc_link = nullptr;
		ctx.uc_stack.ss_sp = stack_ptr;
		ctx.uc_stack.ss_size = stack_size;
		makecontext(&ctx, (void (*)())init_func, 1, init_arg);
	}
	
	void get_context() noexcept {
		getcontext(&ctx);
	}
	
	void set_context() noexcept {
		setcontext(&ctx);
	}
	
	void swap_context(floor_fiber_context* next_ctx) noexcept {
		swapcontext(&ctx, &next_ctx->ctx);
	}
#endif
	
	void init_common(void* stack_ptr_, const size_t& stack_size_,
					 init_func_type init_func_, const uint32_t& init_arg_,
					 floor_fiber_context* exit_ctx_, floor_fiber_context* main_ctx_) noexcept {
		stack_ptr = stack_ptr_;
		stack_size = stack_size_;
		init_func = init_func_;
		exit_ctx = exit_ctx_;
		main_ctx = main_ctx_;
		init_arg = init_arg_;
	}
	
	void exit_to_main() noexcept {
		swap_context(main_ctx);
	}
	
	// ctx vars
	void* stack_ptr { nullptr };
	size_t stack_size { 0 };
	
	// do not change the order of these vars
	init_func_type init_func { nullptr };
	floor_fiber_context* exit_ctx { nullptr };
	floor_fiber_context* main_ctx { nullptr };
	uint32_t init_arg { 0 };
	
};
static_assert(sizeof(floor_fiber_context) <= floor_fiber_context_alignment, "floor_fiber_context alignment is not large enough");

// make sure member variables are at the right offsets when using the sysv abi fiber approach
#if defined(__x86_64__)
static_assert(offsetof(floor_fiber_context, init_func) == 0x50);
static_assert(offsetof(floor_fiber_context, exit_ctx) == 0x58);
static_assert(offsetof(floor_fiber_context, main_ctx) == 0x60);
static_assert(offsetof(floor_fiber_context, init_arg) == 0x68);
#elif defined(__aarch64__) && !defined(__WINDOWS__)
static_assert(offsetof(floor_fiber_context, init_func) == 184);
static_assert(offsetof(floor_fiber_context, exit_ctx) == 192);
static_assert(offsetof(floor_fiber_context, main_ctx) == 200);
static_assert(offsetof(floor_fiber_context, init_arg) == 208);
#endif

// will be initialized to "max h/w threads", note that this is stored in a global var,
// so that get_logical_core_count() doesn't have to called over and over again, and
// so this is actually a consistent value (bad things will happen if it isn't)
static uint32_t floor_max_thread_count { 0 };

// local memory management
static constexpr const size_t floor_local_memory_max_size { host_limits::local_memory_size };
static uint32_t local_memory_alloc_offset { 0 };
static bool local_memory_exceeded { false };
static aligned_ptr<uint8_t> floor_local_memory_data;

// host-device print buffer
static aligned_ptr<uint8_t> floor_host_device_printf_buffer;

// stack memory management
// 8k stack should be enough, considering this runs on GPUs (min 32k with ucontext)
// TODO: stack protection?
static constexpr const size_t item_stack_size { floor_fiber_context::min_stack_size };
static aligned_ptr<uint8_t> floor_stack_memory_data;

static void floor_alloc_host_local_memory() {
	if (!floor_local_memory_data) {
		floor_local_memory_data = make_aligned_ptr<uint8_t>(floor_max_thread_count * floor_local_memory_max_size);
	}
}

static void floor_alloc_host_device_printf_buffer() {
	if (!floor_host_device_printf_buffer) {
		// alloc
		floor_host_device_printf_buffer = make_aligned_ptr<uint8_t>(printf_buffer_size);
		// init
		auto buffer_ptr = floor_host_device_printf_buffer.get_as<uint32_t>();
		*buffer_ptr = printf_buffer_header_size;
		*(buffer_ptr + 1) = printf_buffer_size;
	}
}

static void floor_alloc_host_stack_memory() {
	if (!floor_stack_memory_data) {
		floor_stack_memory_data = make_aligned_ptr<uint8_t>(floor_max_thread_count * item_stack_size * host_limits::max_total_local_size);
	}
}

//
host_function::host_function(const std::string_view function_name_, const void* function_, device_function::function_entry&& entry_) :
device_function(function_name_), function(function_), entry(std::move(entry_)) {
}

host_function::host_function(const std::string_view function_name_, function_map_type&& functions_) :
device_function(function_name_), functions(std::move(functions_)) {
}

const host_function::function_entry* host_function::get_function_entry(const device& dev) const {
	if (function != nullptr) {
		return &entry; // can't really check if the device is correct here
	}
	if (const auto iter = functions.find((const host_device*)&dev); iter != functions.end()) {
		return &iter->second;
	}
	return nullptr;
}

typename host_function::function_map_type::const_iterator host_function::get_function(const device_queue& cqueue) const {
	return functions.find((const host_device*)&cqueue.get_device());
}

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-function-type-strict)
FLOOR_IGNORE_WARNING(cast-qual)
FLOOR_IGNORE_WARNING(ctad-maybe-unsupported)
FLOOR_IGNORE_WARNING(weak-vtables)

struct host_function_wrapper {
	struct host_function_base {
		host_function_base() noexcept = default;
		host_function_base(host_function_base&&) noexcept = default;
		host_function_base(const host_function_base&) noexcept = default;
		virtual ~host_function_base() noexcept = default;
		host_function_base& operator=(host_function_base&&) noexcept = default;
		host_function_base& operator=(const host_function_base&) noexcept = default;
		virtual void call() const noexcept FLOOR_HOST_COMPUTE_CC {}
	};
	
	template <typename int_type, int_type... ints>
	struct host_function final : host_function_base {
		void* func_ptr { nullptr };
		const std::vector<const void*>& vptr_args;
		
		host_function(void* func_ptr_, const std::vector<const void*>& vptr_args_) noexcept :
		host_function_base(), func_ptr(func_ptr_), vptr_args(vptr_args_) {}
		
		//! needed to cast the function type to a function type with the correct amount of parameters
		template <typename... Args> using host_function_type_t = void (FLOOR_HOST_COMPUTE_CC *)(Args...);
		
		//! needed to create a const void* parameter pack
		template <auto> using make_const_void_ptr = const void*;
		
		void call() const noexcept override FLOOR_HOST_COMPUTE_CC {
			(*(host_function_type_t<make_const_void_ptr<ints>...>)func_ptr)(vptr_args[ints]...);
		}
	};
	
	std::unique_ptr<host_function_base> kfunc {};
	
	void operator()() const noexcept FLOOR_HOST_COMPUTE_CC {
		kfunc->call();
	}
	
	explicit operator bool() const {
		return (kfunc != nullptr);
	}
	
};

template <typename int_type, int_type... ints>
static host_function_wrapper make_host_function_wrapper(const void* function_ptr,
													const std::vector<const void*>& vptr_args,
													std::integer_sequence<int_type, ints...>) {
	host_function_wrapper::host_function<int_type, ints...> kfunc((void*)function_ptr, vptr_args);
	return host_function_wrapper {
		.kfunc = std::make_unique<decltype(kfunc)>(std::move(kfunc))
	};
}

static host_function_wrapper make_callable_host_function(const void* function_ptr, const std::vector<const void*>& vptr_args) {
	switch (vptr_args.size()) {
		case 0: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 0u> {});
		case 1: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 1u> {});
		case 2: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 2u> {});
		case 3: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 3u> {});
		case 4: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 4u> {});
		case 5: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 5u> {});
		case 6: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 6u> {});
		case 7: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 7u> {});
		case 8: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 8u> {});
		case 9: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 9u> {});
		case 10: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 10u> {});
		case 11: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 11u> {});
		case 12: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 12u> {});
		case 13: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 13u> {});
		case 14: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 14u> {});
		case 15: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 15u> {});
		case 16: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 16u> {});
		case 17: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 17u> {});
		case 18: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 18u> {});
		case 19: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 19u> {});
		case 20: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 20u> {});
		case 21: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 21u> {});
		case 22: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 22u> {});
		case 23: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 23u> {});
		case 24: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 24u> {});
		case 25: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 25u> {});
		case 26: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 26u> {});
		case 27: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 27u> {});
		case 28: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 28u> {});
		case 29: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 29u> {});
		case 30: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 30u> {});
		case 31: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 31u> {});
		case 32: return make_host_function_wrapper(function_ptr, vptr_args, std::make_integer_sequence<int, 32u> {});
		default:
			log_error("too many function parameters specified (only up to 32 parameters are supported)");
			return {};
	}
}

FLOOR_POP_WARNINGS()

// Host-Compute device execution context (per work-group / logical CPU)
struct device_exec_context_t {
	// dynamic and constant IDs + sizes
	elf_binary::instance_ids_t* ids { nullptr };
	
	// linear 1D local size (for barrier handling)
	uint32_t linear_local_work_size { 0 };
	
	// all fiber contexts in this work-group
	floor_fiber_context* item_contexts { nullptr };
	
	// current active function
	host_function_wrapper func;
	
	// -> sanity check for correct barrier use
#if defined(FLOOR_DEBUG)
	uint32_t unfinished_items { 0 };
#endif
};
static thread_local device_exec_context_t device_exec_context;

// Host-Compute host execution context (per work-group / logical CPU)
struct host_exec_context_t {
	// dynamic and constant IDs + sizes
	elf_binary::instance_ids_t ids {};
	
	// offset in global "local memory" buffer
	uint32_t thread_local_memory_offset { 0 };
	
	// linear 1D local size (for barrier handling)
	uint32_t linear_local_work_size { 0 };
	
	// all fiber contexts in this work-group
	floor_fiber_context* item_contexts { nullptr };
	
	// current active function
	const host_function_wrapper* func { nullptr };
	
	// -> sanity check for correct barrier use
#if defined(FLOOR_DEBUG)
	uint32_t unfinished_items { 0 };
#endif
};
static thread_local host_exec_context_t host_exec_context;

void host_function::execute(const device_queue& cqueue,
						  const bool& is_cooperative,
						  const bool& wait_until_completion floor_unused /* will always wait anyways */,
						  const uint32_t& work_dim,
						  const uint3& global_work_size,
						  const uint3& local_work_size,
						  const std::vector<device_function_arg>& args,
						  const std::vector<const device_fence*>& wait_fences floor_unused,
						  const std::vector<device_fence*>& signal_fences floor_unused,
						  const char* debug_label floor_unused,
						  kernel_completion_handler_f&& completion_handler) const {
	// TODO: implement waiting for "wait_fences" and signaling "signal_fences" (for now, this is blocking anyways)
	
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for Host-Compute");
		return;
	}
	
	// extract/handle function arguments
	std::vector<const void*> vptr_args;
	std::vector<std::unique_ptr<void*[]>> array_args;
	vptr_args.reserve(args.size());
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const device_buffer*>(&arg.var)) {
			vptr_args.emplace_back(((const host_buffer*)(*buf_ptr))->get_host_buffer_ptr_with_sync());
		} else if (auto vec_buf_ptrs = get_if<const std::vector<device_buffer*>*>(&arg.var)) {
			auto arr_arg = std::make_unique<void*[]>((*vec_buf_ptrs)->size());
			auto arr_buf_ptr = arr_arg.get();
			for (const auto& buf : **vec_buf_ptrs) {
				*arr_buf_ptr++ = (buf ? ((const host_buffer*)buf)->get_host_buffer_ptr_with_sync() : nullptr);
			}
			vptr_args.emplace_back(arr_arg.get());
			array_args.emplace_back(std::move(arr_arg));
		} else if (auto vec_buf_sptrs = get_if<const std::vector<std::shared_ptr<device_buffer>>*>(&arg.var)) {
			auto arr_arg = std::make_unique<void*[]>((*vec_buf_sptrs)->size());
			auto arr_buf_ptr = arr_arg.get();
			for (const auto& buf : **vec_buf_sptrs) {
				*arr_buf_ptr++ = (buf ? ((const host_buffer*)buf.get())->get_host_buffer_ptr_with_sync() : nullptr);
			}
			vptr_args.emplace_back(arr_arg.get());
			array_args.emplace_back(std::move(arr_arg));
		} else if (auto img_ptr = get_if<const device_image*>(&arg.var)) {
			vptr_args.emplace_back(((const host_image*)(*img_ptr))->get_host_image_program_info_with_sync());
		} else if (auto vec_img_ptrs = get_if<const std::vector<device_image*>*>(&arg.var); vec_img_ptrs && *vec_img_ptrs) {
			auto arr_arg = std::make_unique<void*[]>((*vec_img_ptrs)->size());
			auto arr_img_ptr = arr_arg.get();
			for (const auto& img : **vec_img_ptrs) {
				*arr_img_ptr++ = (img ? ((const host_image*)img)->get_host_image_program_info_with_sync() : nullptr);
			}
			vptr_args.emplace_back(arr_arg.get());
			array_args.emplace_back(std::move(arr_arg));
		} else if (auto vec_img_sptrs = get_if<const std::vector<std::shared_ptr<device_image>>*>(&arg.var); vec_img_sptrs && *vec_img_sptrs) {
			auto arr_arg = std::make_unique<void*[]>((*vec_img_sptrs)->size());
			auto arr_img_ptr = arr_arg.get();
			for (const auto& img : **vec_img_sptrs) {
				*arr_img_ptr++ = (img ? ((const host_image*)img.get())->get_host_image_program_info_with_sync() : nullptr);
			}
			vptr_args.emplace_back(arr_arg.get());
			array_args.emplace_back(std::move(arr_arg));
		} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			const auto storage_buffer = (const host_buffer*)(*arg_buf_ptr)->get_storage_buffer();
			vptr_args.emplace_back(storage_buffer->get_host_buffer_ptr());
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			vptr_args.emplace_back(*generic_arg_ptr);
		} else {
			log_error("encountered invalid arg");
			return;
		}
	}
	
	// init max thread count (once!)
	if (floor_max_thread_count == 0) {
		floor_max_thread_count = get_logical_core_count();
	}
	
	// device CPU count must be <= h/w thread count, b/c local memory is only allocated for such many threads
	auto cpu_count = cqueue.get_device().units;
	if (const auto max_core_count = floor::get_host_max_core_count(); max_core_count > 0) {
		cpu_count = std::min(cpu_count, max_core_count);
	}
	if (cpu_count > floor_max_thread_count) {
		log_error("device CPU count exceeds h/w count");
		return;
	}
	
	// only a single function can be active/executed at one time
	// TODO: can this be "fixed" by Host-Compute device execution?
	static safe_mutex exec_lock {};
	{
		GUARD(exec_lock);
		
		const uint3 local_dim { check_local_work_size(entry, local_work_size).maxed(1u) };
		const uint3 group_dim_overflow {
			global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % local_dim.x), 1u) : 0u,
			global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % local_dim.y), 1u) : 0u,
			global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % local_dim.z), 1u) : 0u
		};
		uint3 group_dim { (global_work_size / local_dim) + group_dim_overflow };
		group_dim.max(1u);
		
		const auto mod_groups = global_work_size % local_dim;
		uint3 group_size = global_work_size / local_dim;
		if (mod_groups.x > 0) ++group_size.x;
		if (mod_groups.y > 0) ++group_size.y;
		if (mod_groups.z > 0) ++group_size.z;
		
		// alloc stack memory (for all threads) if it hasn't been allocated yet
		floor_alloc_host_stack_memory();
		
		// device or host execution?
		// NOTE: when using a function that has been compiled into the program (not Host-Compute device), "function" will be non-nullptr
		if (function == nullptr) {
			// allow printf buffer memory (only done once)
			floor_alloc_host_device_printf_buffer();
			auto printf_buffer_ptr = floor_host_device_printf_buffer.get_as<uint32_t>();
			if (*printf_buffer_ptr > printf_buffer_header_size) {
				// reset
				*printf_buffer_ptr = printf_buffer_header_size;
				*(printf_buffer_ptr + 1) = printf_buffer_size;
			}
			
			// -> device execution
			const auto function_iter = get_function(cqueue);
			if (function_iter == functions.cend() || function_iter->second.program == nullptr) {
				log_error("no program for this compute queue/device exists!");
				return;
			}
			execute_device(function_iter->second, cpu_count, group_dim, local_dim, work_dim, vptr_args);
			
			// eval printf buffer if anything was written
			if (*printf_buffer_ptr > printf_buffer_header_size) {
				handle_printf_buffer(std::span { printf_buffer_ptr, printf_buffer_size / 4u });
			}
		} else {
			// -> host execution
			auto callable_function = make_callable_host_function(function, vptr_args);
			if (!callable_function) {
				return;
			}
			
			// setup local memory management
			local_memory_alloc_offset = 0;
			local_memory_exceeded = false;
			// alloc local (for all threads) if it hasn't been allocated yet
			floor_alloc_host_local_memory();
			
			execute_host(callable_function, cpu_count, group_dim, group_size, global_work_size, local_dim, work_dim);
		}
		
		// this is always executed synchronously, so we can just directly call the completion handler
		if (completion_handler) {
			completion_handler();
		}
	}
}

//! starts the actual fiber context execution (needed here for CC purposes)
extern "C" void run_exec(floor_fiber_context& main_ctx, floor_fiber_context& first_item) FLOOR_HOST_COMPUTE_CC_MAYBE_NOINLINE {
	static thread_local volatile bool done;
	done = false;
	main_ctx.get_context();
	if (!done) {
		done = true;
		
		// start first fiber
		first_item.set_context();
	}
}

void host_function::execute_host(const host_function_wrapper& func,
							   const uint32_t& cpu_count,
							   const uint3& group_dim,
							   const uint3& group_size,
							   const uint3& global_dim,
							   const uint3& local_dim,
							   const uint32_t& work_dim) const {
	// #work-groups
	const auto group_count = group_dim.x * group_dim.y * group_dim.z;
	// #work-items per group
	const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
	// group ticketing system, each worker thread will grab a new group id, once it's done with one group
	std::atomic<uint32_t> group_idx { 0 };
	
	// start worker threads
#if defined(FLOOR_HOST_FUNCTION_ENABLE_TIMING)
	const auto time_start = floor_timer::start();
#endif
	std::vector<std::unique_ptr<std::thread>> worker_threads(cpu_count);
	for (uint32_t cpu_idx = 0; cpu_idx < cpu_count; ++cpu_idx) {
		worker_threads[cpu_idx] = std::make_unique<std::thread>([this, &func, cpu_idx,
													   &group_idx, group_count, group_dim, group_size,
													   global_dim, local_size, local_dim, work_dim] {
			// set CPU affinity for this thread to a particular CPU to prevent this thread from being constantly moved/scheduled
			// on different CPUs (starting at index 1, with 0 representing no affinity)
			set_thread_affinity(cpu_idx + 1);
			
			// get and init host execution context
			auto& exec_ctx = host_exec_context;
			exec_ctx.ids = {
				.instance_global_idx = { 0, 0, 0 },
				.instance_global_work_size = global_dim,
				.instance_local_idx = { 0, 0, 0 },
				.instance_local_work_size = local_dim,
				.instance_group_idx = { 0, 0, 0 },
				.instance_group_size = group_size,
				.instance_work_dim = work_dim,
				.instance_local_linear_idx = 0u,
			};
			exec_ctx.linear_local_work_size = local_size;
			exec_ctx.func = &func;
			exec_ctx.thread_local_memory_offset = cpu_idx * floor_local_memory_max_size;
			
			// init contexts (aka fibers)
			floor_fiber_context main_ctx;
			main_ctx.init(nullptr, 0, nullptr, ~0u, nullptr, nullptr);
			auto items = std::make_unique<floor_fiber_context[]>(local_size);
			exec_ctx.item_contexts = items.get();
			
			// init fibers
			for (uint32_t i = 0; i < local_size; ++i) {
				items[i].init(&floor_stack_memory_data.get()[(i + local_size * cpu_idx) * floor_fiber_context::min_stack_size],
							  floor_fiber_context::min_stack_size,
							  run_host_group_item, i,
							  // continue with next on return, or return to main ctx when the last item returns
							  // TODO: add option to use randomized order?
							  (i + 1 < local_size ? &items[i + 1] : &main_ctx),
							  &main_ctx);
			}
			
			for (;;) {
				// assign a new group to this thread/CPU and check if we're done
				const auto group_linear_idx = group_idx++;
				if (group_linear_idx >= group_count) {
					break;
				}
				
				// setup group
				const uint3 group_id {
					group_linear_idx % group_dim.x,
					(group_linear_idx / group_dim.x) % group_dim.y,
					group_linear_idx / (group_dim.x * group_dim.y)
				};
				exec_ctx.ids.instance_group_idx = group_id;
				
				// reset fibers
				for (uint32_t i = 0; i < local_size; ++i) {
					items[i].reset();
				}
#if defined(FLOOR_DEBUG)
				exec_ctx.unfinished_items = local_size;
#endif
				
				// run fibers/work-items for this group
				run_exec(main_ctx, items[0]);
				
				// exit due to excessive local memory allocation?
				if (local_memory_exceeded) {
					log_error("exceeded local memory allocation in function \"$\" - requested $ bytes, limit is $ bytes",
							  function_name, local_memory_alloc_offset, floor_local_memory_max_size);
					break;
				}
				
				// check if any items are still unfinished (in a valid program, all must be finished at this point)
				// NOTE: this won't detect all barrier misuses, doing so would require *a lot* of work
#if defined(FLOOR_DEBUG)
				if (exec_ctx.unfinished_items > 0) {
					log_error("barrier misuse detected in function \"$\" - $ unfinished items in group $",
							  function_name, exec_ctx.unfinished_items, group_id);
					break;
				}
#endif
			}
		});
	}
	// wait for worker threads to finish
	for (auto& item : worker_threads) {
		item->join();
	}
#if defined(FLOOR_HOST_FUNCTION_ENABLE_TIMING)
	log_debug("function time: $ms", double(floor_timer::stop<std::chrono::microseconds>(time_start)) / 1000.0);
#endif
}

extern "C" void run_host_group_item(const uint32_t local_linear_idx) FLOOR_HOST_COMPUTE_CC {
	auto& exec_ctx = host_exec_context;
	
	// set local + global id
	const uint3 local_id {
		local_linear_idx % exec_ctx.ids.instance_local_work_size.x,
		(local_linear_idx / exec_ctx.ids.instance_local_work_size.x) % exec_ctx.ids.instance_local_work_size.y,
		local_linear_idx / (exec_ctx.ids.instance_local_work_size.x * exec_ctx.ids.instance_local_work_size.y)
	};
	exec_ctx.ids.instance_local_idx = local_id;
	exec_ctx.ids.instance_local_linear_idx = local_linear_idx;
	
	const uint3 global_id {
		exec_ctx.ids.instance_group_idx.x * exec_ctx.ids.instance_local_work_size.x + local_id.x,
		exec_ctx.ids.instance_group_idx.y * exec_ctx.ids.instance_local_work_size.y + local_id.y,
		exec_ctx.ids.instance_group_idx.z * exec_ctx.ids.instance_local_work_size.z + local_id.z
	};
	exec_ctx.ids.instance_global_idx = global_id;
	
	// execute work-item / function
	(*exec_ctx.func)();
	
#if defined(FLOOR_DEBUG)
	// for barrier misuse checking
	--exec_ctx.unfinished_items;
#endif
}

void host_function::execute_device(const host_function_entry& func_entry,
								 const uint32_t& cpu_count,
								 const uint3& group_dim,
								 const uint3& local_dim,
								 const uint32_t& work_dim,
								 const std::vector<const void*>& vptr_args) const {
	// #work-groups
	const auto group_count = group_dim.x * group_dim.y * group_dim.z;
	// #work-items per group
	const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
	// group ticketing system, each worker thread will grab a new group id, once it's done with one group
	std::atomic<uint32_t> group_idx { 0 };
	
	// start worker threads
#if defined(FLOOR_HOST_FUNCTION_ENABLE_TIMING)
	const auto time_start = floor_timer::start();
#endif
	std::atomic<bool> success { true };
	std::vector<std::unique_ptr<std::thread>> worker_threads(cpu_count);
	for (uint32_t cpu_idx = 0; cpu_idx < cpu_count; ++cpu_idx) {
		worker_threads[cpu_idx] = std::make_unique<std::thread>([this, &success, cpu_idx, &func_entry, &vptr_args,
													   &group_idx, group_count, group_dim,
													   local_size, local_dim, work_dim] {
			// set CPU affinity for this thread to a particular CPU to prevent this thread from being constantly moved/scheduled
			// on different CPUs (starting at index 1, with 0 representing no affinity)
			set_thread_affinity(cpu_idx + 1);
			
			// retrieve the instance for this CPU + reset/init it
			auto instance = func_entry.program->get_instance(cpu_idx);
			if (!instance) {
				log_error("no instance for CPU #$ (for $)", cpu_idx, function_name);
				success = false;
				return;
			}
			instance->reset(local_dim * group_dim, local_dim, group_dim, work_dim);
			
			auto& exec_ctx = device_exec_context;
			exec_ctx.ids = &instance->ids;
			exec_ctx.linear_local_work_size = local_size;
			auto& ids = instance->ids;
			
			// get and set the function for this instance
			const auto& func_info = *func_entry.info;
			const auto func_iter = instance->functions.find(func_info.name);
			if (func_iter == instance->functions.end()) {
				log_error("failed to find function \"$\" for CPU #$", function_name, cpu_idx);
				success = false;
				return;
			}
			exec_ctx.func = make_callable_host_function(func_iter->second, vptr_args);
			if (!exec_ctx.func) {
				log_error("failed to create function \"$\" for CPU #$", function_name, cpu_idx);
				success = false;
				return;
			}
			
			// init contexts (aka fibers)
			floor_fiber_context main_ctx;
			main_ctx.init(nullptr, 0, nullptr, ~0u, nullptr, nullptr);
			auto items = std::make_unique<floor_fiber_context[]>(local_size);
			exec_ctx.item_contexts = items.get();
			
			// init fibers
			for (uint32_t i = 0; i < local_size; ++i) {
				items[i].init(&floor_stack_memory_data.get()[(i + local_size * cpu_idx) * floor_fiber_context::min_stack_size],
							  floor_fiber_context::min_stack_size,
							  run_host_device_group_item, i,
							  // continue with next on return, or return to main ctx when the last item returns
							  (i + 1 < local_size ? &items[i + 1] : &main_ctx),
							  &main_ctx);
			}
			
			for (; success;) {
				// assign a new group to this thread/CPU and check if we're done
				const auto group_linear_idx = group_idx++;
				if (group_linear_idx >= group_count) {
					break;
				}
				
				// setup group
				const uint3 group_id {
					group_linear_idx % group_dim.x,
					(group_linear_idx / group_dim.x) % group_dim.y,
					group_linear_idx / (group_dim.x * group_dim.y)
				};
				ids.instance_group_idx = group_id;
				
				// reset fibers
				for (uint32_t i = 0; i < local_size; ++i) {
					items[i].reset();
				}
#if defined(FLOOR_DEBUG)
				exec_ctx.unfinished_items = local_size;
#endif
				
				// run fibers/work-items for this group
				run_exec(main_ctx, items[0]);
				
				// check if any items are still unfinished (in a valid program, all must be finished at this point)
				// NOTE: this won't detect all barrier misuses, doing so would require *a lot* of work
#if defined(FLOOR_DEBUG)
				if (exec_ctx.unfinished_items > 0) {
					log_error("barrier misuse detected in function \"$\" - $ unfinished items in group $",
							  function_name, exec_ctx.unfinished_items, group_id);
					break;
				}
#endif
			}
		});
	}
	// wait for worker threads to finish
	for (auto& item : worker_threads) {
		item->join();
	}
}

std::unique_ptr<argument_buffer> host_function::create_argument_buffer_internal(const device_queue& cqueue,
																		 const function_entry& kern_entry,
																		 const toolchain::arg_info& arg floor_unused,
																		 const uint32_t& user_arg_index,
																		 const uint32_t& ll_arg_index,
																		 const MEMORY_FLAG& add_mem_flags) const {
	const auto& dev = cqueue.get_device();
	const auto& host_entry = (const host_function_entry&)kern_entry;
	
	// check if info exists
	const auto& arg_info = host_entry.info->args[ll_arg_index].argument_buffer_info;
	if (!arg_info) {
		log_error("no argument buffer info for arg at index #$", user_arg_index);
		return {};
	}
	
	const auto arg_buffer_size = host_entry.info->args[ll_arg_index].size;
	if (arg_buffer_size == 0) {
		log_error("computed argument buffer size is 0");
		return {};
	}
	
	// create the argument buffer
	auto buf = dev.context->create_buffer(cqueue, arg_buffer_size, MEMORY_FLAG::READ | MEMORY_FLAG::HOST_WRITE | add_mem_flags);
	buf->set_debug_label(kern_entry.info->name + "_arg_buffer");
	return std::make_unique<host_argument_buffer>(*this, buf, *arg_info);
}

} // namespace fl

extern "C" void run_host_device_group_item(const uint32_t local_linear_idx) FLOOR_HOST_COMPUTE_CC {
	// set ids for work-item
	auto& exec_ctx = fl::device_exec_context;
	{
		auto& ids = *exec_ctx.ids;
		ids.instance_local_idx = {
			local_linear_idx % ids.instance_local_work_size.x,
			(local_linear_idx / ids.instance_local_work_size.x) % ids.instance_local_work_size.y,
			local_linear_idx / (ids.instance_local_work_size.x * ids.instance_local_work_size.y)
		};
		ids.instance_local_linear_idx = local_linear_idx;
		ids.instance_global_idx = {
			ids.instance_group_idx.x * ids.instance_local_work_size.x + ids.instance_local_idx.x,
			ids.instance_group_idx.y * ids.instance_local_work_size.y + ids.instance_local_idx.y,
			ids.instance_group_idx.z * ids.instance_local_work_size.z + ids.instance_local_idx.z
		};
	}
	
	// execute work-item / function
	exec_ctx.func();
	
#if defined(FLOOR_DEBUG)
	// for barrier misuse checking
	--exec_ctx.unfinished_items;
#endif
}

// -> function lib function implementations
#include <floor/device/backend/host.hpp>

// barrier handling (all the same)
// NOTE: the same barrier _must_ be encountered at the same point for all work-items
void global_barrier() FLOOR_HOST_COMPUTE_CC {
	// save indices, switch to next fiber and restore indices again
	auto& exec_ctx = fl::host_exec_context;
	const auto saved_global_id = exec_ctx.ids.instance_global_idx;
	const auto saved_local_id = exec_ctx.ids.instance_local_idx;
	const auto saved_item_local_linear_idx = exec_ctx.ids.instance_local_linear_idx;
	
	fl::floor_fiber_context* this_ctx = &exec_ctx.item_contexts[saved_item_local_linear_idx];
	fl::floor_fiber_context* next_ctx = &exec_ctx.item_contexts[(saved_item_local_linear_idx + 1u) % exec_ctx.linear_local_work_size];
	this_ctx->swap_context(next_ctx);
	
	// restore
	exec_ctx.ids.instance_local_linear_idx = saved_item_local_linear_idx;
	exec_ctx.ids.instance_local_idx = saved_local_id;
	exec_ctx.ids.instance_global_idx = saved_global_id;
}
void local_barrier() FLOOR_HOST_COMPUTE_CC {
	global_barrier();
}
void image_barrier() FLOOR_HOST_COMPUTE_CC {
	global_barrier();
}
void barrier() FLOOR_HOST_COMPUTE_CC {
	global_barrier();
}

void floor_host_compute_device_barrier() FLOOR_HOST_COMPUTE_CC {
	auto& exec_ctx = fl::device_exec_context;
	auto& ids = *exec_ctx.ids;
	
	// save indices, switch to next fiber and restore indices again
	const auto saved_global_id = ids.instance_global_idx;
	const auto saved_local_id = ids.instance_local_idx;
	const auto saved_item_local_linear_idx = ids.instance_local_linear_idx;
	
	fl::floor_fiber_context* this_ctx = &exec_ctx.item_contexts[ids.instance_local_linear_idx];
	fl::floor_fiber_context* next_ctx = &exec_ctx.item_contexts[(ids.instance_local_linear_idx + 1u) % exec_ctx.linear_local_work_size];
	this_ctx->swap_context(next_ctx);
	
	ids.instance_local_linear_idx = saved_item_local_linear_idx;
	ids.instance_local_idx = saved_local_id;
	ids.instance_global_idx = saved_global_id;
}

uint32_t* floor_host_compute_device_printf_buffer() FLOOR_HOST_COMPUTE_CC {
	return fl::floor_host_device_printf_buffer.get_as<uint32_t>();
}

// memory fence handling (all the same)
// NOTE: compared to a barrier, a memory fence does not have to be encountered by all work-items (no context/fiber switching is necessary)
void global_mem_fence() {
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(atomic-implicit-seq-cst) // we still explicitly want this
	__sync_synchronize();
FLOOR_POP_WARNINGS()
}
void global_read_mem_fence() {
	global_mem_fence();
}
void global_write_mem_fence() {
	global_mem_fence();
}
void local_mem_fence() {
	global_mem_fence();
}
void local_read_mem_fence() {
	global_mem_fence();
}
void local_write_mem_fence() {
	global_mem_fence();
}
void image_mem_fence() {
	global_mem_fence();
}
void image_read_mem_fence() {
	global_mem_fence();
}
void image_write_mem_fence() {
	global_mem_fence();
}

// (host) local memory management
// NOTE: this is called when allocating storage for local buffers
uint8_t* __attribute__((aligned(1024))) floor_requisition_local_memory(const size_t size, uint32_t& offset) noexcept {
	// check if this allocation exceeds the max size
	// note: using the unaligned size, since the padding isn't actually used
	if ((fl::local_memory_alloc_offset + size) > fl::floor_local_memory_max_size) {
		// if so, signal the main thread that things are bad and switch to it
		fl::local_memory_exceeded = true;
		auto& exec_ctx = fl::host_exec_context;
		exec_ctx.item_contexts[exec_ctx.ids.instance_local_linear_idx].exit_to_main();
	}
	
	// align to 1024-bit / 128 bytes
	const auto per_thread_alloc_size = (size % 128 == 0 ? size : (((size / 128) + 1) * 128));
	// set the offset to this allocation
	offset = fl::local_memory_alloc_offset;
	// adjust allocation offset for the next allocation
	fl::local_memory_alloc_offset += per_thread_alloc_size;
	
	return fl::floor_local_memory_data.get();
}

uint32_t floor_host_compute_thread_local_memory_offset_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.thread_local_memory_offset;
}
fl::uint3 floor_host_compute_global_idx_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.ids.instance_global_idx;
}
fl::uint3 floor_host_compute_local_idx_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.ids.instance_local_idx;
}
fl::uint3 floor_host_compute_group_idx_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.ids.instance_group_idx;
}
uint32_t floor_host_compute_work_dim_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.ids.instance_work_dim;
}
fl::uint3 floor_host_compute_global_work_size_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.ids.instance_global_work_size;
}
fl::uint3 floor_host_compute_local_work_size_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.ids.instance_local_work_size;
}
fl::uint3 floor_host_compute_group_size_get() FLOOR_HOST_COMPUTE_CC {
	return fl::host_exec_context.ids.instance_group_size;
}

#endif
