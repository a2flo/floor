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

#include <floor/threading/thread_base.hpp>
#include <floor/threading/thread_safety.hpp>
#include <string>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace fl {
using namespace std::literals;

//! single command completion thread (run/owned by the specified completion handler)
template <typename completion_handler_type, class command_buffer_completion_class>
class generic_cmd_completion_thread final : public thread_base, public command_buffer_completion_class {
public:
	generic_cmd_completion_thread(completion_handler_type& handler_, const std::string name) : thread_base(name), handler(handler_) {
		// never sleep or yield, will wait on "work_cv" in run()
		set_thread_delay(0u);
		set_yield_after_run(false);
	}
	~generic_cmd_completion_thread() override = default;
	
	void run() override {
		try {
			typename completion_handler_type::cmd_t cmd {};
			for (uint32_t run = 0; ; ++run) {
				{
					// wait until we have new work,
					// time out after 200ms in case everything is being shut down or halted
					std::unique_lock<std::mutex> work_lock_guard(handler.work_cv_lock);
					if (run == 0) {
						// if this is the first run/iteration, we haven't completed any work/cmd yet
						handler.work_cv.wait_for(work_lock_guard, 200ms);
						if (!handler.has_work()) {
							return; // -> return to thread_base and (potentially) run again
						}
					}
					// else: run 1+: just completed work, immediately retry to get new work w/o waiting on the CV
					else {
						// get work/cmd if there is any, otherwise return and retry
						if (!handler.has_work()) {
							return;
						}
					}
					cmd = handler.get_work();
				}
				
				// wait on cmd
				// TODO: do we rather want to have a back-off mechanism and handle multiple completions per thread?
				command_buffer_completion_class::complete(cmd);
			}
		} catch (std::exception& exc) {
			log_error("exception during $ work execution: $", thread_name, exc.what());
		}
	}
	
	//! make this public
	using thread_base::start;
	
protected:
	//! ref to the completion handler itself
	completion_handler_type& handler;
};

//! asynchronous command completion handler (runs command completion in separate threads)
template <typename cmd_type, class command_buffer_completion_class>
class generic_cmd_completion_handler {
public:
	//! this type
	using cmd_completion_handler_class = generic_cmd_completion_handler<cmd_type, command_buffer_completion_class>;
	//! externally specified command type that needs completion
	using cmd_t = cmd_type;
	//! worker/handler thread class
	using cmd_completion_thread_class = generic_cmd_completion_thread<cmd_completion_handler_class, command_buffer_completion_class>;
	
	generic_cmd_completion_handler() {
		cmd_completion_threads.resize(completion_thread_count);
		for (uint32_t i = 0; i < completion_thread_count; ++i) {
			cmd_completion_threads[i] = std::make_unique<cmd_completion_thread_class>(*this, "cmpl_hndlr_" + std::to_string(i));
			cmd_completion_threads[i]->start();
		}
	}
	
	~generic_cmd_completion_handler() {
		GUARD(cmd_completion_threads_lock);
		for (auto& th : cmd_completion_threads) {
			th->set_thread_should_finish();
		}
		work_cv.notify_all();
		for (auto& th : cmd_completion_threads) {
			th->finish();
		}
		cmd_completion_threads.clear();
	}
	
	void add_cmd_completion(cmd_type&& cmd) {
		{
			std::unique_lock<std::mutex> work_lock_guard(work_cv_lock);
			work_queue.emplace(std::move(cmd));
		}
		work_cv.notify_one();
	}
	
protected:
	friend cmd_completion_thread_class;
	
	//! max amount of completion threads that are created / will be running at most
	static constexpr const uint32_t completion_thread_count { 8 };
	
	//! access to "cmd_completion_threads" must be thread-safe
	safe_mutex cmd_completion_threads_lock;
	//! command completion threads
	std::vector<std::unique_ptr<cmd_completion_thread_class>> cmd_completion_threads GUARDED_BY(cmd_completion_threads_lock);
	
	//! required lock for "work_cv"
	std::mutex work_cv_lock;
	//! will be signaled once there is new work
	std::condition_variable work_cv;
	//! currently queued command completion work
	std::queue<cmd_type> work_queue;
	
	//! returns true if there is any work that needs to be handled
	//! NOTE: requires holding "work_cv_lock"
	bool has_work() {
		return !work_queue.empty();
	}
	
	//! returns and removes the cmd from the front of the work queue
	//! NOTE: requires holding "work_cv_lock"
	cmd_type get_work() {
		cmd_type cmd = std::move(work_queue.front());
		work_queue.pop();
		return cmd;
	}
};

} // namespace fl
