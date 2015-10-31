/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/compute/metal/metal_queue.hpp>

#if !defined(FLOOR_NO_METAL)

metal_queue::metal_queue(shared_ptr<compute_device> device_, id <MTLCommandQueue> queue_) : compute_queue(device_), queue(queue_) {
}

void metal_queue::finish() const {
	// need to copy current set of command buffers, so we don't deadlock when removing completed command buffers
	vector<id <MTLCommandBuffer>> cur_cmd_buffers;
	{
		GUARD(cmd_buffers_lock);
		cur_cmd_buffers = cmd_buffers;
	}
	for(const auto& cmd_buffer : cur_cmd_buffers) {
		[cmd_buffer waitUntilCompleted];
	}
}

void metal_queue::flush() const {
	// need to copy current set of command buffers, so we don't deadlock when removing completed command buffers
	vector<id <MTLCommandBuffer>> cur_cmd_buffers;
	{
		GUARD(cmd_buffers_lock);
		cur_cmd_buffers = cmd_buffers;
	}
	for(const auto& cmd_buffer : cur_cmd_buffers) {
		[cmd_buffer waitUntilScheduled];
	}
}

const void* metal_queue::get_queue_ptr() const {
	return (__bridge void*)queue;
}

id <MTLCommandQueue> metal_queue::get_queue() {
	return queue;
}

id <MTLCommandBuffer> metal_queue::make_command_buffer() {
	id <MTLCommandBuffer> cmd_buffer = [queue commandBufferWithUnretainedReferences];
	[cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer> buffer) {
		GUARD(cmd_buffers_lock);
		const auto iter = find(cbegin(cmd_buffers), cend(cmd_buffers), buffer);
		if(iter == cend(cmd_buffers)) {
			log_error("failed to find metal command buffer %X!", buffer);
			return;
		}
		cmd_buffers.erase(iter);
	}];
	{
		GUARD(cmd_buffers_lock);
		cmd_buffers.push_back(cmd_buffer);
	}
	return cmd_buffer;
}

#endif
