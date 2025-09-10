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

#include <floor/threading/thread_base.hpp>
#include <floor/threading/thread_helpers.hpp>
#include <floor/core/logger.hpp>
#include <chrono>

namespace fl {
using namespace std::literals;

thread_base::thread_base(const std::string name) : thread_name(name) {
	thread_obj = std::make_unique<std::thread>(_thread_run, this);
}

thread_base::~thread_base() {
	finish();
}

void thread_base::start() {
	if(thread_status != THREAD_STATUS::INIT) {
		// something is wrong, return (thread status must be init!)
		std::cout << "ERROR: thread error: thread status must be INIT before starting the thread!" << std::endl;
		return;
	}
	
	thread_status = THREAD_STATUS::RUNNING;
}

int thread_base::_thread_run(thread_base* this_thread_obj) {
	set_current_thread_name(this_thread_obj->thread_name);
	
	// wait until start() was called
	while (this_thread_obj->thread_status != THREAD_STATUS::RUNNING &&
		   !this_thread_obj->thread_should_finish()) {
		std::this_thread::sleep_for(20ms);
	}
	
	// dummy lock for CV waiting
	std::mutex cv_mtx;
	
	// if the "finish flag" has been set in the mean time, don't rerun
	while (!this_thread_obj->thread_should_finish()) {
		// pause/halt thread if set
		if (this_thread_obj->thread_pause) {
			this_thread_obj->set_thread_status(THREAD_STATUS::PAUSED);
			while (this_thread_obj->thread_pause) {
				std::unique_lock<std::mutex> pause_lock_guard(cv_mtx);
				// NOTE: timeout after 5s so that we don't have an infinite loop
				(void)this_thread_obj->pause_cv.wait_for(pause_lock_guard, 5s);
			}
			this_thread_obj->set_thread_status(THREAD_STATUS::RUNNING);
		}
		
		// run
		try {
			this_thread_obj->run();
		} catch(std::exception& exc) {
			log_error("encountered an unhandled exception while running a thread \"$\": $",
					  get_current_thread_name(), exc.what());
		} catch(...) {
			log_error("encountered an unhandled exception while running a thread \"$\"",
					  get_current_thread_name());
		}
		
		// again: if the "finish flag" has been set, don't wait, but continue immediately
		if (!this_thread_obj->thread_should_finish()) {
			// reduce system load and make other locks possible
			const size_t thread_delay = this_thread_obj->get_thread_delay();
			if (thread_delay > 0) {
				// wait on a CV rather than a simple sleep_for, so that we can quickly abort threads with a long delay
				std::unique_lock<std::mutex> delay_lock_guard(cv_mtx);
				(void)this_thread_obj->delay_cv.wait_for(delay_lock_guard, std::chrono::milliseconds(thread_delay));
			} else {
				if (this_thread_obj->get_yield_after_run()) {
					// just yield when delay == 0 and "yield after run" flag is set
					std::this_thread::yield();
				}
			}
		}
	}
	this_thread_obj->set_thread_status(THREAD_STATUS::FINISHED);
	
	return 0;
}

void thread_base::pause() {
	thread_pause = true;
}

void thread_base::unpause() {
	thread_pause = false;
	pause_cv.notify_one();
}

void thread_base::finish() {
	if (thread_obj != nullptr) {
		if ((get_thread_status() == THREAD_STATUS::FINISHED && !thread_obj->joinable()) ||
			get_thread_status() == THREAD_STATUS::INIT) {
			// already finished or uninitialized, nothing to do here
		} else {
			// signal thread to finish
			set_thread_should_finish();
			
			// ensure thread is unpaused
			unpause();
			
			// this will block until the thread is finished
			if (thread_obj->joinable()) {
				thread_obj->join();
			}
		}
		
		// finally: kill the thread object
		thread_obj = nullptr;
	}
	// else: thread doesn't exist
	
	set_thread_status(THREAD_STATUS::FINISHED);
}

void thread_base::set_thread_status(const thread_base::THREAD_STATUS status) {
	thread_status = status;
}

thread_base::THREAD_STATUS thread_base::get_thread_status() const {
	return thread_status;
}

bool thread_base::is_running() const {
	// copy before use
	const THREAD_STATUS status = thread_status;
	return (status == THREAD_STATUS::RUNNING || status == THREAD_STATUS::INIT);
}

void thread_base::set_thread_should_finish() {
	thread_should_finish_flag = true;
	// wake up if thread is sleeping in delay
	delay_cv.notify_one();
}

bool thread_base::thread_should_finish() {
	return thread_should_finish_flag;
}

void thread_base::set_thread_delay(const size_t delay) {
	thread_delay = delay;
}

size_t thread_base::get_thread_delay() const {
	return thread_delay;
}

void thread_base::set_yield_after_run(const bool state) {
	yield_after_run = state;
}

bool thread_base::get_yield_after_run() const {
	return yield_after_run;
}

const std::string& thread_base::get_thread_name() const {
	return thread_name;
}

} // namespace fl
