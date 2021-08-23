// (C) 2020 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdint.h>
#include <thread>
#include <vector>

#include "packet.h"

class phys;

class protocol
{
protected:
	std::thread *th { nullptr };
	std::atomic_bool stop_flag { false };

        std::mutex pkts_lock;
        std::condition_variable pkts_cv;
	std::vector<const packet *> pkts;

	phys *pdev { nullptr };

public:
	protocol();
	virtual ~protocol();

	void register_phys(phys *const p) { pdev = p; }

	void queue_packet(const packet *p);

	virtual void transmit_packet(const any_addr & dst_mac, const any_addr & dst_ip, const any_addr & src_ip, const uint8_t protocol, const uint8_t *payload, const size_t pl_size, const uint8_t *const header_template) = 0;
	virtual void transmit_packet(const any_addr & dst_ip, const any_addr & src_ip, const uint8_t protocol, const uint8_t *payload, const size_t pl_size, const uint8_t *const header_template) = 0;

	virtual int get_max_packet_size() const = 0;

	virtual void operator()() = 0;
};
