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

#ifndef __FLOOR_IRC_NET_HPP__
#define __FLOOR_IRC_NET_HPP__

#include "core/platform.hpp"
#include "net/net.hpp"
#include "net/net_protocol.hpp"
#include "net/net_tcp.hpp"

template <class protocol_policy, class ssl_protocol_policy> class irc_net {
public:
	irc_net() : plain_protocol(), ssl_protocol() {
		plain_protocol.set_max_packets_per_second(5);
		ssl_protocol.set_max_packets_per_second(5);
		this->set_thread_delay(100); // 100ms should suffice
	}
	
	void send(const string& data) {
		if(data.find("\n") != string::npos) {
			// treat \n as new msg, split string and send each line as new msg (with the same type -> string till first ':')
			size_t colon_pos = data.find(":");
			string msg_type = data.substr(0, colon_pos+1);
			
			size_t old_newline_pos = colon_pos;
			size_t newline_pos = 0;
			this->lock(); // lock before modifying send store
			while((newline_pos = data.find("\n", old_newline_pos+1)) != string::npos) {
				string msg = data.substr(old_newline_pos+1, newline_pos-old_newline_pos-1);
				send_store.push_back(msg_type + msg + "\n");
				old_newline_pos = newline_pos;
			}
			this->unlock();
		}
		// just send the msg
		else {
			this->lock(); // lock before modifying send store
			send_store.push_back(data + "\n");
			this->unlock();
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
	
protected:
	net<protocol_policy> plain_protocol;
	net<ssl_protocol_policy> ssl_protocol;
	
};

// floor_irc_net
typedef irc_net<TCP_protocol, TCP_ssl_protocol> floor_irc_net;

#endif
