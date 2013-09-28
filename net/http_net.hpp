/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#ifndef __FLOOR_HTTP_NET_HPP__
#define __FLOOR_HTTP_NET_HPP__

#include "core/platform.hpp"
#include "net/net.hpp"
#include "net/net_protocol.hpp"
#include "net/net_tcp.hpp"

// TODO: add events (for received packets and failed requests?)
class http_net : public thread_base {
public:
	static constexpr size_t default_timeout { 10 };

	enum class HTTP_STATUS {
		NONE = 0,
		CODE_200 = 200,
		CODE_404 = 404
	};
	// (this, return http status, server, data) -> success bool
	typedef std::function<bool(http_net*, HTTP_STATUS, const string&, const string&)> receive_functor;
	
	// construct the http_net object and only connect to server (no request is being sent)
	http_net(const string& server, const size_t timeout = default_timeout);
	// construct the http_net object, connect to the server and send a url request
	http_net(const string& server_url, receive_functor receive_cb, const size_t timeout = default_timeout);
	// deconstructer -> disconnect from the server
	virtual ~http_net();
	
	//
	void open_url(const string& url, receive_functor receive_cb, const size_t timeout = default_timeout);
	
	// tries to connect to the previously defined server again (note: connection must be closed before calling this)
	bool reconnect();
	
	//
	const string& get_server_name() const;
	const string& get_server_url() const;
	bool uses_ssl() const { return use_ssl; }
	
protected:
	net<TCP_protocol> plain_protocol;
	net<TCP_ssl_protocol> ssl_protocol;
	const bool use_ssl;
	
	receive_functor receive_cb;
	size_t request_timeout { default_timeout };
	string server_name { "" };
	string server_url { "/" };
	unsigned short int server_port { 80 };
	string page_data { "" };
	bool header_read { false };
	deque<string>::iterator header_end;
	
	size_t start_time { SDL_GetTicks() };
	
	HTTP_STATUS status_code { HTTP_STATUS::NONE };
	
	enum class PACKET_TYPE {
		NORMAL,
		CHUNKED
	};
	PACKET_TYPE packet_type { PACKET_TYPE::NORMAL };
	size_t header_length { 0 };
	size_t content_length { 0 };
	
	//
	virtual void run();
	void check_header();
	void send_http_request(const string& url, const string& host);
	
};

http_net::http_net(const string& server, const size_t timeout) :
thread_base("http"), use_ssl(server.size() >= 5 && server.substr(0, 5) == "https"), request_timeout(timeout) {
	this->set_thread_delay(20); // 20ms should suffice
	
	// kill the other protocol
	if(use_ssl) plain_protocol.set_thread_should_finish();
	else ssl_protocol.set_thread_should_finish();
	
	// check if the request is valid
	if(!(use_ssl ?
		 (server.size() >= 8 && server.substr(0, 8) == "https://") :
		 (server.size() >= 7 && server.substr(0, 7) == "http://"))) {
		throw floor_exception("invalid request: "+server);
	}
	
	// extract server name and server url from url
	const size_t server_name_start_pos = (use_ssl ? 8 : 7);
	size_t server_name_end_pos = server_name_start_pos;
	
	// first slash ("server" might be a complete url)
	const size_t slash_pos = server.find("/", server_name_start_pos);
	if(slash_pos != string::npos) {
		server_name_end_pos = slash_pos;
		server_url = server.substr(server_name_end_pos, server.size() - server_name_end_pos);
	}
	else {
		// direct/main request, use /
		server_url = "/";
	}
	server_name = server.substr(server_name_start_pos, server_name_end_pos - server_name_start_pos);
	
	// extract port number if specified
	server_port = (use_ssl ? 443 : 80);
	const size_t colon_pos = server_name.find(":");
	if(colon_pos != string::npos) {
		server_port = strtoul(server_name.substr(colon_pos + 1, server_name.size() - colon_pos - 1).c_str(), nullptr, 10);
		server_name = server_name.substr(0, colon_pos);
	}
	
	// finally: open connection
	if(!reconnect()) {
		throw floor_exception("couldn't connect to server: "+server);
	}
}

bool http_net::reconnect() {
	bool success = false;
	if(use_ssl) {
		success = ssl_protocol.connect_to_server(server_name, server_port);
	}
	else {
		success = plain_protocol.connect_to_server(server_name, server_port);
	}
	return success;
}

http_net::http_net(const string& server_url, receive_functor receive_cb_, const size_t timeout) : http_net(server_url, timeout) {
	// note: server_url has already been extracted in the delegated http_net constructor
	receive_cb = receive_cb_;
	send_http_request(server_url, server_name);
}

http_net::~http_net() {
	ssl_protocol.set_thread_should_finish();
	plain_protocol.set_thread_should_finish();
}

const string& http_net::get_server_name() const {
	return server_name;
}

const string& http_net::get_server_url() const {
	return server_url;
}

void http_net::open_url(const string& url, receive_functor receive_cb_, const size_t timeout) {
	// set the new server_url (note that any amount of urls can be requested consecutively on the same connection)
	server_url = url;
	request_timeout = timeout;
	receive_cb = receive_cb_;
	send_http_request(url, server_name);
}

void http_net::send_http_request(const string& url, const string& host) {
	stringstream packet;
	packet << "GET " << url << " HTTP/1.1" << endl;
	packet << "Accept-Charset: UTF-8" << endl;
	//packet << "Connection: close" << endl;
	packet << "User-Agent: floor" << endl; // TODO: version?
	packet << "Host: " << host << endl;
	packet << endl;
	
	if(use_ssl) ssl_protocol.send_data(packet.str());
	else plain_protocol.send_data(packet.str());
}

void http_net::run() {
	// timeout handling
	if((start_time + request_timeout * 1000) < SDL_GetTicks()) {
		log_error("timeout for %s%s request!", server_name, server_url);
		this->set_thread_should_finish();
	}
	
	// if there is no data to handle, return
	if(use_ssl && !ssl_protocol.is_received_data()) return;
	else if(!use_ssl && !plain_protocol.is_received_data()) return;
	
	// first, try to get the header
	const auto received_data = (use_ssl ? ssl_protocol.get_and_clear_received_data() : plain_protocol.get_and_clear_received_data());
	if(!header_read) {
		header_length = 0;
		for(auto line = receive_store.begin(); line != receive_store.end(); line++) {
			header_length += line->size() + 2; // +2 == CRLF
			// check for empty line
			if(line->length() == 0) {
				header_read = true;
				header_end = line;
				check_header();
				
				// remove header from receive store
				header_end++;
				receive_store.erase(receive_store.begin(), header_end);
				break;
			}
		}
	}
	// if header has been found previously, try to find the message end
	else {
		bool packet_complete = false;
		if(packet_type == http_net::PACKET_TYPE::NORMAL && content_length == (received_length - header_length)) {
			packet_complete = true;
			for(auto line = receive_store.begin(); line != receive_store.end(); line++) {
				page_data += *line + '\n';
			}
		}
		else if(packet_type == http_net::PACKET_TYPE::CHUNKED) {
			// note: this iterates over the receive store twice, once to check if all data was received and sizes are correct and
			// a second time to write the chunk data to page_data
			for(auto line = receive_store.begin(); line != receive_store.end(); line++) {
				// get chunk length
				size_t chunk_len = strtoull(line->c_str(), nullptr, 16);
				if(chunk_len == 0 && line->length() == 1) {
					if(packet_complete) break; // second run is complete, break
					packet_complete = true;
					
					// packet complete, start again, add data to page_data this time
					line = receive_store.begin();
					chunk_len = strtoull(line->c_str(), nullptr, 16);
				}
				
				size_t chunk_received_len = 0;
				while(++line != receive_store.end()) {
					// append chunk data
					if(packet_complete) page_data += *line + '\n';
					chunk_received_len += line->size();
					
					// check if complete chunk was received
					if(chunk_len == chunk_received_len) break;
					chunk_received_len++; // newline
				}
				
				if(line == receive_store.end()) break;
			}
		}
		
		if(packet_complete) {
			// TODO: add http-packet-received event
			
			// we're done here, clear and finish
			this->clear_received_data();
			this->set_thread_should_finish();
		}
	}
}

void http_net::check_header() {
	auto line = receive_store.begin();
	
	// first line contains status code
	const size_t space_1 = line->find(" ")+1;
	const size_t space_2 = line->find(" ", space_1);
	status_code = (HTTP_STATUS)strtoul(line->substr(space_1, space_2-space_1).c_str(), nullptr, 10);
	if(status_code != HTTP_STATUS::CODE_200) {
		log_error("%s%s: received status code %i!", server_name, server_url, status_code);
		this->set_thread_should_finish();
		return;
	}
	
	// continue ...
	for(line++; line != header_end; line++) {
		string line_str = str_to_lower(*line);
		if(line_str.find("transfer-encoding:") == 0) {
			if(line_str.find("chunked") != string::npos) {
				packet_type = http_net::PACKET_TYPE::CHUNKED;
			}
		}
		else if(line_str.find("content-length:") == 0) {
			// ignore content length if a chunked transfer-encoding was already specified (rfc2616 4.4.3)
			if(packet_type != http_net::PACKET_TYPE::CHUNKED) {
				packet_type = http_net::PACKET_TYPE::NORMAL;
				
				const size_t cl_space = line_str.find(" ")+1;
				size_t non_digit = line_str.find_first_not_of("0123456789", cl_space);
				if(non_digit == string::npos) non_digit = line_str.length();
				content_length = strtoull(line_str.substr(cl_space, non_digit-cl_space).c_str(), nullptr, 10);
			}
		}
	}
}

#endif
