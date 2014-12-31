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

#ifndef __HTTP_SCHEDULER_HPP__
#define __HTTP_SCHEDULER_HPP__

#include <floor/net/net_protocol.hpp>
#include <floor/core/cpp_headers.hpp>
#include <floor/threading/thread_base.hpp>
#include <floor/net/net.hpp>
#include <floor/net/net_tcp.hpp>
#include <floor/net/http_net.hpp>

class http_scheduler : public thread_base {
public:
	static void init();
	static void destroy();

	static void add_instances(const string& server, const size_t& amount);
	static void add_request(const string& server, const string& url, http_net::receive_functor cb);

	virtual void run();

protected:
	static bool initialized;
	static http_scheduler* hs;

	static unordered_multimap<string, http_net*> http_instances;
	static unordered_map<string, atomic<unsigned int>> instances_status;
	static unordered_map<string, deque<pair<string, http_net::receive_functor>>> requests;
	static unordered_map<http_net*, http_net::receive_functor> cur_requests;

	static SDL_SpinLock slock;

	//
	bool receive(http_net* hn, http_net::HTTP_STATUS status, const string& server, const string& data);

	//
	http_scheduler() : thread_base("http_scheduler") {
		this->set_thread_delay(20);
		this->start();
	}
	virtual ~http_scheduler();
	http_scheduler& operator=(const http_scheduler& hs) = delete;

};

#endif
