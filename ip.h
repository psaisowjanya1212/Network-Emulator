#ifndef IP_H
#define IP_H
#include "ether.h"

/* ARP packet types */
#define ARP_REQUEST 0 
#define ARP_RESPONSE 1

/* IP protocol types */
#define PROT_TYPE_UDP 0
#define PROT_TYPE_TCP 1
#define PROT_TYPE_OSPF 2 //NOT USED

typedef unsigned long IPAddr;

/* Structure to represent an interface */

typedef struct iface {
  char ifacename[32];
  IPAddr ipaddr;
  IPAddr mask;
  MacAddr macaddr;
  char lanname[32];
} Iface;

/* mapping between interface name and socket id representing link */
typedef struct itface2link {
  char ifacename[32];
  int sockfd;
} ITF2LINK;

typedef struct slTable {
  MacAddr mac;
  int port;
  int sockfd;
  time_t lstConTime; //last connection time to check the connection exp.
} SL_TABLE;


/* Structure for a routing table entry */
typedef struct rtable {
   IPAddr destsubnet;
   IPAddr nexthop;
   IPAddr mask;
   char ifacename[32];
} Rtable;


/* Structure for an ARP cache entry */
typedef struct arpcache { //NOT USED
    IPAddr ipaddr;
    MacAddr macaddr;
    time_t createTime;
} Arpc;

/*--------------------------------------------------------------------*/

/* The Structure of the IP datagram and ARP packets go here */

/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/* Structure for ARP packets */

/*list of arp cache, to use this one to maintain current cache*/

typedef struct arp_list {
  Arpc *arp_item;
  struct arp_list *next;
} ARP_LIST;

/*ARP packet format*/
typedef struct __attribute__((packed)) arp_pkt 
{
  short op; /* op =0 : ARP request; op = 1 : ARP response */
  IPAddr srcip;
  MacAddr srcmac;
  IPAddr dstip;
  MacAddr dstmac;
} ARP_PKT;

/*IP packet format*/
typedef struct __attribute__((packed)) ip_pkt
{
  IPAddr  dstip;
  IPAddr  srcip;
  short   protocol;
  unsigned long    sequenceno;
  short   length;
  char    data[BUFSIZ];
} IP_PKT;

/*queue for ip packet that has not yet sent out*/
typedef struct p_queue
{
  IPAddr next_hop_ipaddr;
  IPAddr dst_ipaddr;
  char *pending_pkt;
  struct p_queue *next;
  
} PENDING_QUEUE;

/*queue to remember the packets we have received*/
typedef struct packet_queue 
{//NOT USED
  char *packet;
  int  length;
  short counter;
  struct packet_queue *next;
} OLD_PACKETS;

/*-------------------------------------------------------------------- */

#define MAXHOSTS 32 //NOT USED
#define MAXINTER 32  //NOT USED

typedef struct host
  {
    char name[32];
    IPAddr addr;
  }
Host;

typedef struct lan_rout { //NOT USED
  short router_attached;
  short counter;
} LAN_ROUT;

/* some global variables here */
Host host[MAXHOSTS];
int hostcnt;

Iface iface_list[MAXINTER];
/* if there is router on this lan, 1; else 0 */
LAN_ROUT lan_router[MAXINTER];     //NOT USED
ITF2LINK link_socket[MAXINTER];
SL_TABLE sl_table[MAXINTER];
int sl_cnt = 0 ;
int MacTimeout = 60;

int intr_cnt; /* counter for interface */

Rtable rt_table[MAXHOSTS*MAXINTER];
int rt_cnt;

PENDING_QUEUE *pending_queue;
ARP_LIST *arp_cache;

int ROUTER;



#endif
