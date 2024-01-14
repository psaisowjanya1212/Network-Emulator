//Name 	: Nikhta Chamarti (nc20ce)
//		: Sai Sowjanya Padamati (sp22bi)

/*----------------------------------------------------------------*/
#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <strings.h>
#include <unistd.h>
#include <netinet/ether.h>
#include <net/ethernet.h>

#include "ether.h"
#include "ip.h"
#include "utils.h"
/*----------------------------------------------------------------*/

#define MAXLANLEN 32
#define MAXFILELEN 256
#define MAXBUFLEN 8232 //chaning this size reduces the 0000 prints
//changes the MAX buf Size from 512 to 8232

/*----------------------------------------------------------------*/
//function declaration
void createlanName_Addr(char *lanname, const char* bAddr);
void createlanName_port(char *lanname,  int bPort);
void unlinkFiles(char *lanname, char * flag);
void init_SLtable();
int lookupSlTable(MacAddr mac);
int addSlTable(MacAddr mac, int port, int sckfd);
void printSL();
int removeExp_SL();
/*----------------------------------------------------------------*/

int main (int argc, char *argv[])
{
	//atexit(freeMemory);
	if (argc != 3) {
			fprintf(stderr, "Use : bridge lan-name num-ports\n");
			exit(0);
		}

	int numPorts = 0;
	char *lanName = "\0";

	lanName = argv[1];
	numPorts = atoi(argv[2]);

	Host bridgeHost;
	struct hostent *he;
	struct in_addr **addr_list;
	int h;

	h = gethostname(bridgeHost.name, sizeof(bridgeHost.name));
	if (h != 0)
	{
		perror("Error getting hostname");
		exit(1);
	}

	he = gethostbyname(bridgeHost.name);
	if (he == NULL) 
	{
		perror("Error getting host details ");
		exit(1);
	}

	addr_list = (struct in_addr **)he->h_addr_list;
	bridgeHost.addr = (addr_list[0])->s_addr;

	int bridgeSockfd, rec_sock, writefd = -1;
	socklen_t bridgeSockLen, rec_len;
	int bridgeSockVector[MAXINTER];
	int numCon = 0;
	struct sockaddr_in bridgeAddr , rec_addr;
	bzero((char*) &bridgeAddr, sizeof(bridgeAddr));
	
	char buf[MAXBUFLEN];
	char outputStr[MAXBUFLEN] = "\0";
	fd_set bridgeSet, rec_set;
	int maxfd;
	int x, y, z;
	struct sockaddr_in rec_addrList[numPorts];

	bridgeSockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(bridgeSockfd < 0)
	{
		perror("Failed to create a TCP socket");
		exit(1);
	}

	bridgeAddr.sin_addr.s_addr = bridgeHost.addr;
	bridgeAddr.sin_family = AF_INET;
	bridgeAddr.sin_port = htons(0); //assigns unused port number
	bridgeSockLen = sizeof(bridgeAddr); 

	x = bind(bridgeSockfd, (struct sockaddr *) &bridgeAddr, bridgeSockLen);
	if(x < 0)
	{
		perror("Socket binding Failed");
		exit(1);
	} 

	y = listen(bridgeSockfd , numPorts);
	if(y < 0)
	{
		perror("Listen Failed");
		exit(1);
	}

	int bPort = 0;
	char* bAddr;

	getsockname(bridgeSockfd, (struct sockaddr *) &bridgeAddr, &bridgeSockLen);
	bAddr = inet_ntoa(bridgeAddr.sin_addr);
	bPort = ntohs(bridgeAddr.sin_port);

	printf("Bridge created on %s:%d\n", bAddr, bPort);
	
	createlanName_Addr(lanName, bAddr);
	createlanName_port(lanName, bPort);

	//accept connections;
	FD_ZERO(&bridgeSet);
	FD_SET(STDIN_FILENO, &bridgeSet);
	FD_SET(bridgeSockfd, &bridgeSet);
	if (bridgeSockfd > STDIN_FILENO)
	{ 
		maxfd = bridgeSockfd + 1;
	}
	else maxfd = STDIN_FILENO + 1;

	memset(bridgeSockVector, -1, sizeof(bridgeSockVector));
	
	init_SLtable();

	char * token ;
	char cmd1[32], cmd2[32], destination[32], msg[MAXBUFLEN];
	const char delim[2] = " ";

	while (1)
	{
		rec_set = bridgeSet;

		struct timeval timeout;
		timeout.tv_sec = 2; // Set timeout
		timeout.tv_usec = 0;

		select(maxfd + 1 , &rec_set, NULL, NULL, &timeout);

		timeout.tv_sec = 2; // Set timeout
		timeout.tv_usec = 0;

		if (FD_ISSET(bridgeSockfd, &rec_set))
		{

			rec_sock = accept(bridgeSockfd, (struct sockaddr *)(&rec_addr), &bridgeSockLen );
			if(rec_sock < 0)
			{
				if (errno == EINTR)
				{
					continue;
				}
				else 
				{
					perror("Accept error");
					exit(1);
				}				
			}
			else if (rec_sock >= 0 && rec_sock < MAXINTER) 
			{
				
				//bridgeSockVector[rec_sock] = rec_sock;
				
				if(numCon < numPorts)
				{		
					rec_addrList[rec_sock] = rec_addr;

					strcpy(outputStr, "accept");
					write(rec_sock, outputStr, sizeof(outputStr));

					printf("\nAccept a new host %s on sockfd %d, port %d! \n",
					inet_ntoa(rec_addr.sin_addr), rec_sock, ntohs(rec_addr.sin_port));
					numCon++;
				}
			}
			else
			{
				strcpy(outputStr, "reject");
				write(rec_sock, outputStr, sizeof(outputStr));

			}

			/*Find an empty slot in the sock_vector*/
			int i;
			for (i = 0; i < MAXINTER; i++) 
			{
				if (bridgeSockVector[i] == -1) 
				{
					bridgeSockVector[i] = rec_sock;
					break;
				}
			}

			FD_SET(rec_sock, &bridgeSet);
			if (rec_sock > maxfd)
			{
				maxfd = rec_sock;
			}

		}

		int a;
		for (a = 0; a < MAXINTER; a++) 
		{
			int fd = bridgeSockVector[a];
			if (fd == -1) 
			{
				continue;
			}
			if (FD_ISSET(fd, &rec_set))
			{
				writefd = -1;
				int num = read(fd, buf, MAXBUFLEN);
				
				rec_addr = rec_addrList[fd]; // Get the correct client address
				char recIP_STR[32], ipbuf[32];
				strcpy(recIP_STR, inet_ntop(AF_INET, &(rec_addr.sin_addr), ipbuf, INET_ADDRSTRLEN));
				
				if (num == 0) 
				{	
					printf( "Host(%s) on port %d disconnected\n", recIP_STR, ntohs(rec_addr.sin_port));
					close(fd);
					FD_CLR(fd, &rec_set);
					bridgeSockVector[a] = -1;
					numCon--;

				}
				else if (num < 0) 
				{
					perror(": read error");
					exit(1);
				}
				else 
				{
					if(num < sizeof(EtherPkt))
					{
						continue;
					}
				 	buf[num] = '\0';  // Ensure null-termination for string data

					rec_addr = rec_addrList[a]; // Get the correct client address //a is num of connections
					EtherPkt *ethPKT = malloc(sizeof(EtherPkt)); //ether packet
					ARP_PKT  *arpPKT = malloc(sizeof(ARP_PKT)); // arp packet
					IP_PKT  *ipPKT = malloc(sizeof(IP_PKT)); // arp packet

					deserETH_PKT(buf , ethPKT); 
					printf("\nReceived %d bytes Ethernet Header!\n", sizeof(EtherPkt));

					addSlTable(ethPKT->src,  ntohs(rec_addr.sin_port) , fd);

					if ((writefd = lookupSlTable(ethPKT->dst)) == -1)
					{
						memcpy(ethPKT->dst, "\xFF\xFF\xFF\xFF\xFF\xFF", ETHER_ADDR_LEN);
					}
					

					if(ethPKT->type == TYPE_ARP_PKT)
					{
						//arp packet
						deserARP_PKT( ethPKT->dat, arpPKT); 						
						printf("Received %d bytes ARP Frame!\n", ethPKT->size); // not the correct value

					}
					else if(ethPKT->type == TYPE_IP_PKT)
					{
						//ip packet
						deserIP_PKT( ethPKT->dat, ipPKT);
						printf("Received %d bytes IP Frame!\n", ethPKT->size); // not the correct value
					}

					char * s_etherBuf = malloc(sizeof(buf) +1);
					if(s_etherBuf != NULL )
					{
						memcpy(s_etherBuf, buf, (sizeof(buf) + 1));
					}
					else{
						printf("memory allocation issue");
					}

					if(writefd > 0)
					{
						printf("%d: sent %d bytes!\n", writefd,  ethPKT->size);
						write(writefd, s_etherBuf, (sizeof(EtherPkt) + ethPKT->size + 1));
					}
					else 
					{

						int i ;
						for (i = 0; i < MAXINTER; i++)
						{
							int broadcastfd = bridgeSockVector[i];
							if (broadcastfd != -1 && broadcastfd != fd)
							{	
								printf("%d: sent %d bytes!\n", broadcastfd,  ethPKT->size);
								write(broadcastfd, s_etherBuf, (sizeof(EtherPkt) + ethPKT->size+1));
							}
						}
					}
										
					// printf("BRIDGE> ");
					// fflush(stdout);
					free(s_etherBuf);
					free(ipPKT);
					free(arpPKT);
					free(ethPKT->dat);
					free(ethPKT);

				}//end of else num > 0
			}
		}// for recieve socket end

		if (FD_ISSET(STDIN_FILENO, &rec_set))
		{
			if (fgets(buf, MAXBUFLEN, stdin) == NULL)
			{
				exit(0);
			} 
			else //input success
			{
				token = strtok(buf, delim);
				int x = 0 ;

				while(token != NULL) 
				{
				if(x == 0)
				{
					strcpy(cmd1, token);
				}
				else if(x == 1)
				{
					strcpy(cmd2, token);
				}
				token = strtok(NULL, delim);
				x++;
				}
				if(strcmp(cmd1, "show") == 0 && strcmp(cmd2, "sl\n") == 0) 
				{
					//show the contents of self-learning table
					printSL();

				}       
				else if(strcmp(cmd1, "quit\n") == 0)
				{
					// close the router
					//printf( "QUIT\n");
					//unlink files .cs files
					unlinkFiles(lanName, "addr");
					unlinkFiles(lanName, "port");
					close(bridgeSockfd);
					exit(0);
				}

			} //  input success end
		} // (FD_ISSET(STDIN_FILENO, &rec_set)) end
		

		maxfd = bridgeSockfd;
		int i ;
		for ( i = 0; i < numPorts; i++)
		{
			if (bridgeSockVector[i] != -1 && bridgeSockVector[i] > maxfd)
			{
				maxfd = bridgeSockVector[i];
			}

		} //for end
		removeExp_SL();
	}  //while end

} // main end


/*----------------------------------------------------------------*/


//create a file lan-name.addr to store the brige's IP address
void createlanName_Addr(char *lanname, const char *bAddr )
{
  char addrFileName[MAXFILELEN] = "\0";
  char *sym_AddrLink;

  //address file name
  strcpy(addrFileName, ".");
  strcat(addrFileName, lanname);
  strcat(addrFileName, ".addr");

  int err = symlink(bAddr, addrFileName);
  if (err != 0) {
    perror("symlink");
    exit(1);
  }
}


/*----------------------------------------------------------------*/


//create a file lan-name.port to store the briges port number
void createlanName_port(char *lanname, int bPort)
{
  char portFileName[MAXFILELEN] = "\0";
  char portStr[MAXBUFLEN];

  //port filename
  strcpy(portFileName, ".");
  strcat(portFileName, lanname);
  strcat(portFileName, ".port");

  sprintf(portStr, "%d", bPort);
  int err = symlink(portStr, portFileName);
  if (err != 0) {
    perror("symlink");
    exit(1);
  }

}

/*----------------------------------------------------------------*/

void unlinkFiles(char *lanname, char * flag)
{
	char fileName[MAXFILELEN] = "";

	if(flag == "port")
	{
		//port filename
		strcpy(fileName, ".");
		strcat(fileName, lanname);
		strcat(fileName, ".port");
	}
	else if(flag == "addr")
	{
		strcpy(fileName, ".");
		strcat(fileName, lanname);
		strcat(fileName, ".addr");
	}
	
	int  ret = unlink(fileName);
	if (ret == -1) {
		perror("Unlink failed");
	}
}

/*----------------------------------------------------------------*/


int lookupSlTable(MacAddr mac)
{
	//look for mac address and return port number
	time_t crtTime;
	int ttl;

	time(&crtTime);
	int sl;

	for(sl = 0; sl < MAXINTER; sl++)
	{
		if(memcmp(sl_table[sl].mac, mac, 6) == 0)
		{ 
			sl_table[sl].lstConTime = crtTime;
			return sl_table[sl].sockfd;
		}
	}
	return -1;
}

/*----------------------------------------------------------------*/

void init_SLtable()
{
	int i;
	for( i = 0; i < MAXINTER ; i++)
	{
		memcpy(sl_table[i].mac, "\x00\x00\x00\x00\x00\x00", ETHER_ADDR_LEN);
		sl_table[i].port = 0;
		sl_table[i].sockfd = -1;
		sl_table[i].lstConTime = 0;
	}
}	

/*----------------------------------------------------------------*/


int addSlTable(MacAddr  mac, int port, int sckfd)
{
	time_t crtTime;
	time(&crtTime);
	int i;
	for( i = 0; i < MAXINTER ; i++)
	{
		
		if(memcmp(sl_table[i].mac, mac, 6) == 0)
		{
			//memcpy(sl_table[i].mac, mac, ETHER_ADDR_LEN);
			sl_table[i].port = port;
			sl_table[i].sockfd = sckfd;
			sl_table[i].lstConTime = crtTime;
			return 0;
		}
	}
	for( i = 0; i < MAXINTER ; i++)
	{
		if(memcmp(&sl_table[i].mac, "\x00\x00\x00\x00\x00\x00", 6) == 0)
		{
			memcpy(sl_table[i].mac, mac, ETHER_ADDR_LEN);
			sl_table[i].port = port;
			sl_table[i].sockfd = sckfd;
			sl_table[i].lstConTime = crtTime;
			return 0;
		}
	}
	
	return 0;
}

/*----------------------------------------------------------------*/

//pop from  sl_table
int removeExp_SL()
{
	time_t crtTime;
	time(&crtTime);

	int ttl = 0;
	char macStr[48];
	

	int i;
	for( i = 0; i < MAXINTER ; i++)
	{

		if(sl_table[i].sockfd != -1)
		{
			MAC2STR(sl_table[i].mac, macStr);
			ttl = MacTimeout - ((int) difftime(crtTime, sl_table[i].lstConTime));
			
			if(ttl <= 0)
			{
				printf("One entry in SL table timed out:\n");

				printf("MAC address: %s\tPort: %d\t Sockfd: %d\tTTL: %d\t\n", macStr, sl_table[i].port , 
						sl_table[i].sockfd, ttl);
				printf("\n");

				memcpy(&sl_table[i].mac, "\x00\x00\x00\x00\x00\x00", ETHER_ADDR_LEN);
				sl_table[i].port = 0;
				sl_table[i].sockfd = -1;
				sl_table[i].lstConTime = 0;	
			}
		}

	}
	return 0;
	
}

/*----------------------------------------------------------------*/

void printSL()
{	
	char macStr[48];
	time_t crtTime;
	int ttl;
	
	time(&crtTime);

	printf("\n*************************************************\n");
	printf("\t\tSelf-Learning Table\n\t\tMAC address/port mappings\n");
	printf("*************************************************\n");
	printf("SHOW SL\n");
	printf("*************************************************\n");

	int sl;
	for(sl = 0; sl < MAXINTER; sl++)
	{
		MAC2STR(sl_table[sl].mac, macStr);
		ttl = MacTimeout - (difftime(crtTime, sl_table[sl].lstConTime));
		if( memcmp(sl_table[sl].mac, "\x00\x00\x00\x00\x00\x00", 6) != 0) 
		{
			printf("MAC address: %s\tPort: %d\t Sockfd: %d\tTTL: %d\t\n", macStr, sl_table[sl].port , 
				sl_table[sl].sockfd, ttl); // MAC address: 0:0:c:4:52:27    Port: 1  Sockfd: 5       TTL: 15
		}
	}

	printf("*************************************************\n");
}

/*----------------------------------------------------------------*/