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
		tcp::socket& socket_layer; // ref to the actual socket layer
		protocol_details<false>(boost::asio::io_service& io_service) :
		socket(io_service), socket_layer(socket) {
		}
		~protocol_details<false>() {
			try {
				socket.close();
			}
			catch(...) {
				// catch uninitialized exceptions
			}
		}
		
		bool handle_post_client_connect() { return true; }
		bool handle_post_server_connect() { return true; }
	};
	
	// ignore ssl deprecation warnings on os x
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
	
	// ssl
	template <> struct protocol_details<true> {
		boost::asio::ssl::context context;
		boost::asio::ssl::stream<tcp::socket> socket;
		tcp::socket& socket_layer; // ref to the actual socket layer
		protocol_details<true>(boost::asio::io_service& io_service) :
		context(io_service, boost::asio::ssl::context::tlsv1), socket(io_service, context), socket_layer(socket.next_layer()) {
			context.set_default_verify_paths();
			socket.set_verify_mode(boost::asio::ssl::verify_peer);
			socket.set_verify_callback(boost::bind(&protocol_details<true>::verify_certificate, this, _1, _2));
		}
		~protocol_details<true>() {
			try {
				socket_layer.close();
				socket.shutdown();
			}
			catch(...) {
				// catch uninitialized exceptions
			}
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
		
		bool handle_post_client_connect() {
			boost::system::error_code ec;
			socket.handshake(boost::asio::ssl::stream_base::client);
			if(ec) {
				log_error("handshake failed: %s", ec.message());
				return false;
			}
			return true;
		}
		bool handle_post_server_connect() {
			boost::system::error_code ec;
			socket.handshake(boost::asio::ssl::stream_base::server);
			if(ec) {
				log_error("handshake failed: %s", ec.message());
				return false;
			}
			return true;
		}
	};
	
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

};

// combined/common ssl and non-ssl protocol implementation
template<bool use_ssl> struct std_protocol<tcp::socket, use_ssl> {
public:
	std_protocol<tcp::socket, use_ssl>() :
	io_service(), resolver(io_service), data(io_service) {}
	
	bool is_valid() const {
		return (valid && ((socket_set && data.socket_layer.is_open()) ||
						  !socket_set));
	}
	bool is_closed() const {
		return closed;
	}
	
	bool connect(const string& address, const unsigned short int& port) {
		if(!valid) return false;
		socket_set = true;
		
		boost::system::error_code ec;
		auto endpoint_iterator = resolver.resolve({ address, uint2string(port) });
		boost::asio::connect(data.socket_layer, endpoint_iterator, ec);
		
		if(ec) {
			log_error("socket connection error: %s", ec.message());
			valid = false;
			return false;
		}
		if(!data.socket_layer.is_open()) {
			log_error("couldn't open socket!");
			valid = false;
			return false;
		}
		
		//
		if(!data.handle_post_client_connect()) {
			valid = false;
			return false;
		}
		
		closed = false;
		
		// set keep-alive flag (this only handles the simple cases and
		// usually has a big timeout value, but still better than nothing)
		boost::asio::socket_base::keep_alive option(true);
		data.socket_layer.set_option(option);
		
		return true;
	}
	// TODO: server socket open
	
	size_t receive(void* recv_data, const size_t max_len) {
		boost::system::error_code ec;
		size_t data_received = data.socket.read_some(boost::asio::buffer(recv_data, max_len), ec);
		if(ec == boost::asio::error::eof) {
			valid = false;
			closed = true;
			return 0;
		}
		if(ec) {
			log_error("error while receiving data (received %u): %s", data_received, ec.message());
			valid = false;
			return 0;
		}
		return data_received;
	}
	
	bool ready() const {
		if(!socket_set || !valid) return false;
		return (data.socket_layer.available() > 0);
	}
	
	bool send(const char* send_data, const size_t len) {
		boost::system::error_code ec;
		const auto data_sent = boost::asio::write(data.socket, boost::asio::buffer(send_data, len), ec);
		if(ec == boost::asio::error::eof) {
			valid = false;
			closed = true;
			return 0;
		}
		if(ec) {
			log_error("error while sending data (sent %u): %s", data_sent, ec.message());
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
		return data.socket_layer.local_endpoint().address();
	}
	unsigned short int get_local_port() const {
		return data.socket_layer.local_endpoint().port();
	}
	
	boost::asio::ip::address get_remote_address() const {
		return data.socket_layer.remote_endpoint().address();
	}
	unsigned short int get_remote_port() const {
		return data.socket_layer.remote_endpoint().port();
	}
	
	void invalidate() {
		valid = false;
	}
	
protected:
	atomic<bool> socket_set { false };
	atomic<bool> valid { true };
	atomic<bool> closed { true };
	
	boost::asio::io_service io_service;
	tcp::resolver resolver;
	
	floor_net::protocol_details<use_ssl> data;
	
};

typedef std_protocol<tcp::socket, false> TCP_protocol;
typedef std_protocol<tcp::socket, true> TCP_ssl_protocol;

#endif
