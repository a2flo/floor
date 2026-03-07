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

#include <thread>
#include <atomic>
#include <type_traits>
#include <array>
#include <cstdint>
#include <cassert>
#include <floor/threading/atomic_spin_lock.hpp>

namespace fl {

//! the max atomic width in bits decides how many resources we can use at most
//! NOTE: only libc++/libstdc++ provide __uint128_t std::atomic support
static constexpr const uint32_t max_platform_resource_count {
#if !defined(_MSVC_STL_UPDATE)
	128u
#else
	64u
#endif
};

//! lock-free resource slot handler
template <uint32_t resource_count>
requires (resource_count <= max_platform_resource_count)
struct resource_slot_handler {
	//! slot bitset/mask type
	using slot_bitset_t = std::conditional_t<resource_count <= 16u, uint16_t,
						  std::conditional_t<resource_count <= 32u, uint32_t,
						  std::conditional_t<resource_count <= 64u, uint64_t, __uint128_t>>>;
	//! need to ensure this for lock-free handling
	static_assert(std::atomic<slot_bitset_t>::is_always_lock_free);
	
	//! indicates an invalid slot / failure
	static constexpr const uint8_t invalid_slot_idx { 255u };
	//! ensure "invalid_slot_idx" is not in-bounds of max_platform_resource_count
	static_assert(max_platform_resource_count < invalid_slot_idx);
	
	//! determines the initial "slots" value:
	//! if resource_count < slot_bitset_t bit-width, set upper bits to 0 to mark them as always-used -> otherwise, set all bits to 1
	static inline constexpr slot_bitset_t slots_init_value() {
		if constexpr (resource_count < sizeof(slot_bitset_t) * 8u) {
			return slot_bitset_t((slot_bitset_t(1) << resource_count) - slot_bitset_t(1));
		} else {
			return slot_bitset_t(~slot_bitset_t(0));
		}
	}
	
	//! 1: slot is unused, 0: slot is used
	alignas(64) std::atomic<slot_bitset_t> slots { slots_init_value() };
	
	//! tries to acquire a free slot index and returns it,
	//! returns "invalid_slot_idx" on failure
	uint8_t try_acquire_slot() {
		// find the lowest unused (1) slot
		auto cur_slots = slots.load();
		if (cur_slots == 0) [[unlikely]] {
			// -> no free slot
			return invalid_slot_idx;
		}
		
		const uint8_t slot_idx = compute_free_slot_idx(cur_slots);
		auto new_slots = slot_bitset_t(cur_slots ^ (slot_bitset_t(1u) << slot_idx));
		if (slots.compare_exchange_strong(cur_slots, new_slots, std::memory_order_acq_rel)) [[likely]] {
			// -> success
			return slot_idx;
		}
		
		// -> failure
		return invalid_slot_idx;
	}
	
	//! acquires an unused slot
	uint8_t acquire_slot() {
		// find the lowest unused (1) slot
		auto cur_slots = slots.load();
		uint32_t attempt = 0u;
		do {
			if (cur_slots == 0) [[unlikely]] {
				// -> no free slot, wait
				if (attempt < 16u) {
					FLOOR_SPIN_WAIT();
					++attempt;
				} else {
					std::this_thread::yield();
					attempt = 0u;
				}
				cur_slots = slots.load();
				continue;
			}
			
			const uint8_t slot_idx = compute_free_slot_idx(cur_slots);
			auto new_slots = slot_bitset_t(cur_slots ^ (slot_bitset_t(1u) << slot_idx));
			if (slots.compare_exchange_strong(cur_slots, new_slots, std::memory_order_acq_rel)) [[likely]] {
				// -> success
				return slot_idx;
			}
			// -> failure, retry
		} while (true);
	}
	
	//! releases a slot again
	void release_slot(const uint8_t slot_idx) {
		assert(slot_idx < resource_count);
		const auto slot_idx_mask = (slot_bitset_t(1u) << slot_idx);
		auto cur_slots = slots.load();
		do {
			auto new_slots = slot_bitset_t(cur_slots ^ slot_idx_mask);
			if (slots.compare_exchange_strong(cur_slots, new_slots, std::memory_order_acq_rel)) [[likely]] {
				// -> success
				return;
			}
			// -> failure, retry
		} while (true);
	}
	
	//! returns the current underlying used slots mask value as an unsigned integer of the appropriate size
	//! NOTE: bit values at bit indices >= resource_count are undefined
	auto get_used_slots_mask() const {
		return slot_bitset_t(~slots.load());
	}
	
	//! returns the amount of currently used slots
	uint32_t get_used_slot_count() const {
		const auto used_slots_mask = get_used_slots_mask();
		uint32_t count = 0u;
		if constexpr (resource_count <= 16u) {
			count = uint32_t(__builtin_popcount(uint32_t(used_slots_mask)));
		} else if constexpr (resource_count <= 32u) {
			count = uint32_t(__builtin_popcount(used_slots_mask));
		} else if constexpr (resource_count <= 64u) {
			count = uint32_t(__builtin_popcountll(used_slots_mask));
		} else if constexpr (resource_count <= 128u) {
			const auto upper = uint64_t(used_slots_mask >> __uint128_t(64));
			const auto lower = uint64_t(used_slots_mask & __uint128_t(~0ull));
			count = uint32_t(__builtin_popcountll(upper) + __builtin_popcountll(lower));
		} else {
			static_assert(false);
		}
		if constexpr (resource_count != sizeof(slot_bitset_t) * 8u) {
			// adjust count for resource counts != slot_bitset_t bit-width
			count -= sizeof(slot_bitset_t) * 8u - resource_count;
			assert(count <= resource_count);
		}
		return count;
	}
	
protected:
	//! computes/returns a free slot index based on "cur_slots" mask
	//! NOTE: requires that "cur_slots" actually contains a free slot (precondition)
	static inline uint8_t compute_free_slot_idx(const slot_bitset_t cur_slots) {
		// find free slot idx
		uint8_t slot_idx = 0u;
		if constexpr (resource_count <= 16u) {
			slot_idx = uint8_t(__builtin_ctzs(cur_slots));
		} else if constexpr (resource_count <= 32u) {
			slot_idx = uint8_t(__builtin_ctz(cur_slots));
		} else if constexpr (resource_count <= 64u) {
			slot_idx = uint8_t(__builtin_ctzll(cur_slots));
		} else if constexpr (resource_count <= 128u) {
			const auto upper = uint64_t(cur_slots >> __uint128_t(64));
			const auto lower = uint64_t(cur_slots & __uint128_t(~0ull));
			const auto ctz_upper = uint32_t(__builtin_ctzll(upper));
			const auto ctz_lower = (lower != 0u ? uint32_t(__builtin_ctzll(lower)) : 64u);
			slot_idx = uint8_t(ctz_lower < 64u ? ctz_lower : (ctz_upper + ctz_lower));
		} else {
			static_assert(false);
		}
		assert(slot_idx < resource_count);
		return slot_idx;
	}
};

//! lock-free resource container using fixed allocation slots for small resource counts
template <typename data_type, uint32_t resource_count>
requires (resource_count > 0)
struct resource_slot_container_small {
	//! NOTE: can't put this in the class requires clause, because this would prevent resource_slot_container from working with larger sizes
	static_assert(resource_count <= max_platform_resource_count, "resource_count must be <= the max platform resource count");
	
	using slot_handler_t = resource_slot_handler<resource_count>;
	
	//! encodes a single resource reference that is returned for external use
	template <bool auto_release = true>
	struct resource_t {
	public:
		data_type* res { nullptr };
		static constexpr const uint8_t invalid_slot_idx { slot_handler_t::invalid_slot_idx };
		
	protected:
		resource_slot_container_small* container { nullptr };
		uint8_t slot_idx { invalid_slot_idx };
		
	public:
		constexpr resource_t() noexcept = default;
		constexpr resource_t(data_type* res_, resource_slot_container_small* container_, const uint8_t slot_idx_) noexcept :
		res(res_), container(container_), slot_idx(slot_idx_) {}
		constexpr resource_t(resource_t&& resource) noexcept : res(resource.res), container(resource.container), slot_idx(resource.slot_idx) {
			resource.res = nullptr;
			resource.container = nullptr;
		}
		resource_t& operator=(resource_t&& resource) {
			res = resource.res;
			container = resource.container;
			slot_idx = resource.slot_idx;
			resource.res = nullptr;
			resource.container = nullptr;
			resource.slot_idx = invalid_slot_idx;
			return *this;
		}
		constexpr resource_t(const resource_t&) = delete;
		resource_t& operator=(const resource_t&) = delete;
		
		~resource_t() {
			if constexpr (auto_release) {
				if (container) {
					assert(slot_idx != invalid_slot_idx);
					container->release_resource(slot_idx);
				}
			}
		}
		
		//! returns true if the resource is valid
		explicit operator bool() const {
			return (res != nullptr);
		}
		
		//! access resource data
		data_type* operator->() {
			return res;
		}
		
		//! access resource data (const)
		const data_type* operator->() const {
			return res;
		}
		
		//! returns the acquire slot index
		uint8_t index() const {
			assert(slot_idx != invalid_slot_idx);
			return slot_idx;
		}
		
		//! manually releases this resource
		void release() {
			if (container) {
				assert(slot_idx != invalid_slot_idx);
				container->release_resource(slot_idx);
				container = nullptr;
			}
		}
	};
	
	using auto_release_resource_t = resource_t<true>;
	using manual_release_resource_t = resource_t<false>;
	
	//! lock-free slot handler
	slot_handler_t slots;
	
	//! the actual fixed resource allocations
	std::array<data_type, resource_count> resources {};
	
	//! acquires a resource (with auto-release on destruction)
	auto_release_resource_t acquire_resource() {
		auto slot_idx = slots.acquire_slot();
		return { &resources[slot_idx], this, slot_idx };
	}
	
	//! acquires a resource (without auto-release on destruction -> must manually call release_resource())
	manual_release_resource_t acquire_resource_no_auto_release() {
		auto slot_idx = slots.acquire_slot();
		return { &resources[slot_idx], this, slot_idx };
	}
	
	//! tries to acquire a resource (with auto-release on destruction)
	//! NOTE: test returned resource to check if the acquisition was successful
	auto_release_resource_t try_acquire_resource() {
		auto slot_idx = slots.try_acquire_slot();
		if (slot_idx != slot_handler_t::invalid_slot_idx) [[likely]] {
			return { &resources[slot_idx], this, slot_idx };
		}
		return {};
	}
	
	//! tries to acquire a resource (without auto-release on destruction -> must manually call release_resource())
	//! NOTE: test returned resource to check if the acquisition was successful
	manual_release_resource_t try_acquire_resource_no_auto_release() {
		auto slot_idx = slots.try_acquire_slot();
		if (slot_idx != slot_handler_t::invalid_slot_idx) [[likely]] {
			return { &resources[slot_idx], this, slot_idx };
		}
		return {};
	}
	
	//! releases a resource slot again
	void release_resource(const uint8_t slot_idx) {
		slots.release_slot(slot_idx);
	}
	
	//! returns the current underlying used slots mask value as an unsigned integer of the appropriate size
	//! NOTE: bit values at bit indices >= resource_count are undefined
	auto get_used_slots_mask() const {
		return slots.get_used_slots_mask();
	}
	
	//! returns the amount of currently used slots
	uint32_t get_used_slot_count() const {
		return slots.get_used_slot_count();
	}
};

//! lock-free resource container using fixed allocation slots for large resource counts
//! NOTE: "resource_count" must be a multiple of the max platform resource count (128 or 64)
template <typename data_type, uint32_t resource_count>
requires (resource_count > 0)
struct resource_slot_container_large {
	//! NOTE: can't put this in the class requires clause, because this would prevent resource_slot_container from working with smaller sizes
	static_assert((resource_count % max_platform_resource_count) == 0, "resource_count must be a multiple of the max platform resource count");
	
	//! numbers of slots per resource_slot_handler
	static constexpr const uint32_t slots_per_block { max_platform_resource_count };
	//! we allocate/use multiple resource_slot_handler in here
	static constexpr const uint32_t block_count { (resource_count + slots_per_block - 1u) / slots_per_block };
	static_assert(block_count >= 1u);
	
	using slot_handler_t = resource_slot_handler<slots_per_block>;
	
	//! encodes a single resource reference that is returned for external use
	template <bool auto_release = true>
	struct resource_t {
	public:
		data_type* res { nullptr };
		static constexpr const uint8_t invalid_slot_idx { slot_handler_t::invalid_slot_idx };
		
	protected:
		resource_slot_container_large* container { nullptr };
		uint32_t slot_idx { invalid_slot_idx };
		
	public:
		constexpr resource_t() noexcept = default;
		constexpr resource_t(data_type* res_, resource_slot_container_large* container_, const uint32_t slot_idx_) noexcept :
		res(res_), container(container_), slot_idx(slot_idx_) {}
		constexpr resource_t(resource_t&& resource) noexcept : res(resource.res), container(resource.container), slot_idx(resource.slot_idx) {
			resource.res = nullptr;
			resource.container = nullptr;
		}
		resource_t& operator=(resource_t&& resource) {
			res = resource.res;
			container = resource.container;
			slot_idx = resource.slot_idx;
			resource.res = nullptr;
			resource.container = nullptr;
			resource.slot_idx = invalid_slot_idx;
			return *this;
		}
		constexpr resource_t(const resource_t&) = delete;
		resource_t& operator=(const resource_t&) = delete;
		
		~resource_t() {
			if constexpr (auto_release) {
				if (container) {
					assert(slot_idx != invalid_slot_idx);
					container->release_resource(slot_idx);
				}
			}
		}
		
		//! returns true if the resource is valid
		explicit operator bool() const {
			return (res != nullptr);
		}
		
		//! access resource data
		data_type* operator->() {
			return res;
		}
		
		//! access resource data (const)
		const data_type* operator->() const {
			return res;
		}
		
		//! returns the acquire slot index
		uint32_t index() const {
			assert(slot_idx != invalid_slot_idx);
			return slot_idx;
		}
		
		//! manually releases this resource
		void release() {
			if (container) {
				assert(slot_idx != invalid_slot_idx);
				container->release_resource(slot_idx);
				container = nullptr;
			}
		}
	};
	
	using auto_release_resource_t = resource_t<true>;
	using manual_release_resource_t = resource_t<false>;
	
	//! lock-free slot handlers
	std::array<resource_slot_handler<slots_per_block>, block_count> slots {};
	
	//! when acquiring a slot, we perform a round-robin across "slots"
	alignas(64) std::atomic<uint32_t> current_block { 0u };
	
	//! the actual fixed resource allocations
	std::array<data_type, resource_count> resources {};
	
	//! acquires a resource (with auto-release on destruction)
	auto_release_resource_t acquire_resource() {
		auto slot_idx = acquire_slot();
		return { &resources[slot_idx], this, slot_idx };
	}
	
	//! acquires a resource (without auto-release on destruction -> must manually call release_resource())
	manual_release_resource_t acquire_resource_no_auto_release() {
		auto slot_idx = acquire_slot();
		return { &resources[slot_idx], this, slot_idx };
	}
	
	//! releases a resource slot again
	void release_resource(const uint32_t slot_idx) {
		release_slot(slot_idx);
	}
	
	//! returns the amount of currently used slots
	uint32_t get_used_slot_count() const {
		uint32_t total = 0u;
		for (uint32_t i = 0; i < block_count; ++i) {
			total += slots[i].get_used_slot_count();
		}
		return total;
	}
	
protected:
	//! acquires a slot from any of the resource_slot_handlers
	uint32_t acquire_slot() {
		uint32_t attempt = 0u;
		do {
			// iterate over all blocks and try to acquire a free slot (but don't spin wait inside any resource_slot_handler)
			// also: offset our search by "current_block", so we don't always hit the same block/resource_slot_handler first
			for (uint32_t block_offset = current_block, block_end = block_offset + block_count;
				 block_offset < block_end; ++block_offset) {
				const auto block_idx = (block_offset % block_count);
				const auto slot_idx = slots[block_idx].try_acquire_slot();
				if (slot_idx != resource_slot_handler<slots_per_block>::invalid_slot_idx) [[likely]] {
					// advance current_block for round-robin fairness
					current_block = block_idx;
					// offset slot index by the per-block slots
					return uint32_t(slot_idx) + block_idx * slots_per_block;
				}
			}
			
			// if we didn't find any free slot among all blocks, we spin wait or yield now, then try again
			if (attempt < 16u) {
				FLOOR_SPIN_WAIT();
				++attempt;
			} else {
				std::this_thread::yield();
				attempt = 0u;
			}
		} while (true);
	}
	
	//! releases a slot from the resp. resource_slot_handler
	void release_slot(const uint32_t slot_idx) {
		assert(slot_idx < resource_count);
		const auto block_idx = slot_idx / slots_per_block;
		assert(block_idx < block_count);
		const auto slot_idx_actual = uint8_t(slot_idx % slots_per_block);
		slots[block_idx].release_slot(slot_idx_actual);
	}
};

//! wrapper around resource_slot_container_small and resource_slot_container_large, selecting the appropriate ones based on "resource_count"
template <typename data_type, uint32_t resource_count>
using resource_slot_container = std::conditional_t<resource_count <= max_platform_resource_count,
												   resource_slot_container_small<data_type, resource_count>,
												   resource_slot_container_large<data_type, resource_count>>;

} // namespace fl
