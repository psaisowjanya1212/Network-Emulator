//Name 	: Nikhta Chamarti (nc20ce)
//		: Sai Sowjanya Padamati (sp22bi)

/*-------------------------------------------------------*/
#define _POSIX_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/ether.h>
#include <net/ethernet.h>
#include <unistd.h>

#include "ip.h"
#include "utils.h"

/*----------------------------------------------------------------*/
#define MAXFILELEN 256
#define MAXBUFLEN 8232 //changes the MAX buf Size from 512 to 8232
#define MAXRETRY 5
/*----------------------------------------------------------------*/

// function definitions
void parseIFace(char *filename);
void parseRTables(char *filename);
void parsehosts(char *filename);
IPAddr getlanAddr(char *lanname);
unsigned short getlanPort(char *lanname);

void sendMsg(char * dsthost, char * msg, EtherPkt * ethPkt, int * sfd);
int lookupHost(char * dstHost, IPAddr * dstIP);
int routeMsg(IPAddr dstip, IPAddr * srcip, IPAddr * nxtHopip, MacAddr srcmac, char * ifacename, int * sfd);
int lookupIFace(int sfd, char * iface, IPAddr *ip, MacAddr mac);

int processEtherPkt(EtherPkt * r_ethPkt, int r_sfd, char * dsthost);

int arpLookUP(IPAddr ipDst, MacAddr macDst);
int pushARPC(Arpc arpc);
int removeExpARPC();

int pushPQ(IPAddr nxtHopIP, IPAddr dstIP, char *pndPKT );
int popPQ(IPAddr dstIP, char *pndPKT);

void writeEtherPkt(EtherPkt * s_ethPkt, int s_sfd);

void printARP();
void printPQ();
void printIface();
void printRtable();
void printHost();


/*----------------------------------------------------------------*/


int main(int argc, char * argv[])
{
	if(argc != 5)
	{
		fprintf(stderr, "Use: station <-route|-no> <interface> <routingtable> <hostname> \n",
			"Here the interface, routingtables and hostname are filenames\n");
		exit(1);
	}
	printf("\nInitializing..\n");

	char prompt[32] = "" , ipstr[32] = "" , macstr[48] = "";
	if(strcmp(argv[1], "-no") == 0)
	{
		ROUTER = 0;
		strcpy(prompt, "STATION> ");
	}
	else if(strcmp(argv[1], "-route") == 0)
	{
		ROUTER = 1;
		strcpy(prompt , "ROUTER> ");
	}

	//filenames for ifacetables, rtables, hosts
	char ifaceFileName[MAXFILELEN] = "\0";
	char rtableFileName[MAXFILELEN] = "\0";
	char hostFileName[MAXFILELEN] = "\0";

	strcpy(ifaceFileName, argv[2]);
	strcpy(rtableFileName, argv[3]);
	strcpy(hostFileName, argv[4]);

	//parse the Interface file
	printf("\n\nReading ifaces..\n\n");
	parseIFace(ifaceFileName);
	printIface();

	//parse the routing table file
	printf("\n\nReading rtables..\n\n");
	parseRTables(rtableFileName);
	printRtable();
	
	//parse the hosts file
	printf("\n\nReading hosts..\n\n");
	parsehosts(hostFileName);
	printHost();

	// parse the Lan-name.addr file and Lan-name.port file
	IPAddr lan_Addr;
	unsigned short lan_port;

	int sockfd = -1, maxf = -1, rv, flag;
	char buf[MAXBUFLEN];
	fd_set  rset, orig_set;
	struct addrinfo hints, *res, *ressave;
	bzero(&hints, sizeof(struct addrinfo));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int retry_cnt = 0;

	int i;
	for(i = 0; i < intr_cnt; i++ )
	{

		lan_Addr = getlanAddr(iface_list[i].lanname); //lan-name.addr
		lan_port = getlanPort(iface_list[i].lanname); //lan-name.port
		IP2STR(lan_Addr, ipstr);
		printf("\n%s %s %hu\n", iface_list[i].lanname, ipstr, lan_port);
		printf("iface %d try to connect to bridge %s...  \n\n", i , iface_list[i].lanname);

		char lanPort_str[32];
		sprintf(lanPort_str, "%d", lan_port);
		rv = getaddrinfo(ipstr, lanPort_str, &hints, &res);
		if ( rv != 0)
		{
			fprintf(stderr, "getaddrinfo Failed %s\n", gai_strerror(rv));
			exit(1);
		}

		ressave = res;
		flag = 0;
		int connected = 0;

		while(retry_cnt < MAXRETRY && flag != 1)
		{
			res = ressave;
			do 
			{
				sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
				if (sockfd < 0) { 	continue;	}
				
				// Set socket to non-blocking
				int f = fcntl(sockfd, F_GETFL, 0);
				if (f == -1) 
				{
					perror("fcntl");
					continue;
				}
				// Set the O_NONBLOCK flag to make the socket non-blocking
				if (fcntl(sockfd, F_SETFL, f | O_NONBLOCK) == -1) 
				{
					perror("fcntl");
					continue;
				}

				if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) 
				{
					//connection established
					flag = 1;
					break;
				} 
				else
				{
					if (errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK) 
					{
						fd_set write_set;
						FD_ZERO(&write_set);
						FD_SET(sockfd, &write_set);

						struct timeval timeout;
						timeout.tv_sec = 2; // Set timeout
						timeout.tv_usec = 0;

						int result = select(sockfd + 1, &write_set, NULL,  NULL, &timeout);
						
						timeout.tv_sec = 2; // reset timeout
						timeout.tv_usec = 0;


						if (result == -1) 
						{
							perror("select");
							close(sockfd);
							continue;
						} 
						else if (result == 0)
						{
							fprintf(stderr, "No response... Retrying\n");
							//retry_cnt++;
							close(sockfd);
							continue;
						}
						else if (FD_ISSET(sockfd, &write_set))
						{
							//connection established
							flag = 1;
							break;
						}
					}

				}

				close(sockfd);
			} while ((res = res->ai_next) != NULL);
			
			retry_cnt++;
		} // end of while loop

		freeaddrinfo(ressave);
		if (flag == 0) 
		{
				fprintf(stderr, "Connection could not be established\n");
				exit(1);
		}
		else if (flag == 1)
		{
			if (read(sockfd, buf, MAXBUFLEN) == 0)
			{
				printf("server crashed.\n");
				exit(1);
			}
			if(strcmp(buf, "accept") == 0)
			{
				connected = 1;
				flag = 0;
				printf("Connection Accepted!\n\n");
				
				strcpy(link_socket[i].ifacename , iface_list[i].ifacename);
				link_socket[i].sockfd = sockfd;

				FD_SET(sockfd, &orig_set);    
				if (sockfd > maxf)
				{ 
					maxf = sockfd + 1;
				}
			} //end if string accept
			else if(strcmp(buf, "reject") == 0)
			{
				flag = 0;
				printf("Connection Rejected!\n\n"); 
				close(sockfd);
				exit(1); 
			}    

		}  //end of connected true

	} //end of for interface

	
	

	FD_ZERO(&orig_set);
	FD_SET(STDIN_FILENO, &orig_set);
	maxf = STDIN_FILENO+ 1;

	for(i = 0; i < intr_cnt ; i++)
	{
		FD_SET(link_socket[i].sockfd , &orig_set);
		if (link_socket[i].sockfd  > maxf)
		{
			maxf = link_socket[i].sockfd + 1;
		}
	}   


	char * token ;
	char cmd1[32], cmd2[32], msg[MAXBUFLEN], dsthost[32];
	const char delim[2] = " ";

	while (1) 
	{
		rset = orig_set;
		
		struct timeval timeout;
		timeout.tv_sec = 2; // Set timeout
		timeout.tv_usec = 0;

		select(maxf + 1, &rset, NULL, NULL, &timeout);

		timeout.tv_sec = 2; // Set timeout
		timeout.tv_usec = 0;

		for (i = 0; i < intr_cnt ; i++ )
		{
			if (FD_ISSET(link_socket[i].sockfd, &rset)) 
			{
				int num = read(link_socket[i].sockfd, buf, MAXBUFLEN);
				if (num == 0) 
				{
					printf("server crashed.\n");
					exit(0);
				}
				else if(num < 0)
				{
					perror(": read error");
					exit(1);
				}
				else 
				{
					printf("\n%s", prompt);
					fflush(stdout);

					if(num < sizeof(EtherPkt))
					{
						continue;
					}
					buf[num] = '\0';
					EtherPkt * r_ethPkt = malloc(sizeof(EtherPkt) + 1);
					deserETH_PKT(buf, r_ethPkt);
					printf("\nMessage from bridge\n");
					printf("Received %d bytes Ethernet header\n", r_ethPkt->size);
					MAC2STR(r_ethPkt->dst, macstr);
					printf("Destination MAC address of the Ethernet header is %s \n", macstr);
					
					processEtherPkt(r_ethPkt, link_socket[i].sockfd, link_socket[i].ifacename);
					
				}
				
			}
			
		} // end of for interface counter

		if (FD_ISSET(STDIN_FILENO, &rset))
		{
			printf("\n%s",prompt); 
			fflush(stdout);
			if (fgets(buf, MAXBUFLEN, stdin) == NULL) 
			{
				exit(0);
			}
			
			token = strtok(buf, delim);
			int x = 0 ;
			strcpy(msg,  "");

			while(token != NULL ) 
			{
				if(x == 0)
				{
					strcpy(cmd1, token);
				}
				else if(x == 1)
				{
					if(strcmp(cmd1, "send") == 0)
					{
						strcpy(dsthost, token);
					}
					else if(strcmp(cmd1, "show") == 0)
					{
						strcpy(cmd2, token);
					}
				}
				else 
				{
					strcat(msg, token);
					strcat(msg, " ");
				}
				x++;
				token = strtok(NULL, delim);
			}
			if(strcmp(cmd1, "send") == 0 && ROUTER == 0)
			{
				EtherPkt * r_ethPkt = malloc(sizeof(EtherPkt));
				int s_sfd = -1;

				if(dsthost != NULL)
				{	
					sendMsg(dsthost, msg, r_ethPkt, &s_sfd);
					// printf("\n%s", prompt);
					// fflush(stdout);
					processEtherPkt(r_ethPkt, STDIN_FILENO, dsthost);
				}
				
				
				
				free(r_ethPkt->dat);
				free(r_ethPkt);

			}
			else if(strcmp(cmd1, "show") == 0)
			{    
				if(strcmp(cmd2, "arp\n") == 0) 
				{ 
					printARP();
				}
				else if(strcmp(cmd2, "pq\n") == 0)
				{
					printPQ();	
				}
				else if(strcmp(cmd2, "host\n") == 0)
				{
					printHost();
				}
				else if(strcmp(cmd2, "iface\n") == 0)
				{
					printIface();
				}
				else if(strcmp(cmd2, "rtable\n") == 0)
				{ 
					printRtable();
				}
			}
			else if(strcmp(cmd1, "quit\n") == 0)
			{
				printf("Free memory..\n");
				for(i = 0; i < intr_cnt ; i++)
				{
					close(link_socket[i].sockfd); //close socket fd
				} 

				PENDING_QUEUE *tempPQ, *pq;

				pq = pending_queue;
				while(pq != NULL)
				{
					tempPQ = pq;
					pq = pq->next;
					free(tempPQ->pending_pkt);
					free(tempPQ);
				}  

				exit(0);
			}

			
		}//end of if(STDIN_FILENO)
		removeExpARPC();
			
	} //end of main while loop

	return 0;
}//end of int main

/*----------------------------------------------------------------*/

void parseIFace(char *filename)
{
	//read iface files
	FILE *ifaceFile = fopen(filename, "r");
	char buf[256] ="\0";
	
	if(ifaceFile == NULL)
	{
		perror("Failed to open the Interface file");
		exit(1);
	}

	char ipBuf[32], maskBuf[32], macBuf[32] ;
	Iface s_iface;
	IPAddr ipaddr;
	MacAddr macaddr;

	//reads iface row 
	while(fscanf(ifaceFile, "%s %s %s %s %s", 
				s_iface.ifacename,
				ipBuf, maskBuf, macBuf,
				s_iface.lanname)  != EOF)
	{
		s_iface.ipaddr = inet_addr(ipBuf);

		s_iface.mask = (in_addr_t)inet_addr((char *)maskBuf);
		
		struct ether_addr *mac;
		mac = ether_aton(macBuf);
		int i;
		for(i=0; i < 6; i++)
		{
		s_iface.macaddr[i] = mac->ether_addr_octet[i];
		}

		iface_list[intr_cnt] = s_iface;
		intr_cnt++;
	}
	fclose(ifaceFile);
}

/*----------------------------------------------------------------*/

void parseRTables(char *filename)
{
	FILE *rTableFile = fopen(filename, "r");
	char buf[256] ="\0";
	rt_cnt = 0;

	if(rTableFile == NULL)
	{
		perror("Failed to open the routing table file: ");
		exit(1);
	}

	char desSubN_Buf[32], nextHop_Buf[32], mask_Buf[32];
	Rtable s_rtable;
	IPAddr destSubnet, nextHop, mask;
	char destSubNet_Str[32], nextHop_Str[32], mask_Str[32], ifaceName_Str[32];
	//reads rtable rows 
	while(fscanf(rTableFile, "%s %s %s %s", desSubN_Buf, nextHop_Buf, mask_Buf,  s_rtable.ifacename)  != EOF)
	{
		strcpy(destSubNet_Str, "");
		strcpy(nextHop_Str, "");
		strcpy(mask_Str, "");
		strcpy(ifaceName_Str, "");
		
		s_rtable.destsubnet = inet_addr(desSubN_Buf);
		s_rtable.nexthop = inet_addr(nextHop_Buf);
		s_rtable.mask = (in_addr_t)inet_addr((char *)mask_Buf);

		rt_table[rt_cnt] = s_rtable;
		rt_cnt++;
	}
	fclose(rTableFile);

}

/*----------------------------------------------------------------*/


void parsehosts(char *filename)
{
	FILE *hostsFile = fopen(filename, "r");
	char buf[256] ="\0";
	hostcnt = 0;

	if(hostsFile == NULL)
	{
		perror("Failed to open the Hosts file : ");
		exit(1);
	}

	char hostIP_Buf[32];
	Host hst;
	IPAddr hostIPAddr;

	while(fscanf(hostsFile, "%s %s", hst.name ,hostIP_Buf) != EOF)
	{
		hst.addr = inet_addr(hostIP_Buf);

		host[hostcnt] = hst;
		hostcnt++;   
	}
	fclose(hostsFile);

}

/*----------------------------------------------------------------*/

IPAddr getlanAddr(char *lanname)
{
	//address file name (lan name)
	char addrFileName[MAXFILELEN] = "\0";
	char bridgeName[256] ="\0";  
	IPAddr lanaddr;
	strcpy(addrFileName, ".");
	strcat(addrFileName, lanname);
	strcat(addrFileName, ".addr");

	int err = readlink(addrFileName, bridgeName, sizeof(bridgeName));
	if (err == -1) {
		perror("readlink");
		return 1;
	}

	lanaddr = inet_addr(bridgeName);
	return lanaddr;
  
}

/*----------------------------------------------------------------*/

unsigned short getlanPort(char *lanname)
{
	//port filename
	char portFileName[MAXFILELEN] = "\0";
	char portNum_Str[256] ="\0"; 
	unsigned short portNum = 0;
	
	strcpy(portFileName, ".");
	strcat(portFileName, lanname);
	strcat(portFileName, ".port");

	int err = readlink(portFileName, portNum_Str, sizeof(portNum_Str));
	if (err == -1) {
		perror("readlink");
		return 1;
	}

	portNum = atoi(portNum_Str);

	return  portNum;
}

/*----------------------------------------------------------------*/

void sendMsg(char * dsthost, char * msg, EtherPkt * ethPkt, int * sfd)
{
	IPAddr dstip, srcip, nxtHopip;
	MacAddr srcmac;
	char ifacename[32];
	char ipstr[32], macstr[48];
	int size;

	IP_PKT * ipPkt =  malloc(sizeof(IP_PKT));

	if(lookupHost(dsthost, &dstip) == -1)
	{
		printf("%s : No such host\n", dsthost);
		return;
	}
	else
	{
		IP2STR( dstip, ipstr);
		printf("\nIP of %s = %s\n", dsthost, ipstr);
	}

	routeMsg(dstip, &srcip, &nxtHopip, srcmac, ifacename, sfd);

	ipPkt->dstip = dstip; 	// IPAddr  dstip;
	ipPkt->srcip = srcip;	// IPAddr  srcip;
	ipPkt->protocol = PROT_TYPE_TCP;	// short   protocol;
	ipPkt->sequenceno = 0; 		// unsigned long    sequenceno;
	ipPkt->length = strlen(msg); 	// short   length;
	strcpy(ipPkt->data, msg);

	IP2STR(nxtHopip, ipstr);
	printf("next hop interface = %s\n", ifacename);
	printf("next hop ip = %s\n", ipstr);
	printf("IP packet is ready to send:\n");
	printIP_PKT(ipPkt);
	char *ipPktBuf = malloc(sizeof(IP_PKT) + 1);
	size = serIP_PKT(ipPktBuf, ipPkt);
	ipPktBuf[size] = '\0';

	memcpy(ethPkt->dst, "\xFF\xFF\xFF\xFF\xFF\xFF", ETHER_ADDR_LEN);
	memcpy(ethPkt->src, srcmac, ETHER_ADDR_LEN);
	ethPkt->type = TYPE_IP_PKT;
	ethPkt->size = sizeof(IP_PKT);
	ethPkt->dat = (char*) malloc(ethPkt->size); //still have to free(ethPKT.dat);
	memcpy(ethPkt->dat, ipPktBuf, ethPkt->size);

	free(ipPktBuf);
	free(ipPkt);

}

/*----------------------------------------------------------------*/

/* 
	lookuphost:- get IP address for the hostname 
	return:- int 0 on success and -1 fail	
*/
int lookupHost(char * dstHost, IPAddr * dstIP)
{
	int i;
	for(i = 0; i < hostcnt; i++)
	{

		if(strcmp(host[i].name, dstHost) == 0)
		{
			*dstIP = host[i].addr;
			return 0;
		}

	}

  	return -1;
}

/*----------------------------------------------------------------*/

/*
	routeMsg:- generate the routing to transmit the msg based on the destinaion ip
	return:- int 
*/
int routeMsg(IPAddr dstip, IPAddr * srcip, IPAddr * nxtHopip, 
					MacAddr srcmac, char * ifacename, int * sfd)
{

	char ipstr[32];
	IPAddr destSNet;
	int ret = -1;

	int i;
	for (i = 0; i < rt_cnt; i++)
	{
		destSNet = dstip & rt_table[i].mask;
		if(rt_table[i].destsubnet == destSNet)
		{
			
			*nxtHopip = rt_table[i].nexthop;
			if(*nxtHopip == 0)
			{
				*nxtHopip = dstip;
			}
			strcpy(ifacename, rt_table[i].ifacename);
			ret = 1;
			break;
		}
	}

	for (i = 0 ; i < intr_cnt; i++)
	{
		if(strcmp(iface_list[i].ifacename , ifacename) == 0)
		{
			*srcip = iface_list[i].ipaddr;
			memcpy(srcmac, iface_list[i].macaddr, sizeof(MacAddr));
			ret = 2;
			break;
		}
	}

	for (i = 0 ; i < intr_cnt; i++)
	{
		if(strcmp(link_socket[i].ifacename, ifacename) == 0)
		{
			*sfd = link_socket[i].sockfd;
			ret = 3;
			break;	
		}

	}

	if(ret != -1)
	{
		
		return 0;
	}

}

/*----------------------------------------------------------------*/

/*
	lookupIFace:  get iface name form  socket fd and get iface ip and mac
*/
int lookupIFace(int sfd, char * iface, IPAddr *ip, MacAddr mac)
{
	if(sfd == -1)
	{
		return -1;
	}

	int i;
	for(i = 0; i < intr_cnt; i++) // get iface name form recived socket fd
	{
		if(link_socket[i].sockfd == sfd)
		{
			strcpy(iface, link_socket[i].ifacename);
			break;
		}
	}

	for(i = 0; i < intr_cnt; i++ )
	{
		if(strcmp(iface_list[i].ifacename , iface) == 0)
		{
			*ip = iface_list[i].ipaddr;
			memcpy(mac, iface_list[i].macaddr, sizeof(MacAddr));
			return 0;
		}
	}
	return -1;
}

/*----------------------------------------------------------------*/

int processEtherPkt(EtherPkt * r_ethPkt, int r_sfd, char * dsthost)
{
	
	IPAddr dstip = 0, s_ip = 0, nxtHopip = 0, r_ip = 0 ;
	MacAddr s_mac, r_mac;
	memset(r_mac , 0 , ETHER_ADDR_LEN);
	memset(s_mac, 0, ETHER_ADDR_LEN);

	char r_iface[32], s_iface[32];
	char ipstr[32], macstr[48];
	int s_sfd = -1;
	 
	
	
	if(r_sfd == STDIN_FILENO && r_ethPkt->type == TYPE_IP_PKT)
	{
		
		IP_PKT * r_ipPkt = malloc(sizeof(IP_PKT));
		deserIP_PKT(r_ethPkt->dat, r_ipPkt); //deser ip pkt
		
		routeMsg(r_ipPkt->dstip, &s_ip, &nxtHopip, s_mac, s_iface, &s_sfd);



		EtherPkt * s_ethPkt = malloc(sizeof(EtherPkt));
		IP_PKT * s_ipPkt = malloc(sizeof(IP_PKT));
		

		//building ip packet
		s_ipPkt->srcip = r_ipPkt->srcip; // IPAddr  srcip;
		s_ipPkt->dstip = r_ipPkt->dstip; // IPAddr  dstip;
		strcpy(s_ipPkt->data, r_ipPkt->data); // char    data[BUFSIZ];
		s_ipPkt->protocol = r_ipPkt->protocol; // short   protocol;
		s_ipPkt->sequenceno = r_ipPkt->sequenceno; // unsigned long    sequenceno;
		s_ipPkt->length = r_ipPkt->length; // short   length;

		char *ipPktBuf = malloc(sizeof(IP_PKT) + 1);
		serIP_PKT(ipPktBuf, s_ipPkt);

		memcpy(s_ethPkt->src, s_mac, ETHER_ADDR_LEN);

		int ek = arpLookUP(nxtHopip, s_ethPkt->dst); //fig out if it is arp or ip packet
		if(ek == -1)
		{
			s_ethPkt->type = TYPE_ARP_PKT;  //type = 0 : ARP frame  
			s_ethPkt->size = sizeof(ARP_PKT);
			s_ethPkt->dat = malloc(s_ethPkt->size + 1);

			ARP_PKT * s_arpPkt = malloc(sizeof(ARP_PKT)); //send arp packet
			s_arpPkt->op = ARP_REQUEST;
			s_arpPkt->srcip = s_ip;
			memcpy(s_arpPkt->srcmac, s_mac, ETHER_ADDR_LEN);
			s_arpPkt->dstip = nxtHopip;
			memcpy(s_arpPkt->dstmac, s_ethPkt->dst, ETHER_ADDR_LEN);

			char * s_arpBuf = (char *) calloc(sizeof(ARP_PKT)+1, sizeof(char));
			memset(s_arpBuf, 0, sizeof(ARP_PKT) + 1);
			serARP_PKT(s_arpBuf, s_arpPkt); //serialize arp packet

			//cpoy arp packet to ether packet data
			memcpy(s_ethPkt->dat, s_arpBuf, s_ethPkt->size);

			//Enter ip packet into pending que
			pushPQ(nxtHopip, s_ipPkt->dstip, ipPktBuf);

			writeEtherPkt(s_ethPkt, s_sfd);


			free(s_arpBuf);
			free(ipPktBuf);
			free(s_arpPkt);
			free(s_ethPkt->dat);
		}
		else
		{

			s_ethPkt->type = TYPE_IP_PKT; 	//type = 1 : IP frame  
			s_ethPkt->size = sizeof(IP_PKT);
			s_ethPkt->dat = (char*) malloc(s_ethPkt->size + 1); 
			memcpy(s_ethPkt->dat, ipPktBuf, s_ethPkt->size);

			writeEtherPkt(s_ethPkt, s_sfd);	
		}

		free(s_ethPkt);
		free(s_ipPkt);
		free(r_ipPkt);
	}
	else
	{
		int r = lookupIFace(r_sfd, r_iface, &r_ip, r_mac);
		if(r == -1)
		{
			printf("Invalid socket FD or interface does not exit\n");
		}

		
	}

	//look if the ether dst mac is same as our mac or broadcast mac
	if((memcmp(r_ethPkt->dst, r_mac, ETHER_ADDR_LEN ) == 0) || 
		(memcmp(r_ethPkt->dst, "\xFF\xFF\xFF\xFF\xFF\xFF", ETHER_ADDR_LEN ) == 0) )
	{
		MAC2STR(r_mac, macstr);
		printf("My MAC address is %s\n", macstr);
		printf("OK, this packet is for me!\n");
		EtherPkt * s_ethPkt = malloc(sizeof(EtherPkt));
		
		if(r_ethPkt->type == TYPE_IP_PKT) //ether type = 1 : IP frame  
		{
			printf("Received %d bytes IP frame\n", r_ethPkt->size);
			IP_PKT * r_ipPkt = malloc(sizeof(IP_PKT));
			deserIP_PKT(r_ethPkt->dat, r_ipPkt); //deser ip pkt

			if(r_ipPkt->dstip == r_ip && ROUTER == 0)
			{
				//print ip pkt
				printf("\nThis message is from host %s\n", dsthost );
				printf("Content:  %s\n",r_ipPkt->data);

				printIP_PKT(r_ipPkt);
				return 0;
	
			}
			else if(r_ipPkt->dstip != r_ip && ROUTER == 1)
			{
				//fprward msg
				printf("Forward it..\n");
				routeMsg(r_ipPkt->dstip, &s_ip, &nxtHopip, s_mac, s_iface, &s_sfd);

				printf("next hop interface = %s\n", s_iface);
				IP2STR(nxtHopip, ipstr);
				printf("next hop ip = %s\n", ipstr);
				printf("IP packet is ready to send: \n");

				//create send ether packet
				EtherPkt * s_ethPkt = malloc(sizeof(EtherPkt));
				IP_PKT * s_ipPkt = malloc(sizeof(IP_PKT));
				
				//building ip packet
				s_ipPkt->srcip = r_ipPkt->srcip; // IPAddr  srcip;
				s_ipPkt->dstip = r_ipPkt->dstip; // IPAddr  dstip;
				strcpy(s_ipPkt->data, r_ipPkt->data); // char    data[BUFSIZ];
				s_ipPkt->protocol = r_ipPkt->protocol; // short   protocol;
				s_ipPkt->sequenceno = r_ipPkt->sequenceno; // unsigned long    sequenceno;
				s_ipPkt->length = r_ipPkt->length; // short   length;

				char *ipPktBuf = malloc(sizeof(IP_PKT) + 1);
				serIP_PKT(ipPktBuf, s_ipPkt);

				printIP_PKT(s_ipPkt);

				memcpy(s_ethPkt->src, s_mac, ETHER_ADDR_LEN);

				int ek = arpLookUP(nxtHopip, s_ethPkt->dst); //fig out if it is arp or ip packet
				if(ek == -1)
				{
					s_ethPkt->type = TYPE_ARP_PKT;  //type = 0 : ARP frame  
					s_ethPkt->size = sizeof(ARP_PKT);
					s_ethPkt->dat = malloc(s_ethPkt->size + 1);

					ARP_PKT * s_arpPkt = malloc(sizeof(ARP_PKT)); //send arp packet
					s_arpPkt->op = ARP_REQUEST;
					s_arpPkt->srcip = s_ip;
					memcpy(s_arpPkt->srcmac, s_mac, ETHER_ADDR_LEN);
					s_arpPkt->dstip = nxtHopip;
					memcpy(s_arpPkt->dstmac, s_ethPkt->dst, ETHER_ADDR_LEN);

					char * s_arpBuf = (char *) calloc(sizeof(ARP_PKT)+1, sizeof(char));
					memset(s_arpBuf, 0, sizeof(ARP_PKT) + 1);
					serARP_PKT(s_arpBuf, s_arpPkt); //serialize arp packet

					//cpoy arp packet to ether packet data
					memcpy(s_ethPkt->dat, s_arpBuf, s_ethPkt->size);

					//Enter ip packet into pending que
					pushPQ(nxtHopip, s_ipPkt->dstip, ipPktBuf);

					writeEtherPkt(s_ethPkt, s_sfd);


					free(s_arpBuf);
					free(ipPktBuf);
					free(s_arpPkt);
					free(s_ethPkt->dat);
				}
				else
				{

					s_ethPkt->type = TYPE_IP_PKT; 	//type = 1 : IP frame  
					s_ethPkt->size = sizeof(IP_PKT);
					s_ethPkt->dat = (char*) malloc(s_ethPkt->size + 1); 
					memcpy(s_ethPkt->dat, ipPktBuf, s_ethPkt->size);

					writeEtherPkt(s_ethPkt, s_sfd);

					
				}

				free(s_ethPkt);
				free(s_ipPkt);
				
			}
			free(r_ipPkt);
	
		}
		else if (r_ethPkt->type == TYPE_ARP_PKT) //ether type = 0 : ARP frame  
		{ 

			printf("Received %d bytes ARP frame\n", r_ethPkt->size);
			ARP_PKT * r_arpPkt = malloc(sizeof(ARP_PKT)); // receive arp packet
			deserARP_PKT(r_ethPkt->dat, r_arpPkt); //deser arp packet

			if(r_arpPkt->dstip == r_ip && (memcmp(r_arpPkt->dstmac, r_mac, ETHER_ADDR_LEN ) == 0 ||
					memcmp(r_arpPkt->dstmac, "\xFF\xFF\xFF\xFF\xFF\xFF", ETHER_ADDR_LEN ) == 0 ))
			{
				Arpc r_arpc;
				r_arpc.ipaddr = r_arpPkt->srcip;
				memcpy(r_arpc.macaddr, r_arpPkt->srcmac, 6);
				time(&r_arpc.createTime);		
				pushARPC(r_arpc);

				char * s_arpBuf = (char *) calloc(sizeof(ARP_PKT)+1, sizeof(char));
				memset(s_arpBuf, 0, sizeof(ARP_PKT) + 1);

				if(r_arpPkt->op == ARP_REQUEST)
				{
					ARP_PKT * s_arpPkt = malloc(sizeof(ARP_PKT)); //send arp packet
				
					//set arp packet
					s_arpPkt->op = ARP_RESPONSE;
					s_arpPkt->srcip = r_ip;
					memcpy(s_arpPkt->srcmac, r_mac, ETHER_ADDR_LEN);
					s_arpPkt->dstip = r_arpPkt->srcip;
					memcpy(s_arpPkt->dstmac, r_arpPkt->srcmac, ETHER_ADDR_LEN);

					serARP_PKT(s_arpBuf, s_arpPkt); //serialize arp packet

					//set ether packet
					memcpy(s_ethPkt->dst, s_arpPkt->dstmac, ETHER_ADDR_LEN);
					memcpy(s_ethPkt->src, s_arpPkt->srcmac, ETHER_ADDR_LEN);
					s_ethPkt->type = TYPE_ARP_PKT;
					s_ethPkt->size = sizeof(ARP_PKT);
					s_ethPkt->dat = malloc(s_ethPkt->size + 1);
					memcpy(s_ethPkt->dat, s_arpBuf, s_ethPkt->size);

					s_sfd = r_sfd;

					IP2STR(s_arpPkt->srcip, ipstr);
					printf("It's an ARP request, asking who has this IP: %s \nand I have this IP!\n", ipstr);
					printf("Sent %d bytes ARP reply!\n", s_ethPkt->size);
					writeEtherPkt(s_ethPkt, s_sfd);

					free(s_arpBuf);
					free(s_arpPkt);
					free(r_arpPkt);
					free(s_ethPkt->dat);
				}
				else if(r_arpPkt->op == ARP_RESPONSE)
				{
					memcpy(s_ethPkt->dst, r_arpPkt->srcmac, ETHER_ADDR_LEN);
					memcpy(s_ethPkt->src, r_arpPkt->dstmac, ETHER_ADDR_LEN);
					s_ethPkt->type = TYPE_IP_PKT;
					s_ethPkt->size = sizeof(IP_PKT);
					s_ethPkt->dat = malloc(s_ethPkt->size + 1);

					s_sfd = r_sfd;

					char * s_ipBuf = malloc(sizeof(IP_PKT)+1);
					
					int pnd = 0;
					
					while(true)
					{
						pnd = popPQ(r_arpPkt->srcip , s_ipBuf);
						if(pnd != -1)
						{
							memcpy(s_ethPkt->dat, s_ipBuf, s_ethPkt->size);
							IP2STR(r_arpPkt->srcip, ipstr);
							MAC2STR(s_ethPkt->dst, macstr);
							printf("It's an ARP reply, saying the MAC address of %s is %s\n", ipstr, macstr);
							printf("Sent %d bytes pending IP pkt!\n", s_ethPkt->size);
							writeEtherPkt(s_ethPkt, s_sfd);

						}
						else
						{
							break;
						}
					}
					free(s_ipBuf);
					free(s_ethPkt->dat);	
				}
			} // end of r_arpPkt->dstip == r_ip
		} //end of ARP type
		free(s_ethPkt);
	}
	return 0;
}

/*----------------------------------------------------------------*/


int arpLookUP(IPAddr dstip, MacAddr dstmac)
{
	ARP_LIST *arplist = arp_cache;
	time_t crtTime;
	time(&crtTime);

	if(arp_cache == NULL)
	{
		memcpy(dstmac, "\xFF\xFF\xFF\xFF\xFF\xFF", ETHER_ADDR_LEN );
		return -1; 
	}
	else
	{
		while(arplist != NULL)
		{
			if(arplist->arp_item->ipaddr == dstip)
			{
				memcpy(dstmac , arplist->arp_item->macaddr, ETHER_ADDR_LEN); 
				arplist->arp_item->createTime = crtTime;
				return 0;
			}

			arplist = arplist->next;
		}
		memcpy(dstmac, "\xFF\xFF\xFF\xFF\xFF\xFF", ETHER_ADDR_LEN );
		return -1;
	}
	
}

/*----------------------------------------------------------------*/

int pushARPC(Arpc arpc)
{
	//Enter arpc to arplist
	ARP_LIST * tempARPl = arp_cache;

	ARP_LIST * lastARPl = malloc(sizeof(ARP_LIST));
	lastARPl->arp_item = malloc(sizeof(Arpc));
	memcpy(lastARPl->arp_item ,  &arpc, sizeof(Arpc));
	lastARPl->next = NULL;

	if(tempARPl == NULL)
	{
		arp_cache = lastARPl;
		return 0;
	}
	// Traverse the list to find the last node
	while(tempARPl != NULL)
	{
		//&& memcmp(tempARPl->arp_item->macaddr, arpc.macaddr, ETHER_ADDR_LEN ) == 0 
		if(tempARPl->arp_item->ipaddr == arpc.ipaddr && arpc.ipaddr  != 0)
		{	
			memcpy(tempARPl->arp_item->macaddr, arpc.macaddr, 6);
			time(&tempARPl->arp_item->createTime);
			free(lastARPl->arp_item);
			free(lastARPl);
			return 0;
		}

		if(tempARPl->next !=  NULL)
		{
			tempARPl = tempARPl->next;
		}
		else 
		{
			tempARPl->next = lastARPl;
			return 0;
		}
	}
	return -1;
	
}

/*----------------------------------------------------------------*/

int removeExpARPC()
{
	time_t crtTime;
	time(&crtTime);
	int ttl = 0;

	//Enter arpc to arplist
	ARP_LIST * currentARPl = arp_cache;
	ARP_LIST * prevARPl = NULL;


	if(currentARPl == NULL)
	{
		return -1;
	}
	else
	{
		// Traverse the list to find the last node
		while(currentARPl != NULL )
		{
			ttl = MacTimeout - ((int) difftime(crtTime, currentARPl->arp_item->createTime));
			if (ttl > 0 )
			{
				if(currentARPl->next != NULL)
				{
					prevARPl = currentARPl;
				}
				currentARPl = currentARPl->next;
			}
			else
			{
				printf("One entry in ARP-cache timed out\n");
				if(prevARPl != NULL )
				{
					// pop arpc from the list
					prevARPl->next = currentARPl->next;
				}
				else if(currentARPl->next != NULL)
				{
					arp_cache = currentARPl->next;
				}
				else
				{
					arp_cache = NULL;
				}

				free(currentARPl->arp_item);
				free(currentARPl);

				return 0;
			}	
		}
		return -1;
	}
}

/*----------------------------------------------------------------*/

int pushPQ(IPAddr nxtHopip, IPAddr dstip, char *pndPKT )
{
	//Enter packet into pending que
	PENDING_QUEUE * pq  = pending_queue;
	PENDING_QUEUE * pqEntry  = malloc(sizeof(PENDING_QUEUE));

	pqEntry->next_hop_ipaddr = nxtHopip;
	pqEntry->dst_ipaddr = dstip;
	pqEntry->pending_pkt = malloc(sizeof(IP_PKT)+1);
	memcpy(pqEntry->pending_pkt, pndPKT, sizeof(IP_PKT)+1);
	pqEntry->pending_pkt[sizeof(IP_PKT)] = '\0';
	pqEntry->next = NULL;

	if(pq == NULL)
	{
		pending_queue = pqEntry;
	}
	else
	{
		while(pq->next != NULL) // Traverse the list to find the last node
		{
			pq = pq->next;
		}
		pq->next = pqEntry; // Add pqNode to the end of the list
	}
	return 0;
}

/*----------------------------------------------------------------*/

int popPQ(IPAddr dstip, char *pndPKT)
{
	//Enter packet into pending que
	PENDING_QUEUE * p_PQEntry  = NULL;
	PENDING_QUEUE * c_PQEntry  = pending_queue;
	int x = 0;
	
	if(pending_queue == NULL)
	{
		return -1;
	}
	else
	{
		// Traverse the list to find the last node
		while(c_PQEntry != NULL )
		{
			if (c_PQEntry->next_hop_ipaddr == dstip ) // || c_PQEntry->dst_ipaddr  == dstip
			{
				memcpy(pndPKT, c_PQEntry->pending_pkt, sizeof(IP_PKT));
				if(p_PQEntry != NULL ) //if more than 1 item in the list
				{
					p_PQEntry->next = c_PQEntry->next; // pop pq from the list
				}
				else if(c_PQEntry->next != NULL) //when the 1st one of the list is being poped and additional item exist
				{
					pending_queue = c_PQEntry->next;
				}
				else // only 1 item exists and it being poped
				{
					pending_queue = NULL;
				}
				free(c_PQEntry->pending_pkt);
				free(c_PQEntry);
				return sizeof(IP_PKT);
				
			}
			else
			{
				if(c_PQEntry->next != NULL)
				{
					p_PQEntry = c_PQEntry;
				}

				c_PQEntry = c_PQEntry->next;
			}	
		}
		return -1;
	}
}

/*----------------------------------------------------------------*/

void writeEtherPkt(EtherPkt * s_ethPkt, int s_sfd)
{
	char * s_ethBuf = (char *) calloc(sizeof(EtherPkt) + s_ethPkt->size + 1 , sizeof(char));
	memset(s_ethBuf, 0, sizeof(EtherPkt) + s_ethPkt->size + 1);

	serETH_PKT(s_ethBuf, s_ethPkt);
	s_ethBuf[sizeof(EtherPkt) + s_ethPkt->size] = '\0';

	write(s_sfd, s_ethBuf, (sizeof(EtherPkt) + s_ethPkt->size + 1) );
	free(s_ethBuf);
}

/*----------------------------------------------------------------*/

void printARP()
{
	ARP_LIST * tempARP_cache = arp_cache;
	char ipstr[32], macstr[48];
	time_t crtTime;
	time(&crtTime);
	int ttl = 0;
	printf("\n*************************************************\n");
	printf("\t\tARP Cache..\n");
	printf("*************************************************\n");

	if(tempARP_cache == NULL)
	{
		printf("EMPTY\n");
	}
	while(tempARP_cache != NULL)
	{
		ttl =  MacTimeout - ((int) difftime(crtTime, tempARP_cache->arp_item->createTime));

		IP2STR(tempARP_cache->arp_item->ipaddr, ipstr);
		MAC2STR(tempARP_cache->arp_item->macaddr, macstr);
		//128.252.11.23 0:0:c:4:52:27     TTL 42
		printf(" %s\t %s \tTTL  %d\n", ipstr, macstr, ttl);
		tempARP_cache = tempARP_cache->next;
	}
	
	printf("*************************************************\n\n");


}

/*----------------------------------------------------------------*/

void printPQ()
{
	printf("\n*************************************************\n");
	printf("\t\tPending Queue..\n");
	printf("*************************************************\n\n");
	
	PENDING_QUEUE  * travPQ ;
	
	travPQ = pending_queue;
	char  ipnexthop[32], dst_ip[32];

	while(travPQ != NULL)
	{
		IP_PKT *pq_ipPKT = malloc(sizeof(IP_PKT)+1); //ip packet
	
		deserIP_PKT(travPQ->pending_pkt, pq_ipPKT);	
		IP2STR(travPQ->next_hop_ipaddr, ipnexthop);
		IP2STR(travPQ->dst_ipaddr, dst_ip);

		printf("next_hop_ipaddr: %s\tdst_ipaddr: %s\n",ipnexthop, dst_ip );
		printf("*************************************************\n");
		printIP_PKT(pq_ipPKT);
		
		travPQ = travPQ->next;
		//pq_ipPKT = NULL;
		free(pq_ipPKT);
	}
	printf("*************************************************\n\n");

}

/*----------------------------------------------------------------*/

void printIface()
{
	printf("\n*************************************************\n");
	printf("\t\tInterface List..\n");
	printf("*************************************************\n");

	char buf[32];
	MacAddr macStr;
	int i;
	for(i = 0; i < intr_cnt; i++)
	{
		char ipStr[32], maskStr[32];
		strcpy(ipStr, inet_ntop(AF_INET, &iface_list[i].ipaddr , buf , INET_ADDRSTRLEN) );
		strcpy(maskStr, inet_ntop(AF_INET, &iface_list[i].mask , buf , INET_ADDRSTRLEN) );
		strcpy(macStr, buf);
		strcpy(buf , "");

		printf("%s  %s  %s  %02x:%02x:%02x:%02x:%02x:%02x  %s\n",
			iface_list[i].ifacename,
			ipStr,
			maskStr,
			iface_list[i].macaddr[0], iface_list[i].macaddr[1],iface_list[i].macaddr[2],
			iface_list[i].macaddr[3],iface_list[i].macaddr[4],iface_list[i].macaddr[5],
			iface_list[i].lanname );
	}
	printf("*************************************************\n\n");

}

/*----------------------------------------------------------------*/

void printRtable()
{
	//parse the routing table file
	char buf[32];
	char destSubNet_Str[32], nextHop_Str[32], mask_Str[32], ifaceName_Str[32];

	printf("\n*************************************************\n");
	printf("\t\tRouting Table..\n");
	printf("*************************************************\n");

	int rt;
	for (rt = 0; rt < rt_cnt; rt++)
	{
		strcpy(destSubNet_Str, inet_ntop(AF_INET, &rt_table[rt].destsubnet , buf , INET_ADDRSTRLEN) );
		strcpy(nextHop_Str, inet_ntop(AF_INET, &rt_table[rt].nexthop , buf , INET_ADDRSTRLEN) ); 
		strcpy(mask_Str, inet_ntop(AF_INET, &rt_table[rt].mask, buf , INET_ADDRSTRLEN) );
		strcpy(ifaceName_Str,  rt_table[rt].ifacename );

		printf("%s  %s  %s  %s\n", destSubNet_Str, nextHop_Str, mask_Str, ifaceName_Str );
	}
	printf("*************************************************\n\n");
      
}

/*----------------------------------------------------------------*/

void printHost()
{
	//parse the hosts file
	printf("\n*************************************************\n");
	printf("\t\tHosts..\n");
	printf("*************************************************\n");

	char buf[32];  
	int hst;
	for (hst = 0; hst < hostcnt; hst++)
	{
		char hostIP_Str[32], hostname[32];
		strcpy(hostIP_Str, inet_ntop(AF_INET, &host[hst].addr , buf , INET_ADDRSTRLEN) );
		printf("%s\t\t%s \n", host[hst].name, hostIP_Str);
	}
	printf("*************************************************\n\n");

}

/*----------------------------------------------------------------*/