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

#include <floor/core/essentials.hpp>
#include <thread>
#include <mutex>
#include <string>
#include <atomic>
#include <memory>
#include <floor/threading/thread_safety.hpp>

namespace fl {

//! if you need a class that should be executed in a separate thread, you can use this simple base class to do so.
//! usage: inherit from this class, add a virtual destructor, override the run() method and call start(). call finish()
//! to finish/end the thread execution. look up the documentation for each of these calls.
//! NOTE: for simpler "execute once in a separate thread" things, it might be easier to use a task (-> task.hpp).
class thread_base {
public:
	//! constructs a thread_base object, locks the thread lock and creates a std::thread object that will
	//! execute the actual "thread code" (the user supplied run method).
	//! NOTE: you can name the thread by supplying a name parameter (this might be helpful when debugging)
	thread_base(const std::string name = "unknown");
	//! destroys the thread (also makes sure that thread has finished execution and locks have been unlocked)
	//! NOTE: remember: all inheriting destructors must be virtual as well!
	virtual ~thread_base();
	
	//! determines the status of the thread
	enum class THREAD_STATUS : int {
		INVALID = -1,	//!< the thread is in an invalid state (do panic!)
		INIT = 0,		//!< the thread is currently being initialized (start hasn't been called yet)
		RUNNING = 1,	//!< start has been called and the thread is still running
		PAUSED = 2,		//!< the thread has been paused
		FINISHED = 3,	//!< the thread has finished execution
	};
	
	//! this is the main run method of the thread, which must be overridden by a custom run method.
	//! NOTE: an infinite loop inside the run method is not needed and highly discouraged, as this will
	//! interfere with the thread communication and render all thread_base functions useless.
	//! the run() method will be called continuously from inside thread_base while making sure that
	//! all thread communication is being processed (finish execution, thread delay, pausing, ...).
	virtual void run() = 0;
	
	//! pauses/halts the thread prior to the next run() iteration
	//! NOTE: this is only viable if run() ever returns
	void pause();
	
	//! unpauses a previously halted thread
	void unpause();
	
	//! this signals the thread to finish its execution and will actually finish after the current or next
	//! run() call has returned. after that, it will kill the thread object and set the state to FINISHED.
	//! NOTE: this is a blocking call that won't return until the thread has been joined (or immediately returns
	//! if the thread object doesn't exist, the thread is still in its initialized state or has already finished)
	void finish();
	
	//! sets the thread status (this should usually not be called from the outside)
	void set_thread_status(const thread_base::THREAD_STATUS status);
	//! returns the current thread status
	THREAD_STATUS get_thread_status() const;
	
	//! shortcut for "get_thread_status() == (RUNNING || INIT)"
	bool is_running() const;
	
	//! this signals the thread to finish its execution
	void set_thread_should_finish();
	//! returns true if the "thread should finish" flag has been set
	bool thread_should_finish();
	
	//! sets the delay (sleep time) in milliseconds that the thread will sleep after each run call
	//! (49 days ought to be enough for anybody).
	//! NOTE: you can disable this by setting the delay to 0
	void set_thread_delay(const size_t delay);
	//! returns the delay (sleep time) in milliseconds
	size_t get_thread_delay() const;
	
	//! if the thread delay is 0 (or the thread couldn't get the lock when/before trying to call the run method),
	//! this flag determines if "std::this_thread::yield()" will be called
	void set_yield_after_run(const bool state);
	//! returns the "yield after run" flag
	bool get_yield_after_run() const;
	
	//! returns the given name of the thread
	const std::string& get_thread_name() const;
	
protected:
	const std::string thread_name;
	std::unique_ptr<std::thread> thread_obj { nullptr };
	std::atomic<THREAD_STATUS> thread_status { THREAD_STATUS::INIT };
	std::atomic<size_t> thread_delay { 50 };
	std::atomic<bool> thread_should_finish_flag { false };
	std::atomic<bool> yield_after_run { true };
	std::atomic<bool> thread_pause { false };
	std::condition_variable pause_cv;
	std::condition_variable delay_cv;
	
	//! this _must_ be called from the inheriting class to actually start the thread
	void start();
	//! the threads internal run method (only used internally)
	static int _thread_run(thread_base* this_thread_obj);
	
	// prohibit copying
	thread_base(const thread_base& tb);
	void operator=(const thread_base& tb);
};

} // namespace fl
