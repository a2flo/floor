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

#include <floor/net/http_scheduler.hpp>

#if !defined(FLOOR_NO_NET)

bool http_scheduler::initialized = false;
http_scheduler* http_scheduler::hs = nullptr;

unordered_multimap<string, http_net*> http_scheduler::http_instances;
unordered_map<string, atomic<unsigned int>> http_scheduler::instances_status;
unordered_map<string, deque<pair<string, http_net::receive_functor>>> http_scheduler::requests;
unordered_map<http_net*, http_net::receive_functor> http_scheduler::cur_requests;
SDL_SpinLock http_scheduler::slock;

void http_scheduler::init() {
	if(!initialized && hs == nullptr) {
		hs = new http_scheduler();
		initialized = true;
	}
}

void http_scheduler::destroy() {
	if(initialized && hs != nullptr) {
		hs->set_thread_should_finish();
		SDL_Delay(500);
		delete hs;
		initialized = false;
	}
}

http_scheduler::~http_scheduler() {
	SDL_AtomicLock(&slock);

	for(auto& instance : http_instances) {
		delete instance.second;
	}

	http_instances.clear();
	requests.clear();
	SDL_AtomicUnlock(&slock);
}

void http_scheduler::add_instances(const string& server, const size_t& amount) {
	SDL_AtomicLock(&slock);

	if(instances_status.count(server) == 0) instances_status[server] = 0;

	for(size_t i = 0; i < amount; i++) {
		try {
			http_net* hn = new http_net(server);
			http_instances.emplace(server, hn);
			instances_status[server]++;
		}
		catch(floor_exception ex) {
			log_error("couldn't add server instance for server \"%s\": %s", server, ex.what());
		}
		catch(...) {
			log_error("couldn't add server instance for server \"%s\"!", server);
		}
	}

	SDL_AtomicUnlock(&slock);
}

void http_scheduler::add_request(const string& server, const string& url, http_net::receive_functor cb) {
	SDL_AtomicLock(&slock);
	requests[server].emplace_back(url, cb);
	SDL_AtomicUnlock(&slock);
}

void http_scheduler::run() {
	SDL_AtomicLock(&slock);

	// check instances status
	for(auto& status : instances_status) {
		// check if a socket is available and there is a request for that server
		if(status.second > 0 && requests.count(status.first) && requests[status.first].size() > 0) {
			// request a slot and check if the request was successful (>= 0!)
			if(((1 + status.second--) & 0x80000000) != 0) {
				status.second++; // reverse request
			}
			else {
				bool successful = false;
				for(const auto& instance : http_instances) {
					if(cur_requests.count(instance.second) == 0 && status.first == instance.first) {
						auto hn = instance.second;
						// remove request from request container
						auto req = requests[status.first][0];
						requests[status.first].pop_front();

						// do the request
						cur_requests[hn] = req.second;
						hn->open_url(req.first, bind(&http_scheduler::receive, hs, _1, _2, _3, _4));
						successful = true;
						break;
					}
				}
				if(!successful) {
					status.second++; // reverse request
				}
			}
		}
	}

	// check if all net instances are still running, if not, create a new instance
	for(const auto& instance : http_instances) {
		auto hn = instance.second;
		if(!hn->is_running()) {
			log_debug("http_net object (%s / %X) did shut down!", instance.first, instance.second);
			const string cur_url = hn->get_server_url();
			
			// finish old thread and start new one
			hn->finish();
			hn->restart();

			// reconnect
			if(hn->reconnect()) {
				log_debug("http_net object successfully reconnected to \"%s\"!", instance.first);
				if(cur_requests.count(hn) > 0 && cur_url != "") {
					log_debug("retrying \"%s\" request ...", cur_url);
					hn->open_url(cur_url, bind(&http_scheduler::receive, hs, _1, _2, _3, _4));
				}
				// else: no request going on?
			}
			else {
				log_error("http_net object (%s / %X) did shut down and reconnection failed!",
						  instance.first, instance.second);
			}
		}
	}

	SDL_AtomicUnlock(&slock);
}

bool http_scheduler::receive(http_net* hn, http_net::HTTP_STATUS status, const string& server, const string& data) {
	SDL_AtomicLock(&slock);

	if(cur_requests.count(hn) == 0) {
		log_error("invalid http class?");
		SDL_AtomicUnlock(&slock);
		return true;
	}

	// call original functor
	if(cur_requests[hn](hn, status, server, data)) {
		// make this http instance available again
		//cout << ":: available++ (" << hn->get_server_url() << ")" << endl;
		cur_requests.erase(hn);
		// we need to add the http:// or https:// again, b/c http_net strips it
		instances_status[(hn->uses_ssl() ? "https://" : "http://") + server]++;
	}
	else {
		// request was unsuccesful, try a retry
		//cout << ":: unsuccesful request, retry (" << hn->get_server_url() << ")" << endl;
		hn->open_url(hn->get_server_url(), bind(&http_scheduler::receive, hs, _1, _2, _3, _4));
	}

	SDL_AtomicUnlock(&slock);

	return true;
}

#endif
