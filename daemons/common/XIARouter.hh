/*
** Copyright 2012 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#ifndef _XIARouter_hh
#define _XIARouter_hh

#include <string>
#include <vector>
#include <map>

// FIXME: stupid hack because csclient.hh requires it, need to modify that code
// to prefix string and vector woth std::
using namespace std;

#include "csclient.hh"

// questions:
//  where do we want to specify the device (router0, etc?) at init time, or in each call?
//  is it ok, to dump the xidType parameter, and just infer it from the xid?

#define XR_OK					0
#define XR_NOT_CONNECTED		-1
#define XR_ALREADY_CONNECTED	-2
#define XR_CLICK_ERROR			-3
#define XR_ROUTE_EXISTS			-4
#define XR_ROUTE_DOESNT_EXIST	-5
#define XR_NOT_XIA				-6
#define XR_ROUTER_NOT_SET		-7
#define XR_BAD_HOSTNAME			-8
#define XR_INVALID_XID			-9
#define XR_INVALID_WEIGHT		-10

#define TOTAL_SPECIAL_CASES 8
#define DESTINED_FOR_DISCARD -1
#define DESTINED_FOR_LOCALHOST -2
#define DESTINED_FOR_DHCP -3
#define DESTINED_FOR_BROADCAST -4
#define REDIRECT -5
#define UNREACHABLE -6
#define FALLBACK -7


// Controls how fast LSAs are sent (ms)
#define L_FREQ_ROUTER     5100
#define L_FREQ_CONTROLLER 5100

// Controls how fast keepalives are sent (ms)
#define H_FREQ_ROUTER     5100
#define H_FREQ_CONTROLLER 5100
#define H_FREQ_HOST       5100

// Expiration times (sec)
#define NEIGHBOR_EXPIRE_TIME 60
#define ROUTE_EXPIRE_TIME    120


#define SET_TIMEVAL(_tv,_ms) do {      \
  (_tv).tv_sec  = ((_ms)*1000) / 1000000;  \
  (_tv).tv_usec = ((_ms)*1000) % 1000000; \
  } while (0);

typedef struct {
	std::string xid;
	std::string nextHop;
	unsigned short port;
	unsigned long  flags;
} XIARouteEntry;

typedef std::map<std::string, XIARouteEntry> XIARouteTable;

class XIARouter {
public:
	XIARouter(const char *_rtr = "router0") { _connected = false;
		_cserr = ControlSocketClient::no_err; _router = _rtr; };
	~XIARouter() { if (connected()) close(); };

	// connect to click
	int connect(std::string clickHost = "localhost", unsigned short controlPort = 7777);
	int connected() { return _connected; };
	void close();

	// returns the click version in <ver>
	int version(std::string &ver);

	// returns the error code from the underlying click control socket interface
	ControlSocketClient::err_t ControlSocketError() { return _cserr; };

	// return a vector of router devices click knows about (host0, router1, ....)
	int listRouters(std::vector<std::string> &rlist);

	// specify which router to operate on, must be called before adding/removing routes
	// defaults to router0
	void setRouter(std::string r) { _router = r; };
	std::string getRouter() { return _router; };

	// get the current set of route entries, return value is number of entries returned or < 0 on err
	int getRoutes(std::string xidtype, std::map<std::string, XIARouteEntry> &xrt);

	// returns 0 success, < 0 on error
	int addRoute(const std::string &xid, int port, const std::string &next, unsigned long flags);
	int setRoute(const std::string &xid, int port, const std::string &next, unsigned long flags);
	int appRoute(const std::string &xid, int port, const std::string &next, unsigned long flags, int weight, const std::string &index);
	int seletiveSetRoute(const std::string &xid, int port, const std::string &next, unsigned long flags, int weight, const std::string &index);
	int delRoute(const std::string &xid);
	int selectiveDelRoute(const std::string &xid, const std::string &index);

	const char *cserror();

    int rawWrite(const std::string &element, const std::string &cmd, const std::string &data);

private:
	bool _connected;
	std::string _router;
	ControlSocketClient _cs;
	ControlSocketClient::err_t _cserr;

	int updateRoute(string cmd, const std::string &xid, int port, const std::string &next, unsigned long flags, int weight, const std::string &index);
	string itoa(signed);
};

#endif
