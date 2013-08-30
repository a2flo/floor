/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef __FLOOR_NET_HPP__
#define __FLOOR_NET_HPP__

#include "platform.h"
#include "event_handler.h"
#include "net_protocol.h"
#include "net_tcp.h"
#include "config.h"
#include "logger.h"
#include "threading/thread_base.h"

#define PACKET_MAX_LEN 65536

template <class protocol_policy> class net : public thread_base {
public:
	net(config* conf);
	virtual ~net();
	
	virtual void run();
	virtual bool connect_to_server(const char* server_name, const unsigned short int port, const unsigned short int local_port = 65535);
	
	virtual bool is_received_data() const;
	virtual deque<string> get_and_clear_received_data();
	virtual void clear_received_data();
	
	virtual boost::asio::ip::address get_local_address() const;
	virtual unsigned short int get_local_port() const;
	virtual boost::asio::ip::address get_remote_address() const;
	virtual unsigned short int get_remote_port() const;
	
	virtual const protocol_policy& get_protocol() const;
	virtual protocol_policy& get_protocol();

protected:
	config* conf;
	protocol_policy protocol;
	atomic<bool> connected { false };
	
	string last_packet_remains;
	size_t received_length;
	size_t packets_per_second;
	size_t last_packet_send;
	deque<string> receive_store;
	deque<string> send_store;
	
	char receive_data[PACKET_MAX_LEN];
	virtual int receive_packet(char* data, const unsigned int max_len);
	virtual void send_packet(const char* data, const unsigned int len);
	virtual int process_packet(const string& data, const unsigned int max_len);
	
};

template <class protocol_policy> net<protocol_policy>::net(config* conf) :
thread_base(), conf(conf), protocol(), last_packet_remains(""), received_length(0), packets_per_second(0), last_packet_send(0) {
	unibot_debug("net initialized!");
	this->start(); // start thread
}

template <class protocol_policy> net<protocol_policy>::~net() {
	unibot_debug("net deleted!");
}

template <class protocol_policy>
bool net<protocol_policy>::connect_to_server(const char* server_name,
											 const unsigned short int port,
											 const unsigned short int local_port unibot_unused) {
	lock(); // we need to lock the net class, so run() isn't called while we're connecting
	
	try {
		if(!protocol.is_valid()) throw exception();
		
		// create server socket
		if(!protocol.open_socket(server_name, port)) throw exception();
		
		// connection created - data transfer is now possible
		unibot_debug("successfully connected to server!");
		connected = true;
	}
	catch(...) {
		unibot_error("failed to connect to server!");
		unlock();
		set_thread_should_finish(); // and quit ...
		return false;
	}
	
	unlock();
	return true;
}

template <class protocol_policy> void net<protocol_policy>::run() {
	if(!connected) return;
	
	// receive data - if possible
	int len = 0, used = 0;
	try {
		if(protocol.is_valid()) {
			if(protocol.ready()) {
				//cout << "server_ready" << endl;
				memset(receive_data, 0, PACKET_MAX_LEN);
				len = receive_packet(receive_data, PACKET_MAX_LEN);
				if(len <= 0) {
					// failure, end bot
					throw exception();
				}
				else {
					string data = receive_data;
					if(last_packet_remains.length() > 0) {
						data = last_packet_remains + data;
						len += (int)last_packet_remains.length();
						last_packet_remains = "";
					}
					
					used = process_packet(data, len);
					
					len -= used;
					if(used == 0 || len > 0) {
						last_packet_remains = data.substr(used, len);
					}
				}
			}
		}
		else throw runtime_error("invalid protocol");
	}
	catch(exception& e) {
		unibot_error("net error: %s", e.what());
		set_thread_should_finish();
		return;
	}
	catch(...) {
		// something is wrong, finsh and return
		unibot_error("unknown net error, exiting ...");
		set_thread_should_finish();
		return;
	}
	
	// send data - if possible
	if(!send_store.empty()) {
		deque<string>::iterator send_end = send_store.end();
		if(packets_per_second != 0 && last_packet_send > SDL_GetTicks()-1000) {
			// wait
		}
		else {
			if(packets_per_second != 0 && send_store.size() > packets_per_second) {
				send_end = send_store.begin()+packets_per_second;
			}
			
			for(auto send_iter = send_store.begin(); send_iter != send_end; send_iter++) {
				send_packet(send_iter->c_str(), (int)send_iter->length());
			}
			if(packets_per_second != 0) last_packet_send = SDL_GetTicks();
			
			send_store.erase(send_store.begin(), send_end);
		}
	}
}

template <class protocol_policy> int net<protocol_policy>::receive_packet(char* data, const unsigned int max_len) {
	if(!protocol.is_valid()) {
		unibot_error("invalid protocol and/or sockets!");
		return -1;
	}
	
	// receive the package
	int len = protocol.receive(data, max_len);
	// received packet length is equal or less than zero, return -1
	if(len <= 0) {
		unibot_error("invalid data received!");
		return -1;
	}
	
	return len;
}

template <class protocol_policy> int net<protocol_policy>::process_packet(const string& data, const unsigned int max_len unibot_unused) {
	size_t old_pos = 0, pos = 0;
	size_t lb_offset = 1;
	const size_t len = data.length();
	for(;;) {
		// handle \n and \r\n newlines
		if((pos = data.find("\r", old_pos)) == string::npos) {
			if((pos = data.find("\n", old_pos)) == string::npos) {
				break;
			}
			else lb_offset = 1;
		}
		else {
			if(pos+1 >= len) {
				// \n not received yet
				break;
			}
			if(data[pos+1] != '\n') {
				// \r must be followed by \n!
				break;
			}
			pos++;
			lb_offset = 2;
		}
		
		receive_store.push_back(data.substr(old_pos, pos - old_pos - lb_offset + 1));
		//cout << "received (" << old_pos << "/" << (pos - old_pos) << "/" << data.length() << "): " << receive_store.back() << endl;
		old_pos = pos + 1;
	}
	//cout << ":: end " << old_pos << endl;
	
	received_length += old_pos;
	return (int)old_pos;
}

template <class protocol_policy> void net<protocol_policy>::send_packet(const char* data, const unsigned int len) {
	if(!protocol.send(data, len)) {
		unibot_error("couldn't send packet!");
	}
	else if(conf->get_verbosity() >= logger::LOG_TYPE::DEBUG_MSG) {
		unibot_debug("send (%i): %s", len, string(data).substr(0, len-1));
	}
}

template <class protocol_policy> bool net<protocol_policy>::is_received_data() const {
	return !receive_store.empty();
}

template <class protocol_policy> deque<string> net<protocol_policy>::get_and_clear_received_data() {
	deque<string> ret;
	this->lock();
	std::swap(ret, receive_store);
	receive_store.clear();
	this->unlock();
	return ret;
}

template <class protocol_policy> void net<protocol_policy>::clear_received_data() {
	receive_store.clear();
}

template <class protocol_policy> boost::asio::ip::address net<protocol_policy>::get_local_address() const {
	return protocol.get_local_address();
}
template <class protocol_policy> unsigned short int net<protocol_policy>::get_local_port() const {
	return protocol.get_local_port();
}

template <class protocol_policy> boost::asio::ip::address net<protocol_policy>::get_remote_address() const {
	return protocol.get_remote_address();
}
template <class protocol_policy> unsigned short int net<protocol_policy>::get_remote_port() const {
	return protocol.get_remote_port();
}

template <class protocol_policy> const protocol_policy& net<protocol_policy>::get_protocol() const {
	return protocol;
}

template <class protocol_policy> protocol_policy& net<protocol_policy>::get_protocol() {
	return protocol;
}

#endif
