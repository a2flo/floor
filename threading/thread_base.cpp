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

#include "thread_base.h"

thread_base::thread_base(const string name) : thread_name(name), thread_obj(nullptr), thread_lock(), thread_status(THREAD_STATUS::INIT), thread_delay(50) {
	this->lock(); // lock thread, so start (or unlock) must be called before the thread starts running
	thread_obj = new std::thread(&thread_base::_thread_run, this);
}

thread_base::~thread_base() {
	finish();
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
	thread_should_finish_flag = false;
	this->lock();
	thread_status = THREAD_STATUS::INIT;
	thread_obj = new thread(_thread_run, this);
	start();
}

int thread_base::_thread_run(thread_base* this_thread_obj) {
	while(true) {
		if(this_thread_obj->try_lock()) {
			this_thread_obj->run();
			this_thread_obj->unlock();
			
			// reduce system load and make other locks possible
			const size_t thread_delay = this_thread_obj->get_thread_delay();
			if(thread_delay > 0) {
				this_thread::sleep_for(chrono::milliseconds(thread_delay));
			}
			else this_thread::yield(); // just yield when delay == 0
		}
		else this_thread::yield();
		
		if(this_thread_obj->thread_should_finish()) {
			break;
		}
	}
	this_thread_obj->set_thread_status(THREAD_STATUS::FINISHED);
	
	return 0;
}

void thread_base::finish() {
	if(get_thread_status() == THREAD_STATUS::FINISHED && !thread_obj->joinable()) {
		return; // nothing to do here
	}
	
	// signal thread to finish
	set_thread_should_finish();
	
	// wait a few ms
	this_thread::sleep_for(chrono::milliseconds(100));
	
	// this will block until the thread is finished
	if(thread_obj->joinable()) thread_obj->join();
	
	set_thread_status(THREAD_STATUS::FINISHED);
}

void thread_base::lock() {
	try {
		thread_lock.lock();
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
		return thread_lock.try_lock();
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
	}
	catch(system_error& sys_err) {
		cout << "unable to unlock thread: " << sys_err.code() << ": " << sys_err.what() << endl;
	}
	catch(...) {
		cout << "unable to unlock thread" << endl;
	}
}

void thread_base::set_thread_status(const thread_base::THREAD_STATUS status) {
	this->lock();
	thread_status = status;
	this->unlock();
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

void thread_base::set_thread_delay(const unsigned int delay) {
	this->lock();
	thread_delay = delay;
	this->unlock();
}

size_t thread_base::get_thread_delay() const {
	return thread_delay;
}
