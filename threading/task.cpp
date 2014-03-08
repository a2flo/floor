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

#include "task.hpp"
#include "core/logger.hpp"

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
#if defined(_PTHREAD_H)
	pthread_setname_np(this_task->task_name.c_str());
#endif
	
	try {
		// NOTE: this is the function object created above (not the users task op!)
		task_op();
	}
	catch(exception& exc) {
		log_error("encountered an unhandled exception while running a task: %s", exc.what());
	}
	catch(...) {
		log_error("encountered an unhandled exception while running a task");
	}
	delete this_task;
}
