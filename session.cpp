#include <stdlib.h>

#include "buffer_out.h"
#include "hash.h"
#include "session.h"


session::session(pstream *const t, const any_addr & my_addr, const int my_port, const any_addr & their_addr, const int their_port, private_data *const application_private_data) :
	t(t),
	my_addr(my_addr),
	my_port(my_port),
	their_addr(their_addr),
	their_port(their_port),
	application_private_data(application_private_data)
{
}

session::~session()
{
	free(callback_private_data);
}

const any_addr session::get_their_addr() const
{
	return their_addr;
}

const uint16_t session::get_their_port() const
{
	return their_port;
}

const any_addr session::get_my_addr() const
{
	return my_addr;
}

const uint16_t session::get_my_port() const
{
	return my_port;
}

uint64_t session::get_hash() const
{
	return get_hash(their_addr, their_port, my_port);
}

uint64_t session::get_hash(const any_addr & their_addr, const uint16_t their_port, const uint16_t my_port)
{
	buffer_out temp;

	temp.add_any_addr(their_addr);
	temp.add_net_short(their_port);
	temp.add_net_short(my_port);

	return MurmurHash64A(temp.get_content(), temp.get_size(), 123 /* TODO: replace 123 */);
}

void session::set_callback_private_data(void *p)
{
	callback_private_data = p;
}

void * session::get_callback_private_data()
{
	return callback_private_data;
}