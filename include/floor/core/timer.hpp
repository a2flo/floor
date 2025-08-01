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

#include <floor/core/platform.hpp>
#include <chrono>

namespace fl {

//! simple timer class based on std chrono functionality and capable of using arbitrary duration types (defaults to ms)
//! NOTE: timer needs a monotonic/steady clock, uses high_resolution_clock as a default if it is steady, otherwise uses steady_clock
template <class clock_type = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
										   std::chrono::high_resolution_clock,
										   std::chrono::steady_clock>>
class floor_timer_t {
public:
	//! the active clock type for this timer
	using timer_clock_type = clock_type;
	
	//! "starts" the timer: returns the current time_point of a monotonic clock
	static std::chrono::time_point<clock_type> start() {
		return clock_type::now();
	}
	
	//! "stops" the timer: subtracts the start time_point from the current time_point of a monotonic clock
	//! and casts the result to a specified duration type (or milliseconds by default)
	template <typename duration_type = std::chrono::milliseconds>
	static uint64_t stop(const std::chrono::time_point<clock_type>& start_time) {
		return (uint64_t)std::chrono::duration_cast<duration_type>(clock_type::now() - start_time).count();
	}
	
protected:
	// static class
	floor_timer_t(const floor_timer_t&) = delete;
	~floor_timer_t() = delete;
	floor_timer_t& operator=(const floor_timer_t&) = delete;
	
};
//! floor_timer_t with default clock_type (this is for easier use, so <> doesn't have to be used all the time)
using floor_timer = floor_timer_t<>;

//! multi-timer, can add multiple incrementally timed entries, each entry with its own name, will compute total at the end
template <class clock_type = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
										   std::chrono::high_resolution_clock,
										   std::chrono::steady_clock>>
class floor_multi_timer_t {
public:
	floor_multi_timer_t() { add("start", false); }
	
	void add(const std::string& name, const bool print_diff = true) {
		entries.emplace_back(name, clock_type::now());
		if(print_diff) {
			print_entry(prev(entries.cend()));
		}
	}
	
	void end(const bool print_all = false) const {
		if(print_all) {
			for(auto entry = entries.cbegin(); entry != entries.cend(); ++entry) {
				print_entry(entry);
			}
		}
		const auto total_diff = entries.back().second - entries.front().second;
		log_undecorated("[TOTAL] $s # $ms # $",
						to_s(entries.back().second, entries.front().second),
						to_ms(entries.back().second, entries.front().second),
						(uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(total_diff).count());
	}
	
protected:
	using time_entry = std::chrono::time_point<clock_type>;
	std::vector<std::pair<const std::string, time_entry>> entries;
	
	void print_entry(const typename decltype(entries)::const_iterator& iter) const {
		// don't print the start entry
		if(iter == entries.cbegin()) return;
		
		log_undecorated("[$] $ms # $",
						iter->first,
						to_ms(iter->second, prev(iter)->second),
						to_s(iter->second, prev(iter)->second));
	}
	
	static double to_ms(const time_entry& first, const time_entry& second) {
		return double(std::chrono::duration_cast<std::chrono::microseconds>(first - second).count()) / 1000.0;
	}
	
	static double to_s(const time_entry& first, const time_entry& second) {
		return double(std::chrono::duration_cast<std::chrono::milliseconds>(first - second).count()) / 1000.0;
	}
	
};
using floor_multi_timer = floor_multi_timer_t<>;

} // namespace fl
