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

#include <cassert>
#include <cstdint>
#include <utility>
#include <span>
#include <type_traits>

namespace fl {

//! a ring-buffer for a small amount of values
//! NOTE: this is not thread-safe
template <typename T, uint32_t count>
requires (count <= 0x3FFF'FFFFu)
class small_ring_buffer {
public:
	using data_type = T;
	
	constexpr small_ring_buffer() noexcept = default;
	small_ring_buffer(const small_ring_buffer&) noexcept = default;
	small_ring_buffer(small_ring_buffer&&) noexcept = default;
	small_ring_buffer& operator=(const small_ring_buffer&) noexcept = default;
	small_ring_buffer& operator=(small_ring_buffer&& rb) noexcept = default;
	
	//! tries to enqueue "value" into the ring buffer, returns true on success, false on failure
	bool enqueue(std::conditional_t<std::is_fundamental_v<data_type>, data_type, data_type&&> value) {
		if (compute_current_writable_elements() == 0) {
			return false;
		}
		data[write_offset()] = std::move(value);
		advance_write_index();
		return true;
	}
	
	//! tries to dequeue a value from the ring buffer, returns { value, success }
	std::pair<data_type, bool> dequeue() {
		if (compute_current_readable_elements() == 0) {
			return { {}, false };
		}
		auto ret_value = data[read_offset()];
		advance_read_index();
		return { ret_value, true };
	}
	
	//! peeks at and returns the next readable value in the ring buffer w/o modifying the read index,
	//! returns { value, success }
	std::pair<data_type, bool> peek() const {
		if (compute_current_readable_elements() == 0) {
			return { {}, false };
		}
		return { data[read_offset()], true };
	}
	
	//! peeks at and returns the last readable value in the ring buffer w/o modifying the read index,
	//! returns { value, success }
	std::pair<data_type, bool> peek_back() const {
		if (compute_current_readable_elements() == 0) {
			return { {}, false };
		}
		auto back_offset = write_offset();
		back_offset = (back_offset == 0 ? count - 1u : back_offset - 1u);
		return { data[back_offset], true };
	}
	
	//! sequentially iterates from front to back over all currently readable elements,
	//! calling "cb" for each element, which may return "true" to abort iteration
	//! NOTE: this may modify any or all of the readable elements (e.g. [](auto& elem) { elem = 42; return false; })
	template <typename F>
	void peek_iterate(F&& cb) {
		for (uint32_t idx = 0u, offset = read_offset(), elem_count = readable_elements(); idx < elem_count; ++idx) {
			if (cb(data[(offset + idx) % count])) {
				break;
			}
		}
	}
	
	//! returns the size (element count) of this ring buffer
	constexpr size_t size() const {
		return count;
	}
	
	//! returns the current write offset in #elements in [0, count)
	//! NOTE: write offset may be less than, equal to or greater than the read offset!
	uint32_t write_offset() const {
		return write_idx % count;
	}
	
	//! returns the amount of elements that can still be written into the ring buffer
	uint32_t writable_elements() const {
		return compute_current_writable_elements();
	}
	
	//! returns the current read offset in #elements in [0, count)
	//! NOTE: read offset may be less than, equal to or greater than the write offset!
	uint32_t read_offset() const {
		return read_idx % count;
	}
	
	//! returns the amount of elements that can be read from the ring buffer
	uint32_t readable_elements() const {
		return compute_current_readable_elements();
	}
	
	//! discards all currently enqueued values
	void clear() {
		advance_read_index(compute_current_readable_elements());
	}
	
	//! returns true if the ring buffer is currently empty (no readable elements)
	bool empty() const {
		return (compute_current_readable_elements() == 0u);
	}
	
	//! gets the full ring buffer memory region
	std::span<data_type> get() {
		return std::span { data.data(), count };
	}
	
	//! gets the full ring buffer memory region (const)
	std::span<const data_type> get() const {
		return std::span { data.data(), count };
	}
	
protected:
	//! the underlying ring buffer data
	std::array<data_type, count> data;
	//! current write/producer index
	//! NOTE: write_idx >= read_idx
	uint32_t write_idx { 0u };
	//! current read/consumer index
	//! NOTE: read_idx <= write_idx
	uint32_t read_idx { 0u };
	
	//! computes the current amount of writable/producible elements
	uint32_t compute_current_writable_elements() const {
		assert(write_idx >= read_idx);
		assert(write_idx - read_idx <= count);
		const auto writable_elements = count - (write_idx - read_idx);
		assert(writable_elements <= count);
		return writable_elements;
	}
	
	//! computes the current amount of readable/consumable elements
	uint32_t compute_current_readable_elements() const {
		assert(write_idx >= read_idx);
		assert(write_idx - read_idx <= count);
		const auto readable_elements = write_idx - read_idx;
		assert(readable_elements <= count);
		return readable_elements;
	}
	
	//! advances the current write index by one
	//! NOTE: only advance_read_index() will make the write index wrap around, this is to ensure write_idx >= read_idx
	void advance_write_index() {
		++write_idx;
	}
	
	//! advances the current read index by "adv_elem_count" elements
	void advance_read_index(const uint32_t adv_elem_count = 1u) {
		assert(adv_elem_count <= count);
		assert(read_idx + adv_elem_count <= count);
		
		auto next_read_idx = read_idx + adv_elem_count;
		if (next_read_idx >= count) {
			assert(next_read_idx <= write_idx);
			next_read_idx %= count;
			
			// we can now also wrap the write index
			assert(write_idx >= count);
			write_idx = write_idx % count;
			assert(next_read_idx <= write_idx);
		}
		read_idx = next_read_idx;
	}
};

} // namespace namespace fl
