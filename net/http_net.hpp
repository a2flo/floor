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

#ifndef __FLOOR_HTTP_NET_HPP__
#define __FLOOR_HTTP_NET_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_NET)

#include <floor/net/net_protocol.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/core.hpp>
#include <floor/net/net.hpp>
#include <floor/net/net_tcp.hpp>
#include <floor/floor/floor.hpp>

#define http_endl "\r\n"

class FLOOR_API http_net : public thread_base {
public:
	static constexpr size_t default_timeout { 10 };

	enum class HTTP_STATUS : unsigned int {
		NONE = 0,
		TIMEOUT = 1,
		CODE_100 = 100,
		CODE_200 = 200,
		CODE_404 = 404,
		CODE_599 = 599,
	};
	static constexpr const char* status_code_to_string(const HTTP_STATUS& code);
	
	// (this, return http status, server, data) -> success bool
	typedef std::function<bool(http_net*, HTTP_STATUS, const string&, const string&)> receive_functor;
	
	// construct the http_net object and only connect to server (no request is being sent)
	http_net(const string& server,
			 const size_t timeout = default_timeout, const bool continue_on_error_status = true);
	// construct the http_net object, connect to the server and send a url request
	http_net(const string& server_url, receive_functor receive_cb,
			 const size_t timeout = default_timeout, const bool continue_on_error_status = true);
	// deconstructer -> disconnect from the server
	virtual ~http_net();
	
	//
	void open_url(const string& url, receive_functor receive_cb,
				  const size_t timeout = default_timeout, const bool continue_on_error_status = true);
	
	// tries to connect to the previously defined server again (note: connection must be closed before calling this)
	bool reconnect();
	
	//
	const string& get_server_name() const;
	const string& get_server_url() const;
	bool uses_ssl() const { return use_ssl; }
	
protected:
	net<TCP_protocol, net_receive_raw> plain_protocol;
	net<TCP_ssl_protocol, net_receive_raw> ssl_protocol;
	const bool use_ssl;
	
	receive_functor receive_cb;
	size_t request_timeout { default_timeout };
	bool continue_on_error_status { true };
	string server_name { "" };
	string server_url { "/" };
	unsigned short int server_port { 80 };
	
	vector<string> receive_store;
	string page_data { "" };
	bool prev_crlf { false };
	bool header_read { false };
	void check_header(decltype(receive_store)::const_iterator header_end_iter);
	
	enum class PACKET_TYPE {
		NORMAL,
		CHUNKED
	};
	PACKET_TYPE packet_type { PACKET_TYPE::NORMAL };
	size_t header_length { 0 };
	size_t content_length { 0 };
	HTTP_STATUS status_code { HTTP_STATUS::NONE };
	size_t start_time { SDL_GetTicks() };
	
	//
	virtual void run();
	void send_http_request(const string& url, const string& host);
	
};

http_net::http_net(const string& server, const size_t timeout, const bool continue_on_error_status_) :
thread_base("http"), use_ssl(server.size() >= 5 && server.substr(0, 5) == "https"), request_timeout(timeout), continue_on_error_status(continue_on_error_status_) {
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
	size_t server_name_end_pos = server.size();
	
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
		server_port = (unsigned short int)strtoul(server_name.substr(colon_pos + 1, server_name.size() - colon_pos - 1).c_str(),
												  nullptr, 10);
		server_name = server_name.substr(0, colon_pos);
	}
	
	// finally: open connection ...
	if(!reconnect()) {
		throw floor_exception("couldn't connect to server: "+server);
	}
	
	// ... and start the worker thread
	this->start();
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

http_net::http_net(const string& server_url_, receive_functor receive_cb_,
				   const size_t timeout, const bool continue_on_error_status_) :
http_net(server_url_, timeout, continue_on_error_status_) {
	// note: server_url has already been extracted in the delegated http_net constructor
	receive_cb = receive_cb_;
	send_http_request(server_url, server_name);
}

http_net::~http_net() {
	lock(); // must be locked so that the worker thread won't be running
	set_thread_should_finish();
	
	// if status_code hasn't been changed (-> no other callback happened), call the callback function to signal destruction
	if(status_code == HTTP_STATUS::NONE) {
		receive_cb(this, HTTP_STATUS::NONE, server_name, "destructor");
	}
	
	unlock();
}

const string& http_net::get_server_name() const {
	return server_name;
}

const string& http_net::get_server_url() const {
	return server_url;
}

void http_net::open_url(const string& url, receive_functor receive_cb_, const size_t timeout, const bool continue_on_error_status_) {
	// set the new server_url (note that any amount of urls can be requested consecutively on the same connection)
	server_url = url;
	request_timeout = timeout;
	continue_on_error_status = continue_on_error_status_;
	receive_cb = receive_cb_;
	send_http_request(url, server_name);
}

void http_net::send_http_request(const string& url, const string& host) {
	stringstream packet;
	packet << "GET " << url << " HTTP/1.1" << http_endl;
	packet << "Accept-Charset: UTF-8" << http_endl;
	//packet << "Connection: close" << http_endl; // TODO: make this configurable
	packet << "User-Agent: " << floor::get_version() << http_endl; // TODO: make user-agant configurable
	packet << "Host: " << host << http_endl;
	packet << http_endl;
	
	if(use_ssl) ssl_protocol.send_data(packet.str());
	else plain_protocol.send_data(packet.str());
}

void http_net::run() {
	// timeout handling
	if((start_time + request_timeout * 1000) < SDL_GetTicks()) {
		if(status_code == HTTP_STATUS::NONE) {
			status_code = HTTP_STATUS::TIMEOUT;
		}
		log_error("timeout for %s%s request!", server_name, server_url);
		receive_cb(this, status_code, server_name, "timeout");
		this->set_thread_should_finish();
	}
	
	// if there is no data to handle, return
	if(use_ssl) {
		if(ssl_protocol.is_running() && !ssl_protocol.is_received_data()) return;
	}
	else {
		if(plain_protocol.is_running() && !plain_protocol.is_received_data()) return;
	}
	
	// get and process new data (first concat everything, then split after \r\n)
	auto received_data = (use_ssl ? ssl_protocol.get_and_clear_received_data() : plain_protocol.get_and_clear_received_data());
	string received_data_str = "";
	for(const auto& recv_elem : received_data) {
		received_data_str += string(recv_elem.data(), recv_elem.size());
	}
	vector<string> lines { core::tokenize(received_data_str, "\r\n") };
	
	// if the received data ends on "\r\n", tokenize will create an additional unwanted empty line -> remove it
	// also: ending data on "\r\n" will also signal the next received data not to append the first line to the last received line (see below)
	const size_t recv_lev = received_data_str.size();
	const bool cur_crlf = (recv_lev >= 2 && received_data_str[recv_lev - 2] == '\r' && received_data_str[recv_lev - 1] == '\n');
	if(cur_crlf) {
		lines.pop_back();
	}
	
	// insert new lines into receive store
	auto lines_begin = begin(lines);
	if(!receive_store.empty() && !prev_crlf) {
		// if the previous received data didn't end on "\r\n", add the first line to the last line/element of the receive store
		receive_store.back() += lines[0];
		lines_begin++; // insert from the next line onwards
	}
	receive_store.insert(end(receive_store), lines_begin, end(lines));
	prev_crlf = cur_crlf;
	
	// first, try to get the header
	if(!header_read) {
		header_length = 0;
		for(auto line_iter = cbegin(receive_store), end_iter = cend(receive_store); line_iter != end_iter; line_iter++) {
			header_length += line_iter->size() + 2; // +2 == CRLF
			// check for empty line
			if(line_iter->size() == 0) {
				header_read = true;
				check_header(line_iter);
				
				// remove header from receive store
				line_iter++;
				receive_store.erase(begin(receive_store), line_iter);
				break;
			}
		}
	}
	
	// if header has been found (previously or just now), try to find the message end
	if(header_read) {
		bool packet_complete = false;
		const auto received_length = (use_ssl ? ssl_protocol.get_received_length() : plain_protocol.get_received_length());
		if(packet_type == http_net::PACKET_TYPE::NORMAL && (received_length - header_length) == content_length) {
			packet_complete = true;
			for(const auto& line : receive_store) {
				page_data += line;
				page_data += "\r\n";
			}
			
			// reset received data counter
			if(use_ssl) ssl_protocol.subtract_received_length(content_length);
			else plain_protocol.subtract_received_length(content_length);
			// TODO: reset for chunked packets as well
		}
		else if(packet_type == http_net::PACKET_TYPE::CHUNKED) {
			// note: this iterates over the receive store twice, once to check if all data was received and sizes are correct and
			// a second time to write the chunk data to page_data
			for(auto line_iter = cbegin(receive_store), line_end = cend(receive_store); line_iter != line_end; line_iter++) {
				// get chunk length
				size_t chunk_len = (size_t)strtoull(line_iter->c_str(), nullptr, 16);
				if(chunk_len == 0 && line_iter->size() > 0) {
					if(packet_complete) break; // second run is complete, break
					packet_complete = true;
					
					// packet complete, start again, add data to page_data this time
					line_iter = cbegin(receive_store);
					chunk_len = (size_t)strtoull(line_iter->c_str(), nullptr, 16);
				}
				
				size_t chunk_received_len = 0;
				while(++line_iter != cend(receive_store)) {
					// append chunk data
					if(packet_complete) page_data += *line_iter + "\r\n";
					chunk_received_len += line_iter->size();
					
					// check if complete chunk was received
					if(chunk_len == chunk_received_len) break;
					chunk_received_len += 2; // newline / data \r\n (not part of the protocol)
				}
				
				if(line_iter == cend(receive_store)) break;
			}
		}
		
		if(packet_complete) {
			receive_cb(this, status_code, server_name, page_data);
			
			// we're done here, clear and finish
			this->set_thread_should_finish();
		}
	}
}

void http_net::check_header(decltype(receive_store)::const_iterator header_end_iter) {
	auto line = receive_store.begin();
	
	// first line contains status code
	const size_t space_1 = line->find(" ")+1;
	const size_t space_2 = line->find(" ", space_1);
	status_code = (HTTP_STATUS)strtoul(line->substr(space_1, space_2 - space_1).c_str(), nullptr, 10);
	if(status_code != HTTP_STATUS::CODE_200 && !continue_on_error_status) {
		receive_cb(this, status_code, server_name, page_data);
		this->set_thread_should_finish();
		return;
	}
	
	// continue ...
	for(line++; line != header_end_iter; line++) {
		if(line->find("Transfer-Encoding:") == 0) {
			if(line->find("chunked") != string::npos) {
				packet_type = http_net::PACKET_TYPE::CHUNKED;
			}
		}
		else if(line->find("Content-Length:") == 0) {
			// ignore content length if a chunked transfer-encoding was already specified (rfc2616 4.4.3)
			if(packet_type != http_net::PACKET_TYPE::CHUNKED) {
				packet_type = http_net::PACKET_TYPE::NORMAL;
				
				const size_t cl_space = line->find(" ") + 1;
				size_t non_digit = line->find_first_not_of("0123456789", cl_space);
				if(non_digit == string::npos) non_digit = line->size();
				content_length = (size_t)strtoull(line->substr(cl_space, non_digit - cl_space).c_str(), nullptr, 10);
			}
		}
	}
}

// TODO: when c++14 comes around, do this inside the constexpr (switch)
namespace floor_http_net {
	// https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
	// note: only standardized, rfc and widely used codes are included
	static constexpr array<array<const char*, 100>, 5> codes {{
		{{
			"Continue", "Switching Protocols", "Processing"
		}},
		{{
			"OK", "Created", "Accepted", "Non-Authoritative Information", "No Content",
			"Reset Content", "Partial Content", "Multi-Status", "Already Reported", "",
			"", "", "", "", "", "", "", "", "", "",
			"", "", "", "", "", "", "IM Used", "", "", "",
		}},
		{{
			"Multiple Choices", "Moved Permanently", "Found", "See Other", "Not Modified",
			"Use Proxy", "Switch Proxy", "Temporary Redirect", "Permanent Redirect", "",
		}},
		{{
			"Bad Request", "Unauthorized", "Payment Required", "Forbidden", "Not Found",
			"Method Not Allowed", "Not Acceptable", "Proxy Authentication Required", "Request Timeout", "Conflict",
			"Gone", "Length Required", "Precondition Failed", "Request Entity Too Large", "Request-URI Too Long",
			"Unsupported Media Type", "Requested Range Not Satisfiable", "Expectation Failed", "I'm a teapot", "Authentication Timeout",
			"", "", "Unprocessable Entity", "Locked", "Failed Dependency",
			"Unordered Collection", "Upgrade Required", "", "Precondition Required", "Too Many Requests",
			"", "Request Header Fields Too Large", "", "", "",
		}},
		{{
			"Internal Server Error", "Not Implemented", "Bad Gateway", "Service Unavailable", "Gateway Timeout",
			"HTTP Version Not Supported", "Variant Also Negotiates", "Insufficient Storage", "Loop Detected", "Bandwidth Limit Exceeded",
			"Not Extended", "Network Authentication Required", "", "", "",
			"", "", "", "", "",
			"", "", "Connection timed out", "", "",
		}},
	}};
};
constexpr const char* http_net::status_code_to_string(const HTTP_STATUS& code) {
	return (code >= HTTP_STATUS::CODE_100 && code <= HTTP_STATUS::CODE_599 ?
			floor_http_net::codes[((unsigned int)code - 100u) / 100u][(unsigned int)code - 100u * ((unsigned int)code / 100u)] :
			"invalid code");
}

#endif

#endif
