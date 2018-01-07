/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_NET)

#include <floor/net/net_protocol.hpp>
#include <floor/net/asio_error_handler.hpp>
#include <floor/core/platform.hpp>

// non-ssl and ssl specific implementation
namespace floor_net {
	template <bool use_ssl> struct protocol_details {};
	
	// non-ssl
	template <> struct protocol_details<false> {
		tcp::socket socket;
		tcp::socket& socket_layer; // ref to the actual socket layer
		protocol_details<false>(asio::io_service& io_service) :
		socket(io_service), socket_layer(socket) {
		}
		~protocol_details<false>() {
			socket.close();
			// ignore errors (socket might have been uninitialized)
			asio_error_handler::get_error();
		}
		
		bool handle_post_client_connect() { return true; }
		bool handle_post_server_connect() { return true; }
	};
	
	// ignore ssl deprecation warnings on os x
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(deprecated-declarations)
	
	// ssl
	template <> struct protocol_details<true> {
		asio::ssl::context context;
		asio::ssl::stream<tcp::socket> socket;
		tcp::socket& socket_layer; // ref to the actual socket layer
		protocol_details<true>(asio::io_service& io_service) :
		context(asio::ssl::context::tlsv12),
		socket(io_service, context), socket_layer(socket.next_layer()) {
			context.set_options(asio::ssl::context::no_compression |
								asio::ssl::context::default_workarounds |
								asio::ssl::context::no_sslv2 |
								asio::ssl::context::no_sslv3 |
								asio::ssl::context::no_tlsv1 |
								asio::ssl::context::no_tlsv1_1);
			context.set_default_verify_paths();
			
			if(asio_error_handler::is_error()) {
				log_error("error on setting context options: %s", asio_error_handler::handle_all());
			}
			
			// enable ecdh(e)
			SSL_CTX_set_ecdh_auto(context.native_handle(), true);
			
			// TODO: make this properly configurable
#if !defined(FLOOR_SSL_CIPHER_LIST)
			SSL_CTX_set_cipher_list(context.native_handle(), "ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA");
			SSL_set_cipher_list(socket.native_handle(), "ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA");
#else
#define FLOOR_SSL_CIPHER_LIST_STRINGIFY(ciphers) #ciphers
#define FLOOR_SSL_CIPHER_LIST_STR(ciphers) FLOOR_SSL_CIPHER_LIST_STRINGIFY(ciphers)
			SSL_CTX_set_cipher_list(context.native_handle(), FLOOR_SSL_CIPHER_LIST_STR(FLOOR_SSL_CIPHER_LIST));
			SSL_set_cipher_list(socket.native_handle(), FLOOR_SSL_CIPHER_LIST_STR(FLOOR_SSL_CIPHER_LIST));
#endif
			
			socket.set_verify_mode(asio::ssl::verify_peer);
			socket.set_verify_callback(bind(&protocol_details<true>::verify_certificate, this,
											std::placeholders::_1, std::placeholders::_2));
			
			if(asio_error_handler::is_error()) {
				log_error("error on setting socket options: %s", asio_error_handler::handle_all());
			}
		}
		~protocol_details<true>() {
			socket_layer.close();
			asio_error_handler::get_error(); // ignore errors
			socket.shutdown();
			asio_error_handler::get_error(); // ignore errors
		}
		
		//
		bool verify_certificate(bool preverified floor_unused, asio::ssl::verify_context& ctx) {
			// TODO: actually verify cert
			// TODO: don't use deprecated/legacy functions
			char subject_name[256];
			X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
			X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
			log_msg("cert subject name: %s", subject_name);
			return true;
		}
		
		bool handle_post_client_connect() {
			asio::error_code ec;
			socket.handshake(asio::ssl::stream_base::client, ec);
			if(ec) {
				log_error("handshake failed: %s", ec.message());
				return false;
			}
			if(asio_error_handler::is_error()) {
				log_error("handshake failed: %s", asio_error_handler::handle_all());
				return false;
			}
			return true;
		}
		bool handle_post_server_connect() {
			asio::error_code ec;
			socket.handshake(asio::ssl::stream_base::server, ec);
			if(ec) {
				log_error("handshake failed: %s", ec.message());
				return false;
			}
			if(asio_error_handler::is_error()) {
				log_error("handshake failed: %s", asio_error_handler::handle_all());
				return false;
			}
			return true;
		}
		
		const char* get_current_cipher() {
			return SSL_CIPHER_get_name(SSL_get_current_cipher(socket.native_handle()));
		}
	};
	
FLOOR_POP_WARNINGS()

};

// combined/common ssl and non-ssl protocol implementation
template<bool use_ssl> struct std_protocol<tcp::socket, use_ssl> {
public:
	std_protocol<tcp::socket, use_ssl>() :
	io_service(), resolver(io_service), acceptor(io_service), data(io_service) {}
	
	bool is_valid() const {
		return (valid && ((socket_set && data.socket_layer.is_open()) ||
						  !socket_set));
	}
	bool is_closed() const {
		return closed;
	}
	
	// client connect
	bool connect(const string& address, const uint16_t& port) {
		if(!valid) return false;
		socket_set = true;
		
		asio::error_code ec;
		auto endpoint_iterator = resolver.resolve({ address, to_string(port) });
		asio::connect(data.socket_layer, endpoint_iterator, ec);
		
		if(ec) {
			log_error("socket connection error: %s", ec.message());
			valid = false;
			return false;
		}
		if(asio_error_handler::is_error()) {
			log_error("socket connection error: %s", asio_error_handler::handle_all());
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
		asio::socket_base::keep_alive option(true);
		data.socket_layer.set_option(option);
		
		return true;
	}
	
	// server listen/open
	bool listen(const string& address, const uint16_t& port) {
		if(!valid) return false;
		socket_set = true;
		
		asio::error_code ec;
		tcp::endpoint endpoint = *resolver.resolve({ address, to_string(port) });
		acceptor.open(endpoint.protocol(), ec);
		if(ec) {
			log_error("couldn't open server socket: %s", ec.message());
			valid = false;
			return false;
		}
		if(asio_error_handler::is_error()) {
			log_error("couldn't open server socket: %s", asio_error_handler::handle_all());
			valid = false;
			return false;
		}
		
		acceptor.set_option(tcp::acceptor::reuse_address(true));
		acceptor.bind(endpoint, ec);
		if(ec) {
			log_error("couldn't bind to endpoint: %s", ec.message());
			valid = false;
			return false;
		}
		if(asio_error_handler::is_error()) {
			log_error("couldn't bind to endpoint: %s", asio_error_handler::handle_all());
			valid = false;
			return false;
		}
		
		acceptor.listen();
		if(asio_error_handler::is_error()) {
			log_error("acceptor failed to listen: %s", asio_error_handler::handle_all());
			valid = false;
			return false;
		}
		return true;
	}
	
	size_t receive(void* recv_data, const size_t max_len) {
		asio::error_code ec;
		size_t data_received = data.socket.read_some(asio::buffer(recv_data, max_len), ec);
		if(ec == asio::error::eof) {
			valid = false;
			closed = true;
			return 0;
		}
		if(ec) {
			log_error("error while receiving data (received %u): %s", data_received, ec.message());
			valid = false;
			return 0;
		}
		if(asio_error_handler::is_error()) {
			log_error("error while receiving data (received %u): %s", data_received, asio_error_handler::handle_all());
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
		asio::error_code ec;
		const auto data_sent = asio::write(data.socket, asio::buffer(send_data, len), ec);
		if(ec == asio::error::eof) {
			valid = false;
			closed = true;
			return 0;
		}
		if(ec) {
			log_error("error while sending data (sent %u): %s", data_sent, ec.message());
			valid = false;
			return false;
		}
		if(asio_error_handler::is_error()) {
			log_error("error while sending data (sent %u): %s", data_sent, asio_error_handler::handle_all());
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
	
	asio::ip::address get_local_address() const {
		return data.socket_layer.local_endpoint().address();
	}
	uint16_t get_local_port() const {
		return data.socket_layer.local_endpoint().port();
	}
	
	asio::ip::address get_remote_address() const {
		return data.socket_layer.remote_endpoint().address();
	}
	uint16_t get_remote_port() const {
		return data.socket_layer.remote_endpoint().port();
	}
	
	void invalidate() {
		valid = false;
	}
	
	//! returns the protocol_details object containing the socket and context
	floor_net::protocol_details<use_ssl>& get_protocol_details() {
		return data;
	}
	
	auto& get_socket() {
		return data.socket_layer;
	}
	
	// allow direct access to these (use with caution)
	asio::io_service io_service;
	tcp::resolver resolver;
	tcp::acceptor acceptor;
	
protected:
	atomic<bool> socket_set { false };
	atomic<bool> valid { true };
	atomic<bool> closed { true };
	
	floor_net::protocol_details<use_ssl> data;
	
};

typedef std_protocol<tcp::socket, false> TCP_protocol;
typedef std_protocol<tcp::socket, true> TCP_ssl_protocol;

#endif

#endif
