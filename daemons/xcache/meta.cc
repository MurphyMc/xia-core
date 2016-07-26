#include "slice.h"
#include "meta.h"
#include "stores/store.h"

#define IGNORE_PARAM(__param) ((void)__param)

xcache_meta::xcache_meta(std::string cid)
{
	store = NULL;
	this->cid = cid;
	len = 0;
	pthread_mutex_init(&meta_lock, NULL);
}

std::string xcache_meta::get(void)
{
	return store->get(this);
}

std::string xcache_meta::get(off_t off, size_t len)
{
	return store->get_partial(this, off, len);
}

std::string xcache_meta::safe_get(void)
{
	std::string data;

	this->lock();
	data = get();
	this->unlock();

	return data;
}
xcache_meta::xcache_meta()
{
	store = NULL;
	pthread_mutex_init(&meta_lock, NULL);
}

void xcache_meta::status(void)
{
	syslog(LOG_INFO, "[%s] %s", cid.c_str(), store->get(this).c_str());
}
