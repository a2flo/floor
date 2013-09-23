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

#ifndef __FLOOR_NET_TCP_HPP__
#define __FLOOR_NET_TCP_HPP__

#include "core/platform.hpp"
#include "net/net_protocol.hpp"

// non-ssl and ssl specific implementation
namespace floor_net {
	template <bool use_ssl> struct protocol_details {};
	
	// non-ssl
	template <> struct protocol_details<false> {
		tcp::socket socket;
		protocol_details<false>(boost::asio::io_service& io_service) :
		socket(io_service) {
		}
		~protocol_details<false>() {
			socket.close();
		}
		
		bool handle_post_connect() { return true; }
	};
	
	// ssl
	template <> struct protocol_details<true> {
		boost::asio::ssl::context context;
		boost::asio::ssl::stream<tcp::socket> socket;
		protocol_details<true>(boost::asio::io_service& io_service) :
		context(boost::asio::ssl::context::tlsv12), socket(io_service, context) {
			context.set_default_verify_paths();
			socket.set_verify_mode(boost::asio::ssl::verify_peer);
			socket.set_verify_callback(boost::bind(&protocol_details<true>::verify_certificate, this, _1, _2));
		}
		~protocol_details<true>() {
			socket.shutdown();
		}
		
		//
		bool verify_certificate(bool preverified floor_unused, boost::asio::ssl::verify_context& ctx) {
			// TODO: actually verify cert
			// TODO: don't use deprecated/legacy functions
			char subject_name[256];
			X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
			X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
			log_msg("cert subject name: %s", subject_name);
			return true;
		}
		
		// TODO: client/server specific code (+option)
		bool handle_post_connect() {
			boost::system::error_code ec;
			socket.handshake(boost::asio::ssl::stream_base::client);
			if(ec) {
				log_error("handshake failed: %s", ec.message());
				return false;
			}
			return true;
		}
	};
};

// combined/common ssl and non-ssl protocol implementation
template<bool use_ssl> struct std_protocol<tcp::socket, use_ssl> {
public:
	std_protocol<tcp::socket, use_ssl>() :
	io_service(), resolver(io_service), data(io_service) {}
	
	bool is_valid() const {
		return (valid && ((socket_set && data.socket.is_open()) ||
						  !socket_set));
	}
	
	bool open_socket(const string& address, const unsigned short int& port) {
		if(!valid) return false;
		socket_set = true;
		
		boost::system::error_code ec;
		auto endpoint_iterator = resolver.resolve({ address, uint2string(port) });
		boost::asio::connect(data.socket, endpoint_iterator, ec);
		
		if(ec) {
			log_error("socket connection error: %s", ec.message());
			valid = false;
			return false;
		}
		if(!data.socket.is_open()) {
			log_error("couldn't open socket!");
			valid = false;
			return false;
		}
		
		//
		if(!data.handle_post_connect()) {
			valid = false;
			return false;
		}
		
		// set keep-alive flag (this only handles the simple cases and
		// usually has a big timeout value, but still better than nothing)
		boost::asio::socket_base::keep_alive option(true);
		data.socket.set_option(option);
		
		return true;
	}
	
	int receive(void* recv_data, const unsigned int max_len) {
		boost::system::error_code ec;
		auto data_received = data.socket.receive(boost::asio::buffer(recv_data, max_len), 0, ec);
		if(ec) {
			log_error("error while receiving data: %s", ec.message());
			valid = false;
			return 0;
		}
		return (int)data_received;
	}
	
	bool ready() const {
		if(!socket_set || !valid) return false;
		return (data.socket.available() > 0);
	}
	
	bool send(const char* send_data, const int len) {
		boost::system::error_code ec;
		const auto data_sent = boost::asio::write(data.socket, boost::asio::buffer(send_data, len), ec);
		if(ec) {
			log_error("error while sending data: %s", ec.message());
			valid = false;
			return false;
		}
		if(data_sent != (size_t)len) {
			log_error("error while sending data: sent data length (%u) != requested data length (%u)!",
					  data_sent, len);
			valid = false;
			return false;
		}
		return true;
	}
	
	boost::asio::ip::address get_local_address() const {
		return data.socket.local_endpoint().address();
	}
	unsigned short int get_local_port() const {
		return data.socket.local_endpoint().port();
	}
	
	boost::asio::ip::address get_remote_address() const {
		return data.socket.remote_endpoint().address();
	}
	unsigned short int get_remote_port() const {
		return data.socket.remote_endpoint().port();
	}
	
	void invalidate() {
		valid = false;
	}
	
protected:
	atomic<bool> socket_set { false };
	atomic<bool> valid { true };
	
	boost::asio::io_service io_service;
	tcp::resolver resolver;
	
	floor_net::protocol_details<use_ssl> data;
	
};

typedef std_protocol<tcp::socket, false> TCP_protocol;
typedef std_protocol<tcp::socket, true> TCP_ssl_protocol;

#endif
