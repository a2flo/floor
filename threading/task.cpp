/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#include <floor/threading/task.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>

task::task(std::function<void()> op_, const string task_name_) :
op(op_), task_name(task_name_),
thread_obj(&task::run, this, [this]() {
	// the task thread is not allowed to run until the task thread object has been detached from the callers thread
	while(!initialized) { this_thread::yield(); }
	// finally: call the users task op
	op();
}) {
	thread_obj.detach();
	initialized = true;
}

void task::run(task* this_task, std::function<void()> task_op) {
	// the task thread is not allowed to run until the task thread object has been detached from the callers thread
	while(!this_task->initialized) { this_thread::yield(); }
	
	core::set_current_thread_name(this_task->task_name);
	
#if !defined(FLOOR_NO_EXCEPTIONS)
	try {
#endif
		// NOTE: this is the function object created above (not the users task op!)
		task_op();
#if !defined(FLOOR_NO_EXCEPTIONS)
	}
	catch(exception& exc) {
		log_error("encountered an unhandled exception while running task \"%s\": %s",
				  core::get_current_thread_name(), exc.what());
	}
	catch(...) {
		log_error("encountered an unhandled exception while running task \"%s\"",
				  core::get_current_thread_name());
	}
#endif
	delete this_task;
}
