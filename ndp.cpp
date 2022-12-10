// (C) 2020-2022 by folkert van heusden <mail@vanheusden.com>, released under Apache License v2.0
#include <assert.h>
#include <chrono>
#include <string.h>

#include "any_addr.h"
#include "ndp.h"
#include "phys.h"


ndp::ndp(stats *const s) : mac_resolver(s, nullptr), address_cache(s)
{
	// 1.3.6.1.2.1.4.57850.1.9: ndp
        ndp_cache_req = s->register_stat("ndp_cache_req", "1.3.6.1.2.1.4.57850.1.9.1");
        ndp_cache_hit = s->register_stat("ndp_cache_hit", "1.3.6.1.2.1.4.57850.1.9.2");
}

ndp::~ndp()
{
}

std::optional<any_addr> ndp::get_mac(const any_addr & ip)
{
	return { };
}

void ndp::operator()()
{
}
