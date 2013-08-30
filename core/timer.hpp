/*
 *  Flexible OpenCL Rasterizer (oclraster)
 *  Copyright (C) 2012 - 2013 Florian Ziesche
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

#ifndef __FLOOR_TIMER_HPP__
#define __FLOOR_TIMER_HPP__

#include "core/platform.h"

class OCLRASTER_API oclr_timer {
public:
	oclr_timer() { add("start", false); }
	~oclr_timer() {}
	
	void add(const string& name, const bool print_diff = true) {
		entries.emplace_back(name, SDL_GetPerformanceCounter());
		if(print_diff) {
			print_entry(entries.end() - 1);
		}
	}
	
	void end(const bool print_all = false) const {
		if(print_all) {
			for(auto entry = entries.cbegin(); entry != entries.cend(); entry++) {
				print_entry(entry);
			}
		}
		const auto total_diff = entries.back().second - entries.front().second;
		oclr_log("[TOTAL] %_s # %_ms # %_",
				 double(total_diff) / double(ticks_per_second),
				 double(total_diff) / double(ticks_per_second / 1000ull),
				 total_diff);
	}
	
protected:
	const unsigned long long int ticks_per_second { SDL_GetPerformanceFrequency() };
	vector<pair<const string, const unsigned long long int>> entries;
	
	void print_entry(const decltype(entries)::const_iterator& iter) const {
		// don't print the start entry
		if(iter == entries.cbegin()) return;
		
		const auto prev_iter = iter - 1;
		const auto diff = iter->second - prev_iter->second;
		oclr_log("[%_] %_ms # %_",
				 iter->first,
				 double(diff) / double(ticks_per_second / 1000ull),
				 diff);
	}
	
};

#endif
