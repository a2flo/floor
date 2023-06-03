/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include <floor/compute/host/host_kernel.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/host/host_buffer.hpp>
#include <floor/compute/host/host_image.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/compute/host/elf_binary.hpp>
#include <floor/compute/host/host_argument_buffer.hpp>
#include <floor/compute/device/host_limits.hpp>
#include <floor/compute/device/host_id.hpp>

//#define FLOOR_HOST_KERNEL_ENABLE_TIMING 1
#if defined(FLOOR_HOST_KERNEL_ENABLE_TIMING)
#include <floor/core/timer.hpp>
#endif

#if !defined(_WIN32)
// sanity check (mostly necessary on os x where some fool had the idea to make the size of ucontext_t define dependent)
static_assert(sizeof(ucontext_t) > 64, "ucontext_t should not be this small, something is wrong!");
#endif

#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp> // cleanup

// ignore warnings about deprecated functions
FLOOR_IGNORE_WARNING(deprecated-declarations)

//
static const function<void()>* cur_kernel_function { nullptr };
extern "C" void run_mt_group_item(const uint32_t local_linear_idx);
extern "C" void run_host_device_group_item(const uint32_t local_linear_idx);

// NOTE: due to rather fragile stack handling (rsp), this is completely done in asm, so that the compiler can't do anything wrong
#if defined(__x86_64__) && !defined(__WINDOWS__)
#if defined(__AVX512F__) && defined(__AVX512DQ__)
asm("floor_get_context_sysv_x86_64:"
	// store all registers in fiber_context*
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
	// restore all registers from fiber_context*
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
	// store all registers in fiber_context*
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
	// restore all registers from fiber_context*
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
	// store all registers in fiber_context*
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
	// restore all registers from fiber_context*
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
	// retrieve fiber_context*
	"movq 0x8(%rsp), %rax;"
	// fiber_context->init_func
	"movq 0x50(%rax), %rcx;"
	// fiber_context->init_arg
	"movl 0x68(%rax), %edi;"
	// call init_func(init_arg)
	"callq *%rcx;"
	// context is done, -> exit to set exit context, or exit(0)
	// retrieve fiber_context* again
	"movq 0x8(%rsp), %rax;"
	// exit fiber_context*
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
	// store all registers in fiber_context*
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
	// restore all registers from fiber_context*
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
	// retrieve fiber_context*
	"ldr x9, [sp, #8]\n"
	// fiber_context->init_func
	"ldr x10, [x9, #184]\n"
	// fiber_context->init_arg
	"ldr w0, [x9, #208]\n"
	// call init_func(init_arg)
	"blr x10\n"
	// context is done, -> exit to set exit context, or exit(0)
	// retrieve fiber_context* again
	"ldr x9, [sp, #8]\n"
	// exit fiber_context*
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

static constexpr const size_t fiber_context_alignment {
#if defined(__x86_64__)
	128u
#elif defined(__aarch64__)
	256u
#else
#error "unhandled arch"
#endif
};

struct alignas(fiber_context_alignment) fiber_context {
	typedef void (*init_func_type)(const uint32_t);

#if (defined(__x86_64__) || defined(__aarch64__)) && !defined(__WINDOWS__)
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
			  fiber_context* exit_ctx_, fiber_context* main_ctx_) noexcept {
		init_common(stack_ptr_, stack_size_, init_func_, init_arg_, exit_ctx_, main_ctx_);
		
		if(size_t(this) % fiber_context_alignment != 0u) {
			log_error("fiber_context must be $-byte aligned!", fiber_context_alignment); logger::flush();
			return;
		}

		if(stack_ptr != nullptr) {
			// check stack pointer validity (must be 16-byte aligned)
			if(size_t(stack_ptr) % 16u != 0u) {
				log_error("stack must be 16-byte aligned!"); logger::flush();
				return;
			}
			
			// set the first 64-bit value on the stack to this context and the second value to a canary value
			// note that this is only done once, not on every reset, because:
			//  a) it isn't necessary (if everything goes well)
			//  b) if the user kernel code does overwrite this (stack overflow), this will certainly crash (as it should!)
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

	void get_context() noexcept {
		floor_get_context(this);
	}

// ignore the "missing noreturn" warning here, because actually setting [[noreturn]] leads to unwanted codegen
// (ud2 insertion at a point we don't want this to happen -> we already have a ud2 trap in enter_context)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(missing-noreturn)
	void set_context() noexcept {
		floor_set_context(this);
		floor_unreachable();
	}
FLOOR_POP_WARNINGS()

	void swap_context(fiber_context* next_ctx) noexcept {
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
		if(!swapped) {
			swapped = true;
#if defined(__clang_analyzer__)
			(void)swapped; // -> mark as used (will be read again)
#endif
			next_ctx->set_context();
		}
	}

#elif defined(__WINDOWS__)
	static constexpr const size_t min_stack_size { aligned_ptr<int>::page_size };

	// the windows fiber context
	void* ctx { nullptr };

	static void fiber_run(void* data) noexcept {
		auto this_ctx = (fiber_context*)data;
		(*this_ctx->init_func)(this_ctx->init_arg);
		if(this_ctx->exit_ctx != nullptr) {
			SwitchToFiber(this_ctx->exit_ctx->ctx);
		}
	}

	void init(void* stack_ptr_, const size_t& stack_size_,
			  init_func_type init_func_, const uint32_t& init_arg_,
			  fiber_context* exit_ctx_, fiber_context* main_ctx_) noexcept {
		init_common(stack_ptr_, stack_size_, init_func_, init_arg_, exit_ctx_, main_ctx_);

		if(stack_ptr == nullptr) {
			// this is the main thread
			// -> need to convert to fiber before creating/using all other fibers
			ctx = ConvertThreadToFiber(nullptr);
			if(ctx == nullptr) {
				log_error("failed to convert thread to fiber: $", GetLastError());
				logger::flush();
			}
		}
	}

	~fiber_context() {
		if(ctx == nullptr) return;
		if(stack_ptr == nullptr) {
			// main thread, convert fiber back to thread
			if(!ConvertFiberToThread()) {
				log_error("failed to convert fiber to thread: $", GetLastError());
				logger::flush();
			}
		}
		else {
			// worker fiber
			DeleteFiber(ctx);
		}
		ctx = nullptr;
	}

	void reset() noexcept {
		// don't do anything in the main fiber/thread
		if(stack_ptr == nullptr) return;

		// kill the old fiber if there was one (can't simply reset a windows fiber)
		if(ctx != nullptr) {
			DeleteFiber(ctx);
			ctx = nullptr;
		}

		// this is a worker fiber/context
		// -> create a new windows fiber context for this
		ctx = CreateFiberEx(stack_size, stack_size, 0, fiber_run, this);
		if(ctx == nullptr) {
			log_error("failed to create worker fiber context: $", GetLastError());
			logger::flush();
		}
	}

	void get_context() noexcept {
		// nop
	}

	void set_context() noexcept {
		SwitchToFiber(ctx);
	}

	void swap_context(fiber_context* next_ctx) noexcept {
		SwitchToFiber(next_ctx->ctx);
	}
#else
	// fallback to posix ucontext_t/etc
	static constexpr const size_t min_stack_size { 32768 };
	
	ucontext_t ctx;
	
	void init(void* stack_ptr_, const size_t& stack_size_,
			  init_func_type init_func_, const uint32_t& init_arg_,
			  fiber_context* exit_ctx_, fiber_context* main_ctx_) noexcept {
		init_common(stack_ptr_, stack_size_, init_func_, init_arg_, exit_ctx_, main_ctx_);
		
		memset(&ctx, 0, sizeof(ucontext_t));
		getcontext(&ctx);
		
		// unknown context vars -> query external
		if(stack_ptr == nullptr) {
			stack_ptr = ctx.uc_stack.ss_sp;
			stack_size = ctx.uc_stack.ss_size;
		}
	}
	
	void reset() noexcept {
		if(exit_ctx != nullptr) {
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
	
	void swap_context(fiber_context* next_ctx) noexcept {
		swapcontext(&ctx, &next_ctx->ctx);
	}
#endif

	void init_common(void* stack_ptr_, const size_t& stack_size_,
					 init_func_type init_func_, const uint32_t& init_arg_,
					 fiber_context* exit_ctx_, fiber_context* main_ctx_) noexcept {
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
	fiber_context* exit_ctx { nullptr };
	fiber_context* main_ctx { nullptr };
	uint32_t init_arg { 0 };
	
};
static_assert(sizeof(fiber_context) <= fiber_context_alignment, "fiber_context alignment is not large enough");

// make sure member variables are at the right offsets when using the sysv abi fiber approach
#if defined(__x86_64__) && !defined(__WINDOWS__)
static_assert(offsetof(fiber_context, init_func) == 0x50);
static_assert(offsetof(fiber_context, exit_ctx) == 0x58);
static_assert(offsetof(fiber_context, main_ctx) == 0x60);
static_assert(offsetof(fiber_context, init_arg) == 0x68);
#elif defined(__aarch64__) && !defined(__WINDOWS__)
static_assert(offsetof(fiber_context, init_func) == 184);
static_assert(offsetof(fiber_context, exit_ctx) == 192);
static_assert(offsetof(fiber_context, main_ctx) == 200);
static_assert(offsetof(fiber_context, init_arg) == 208);
#endif

// id handling vars
uint32_t floor_work_dim { 1u };
uint3 floor_global_work_size;
static uint32_t floor_linear_global_work_size;
uint3 floor_local_work_size;
static uint32_t floor_linear_local_work_size;
uint3 floor_group_size;
static uint32_t floor_linear_group_size;
#if !defined(__WINDOWS__) // TLS dllexport vars are handled differently on Windows
thread_local uint3 floor_global_idx;
thread_local uint3 floor_local_idx;
thread_local uint3 floor_group_idx;
#endif
// will be initialized to "max h/w threads", note that this is stored in a global var,
// so that core::get_hw_thread_count() doesn't have to called over and over again, and
// so this is actually a consistent value (bad things will happen if it isn't)
static uint32_t floor_max_thread_count { 0 };

// barrier handling vars
// -> mt-item
#if defined(FLOOR_HOST_COMPUTE_MT_ITEM)
static atomic<uint32_t> barrier_counter { 0 };
static atomic<uint32_t> barrier_gen { 0 };
static uint32_t barrier_users { 0 };
#endif
// -> mt-group
#if defined(FLOOR_HOST_COMPUTE_MT_GROUP)
static thread_local uint32_t item_local_linear_idx { 0 };
static thread_local fiber_context* item_contexts { nullptr };
#endif
// -> sanity check for correct barrier use
#if defined(FLOOR_DEBUG)
static thread_local uint32_t unfinished_items { 0 };
#endif

// local memory management
static constexpr const size_t floor_local_memory_max_size { host_limits::local_memory_size };
static uint32_t local_memory_alloc_offset { 0 };
static bool local_memory_exceeded { false };
static aligned_ptr<uint8_t> floor_local_memory_data;

// extern in host_kernel.hpp and common.hpp
#if !defined(__WINDOWS__) // TLS dllexport vars are handled differently on Windows
thread_local uint32_t floor_thread_idx { 0 };
thread_local uint32_t floor_thread_local_memory_offset { 0 };
#endif

// stack memory management
// 4k - 8k stack should be enough, considering this runs on gpus (min 32k with ucontext)
// TODO: stack protection?
static constexpr const size_t item_stack_size { fiber_context::min_stack_size };
static aligned_ptr<uint8_t> floor_stack_memory_data;

static void floor_alloc_host_local_memory() {
	if (!floor_local_memory_data) {
		floor_local_memory_data = make_aligned_ptr<uint8_t>(floor_max_thread_count * floor_local_memory_max_size);
	}
}

static void floor_alloc_host_stack_memory() {
#if defined(FLOOR_HOST_COMPUTE_MT_GROUP) || defined(FLOOR_COMPUTE_HOST_DEVICE)
	if (!floor_stack_memory_data) {
		floor_stack_memory_data = make_aligned_ptr<uint8_t>(floor_max_thread_count * item_stack_size * host_limits::max_total_local_size);
	}
#endif
}

// host-compute device execution context
struct device_exec_context_t {
	elf_binary::instance_ids_t* ids { nullptr };
	function<void()> kernel_func;
};
static thread_local device_exec_context_t device_exec_context;

//
host_kernel::host_kernel(const void* kernel_, const string& func_name_, compute_kernel::kernel_entry&& entry_) :
kernel((const kernel_func_type)const_cast<void*>(kernel_)), func_name(func_name_), entry(std::move(entry_)) {
}

host_kernel::host_kernel(kernel_map_type&& kernels_) : kernels(std::move(kernels_)) {
}

const host_kernel::kernel_entry* host_kernel::get_kernel_entry(const compute_device& dev) const {
	if (kernel != nullptr) {
		return &entry; // can't really check if the device is correct here
	} else {
		const auto ret = kernels.get((const host_device&)dev);
		return !ret.first ? nullptr : &ret.second->second;
	}
}

typename host_kernel::kernel_map_type::const_iterator host_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const host_device&)cqueue.get_device());
}

// needed to cast variadic kernel function type to a function type with the correct amount of parameters
template <typename... Args> using kernel_func_type_t = void (*)(Args...);

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-function-type-strict)

static function<void()> make_callable_kernel_function(const host_kernel::kernel_func_type kernel_ptr, const vector<const void*>& vptr_args) {
	function<void()> kernel_func;
	switch (vptr_args.size()) {
		case 0: kernel_func = [kernel_ptr]() { (*(kernel_func_type_t<>)kernel_ptr)(); }; break;

#define EXPAND_1(var, offset) var[0 + offset]
#define EXPAND_2(var, offset) var[0 + offset], var[1 + offset]
#define EXPAND_3(var, offset) var[0 + offset], var[1 + offset], var[2 + offset]
#define EXPAND_4(var, offset) var[0 + offset], var[1 + offset], var[2 + offset], var[3 + offset]
#define EXPAND_5(var, offset) var[0 + offset], var[1 + offset], var[2 + offset], var[3 + offset], var[4 + offset]
#define EXPAND_6(var, offset) var[0 + offset], var[1 + offset], var[2 + offset], var[3 + offset], var[4 + offset], var[5 + offset]
#define EXPAND_7(var, offset) var[0 + offset], var[1 + offset], var[2 + offset], var[3 + offset], var[4 + offset], var[5 + offset], var[6 + offset]
#define EXPAND_8(var, offset) var[0 + offset], var[1 + offset], var[2 + offset], var[3 + offset], var[4 + offset], var[5 + offset], var[6 + offset], var[7 + offset]
#define EXPAND_PTR_1() const void*
#define EXPAND_PTR_2() const void*, EXPAND_PTR_1()
#define EXPAND_PTR_3() const void*, EXPAND_PTR_2()
#define EXPAND_PTR_4() const void*, EXPAND_PTR_3()
#define EXPAND_PTR_5() const void*, EXPAND_PTR_4()
#define EXPAND_PTR_6() const void*, EXPAND_PTR_5()
#define EXPAND_PTR_7() const void*, EXPAND_PTR_6()
#define EXPAND_PTR_8() const void*, EXPAND_PTR_7()
		
		case 1: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_1()>)kernel_ptr)(EXPAND_1(vptr_args, 0)); }; break;
		case 2: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_2()>)kernel_ptr)(EXPAND_2(vptr_args, 0)); }; break;
		case 3: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_3()>)kernel_ptr)(EXPAND_3(vptr_args, 0)); }; break;
		case 4: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_4()>)kernel_ptr)(EXPAND_4(vptr_args, 0)); }; break;
		case 5: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_5()>)kernel_ptr)(EXPAND_5(vptr_args, 0)); }; break;
		case 6: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_6()>)kernel_ptr)(EXPAND_6(vptr_args, 0)); }; break;
		case 7: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_7()>)kernel_ptr)(EXPAND_7(vptr_args, 0)); }; break;
		case 8: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8()>)kernel_ptr)(EXPAND_8(vptr_args, 0)); }; break;
		
		case 9: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_1()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_1(vptr_args, 8)); }; break;
		case 10: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_2()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_2(vptr_args, 8)); }; break;
		case 11: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_3()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_3(vptr_args, 8)); }; break;
		case 12: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_4()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_4(vptr_args, 8)); }; break;
		case 13: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_5()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_5(vptr_args, 8)); }; break;
		case 14: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_6()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_6(vptr_args, 8)); }; break;
		case 15: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_7()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_7(vptr_args, 8)); }; break;
		case 16: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8)); }; break;
		
		case 17: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_1()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_1(vptr_args, 16)); }; break;
		case 18: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_2()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_2(vptr_args, 16)); }; break;
		case 19: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_3()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_3(vptr_args, 16)); }; break;
		case 20: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_4()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_4(vptr_args, 16)); }; break;
		case 21: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_5()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_5(vptr_args, 16)); }; break;
		case 22: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_6()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_6(vptr_args, 16)); }; break;
		case 23: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_7()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_7(vptr_args, 16)); }; break;
		case 24: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16)); }; break;
		
		case 25: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_1()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_1(vptr_args, 24)); }; break;
		case 26: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_2()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_2(vptr_args, 24)); }; break;
		case 27: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_3()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_3(vptr_args, 24)); }; break;
		case 28: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_4()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_4(vptr_args, 24)); }; break;
		case 29: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_5()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_5(vptr_args, 24)); }; break;
		case 30: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_6()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_6(vptr_args, 24)); }; break;
		case 31: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_7()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_7(vptr_args, 24)); }; break;
		case 32: kernel_func = [kernel_ptr, &vptr_args]() { (*(kernel_func_type_t<EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8(), EXPAND_PTR_8()>)kernel_ptr)(EXPAND_8(vptr_args, 0), EXPAND_8(vptr_args, 8), EXPAND_8(vptr_args, 16), EXPAND_8(vptr_args, 24)); }; break;
		
#undef EXPAND_1
#undef EXPAND_2
#undef EXPAND_3
#undef EXPAND_4
#undef EXPAND_5
#undef EXPAND_6
#undef EXPAND_7
#undef EXPAND_8
#undef EXPAND_PTR_1
#undef EXPAND_PTR_2
#undef EXPAND_PTR_3
#undef EXPAND_PTR_4
#undef EXPAND_PTR_5
#undef EXPAND_PTR_6
#undef EXPAND_PTR_7
#undef EXPAND_PTR_8

FLOOR_POP_WARNINGS()
		
		default:
			log_error("too many kernel parameters specified (only up to 32 parameters are supported)");
			return {};
	}
	return kernel_func;
}

void host_kernel::execute(const compute_queue& cqueue,
						  const bool& is_cooperative,
						  const bool& wait_until_completion floor_unused /* will always wait anyways */,
						  const uint32_t& work_dim,
						  const uint3& global_work_size,
						  const uint3& local_work_size,
						  const vector<compute_kernel_arg>& args,
						  const vector<const compute_fence*>& wait_fences floor_unused,
						  const vector<compute_fence*>& signal_fences floor_unused,
						  const char* debug_label floor_unused,
						  kernel_completion_handler_f&& completion_handler) const {
	// TODO: implement waiting for "wait_fences" and signaling "signal_fences" (for now, this is blocking anyways)
	
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for Host-Compute");
		return;
	}
	
	// extract/handle kernel arguments
	vector<const void*> vptr_args;
	vptr_args.reserve(args.size());
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			vptr_args.emplace_back(((const host_buffer*)(*buf_ptr))->get_host_buffer_ptr());
		} else if (auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
			log_error("array of buffers is not yet supported for Host-Compute");
			return;
		} else if (auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
			log_error("array of buffers is not yet supported for Host-Compute");
			return;
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			vptr_args.emplace_back(((const host_image*)(*img_ptr))->get_host_image_program_info());
		} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			log_error("array of images is not supported for Host-Compute");
			return;
		} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			log_error("array of images is not supported for Host-Compute");
			return;
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
		floor_max_thread_count = core::get_hw_thread_count();
	}
	
	// device cpu count must be <= h/w thread count, b/c local memory is only allocated for such many threads
	const auto cpu_count = cqueue.get_device().units;
	if (cpu_count > floor_max_thread_count) {
		log_error("device cpu count exceeds h/w count");
		return;
	}
	
	// only a single kernel can be active/executed at one time
	// TODO: can this be "fixed" by host-compute device execution?
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
		// NOTE: when using a kernel that has been compiled into the program (not host-compute device), "kernel" will be non-nullptr
		if (kernel == nullptr) {
			// -> device execution
			const auto kernel_iter = get_kernel(cqueue);
			if (kernel_iter == kernels.cend() || kernel_iter->second.program == nullptr) {
				log_error("no program for this compute queue/device exists!");
				return;
			}
			execute_device(kernel_iter->second, cpu_count, group_dim, local_dim, work_dim, vptr_args);
		} else {
			// -> host execution
			auto kernel_func = make_callable_kernel_function(kernel, vptr_args);
			if (!kernel_func) {
				return;
			}
			
			// setup/reset id and other global variables
			floor_work_dim = work_dim;
			floor_global_work_size = global_work_size;
			floor_local_work_size = local_dim;
			floor_group_size = group_size;
			
			floor_linear_global_work_size = floor_global_work_size.x * floor_global_work_size.y * floor_global_work_size.z;
			floor_linear_local_work_size = local_dim.x * local_dim.y * local_dim.z;
			floor_linear_group_size = floor_group_size.x * floor_group_size.y * floor_group_size.z;
			
			// setup local memory management
			local_memory_alloc_offset = 0;
			local_memory_exceeded = false;
			// alloc local (for all threads) if it hasn't been allocated yet
			floor_alloc_host_local_memory();
			
			cur_kernel_function = &kernel_func;
			execute_host(cpu_count, group_dim, local_dim);
			cur_kernel_function = nullptr;
		}
		
		// this is always executed synchronously, so we can just directly call the completion handler
		if (completion_handler) {
			completion_handler();
		}
	}
}

void host_kernel::execute_host(const uint32_t& cpu_count, const uint3& group_dim, const uint3& local_dim) const {
#if defined(FLOOR_HOST_COMPUTE_ST) // single-threaded
	// it's usually best to go from largest to smallest loop count (usually: X > Y > Z)
	uint3& global_idx = floor_global_idx;
	uint3& local_idx = floor_local_idx;
	uint3& group_idx = floor_group_idx;
	for(uint32_t group_x = 0; group_x < group_dim.x; ++group_x) {
		for(uint32_t group_y = 0; group_y < group_dim.y; ++group_y) {
			for(uint32_t group_z = 0; group_z < group_dim.z; ++group_z) {
				group_idx.set(group_x, group_y, group_z);
				global_idx.set(group_x * local_dim.x,
							   group_y * local_dim.y,
							   group_z * local_dim.z);
				local_idx.set(0, 0, 0);
				
				// this time, go from potentially smallest to largest (it's better to execute this in X, Y, Z order)
				for(; local_idx.z < local_dim.z; ++local_idx.z, ++global_idx.z) {
					local_idx.y = 0;
					global_idx.y = group_y * local_dim.y;
					for(; local_idx.y < local_dim.y; ++local_idx.y, ++global_idx.y) {
						local_idx.x = 0;
						global_idx.x = group_x * local_dim.x;
						for(; local_idx.x < local_dim.x; ++local_idx.x, ++global_idx.x) {
							kernel_func();
						}
					}
				}
			}
		}
	}
#elif defined(FLOOR_HOST_COMPUTE_MT_ITEM)
	// #work-items per group
	const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
	// amount of work-items (in a group) in-flight (0 when group is done, then reset for every group)
	atomic<uint32_t> items_in_flight { 0 };
	// for group syncing purposes, waited on until all work-items in a group are done
	atomic<uint32_t> group_id { ~0u };
	
	// init barrier vars
	barrier_counter = local_size;
	barrier_gen = 0;
	barrier_users = local_size;
	
	// start worker threads
	vector<unique_ptr<thread>> worker_threads(local_size);
	for(uint32_t local_linear_idx = 0; local_linear_idx < local_size; ++local_linear_idx) {
		worker_threads[local_linear_idx] = make_unique<thread>([&items_in_flight, &group_id,
																local_linear_idx, local_size,
																local_dim, group_dim,
																this, &kernel_func] {
			// local id is fixed for all execution
			const uint3 local_id {
				local_linear_idx % local_dim.x,
				(local_linear_idx / local_dim.x) % local_dim.y,
				local_linear_idx / (local_dim.x * local_dim.y)
			};
			floor_local_idx = local_id;
			
#if defined(FLOOR_DEBUG)
			// set thread name for debugging purposes, shortened as far as possible
			// note that thread name max size is 15 (-2 commas -> 13)
			if((const_math::int_width(local_dim.x - 1) +
				const_math::int_width(local_dim.y - 1) +
				const_math::int_width(local_dim.z - 1)) <= 13) {
				core::set_current_thread_name(to_string(local_id.x) + "," +
											  to_string(local_id.y) + "," +
											  to_string(local_id.z));
			}
			else core::set_current_thread_name("#" + to_string(local_linear_idx));
#endif
			
			// iterate over groups - note that the group id is always identical for all threads,
			// as a single group item is worked by all worker threads (before continuing)
			uint32_t linear_group_id = 0;
			for(uint32_t group_x = 0; group_x < group_dim.x; ++group_x) {
				for(uint32_t group_y = 0; group_y < group_dim.y; ++group_y) {
					for(uint32_t group_z = 0; group_z < group_dim.z; ++group_z, ++linear_group_id) {
						// last thread is responsible for sync
						if(local_linear_idx == local_size - 1) {
							// wait until all prior work-items are done
							while(items_in_flight != 0) {
								this_thread::yield();
							}
							
							// reset + signal that group is ready for execution
							items_in_flight = local_size;
							group_id = linear_group_id;
						}
						else {
							// wait until group init is done
							while(group_id != linear_group_id) {
								this_thread::yield();
							}
						}
						
						// setup group
						floor_group_idx = { group_x, group_y, group_z };
						
						// compute global id for this work-item
						const uint3 global_id {
							group_x * local_dim.x + local_id.x,
							group_y * local_dim.y + local_id.y,
							group_z * local_dim.z + local_id.z
						};
						floor_global_idx = global_id;
						
						// finally: execute work-item
						kernel_func();
						
						// work-item done
						--items_in_flight;
					}
				}
			}
		});
	}
	// wait for worker threads to finish
	for(auto& item : worker_threads) {
		item->join();
	}
#elif defined(FLOOR_HOST_COMPUTE_MT_GROUP)
	// #work-groups
	const auto group_count = group_dim.x * group_dim.y * group_dim.z;
	// #work-items per group
	const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
	// group ticketing system, each worker thread will grab a new group id, once it's done with one group
	atomic<uint32_t> group_idx { 0 };
	
	// start worker threads
#if defined(FLOOR_HOST_KERNEL_ENABLE_TIMING)
	const auto time_start = floor_timer::start();
#endif
	vector<unique_ptr<thread>> worker_threads(cpu_count);
	for(uint32_t cpu_idx = 0; cpu_idx < cpu_count; ++cpu_idx) {
		worker_threads[cpu_idx] = make_unique<thread>([this, cpu_idx,
													   &group_idx, group_count, group_dim,
													   local_size, local_dim] {
			// set cpu affinity for this thread to a particular cpu to prevent this thread from being constantly moved/scheduled
			// on different cpus (starting at index 1, with 0 representing no affinity)
			core::set_thread_affinity(cpu_idx + 1);
			
			// set the tls thread index for this (needed to compute local memory offsets)
			floor_thread_idx = cpu_idx;
			floor_thread_local_memory_offset = cpu_idx * floor_local_memory_max_size;
			
			// init contexts (aka fibers)
			fiber_context main_ctx;
			main_ctx.init(nullptr, 0, nullptr, ~0u, nullptr, nullptr);
			auto items = make_unique<fiber_context[]>(local_size);
			item_contexts = items.get();
			
			// init fibers
			for(uint32_t i = 0; i < local_size; ++i) {
				items[i].init(&floor_stack_memory_data.get()[(i + local_size * cpu_idx) * fiber_context::min_stack_size],
							  fiber_context::min_stack_size,
							  run_mt_group_item, i,
							  // continue with next on return, or return to main ctx when the last item returns
							  // TODO: add option to use randomized order?
							  (i + 1 < local_size ? &items[i + 1] : &main_ctx),
							  &main_ctx);
			}
			
			for(;;) {
				// assign a new group to this thread/cpu and check if we're done
				const auto group_linear_idx = group_idx++;
				if(group_linear_idx >= group_count) break;
				
				// setup group
				const uint3 group_id {
					group_linear_idx % group_dim.x,
					(group_linear_idx / group_dim.x) % group_dim.y,
					group_linear_idx / (group_dim.x * group_dim.y)
				};
				floor_group_idx = group_id;
				
				// reset fibers
				for(uint32_t i = 0; i < local_size; ++i) {
					items[i].reset();
				}
#if defined(FLOOR_DEBUG)
				unfinished_items = local_size;
#endif
				
				// run fibers/work-items for this group
				static thread_local volatile bool done;
				done = false;
				main_ctx.get_context();
				if(!done) {
					done = true;
					
					// start first fiber
					items[0].set_context();
				}
				
				// exit due to excessive local memory allocation?
				if(local_memory_exceeded) {
					log_error("exceeded local memory allocation in kernel \"$\" - requested $ bytes, limit is $ bytes",
							  func_name, local_memory_alloc_offset, floor_local_memory_max_size);
					break;
				}
				
				// check if any items are still unfinished (in a valid program, all must be finished at this point)
				// NOTE: this won't detect all barrier misuses, doing so would require *a lot* of work
#if defined(FLOOR_DEBUG)
				if(unfinished_items > 0) {
					log_error("barrier misuse detected in kernel \"$\" - $ unfinished items in group $",
							  func_name, unfinished_items, group_id);
					break;
				}
#endif
			}
		});
	}
	// wait for worker threads to finish
	for(auto& item : worker_threads) {
		item->join();
	}
#if defined(FLOOR_HOST_KERNEL_ENABLE_TIMING)
	log_debug("kernel time: $ms", double(floor_timer::stop<chrono::microseconds>(time_start)) / 1000.0);
#endif
#endif
}

extern "C" void run_mt_group_item(const uint32_t local_linear_idx) {
	// set local + global id
	const uint3 local_id {
		local_linear_idx % floor_local_work_size.x,
		(local_linear_idx / floor_local_work_size.x) % floor_local_work_size.y,
		local_linear_idx / (floor_local_work_size.x * floor_local_work_size.y)
	};
	floor_local_idx = local_id;
	item_local_linear_idx = local_linear_idx;
	
	const uint3 global_id {
		floor_group_idx.x * floor_local_work_size.x + local_id.x,
		floor_group_idx.y * floor_local_work_size.y + local_id.y,
		floor_group_idx.z * floor_local_work_size.z + local_id.z
	};
	floor_global_idx = global_id;
	
	// execute work-item / kernel function
	(*cur_kernel_function)();
	
	// for barrier misuse checking
#if defined(FLOOR_DEBUG)
	--unfinished_items;
#endif
}

void host_kernel::execute_device(const host_kernel_entry& func_entry,
								 const uint32_t& cpu_count,
								 const uint3& group_dim,
								 const uint3& local_dim,
								 const uint32_t& work_dim,
								 const vector<const void*>& vptr_args) const {
	// #work-groups
	const auto group_count = group_dim.x * group_dim.y * group_dim.z;
	// #work-items per group
	const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
	// group ticketing system, each worker thread will grab a new group id, once it's done with one group
	atomic<uint32_t> group_idx { 0 };
	
	// start worker threads
#if defined(FLOOR_HOST_KERNEL_ENABLE_TIMING)
	const auto time_start = floor_timer::start();
#endif
	atomic<bool> success { true };
	vector<unique_ptr<thread>> worker_threads(cpu_count);
	for (uint32_t cpu_idx = 0; cpu_idx < cpu_count; ++cpu_idx) {
		worker_threads[cpu_idx] = make_unique<thread>([this, &success, cpu_idx, &func_entry, &vptr_args,
													   &group_idx, group_count, group_dim,
													   local_size, local_dim, work_dim] {
			// set cpu affinity for this thread to a particular cpu to prevent this thread from being constantly moved/scheduled
			// on different cpus (starting at index 1, with 0 representing no affinity)
			core::set_thread_affinity(cpu_idx + 1);
			
			// retrieve the instance for this CPU + reset/init it
			auto instance = func_entry.program->get_instance(cpu_idx);
			if (!instance) {
				log_error("no instance for CPU #$", cpu_idx);
				success = false;
				return;
			}
			instance->reset(local_dim * group_dim, local_dim, group_dim, work_dim);
			device_exec_context.ids = &instance->ids;
			auto& ids = instance->ids;
			
			// get and set the (kernel) function for this instance
			const auto& func_info = *func_entry.info;
			const auto func_iter = instance->functions.find(func_info.name);
			if (func_iter == instance->functions.end()) {
				log_error("failed to find function \"$\" for CPU #$", func_name, cpu_idx);
				success = false;
				return;
			}
			const auto func_ptr = (const kernel_func_type)const_cast<void*>(func_iter->second);
			device_exec_context.kernel_func = make_callable_kernel_function(func_ptr, vptr_args);
			if (!device_exec_context.kernel_func) {
				log_error("failed to create kernel function for CPU #$", cpu_idx);
				success = false;
				return;
			}
			
			// init contexts (aka fibers)
			fiber_context main_ctx;
			main_ctx.init(nullptr, 0, nullptr, ~0u, nullptr, nullptr);
			auto items = make_unique<fiber_context[]>(local_size);
			item_contexts = items.get();
			
			// init fibers
			for (uint32_t i = 0; i < local_size; ++i) {
				items[i].init(&floor_stack_memory_data.get()[(i + local_size * cpu_idx) * fiber_context::min_stack_size],
							  fiber_context::min_stack_size,
							  run_host_device_group_item, i,
							  // continue with next on return, or return to main ctx when the last item returns
							  (i + 1 < local_size ? &items[i + 1] : &main_ctx),
							  &main_ctx);
			}
			
			for (; success;) {
				// assign a new group to this thread/cpu and check if we're done
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
				for(uint32_t i = 0; i < local_size; ++i) {
					items[i].reset();
				}
#if defined(FLOOR_DEBUG)
				unfinished_items = local_size;
#endif
				
				// run fibers/work-items for this group
				static thread_local volatile bool done;
				done = false;
				main_ctx.get_context();
				if(!done) {
					done = true;
					
					// start first fiber
					items[0].set_context();
				}
				
				// check if any items are still unfinished (in a valid program, all must be finished at this point)
				// NOTE: this won't detect all barrier misuses, doing so would require *a lot* of work
#if defined(FLOOR_DEBUG)
				if (unfinished_items > 0) {
					log_error("barrier misuse detected in kernel \"$\" - $ unfinished items in group $",
							  func_name, unfinished_items, group_id);
					break;
				}
#endif
			}
		});
	}
	// wait for worker threads to finish
	for(auto& item : worker_threads) {
		item->join();
	}
}

extern "C" void run_host_device_group_item(const uint32_t local_linear_idx) {
	// set ids for work-item
	{
		auto& ids = *device_exec_context.ids;
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
	
	// execute work-item / kernel function
	device_exec_context.kernel_func();
	
	// for barrier misuse checking
#if defined(FLOOR_DEBUG)
	--unfinished_items;
#endif
}

// -> kernel lib function implementations
#include <floor/compute/device/host.hpp>

// barrier handling (all the same)
// NOTE: the same barrier _must_ be encountered at the same point for all work-items
void global_barrier() {
#if defined(FLOOR_HOST_COMPUTE_MT_ITEM)
	// save current barrier generation/id
	const uint32_t cur_gen = barrier_gen;
	
	// dec counter, and:
	if(--barrier_counter == 0) {
		// if this is the last thread to encounter the barrier,
		// reset the counter and increase the gen/id, so that the other threads can continue
		barrier_counter = barrier_users;
		++barrier_gen; // note: overflow doesn't matter
	}
	else {
		// if this isn't the last thread to encounter the barrier,
		// wait until the barrier gen/id changes, then continue
		while(cur_gen == barrier_gen) {
			this_thread::yield();
		}
	}
#elif defined(FLOOR_HOST_COMPUTE_MT_GROUP)
	// save indices, switch to next fiber and restore indices again
	const auto saved_global_id = floor_global_idx;
	const auto saved_local_id = floor_local_idx;
	const auto save_item_local_linear_idx = item_local_linear_idx;
	
	fiber_context* this_ctx = &item_contexts[item_local_linear_idx];
	fiber_context* next_ctx = &item_contexts[(item_local_linear_idx + 1u) % floor_linear_local_work_size];
	this_ctx->swap_context(next_ctx);
	
	item_local_linear_idx = save_item_local_linear_idx;
	floor_local_idx = saved_local_id;
	floor_global_idx = saved_global_id;
#endif
}
void local_barrier() {
	global_barrier();
}
void image_barrier() {
	global_barrier();
}
void barrier() {
	global_barrier();
}

void host_compute_device_barrier() {
	auto& ids = *device_exec_context.ids;
	
	// save indices, switch to next fiber and restore indices again
	const auto saved_global_id = ids.instance_global_idx;
	const auto saved_local_id = ids.instance_local_idx;
	const auto save_item_local_linear_idx = ids.instance_local_linear_idx;
	
	fiber_context* this_ctx = &item_contexts[ids.instance_local_linear_idx];
	fiber_context* next_ctx = &item_contexts[(ids.instance_local_linear_idx + 1u) % ids.instance_local_work_size.extent()];
	this_ctx->swap_context(next_ctx);
	
	ids.instance_local_linear_idx = save_item_local_linear_idx;
	ids.instance_local_idx = saved_local_id;
	ids.instance_global_idx = saved_global_id;
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

// local memory management
// NOTE: this is called when allocating storage for local buffers when using mt-group
uint8_t* __attribute__((aligned(1024))) floor_requisition_local_memory(const size_t size, uint32_t& offset) noexcept {
	// check if this allocation exceeds the max size
	// note: using the unaligned size, since the padding isn't actually used
	if((local_memory_alloc_offset + size) > floor_local_memory_max_size) {
		// if so, signal the main thread that things are bad and switch to it
		local_memory_exceeded = true;
		item_contexts[item_local_linear_idx].exit_to_main();
	}
	
	// align to 1024-bit / 128 bytes
	const auto per_thread_alloc_size = (size % 128 == 0 ? size : (((size / 128) + 1) * 128));
	// set the offset to this allocation
	offset = local_memory_alloc_offset;
	// adjust allocation offset for the next allocation
	local_memory_alloc_offset += per_thread_alloc_size;
	
	return floor_local_memory_data.get();
}

unique_ptr<argument_buffer> host_kernel::create_argument_buffer_internal(const compute_queue& cqueue,
																		 const kernel_entry& kern_entry,
																		 const llvm_toolchain::arg_info& arg floor_unused,
																		 const uint32_t& user_arg_index,
																		 const uint32_t& ll_arg_index,
																		 const COMPUTE_MEMORY_FLAG& add_mem_flags) const {
	const auto& dev = cqueue.get_device();
	const auto& host_entry = (const host_kernel_entry&)kern_entry;
	
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
	auto buf = dev.context->create_buffer(cqueue, arg_buffer_size, COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE | add_mem_flags);
	buf->set_debug_label(kern_entry.info->name + "_arg_buffer");
	return make_unique<host_argument_buffer>(*this, buf, *arg_info);
}

#endif
