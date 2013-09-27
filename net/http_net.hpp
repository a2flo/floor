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
template <class protocol_policy> class http_net : public net<protocol_policy> {
public:
	http_net();
	virtual ~http_net() {} 

	enum class HTTP_STATUS {
		NONE = 0,
		CODE_200 = 200,
		CODE_404 = 404
	};
	
	void open_url(const string& url, const size_t timeout = 30);
	virtual void run();
	
protected:
	size_t request_timeout { request_timeout };
	string server_name { "" };
	string server_url { "/" };
	string page_data { "" };
	bool header_read { false };
	deque<string>::iterator header_end;
	
	size_t start_time { 0 };
	
	HTTP_STATUS status_code { HTTP_STATUS::NONE };
	
	enum class PACKET_TYPE {
		NORMAL,
		CHUNKED
	};
	PACKET_TYPE packet_type { PACKET_TYPE::NORMAL };
	size_t header_length { 0 };
	size_t content_length { 0 };
	
	void check_header();
	void send_http_request(const char* url, const char* host);
	
	// seems like the compiler needs to know about these when using template inheritance
	using net<protocol_policy>::received_length;
	using net<protocol_policy>::receive_store;
	using net<protocol_policy>::send_store;
};

// floor_http_net/floor_https_net
typedef http_net<TCP_protocol> floor_http_net;
typedef http_net<TCP_ssl_protocol> floor_https_net;


template <class protocol_policy> http_net<protocol_policy>::http_net() : net<protocol_policy>() {
	this->set_thread_delay(20); // 20ms should suffice
}

template <class protocol_policy> void http_net<protocol_policy>::open_url(const string& url, const size_t timeout) {
	request_timeout = timeout;
	start_time = SDL_GetTicks();
	server_name = "";
	server_url = "/";
	
	// extract server name and server url from url
	string url_str = url;
	size_t sn_start = 0;
	size_t sn_end = url_str.length();
	
	const size_t http_pos = url_str.find("http://");
	if(http_pos != string::npos) {
		sn_start = http_pos+7;
	}
	else {
		// if http:// wasn't found, check for other ://
		if(url_str.find("://") != string::npos) {
			// if so, another protocol is requested, exit
			log_error("%s: invalid protocol!", url_str);
			this->set_thread_should_finish();
			return;
		}
	}
	
	// first slash
	const size_t slash_pos = url_str.find("/", sn_start);
	if(slash_pos != string::npos) {
		sn_end = slash_pos;
		server_url = url_str.substr(sn_end, url_str.length()-sn_end);
	}
	else {
		// direct/main request, use /
		server_url = "/";
	}
	
	server_name = url_str.substr(sn_start, sn_end-sn_start);
	
	// open connection
	unsigned short int port = 80;
	const size_t colon_pos = server_name.find(":");
	if(colon_pos != string::npos) {
		port = strtoul(server_name.substr(colon_pos+1, server_name.length()-colon_pos-1).c_str(), nullptr, 10);
		server_name = server_name.substr(0, colon_pos);
	}
	if(!this->connect_to_server(server_name.c_str(), port)) {
		log_error("couldn't connect to server %s!", server_name);
		this->set_thread_should_finish();
		return;
	}
	
	// finally, send the request
	send_http_request(server_url.c_str(), server_name.c_str());
}

template <class protocol_policy> void http_net<protocol_policy>::send_http_request(const char* url, const char* host) {
	stringstream packet;
	packet << "GET " << url << " HTTP/1.1" << endl;
	packet << "Accept-Charset: UTF-8" << endl;
	//packet << "Connection: close" << endl;
	packet << "User-Agent: floor" << endl; // TODO: version?
	packet << "Host: " << host << endl;
	packet << endl;
	
	this->send_packet(packet.str().c_str(), packet.str().length());
}

template <class protocol_policy> void http_net<protocol_policy>::run() {
	net<protocol_policy>::run();
	
	if(this->is_received_data()) {
		// first, try to get the header
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
		// if header was found previously, try to find the message end
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
	
	if((start_time + request_timeout*1000) < SDL_GetTicks()) {
		log_error("timeout for %s%s request!", server_name, server_url);
		this->set_thread_should_finish();
	}
}

template <class protocol_policy> void http_net<protocol_policy>::check_header() {
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
