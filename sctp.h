// (C) 2022 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
#pragma once

#include <functional>
#include <map>
#include <shared_mutex>

#include "buffer_in.h"
#include "buffer_out.h"
#include "ip_protocol.h"
#include "packet.h"
#include "utils.h"


class icmp;

class sctp : public ip_protocol
{
private:
	uint8_t state_cookie_key[32]       { 0 };
	time_t  state_cookie_key_timestamp { 0 };

	icmp *const icmp_;

	std::map<int, uint64_t> allocated_ports;
	std::mutex              ports_lock;

	uint64_t *sctp_msgs        { nullptr };
	uint64_t *sctp_failed_msgs { nullptr };

	std::pair<uint16_t, buffer_in> get_parameter(buffer_in & chunk_payload);

	buffer_out generate_state_cookie(const any_addr & their_addr, const int their_port, const int local_port);
	buffer_out chunk_gen_abort();
	buffer_out chunk_gen_cookie_ack();

	void chunk_init(buffer_in & chunk_payload, const uint32_t my_verification_tag, const uint32_t buffer_size, const any_addr & their_addr, const int their_port, const int local_port, buffer_out *const out, uint32_t *const initiate_tag);
	bool chunk_cookie_echo(buffer_in & chunk_payload, const any_addr & their_addr, const int their_port, const int local_port);

public:
	sctp(stats *const s, icmp *const icmp_);
	virtual ~sctp();

	void add_handler(const int port, std::function<void(const any_addr &, int, const any_addr &, int, packet *, void *const pd)> h, void *const pd);
	void remove_handler(const int port);

	bool transmit_packet(const any_addr & dst_ip, const int dst_port, const any_addr & src_ip, const int src_port, const uint8_t *payload, const size_t pl_size);

	virtual void operator()() override;
};
