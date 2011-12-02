#ifndef CLICK_XTRANSPORT_HH
#define CLICK_XTRANSPORT_HH

#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>
#include <click/hashtable.hh>
#include "xiaxidroutetable.hh"
#include <click/handlercall.hh>
#include <click/xiapath.hh>
#include <clicknet/xia.h>
#include "xiacontentmodule.hh"
#include "xiaxidroutetable.hh"
#include <clicknet/udp.h>
#include <click/string.hh>
#if CLICK_USERLEVEL
#include <list>
#include <stdio.h>
#include <iostream>
#include <click/xidpair.hh>
#include <click/timer.hh>
#include <click/packet.hh>
#include <queue>

#include "../../userlevel/xia.pb.h"

using namespace std;


#endif

#define CLICKCONTROLPORT 5001
//#define CLICKOPENPORT 5001
#define CLICKBINDPORT 5002
#define CLICKCLOSEPORT 5003
#define CLICKCONNECTPORT 5004
#define CLICKACCEPTPORT 5005

#define CLICKPUTCIDPORT 10002
#define CLICKSENDTOPORT 10001
#define CLICKDATAPORT 10000


#define MAX_WIN_SIZE 100


CLICK_DECLS

/**
XTRANSPORT:   
input port[0]:  control port
input port[1]:  Socket Rx data port
input port[2]:  Network Rx data port
input port[3]:  in from cache

output[3]: To cache for putCID
output[2]: Network Tx data port 
output[1]: Socket Tx data port

Might need other things to handle chunking
*/

class XIAContentModule;   

class XTRANSPORT : public Element { 
  public:
    XTRANSPORT();
    ~XTRANSPORT();
    const char *class_name() const		{ return "XTRANSPORT"; }
    const char *port_count() const		{ return "4/4"; }
    const char *processing() const		{ return PUSH; }
    int configure(Vector<String> &, ErrorHandler *);         
    void push(int port, Packet *);            
    XID local_hid() { return _local_hid; };
    XIAPath local_addr() { return _local_addr; };
    void add_handlers();
    static int write_param(const String &, Element *, void *vparam, ErrorHandler *);
    
    int initialize(ErrorHandler *);
    void run_timer(Timer *timer);
    
    WritablePacket* copy_packet(Packet *);
    WritablePacket* copy_cid_req_packet(Packet *);
    
  private:
    Timer _timer;
    
    unsigned _ackdelay_ms;
    unsigned _teardown_wait_ms;
    
    uint32_t _cid_type;
    XID _local_hid;
    XIAPath _local_addr;
    bool isConnected;

    // protobuf message
    xia::XSocketMsg xia_socket_msg;
    //enum xia_socket_msg::XSocketMsgType type;
    
    Packet* UDPIPEncap(Packet *, int,int);
    
    struct DAGinfo{
    DAGinfo(): port(0), isConnected(false), initialized(false), timer_on(false), synack_waiting(false), dataack_waiting(false), teardown_waiting(false) {};
    unsigned short port;
    XID xid;
    XIAPath src_path;
    XIAPath dst_path;
    int nxt;
    int last;
    uint8_t hlim;
    bool isConnected;
    bool initialized;
    int sock_type; // 0: Reliable transport, 1: Unreliable transport
    String sdag;
    String ddag;
    //Vector<WritablePacket*> pkt_buf;
    WritablePacket *syn_pkt;
    WritablePacket *sent_pkt[MAX_WIN_SIZE];
    HashTable<XID, WritablePacket*> XIDtoCIDreqPkt;
    HashTable<XID, Timestamp> XIDtoExpiryTime;
    HashTable<XID, bool> XIDtoTimerOn;
    uint32_t seq_num;
    uint32_t ack_num;
    uint32_t base; // the sequence # of the oldest unacked packet
    uint32_t next_seqnum; // the smallest unused sequence # (i.e., the sequence # of the next packet to be sent)
    uint32_t expected_seqnum; // the sequence # of the next in-order packet (this is used at receiver-side)
    bool timer_on;
    Timestamp expiry;
    bool synack_waiting;
    bool dataack_waiting;
    bool teardown_waiting;
    Timestamp teardown_expiry;
    } ;
    
 
    HashTable<XID, unsigned short> XIDtoPort;
    HashTable<XIDpair , unsigned short> XIDpairToPort;
    HashTable<unsigned short, DAGinfo> portToDAGinfo;
    HashTable<unsigned short, int> portRxSeqNo;
    HashTable<unsigned short, int> portTxSeqNo;
    HashTable<unsigned short, int> portAckNo;
    HashTable<unsigned short, bool> portToActive;
    queue<DAGinfo> pending_connection_buf;
    
    struct in_addr _CLICKaddr;
    struct in_addr _APIaddr;
    atomic_uint32_t _id;
    bool _cksum;
    XIAXIDRouteTable *_routeTable;
    
    //modify routing table
    void addRoute(const XID &sid) {
        String cmd=sid.unparse()+" 4";
        HandlerCall::call_write(_routeTable, "add", cmd);
    }   
        
    void delRoute(const XID &sid) {
        String cmd= sid.unparse();
        HandlerCall::call_write(_routeTable, "remove", cmd);
    }

};


CLICK_ENDDECLS

#endif




