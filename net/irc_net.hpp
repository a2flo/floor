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

#ifndef __FLOOR_IRC_NET_HPP__
#define __FLOOR_IRC_NET_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_NET)

#include <floor/core/platform.hpp>
#include <floor/net/net.hpp>
#include <floor/net/net_protocol.hpp>
#include <floor/net/net_tcp.hpp>

template <class protocol_policy, class ssl_protocol_policy> class irc_net {
public:
	irc_net(const bool& use_ssl_) : plain_protocol(), ssl_protocol(), use_ssl(use_ssl_) {
		plain_protocol.set_max_packets_per_second(5);
		ssl_protocol.set_max_packets_per_second(5);
		plain_protocol.set_thread_delay(100); // 100ms should suffice
		ssl_protocol.set_thread_delay(100);
		
		// kill the other protocol
		if(use_ssl) plain_protocol.set_thread_should_finish();
		else ssl_protocol.set_thread_should_finish();
	}
	
	bool is_ssl() const {
		return use_ssl;
	}
	
	void send(const string& data) {
		if(data.find("\n") != string::npos) {
			// treat \n as new msg, split string and send each line as new msg (with the same type -> string till first ':')
			size_t colon_pos = data.find(":");
			string msg_type = data.substr(0, colon_pos+1);
			
			size_t old_newline_pos = colon_pos;
			size_t newline_pos = 0;
			vector<vector<char>> packets;
			while((newline_pos = data.find("\n", old_newline_pos+1)) != string::npos) {
				string msg = msg_type + data.substr(old_newline_pos+1, newline_pos-old_newline_pos-1) + "\n";
				packets.emplace_back(begin(msg), end(msg));
				old_newline_pos = newline_pos;
			}
			if(use_ssl) ssl_protocol.send_data(packets);
			else plain_protocol.send_data(packets);
		}
		// just send the msg
		else {
			if(use_ssl) ssl_protocol.send_data(data + "\n");
			else plain_protocol.send_data(data + "\n");
		}
	}
	
	void send_channel_msg(const string& channel, const string& msg) {
		send("PRIVMSG " + channel + " :" + msg);
	}
	void send_private_msg(const string& where, const string& msg) {
		send("PRIVMSG " + where + " :" + msg);
	}
	void send_action_msg(const string& where, const string& msg) {
		send("PRIVMSG " + where + " :" + (char)0x01 + "ACTION " + msg + (char)0x01);
	}
	void send_ctcp_msg(const string& where, const string& type, const string& msg) {
		send("NOTICE " + where + " :" + (char)0x01 + type + " " + msg + (char)0x01);
	}
	void send_ctcp_request(const string& where, const string& type) {
		send("PRIVMSG " + where + " :" + (char)0x01 + type + (char)0x01);
	}
	void send_kick(const string& channel, const string& who, const string& reason) {
		send("KICK " + channel + " " + who + " :" + reason);
	}
	void send_connect(const string& name, const string& real_name) {
		send_nick(name);
		send("USER " + name + " 0 * :" + real_name);
	}
	void send_identify(const string& password) {
		send_private_msg("NickServ", "identify " + password);
	}
	void send_nick(const string& nick) {
		send("NICK " + nick);
	}
	void part(const string& channel) {
		send("PART " + channel + " :EOL");
	}
	void quit() {
		send("QUIT :EOL");
	}
	void join_channel(const string& channel) {
		send("JOIN " + channel);
	}
	void ping(const string& server_name) {
		send("PING " + server_name);
	}
	
	void invalidate() {
		if(use_ssl) ssl_protocol.invalidate();
		else plain_protocol.invalidate();
	}
	
	// protocol passthrough functions
	bool connect_to_server(const string& server_name,
						   const uint16_t port) {
		return (use_ssl ? ssl_protocol.connect_to_server(server_name, port) : plain_protocol.connect_to_server(server_name, port));
	}
	bool is_running() const {
		return (use_ssl ? ssl_protocol.is_running() : plain_protocol.is_running());
	}
	bool is_received_data() const {
		return (use_ssl ? ssl_protocol.is_received_data() : plain_protocol.is_received_data());
	}
	void clear_received_data() {
		(use_ssl ? ssl_protocol.clear_received_data() : plain_protocol.clear_received_data());
	}
	deque<vector<char>> get_and_clear_received_data() {
		return (use_ssl ? ssl_protocol.get_and_clear_received_data() : plain_protocol.get_and_clear_received_data());
	}
	asio::ip::address get_local_address() const {
		return (use_ssl ? ssl_protocol.get_local_address() : plain_protocol.get_local_address());
	}
	uint16_t get_local_port() const {
		return (use_ssl ? ssl_protocol.get_local_port() : plain_protocol.get_local_port());
	}
	asio::ip::address get_remote_address() const {
		return (use_ssl ? ssl_protocol.get_remote_address() : plain_protocol.get_remote_address());
	}
	uint16_t get_remote_port() const {
		return (use_ssl ? ssl_protocol.get_remote_port() : plain_protocol.get_remote_port());
	}
	
	//
	net<protocol_policy, net_receive_split_on_newline>& get_plain_protocol() { return plain_protocol; }
	net<ssl_protocol_policy, net_receive_split_on_newline>& get_ssl_protocol() { return ssl_protocol; }
	
protected:
	net<protocol_policy, net_receive_split_on_newline> plain_protocol;
	net<ssl_protocol_policy, net_receive_split_on_newline> ssl_protocol;
	const bool use_ssl;
	
};

// floor_irc_net
typedef irc_net<TCP_protocol, TCP_ssl_protocol> floor_irc_net;

#endif

#endif
