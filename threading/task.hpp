/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_TASK_HPP__
#define __FLOOR_TASK_HPP__

#include <floor/core/essentials.hpp>
#include <thread>
#include <atomic>
#include <string>
#include <functional>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
using namespace std;

namespace task {

//! creates ("spawns") a new task that asynchronously executes the supplied function in a separate thread
//! NOTE: memory management of the task object is not necessary as it will automatically self-destruct
//! after completing the task op or after encountering an unhandled exception.
//! NOTE: example usage: task::spawn([]() { cout << "do something in here" << endl; });
static inline void spawn(std::function<void()> op, const string task_name = "task") {
	struct task {
		task(std::function<void()> op_, const string task_name_) :
		op(op_), task_name(task_name_),
		thread_obj(&task::run, this, [this]() {
			// the task thread is not allowed to run until the task thread object has been detached from the callers thread
			while(!initialized) { this_thread::yield(); }
			// finally: call the users task op
			op();
		}) {
			thread_obj.detach();
		}
		~task() = default;
		
		const std::function<void()> op;
		const string task_name;
		atomic<bool> initialized { false };
		thread thread_obj;
		
		unique_ptr<task> task_alloc;
		
		//! moves the outside task allocation to the interior and starts the task execution
		void set_alloc_and_start(unique_ptr<task>&& alloc) {
			task_alloc = move(alloc);
			task_alloc->initialized = true;
		}
		
		//! the tasks threads run method (only used internally)
		static void run(task* this_task, std::function<void()> task_op) {
			// the task thread is not allowed to run until the task thread object has been detached from the callers thread
			while(!this_task->initialized) { this_thread::yield(); }
			
			core::set_current_thread_name(this_task->task_name);
			
			try {
				// NOTE: this is the function object created above (not the users task op!)
				task_op();
			} catch (exception& exc) {
				log_error("encountered an unhandled exception while running task \"$\": $",
						  core::get_current_thread_name(), exc.what());
			} catch (...) {
				log_error("encountered an unhandled exception while running task \"$\"",
						  core::get_current_thread_name());
			}
			this_task->task_alloc = nullptr;
		}
		
		// prohibit copying
		task(const task& tsk) = delete;
		task& operator=(const task& tsk) = delete;
		
	};
	auto task_obj = make_unique<task>(op, task_name);
	auto task_ptr = task_obj.get();
	task_ptr->set_alloc_and_start(move(task_obj));
}

} // namespace task

#endif
