/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#ifndef __FLOOR_THREAD_BASE_HPP__
#define __FLOOR_THREAD_BASE_HPP__

#include "core/cpp_headers.hpp"

#include <thread>
#include <mutex>

class thread_base {
public:
	thread_base(const string name = "unknown");
	virtual ~thread_base();
	
	enum class THREAD_STATUS : int {
		INVALID = -1,
		INIT = 0,
		RUNNING = 1,
		FINISHED = 2
	};
	
	// this is the main run function of the thread
	virtual void run() = 0;
	
	void finish();
	virtual void restart();
	void lock();
	bool try_lock();
	void unlock();
	
	void set_thread_status(const thread_base::THREAD_STATUS status);
	THREAD_STATUS get_thread_status() const;
	bool is_running() const; // shortcut for get_thread_status() == RUNNING || INIT
	void set_thread_should_finish();
	bool thread_should_finish();
	void set_thread_delay(const unsigned int delay);
	size_t get_thread_delay() const;
	
protected:
	const string thread_name;
	unique_ptr<thread> thread_obj { nullptr };
	recursive_mutex thread_lock {};
	// there is no way in c++ to figure out if a lock is still held -> count the locks/unlocks manually
	atomic<unsigned int> thread_lock_count { 0 };
	atomic<THREAD_STATUS> thread_status { THREAD_STATUS::INIT };
	atomic<bool> thread_should_finish_flag { false };
	size_t thread_delay { 50 };
	
	void start();
	static int _thread_run(thread_base* this_thread_obj);
	
	thread_base(const thread_base& tb);
	void operator=(const thread_base& tb);
};

#endif
