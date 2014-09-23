/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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
#include <floor/core/cpp_headers.hpp>
#include <floor/core/logger.hpp>

thread_base::thread_base(const string name) : thread_name(name) {
	this->lock(); // lock thread, so start (or unlock) must be called before the thread starts running
	thread_obj = make_unique<std::thread>(_thread_run, this);
}

thread_base::~thread_base() {
	finish();
	
	// when destructing a mutex it must not be locked!
	for(unsigned int i = 0, unlocks = thread_lock_count; i < unlocks; i++) {
		this->unlock();
	}
}

void thread_base::start() {
	if(thread_status != THREAD_STATUS::INIT) {
		// something is wrong, return (thread status must be init!)
		cout << "ERROR: thread error: thread status must be INIT before starting the thread!" << endl;
		return;
	}
	
	thread_status = THREAD_STATUS::RUNNING;
	this->unlock();
}

void thread_base::restart() {
	this->lock();
	
	// terminate old thread if it is still running
	if(thread_obj != nullptr) {
		if(thread_obj->joinable()) {
			thread_should_finish_flag = true;
			thread_obj->join();
		}
	}
	thread_should_finish_flag = false;
	
	thread_obj = make_unique<std::thread>(_thread_run, this);
	start();
}

int thread_base::_thread_run(thread_base* this_thread_obj) {
#if defined(_PTHREAD_H)
	pthread_setname_np(
#if !defined(__APPLE__)
					   this_thread_obj->thread_obj->native_handle(),
#endif
					   this_thread_obj->thread_name.c_str());
#endif
	
	while(true) {
		// wait until we get the thread lock
		if(this_thread_obj->try_lock()) {
			// if the "finish flag" has been set in the mean time, don't call the run method!
			if(!this_thread_obj->thread_should_finish()) {
				try {
					this_thread_obj->run();
				}
				catch(exception& exc) {
					log_error("encountered an unhandled exception while running a thread \"%s\": %s",
							  this_thread_obj->thread_name, exc.what());
				}
				catch(...) {
					log_error("encountered an unhandled exception while running a thread \"%s\"",
							  this_thread_obj->thread_name);
				}
			}
			this_thread_obj->unlock();
			
			// again: if the "finish flag" has been set, don't wait, but continue immediately
			if(!this_thread_obj->thread_should_finish()) {
				// reduce system load and make other locks possible
				const size_t thread_delay = this_thread_obj->get_thread_delay();
				if(thread_delay > 0) {
					this_thread::sleep_for(chrono::milliseconds(thread_delay));
				}
				else {
					if(this_thread_obj->get_yield_after_run()) {
						// just yield when delay == 0 and "yield after run" flag is set
						this_thread::yield();
					}
				}
			}
		}
		else {
			if(this_thread_obj->get_yield_after_run()) {
				this_thread::yield();
			}
		}
		
		if(this_thread_obj->thread_should_finish()) {
			break;
		}
	}
	this_thread_obj->set_thread_status(THREAD_STATUS::FINISHED);
	
	return 0;
}

void thread_base::finish() {
	if(thread_obj != nullptr) {
		if((get_thread_status() == THREAD_STATUS::FINISHED || get_thread_status() == THREAD_STATUS::INIT) &&
		   !thread_obj->joinable()) {
			// already finished or uninitialized, nothing to do here
		}
		else {
			// signal thread to finish
			set_thread_should_finish();
			
			// wait a few ms
			this_thread::sleep_for(100ms);
			
			// this will block until the thread is finished
			if(thread_obj->joinable()) thread_obj->join();
		}
		
		// finally: kill the thread object
		thread_obj = nullptr;
	}
	// else: thread doesn't exist
	
	set_thread_status(THREAD_STATUS::FINISHED);
}

void thread_base::lock() {
	try {
		thread_lock.lock();
		thread_lock_count++;
	}
	catch(system_error& sys_err) {
		cout << "unable to lock thread: " << sys_err.code() << ": " << sys_err.what() << endl;
	}
	catch(...) {
		cout << "unable to lock thread" << endl;
	}
}

bool thread_base::try_lock() {
	try {
		const bool ret = thread_lock.try_lock();
		if(ret) thread_lock_count++;
		return ret;
	}
	catch(system_error& sys_err) {
		cout << "unable to try-lock thread: " << sys_err.code() << ": " << sys_err.what() << endl;
	}
	catch(...) {
		cout << "unable to try-lock thread" << endl;
	}
	return false;
}

void thread_base::unlock() {
	try {
		thread_lock.unlock();
		thread_lock_count--;
	}
	catch(system_error& sys_err) {
		cout << "unable to unlock thread: " << sys_err.code() << ": " << sys_err.what() << endl;
	}
	catch(...) {
		cout << "unable to unlock thread" << endl;
	}
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

const string& thread_base::get_thread_name() const {
	return thread_name;
}
