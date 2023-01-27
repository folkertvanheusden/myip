// (C) 2023 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0

#include <errno.h>
#include <map>
#include <mutex>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <thread>

#include "hash.h"
#include "ipv4.h"
#include "log.h"
#include "stats_tracker.h"
#include "stats_utils.h"
#include "str.h"
#include "tcp.h"
#include "types.h"
#include "utils.h"


using namespace std::chrono_literals;

class person
{
public:
	std::string           real_name;
	std::set<std::string> channels;
	session              *tcp_session { nullptr };
};

static std::map<std::string, person> nicknames;
static std::mutex nicknames_lock;

static std::map<std::string, std::string> topicnames;
static std::mutex topicnames_lock;

void irc_init()
{
}

void irc_deinit()
{
}

// I.S.: Internet Session
typedef enum { IS_wait_nick, IS_wait_user, IS_running, IS_disconnect } irc_state_t;

void transmit_to_channel(const std::string & channel, const std::string & msg_line)
{
	for(auto & nick : nicknames) {
		if (nick.second.channels.find(channel) != nick.second.channels.end() || channel == nick.first)
			nick.second.tcp_session->get_stream_target()->send_data(nick.second.tcp_session, reinterpret_cast<const uint8_t *>(msg_line.c_str()), msg_line.size());
	}
}

void send_user_for_channel(const std::string & channel)
{
	std::string out   = ": 353 " + channel + " :";
	bool        first = true;

	for(auto & nick : nicknames) {
		if (nick.second.channels.find(channel) != nick.second.channels.end()) {
			if (first)
				first = false;
			else
				out += " ";

			out += nick.first;
		}
	}

	out += "\r\n";

	out += ": 366 * " + channel + " :End of /NAMES list.\r\n";

	for(auto & nick : nicknames) {
		if (nick.second.channels.find(channel) != nick.second.channels.end()) {
			nick.second.tcp_session->get_stream_target()->send_data(nick.second.tcp_session, reinterpret_cast<const uint8_t *>(out.c_str()), out.size());
		}
	}
}

static void process_line(session *const tcp_session, irc_state_t *const is, const std::string & line)
{
	DOLOG(ll_debug, "irc::process_line: |%s|\n", line.c_str());

	if (line.empty())
		return;

	std::vector<std::string> parts = split(line, " ");

	if (parts.size() == 0)
		return;

	irc_session_data *isd = dynamic_cast<irc_session_data *>(tcp_session->get_callback_private_data());

	if (*is == IS_wait_nick) {
		if (parts.at(0) == "NICK" && parts.size() == 2) {  // ignoring hop count
			// nick must be unique
			auto nick = str_tolower(parts.at(1));

			std::unique_lock<std::mutex> lck(nicknames_lock);

			auto it   = nicknames.find(nick);
			if (it == nicknames.end()) {
				person p;

				p.tcp_session = tcp_session;

				nicknames.insert({ nick, p });

				lck.unlock();

				isd->nick        = nick;

				*is = IS_wait_user;
			}
			else {
				lck.unlock();

				std::string error = ": 433 * " + nick + " :Nickname is already in use.\r\n";

				if (tcp_session->get_stream_target()->send_data(tcp_session, reinterpret_cast<const uint8_t *>(error.c_str()), error.size()) == false)
					*is = IS_disconnect;
			}
		}
		else {
			std::string error = ": 421 * :Something is not right.\r\n";

			if (tcp_session->get_stream_target()->send_data(tcp_session, reinterpret_cast<const uint8_t *>(error.c_str()), error.size()) == false)
				*is = IS_disconnect;
		}
	}
	else if (*is == IS_wait_user) {
		if (parts.at(0) == "USER" && parts.size() >= 5) {
			std::unique_lock<std::mutex> lck(nicknames_lock);

			auto it = nicknames.find(isd->nick);

			if (it == nicknames.end()) {
				lck.unlock();

				std::string error = ": 401 * :What is your nick?\r\n";

				if (tcp_session->get_stream_target()->send_data(tcp_session, reinterpret_cast<const uint8_t *>(error.c_str()), error.size()) == false)
					*is = IS_disconnect;
			}
			else {
				auto rn_offset = line.find(":");
				auto real_name = rn_offset == std::string::npos ? "" : line.substr(rn_offset + 1);

				it->second.real_name = real_name;

				lck.unlock();

				isd->username  = parts.at(1);

				*is = IS_running;

				std::vector<std::string> welcome {
					": 001 " + isd->nick + " :Welcome\r\n",
					": 002 " + isd->nick + " :Your host runs MyIP\r\n",
					": 003 " + isd->nick + " :003\r\n",
					": 004 " + isd->nick + " \r\n",
					": 005 " + isd->nick + " :\r\n",
					": 005 " + isd->nick + " :\r\n",
					": 251 " + isd->nick + " :\r\n",
					": 252 " + isd->nick + " 0 :operator(s) online\r\n",
					": 253 " + isd->nick + " 0 :unknown connections\r\n",
					": 254 " + isd->nick + " 0 :channels formed\r\n",
					": 255 " + isd->nick + " :I have 0 clients and 1 server\r\n",
					": 265 " + isd->nick + " :Current local users: 0  Max: 0\r\n",
					": 266 " + isd->nick + " :Current global users: 0  Max: 0\r\n",
					": 375 " + isd->nick + " :message of the day\r\n",
					": 372 " + isd->nick + " :\r\n",
					": 376 " + isd->nick + " :End of message of the day.\r\n"
				};

				for(auto & line : welcome) {
					if (tcp_session->get_stream_target()->send_data(tcp_session, reinterpret_cast<const uint8_t *>(line.c_str()), line.size()) == false) {
						*is = IS_disconnect;
						break;
					}
				}
			}
		}
	}
	else if (*is == IS_running) {
		if (parts.at(0) == "JOIN" && parts.size() >= 2) {
			auto channels = split(parts.at(1), ",");

			std::unique_lock<std::mutex> lck(nicknames_lock);

			auto it = nicknames.find(isd->nick);

			for(auto & channel : channels) {
				it->second.channels.insert(str_tolower(channel));

				std::string join_line = ":" + isd->nick + "!" + isd->username + "@" + tcp_session->get_their_addr().to_str() + " JOIN " + channel + "\r\n";

				transmit_to_channel(channel, join_line);

				send_user_for_channel(channel);
			}

			// TODO: notify each user
		}
		else if (parts.at(0) == "PART" && parts.size() >= 2) {
			auto channels = split(parts.at(1), ",");

			std::unique_lock<std::mutex> lck(nicknames_lock);

			auto it = nicknames.find(isd->nick);

			for(auto & channel : channels) {
				it->second.channels.erase(str_tolower(channel));

				send_user_for_channel(channel);

				std::string part_line = ":" + isd->nick + "!" + isd->username + "@" + tcp_session->get_their_addr().to_str() + " PART " + channel + "\r\n";

				transmit_to_channel(channel, part_line);
			}
		}
		else if ((parts.at(0) == "PRIVMSG" || parts.at(0) == "NOTICE") && parts.size() >= 2) {
			auto target = str_tolower(parts.at(1));

			std::unique_lock<std::mutex> lck(nicknames_lock);

			std::string msg_line = ":" + isd->nick + "!" + isd->username + "@" + tcp_session->get_their_addr().to_str() + " " + line + "\r\n";

			transmit_to_channel(target, msg_line);
		}
		// TODO
		else {
			std::string error = ": 421 * :Unknown command.\r\n";

			if (tcp_session->get_stream_target()->send_data(tcp_session, reinterpret_cast<const uint8_t *>(error.c_str()), error.size()) == false)
				*is = IS_disconnect;
		}
	}
	else {
		std::string error = ": 421 * :Internal error.\r\n";

		tcp_session->get_stream_target()->send_data(tcp_session, reinterpret_cast<const uint8_t *>(error.c_str()), error.size());

		*is = IS_disconnect;
	}
}

void irc_thread(session *const tcp_session)
{
        set_thread_name("myip-irc");

        irc_session_data *ts = dynamic_cast<irc_session_data *>(tcp_session->get_callback_private_data());

	irc_state_t is = IS_wait_nick;

        for(;ts->terminate == false && is != IS_disconnect;) {
		std::unique_lock<std::mutex> lck(ts->r_lock);

		std::size_t crlf = ts->input.find("\r\n");

		if (crlf != std::string::npos) {
			process_line(tcp_session, &is, ts->input.substr(0, crlf));

			ts->input.erase(0, crlf + 2);
		}
		else if (is != IS_disconnect) {
			ts->r_cond.wait_for(lck, 500ms);
		}
	}

	tcp_session->get_stream_target()->end_session(tcp_session);
}

bool irc_new_session(pstream *const t, session *t_s)
{
	irc_session_data *ts = new irc_session_data();

	any_addr src_addr = t_s->get_their_addr();
	ts->client_addr   = src_addr.to_str();

	t_s->set_callback_private_data(ts);

	ts->th = new std::thread(irc_thread, t_s);

	return true;
}

bool irc_new_data(pstream *ps, session *ts, buffer_in b)
{
	if (!ts) {
		DOLOG(ll_info, "IRC: data for a non-existing session\n");

		return false;
	}

	irc_session_data *t_s = dynamic_cast<irc_session_data *>(ts->get_callback_private_data());

	int data_len = b.get_n_bytes_left();

	if (data_len == 0) {
		DOLOG(ll_debug, "IRC: client closed session\n");

		return true;
	}

	const std::lock_guard<std::mutex> lck(t_s->r_lock);

	t_s->input += std::string(reinterpret_cast<const char *>(b.get_bytes(data_len)), data_len);

	t_s->r_cond.notify_one();

	return true;
}

bool irc_close_session_1(pstream *const ps, session *ts)
{
	return true;
}

bool irc_close_session_2(pstream *const ps, session *ts)
{
	session_data *sd = ts->get_callback_private_data();

	if (sd) {
		irc_session_data *nsd = dynamic_cast<irc_session_data *>(sd);

		nsd->terminate = true;

		nsd->th->join();
		delete nsd->th;
		nsd->th = nullptr;

		delete nsd;

		ts->set_callback_private_data(nullptr);
	}

	return true;
}

port_handler_t irc_get_handler(stats *const s)
{
	port_handler_t tcp_irc;

	tcp_irc.init             = irc_init;
	tcp_irc.new_session      = irc_new_session;
	tcp_irc.new_data         = irc_new_data;
	tcp_irc.session_closed_1 = irc_close_session_1;
	tcp_irc.session_closed_2 = irc_close_session_2;
	tcp_irc.deinit           = irc_deinit;

	tcp_irc.pd               = nullptr;

	return tcp_irc;
}