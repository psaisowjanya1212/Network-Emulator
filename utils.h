#include "ether.h"
#include "ip.h"


void IP2STR(IPAddr ip, char * ipstr)
{
	char ipbuf[32];
	strcpy(ipstr, inet_ntop(AF_INET, &ip , ipbuf , INET_ADDRSTRLEN) );
}

void printMAC(MacAddr mac)
{
	printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void MAC2STR(MacAddr mac , char * macstr)
{
    sprintf(macstr, "%02x:%02x:%02x:%02x:%02x:%02x",
	mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int serIP_PKT(char *buffer, const IP_PKT *ipPKT)
{
	unsigned long offset = 0 ;

	uint32_t dstip_n  = htonl(ipPKT->dstip);
	uint32_t srcip_n = htonl(ipPKT->srcip);
	uint16_t protocol_n = htons(ipPKT->protocol);
	uint32_t sequenceno_n = htonl(ipPKT->sequenceno);
	uint16_t length_n  = htons(ipPKT->length);

    // Copy data to the buffer
    memcpy(buffer, &dstip_n, sizeof(uint32_t));
	offset += sizeof(uint32_t);
    memcpy(buffer + offset, &srcip_n, sizeof(uint32_t));
	offset += sizeof(uint32_t);
    memcpy(buffer + offset, &protocol_n, sizeof(uint16_t));
	offset += sizeof(uint16_t);
    memcpy(buffer + offset, &sequenceno_n, sizeof(uint32_t));
	offset += sizeof(uint32_t);
    memcpy(buffer + offset, &length_n, sizeof(uint16_t));
	offset += sizeof(uint16_t);
    memcpy(buffer + offset, ipPKT->data, sizeof(ipPKT->data));
	offset += sizeof(ipPKT->data);
	return offset;

}

// Function to deserialize a character array into an IP packet structure
void deserIP_PKT(const char *buffer,  IP_PKT *ipPkt) 
{
	unsigned long offset = 0 ;

    // Copy data from the buffer to the structure
    memcpy(&ipPkt->dstip, buffer, sizeof(uint32_t));
	offset += sizeof(uint32_t);
    memcpy(&ipPkt->srcip, buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);
    memcpy(&ipPkt->protocol, buffer + offset, sizeof(uint16_t));
	offset += sizeof(uint16_t);
    memcpy(&ipPkt->sequenceno, buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);
    memcpy(&ipPkt->length, buffer + offset, sizeof(uint16_t));
	offset += sizeof(uint16_t);
    memcpy(ipPkt->data, buffer + offset, sizeof(ipPkt->data));
	//ipPkt->data[ipPkt->length] = '\0';

    // Convert fields back to host byte order
    ipPkt->dstip = ntohl(ipPkt->dstip);
    ipPkt->srcip = ntohl(ipPkt->srcip);
    ipPkt->protocol = ntohs(ipPkt->protocol);
    ipPkt->sequenceno = ntohl(ipPkt->sequenceno);
    ipPkt->length = ntohs(ipPkt->length);

}


void serARP_PKT(char *buffer, const ARP_PKT * arpPKT)
{
	unsigned long offset = 0 ;

	uint16_t op_n = htons(arpPKT->op);
	uint32_t srcip_n = htonl(arpPKT->srcip);
	uint32_t dstip_n  = htonl(arpPKT->dstip);

    // Copy data to the buffer

    memcpy(buffer + offset, &op_n, sizeof(uint16_t));
	offset += sizeof(uint16_t);
    memcpy(buffer + offset, &srcip_n, sizeof(IPAddr));
	offset += sizeof(IPAddr);
    memcpy(buffer + offset, &dstip_n, sizeof(IPAddr));
	offset += sizeof(IPAddr);
	memcpy(buffer + offset, arpPKT->srcmac, sizeof(MacAddr));
	offset += sizeof(MacAddr);
    memcpy(buffer + offset, arpPKT->dstmac, sizeof(MacAddr));
    // offset += sizeof(MacAddr);
    // buffer[offset] = '\0';


}

void deserARP_PKT(const char *buffer, ARP_PKT * arpPKT)
{
	unsigned long offset = 0 ;
	uint16_t op_n;
	uint32_t srcip_n;
	uint32_t dstip_n;

    // Copy data from the buffer to the structure
    memcpy(&op_n, buffer + offset, sizeof(uint16_t));
	offset += sizeof(uint16_t);
    memcpy(&srcip_n, buffer + offset, sizeof(IPAddr));
	offset += sizeof(IPAddr);
    memcpy(&dstip_n, buffer + offset, sizeof(IPAddr));
	offset += sizeof(IPAddr);
    memcpy(&arpPKT->srcmac, buffer + offset, sizeof(MacAddr));
	offset += sizeof(MacAddr);
    memcpy(&arpPKT->dstmac, buffer + offset, sizeof(MacAddr));
	offset += sizeof(MacAddr);

    // Convert fields back to host byte order
	arpPKT->op = ntohs(op_n);
    arpPKT->srcip = ntohl(srcip_n);
	arpPKT->dstip = ntohl(dstip_n);
}


void serETH_PKT(char *buffer, const EtherPkt *ethPKT)
{
	unsigned long offset = 0 ;

	uint16_t type_n = htons(ethPKT->type);
	uint16_t size_n = htons(ethPKT->size);

    // Copy data to the buffer
    memcpy(buffer + offset, ethPKT->dst, sizeof(MacAddr));
	offset += sizeof(MacAddr);
    memcpy(buffer + offset, ethPKT->src, sizeof(MacAddr));
	offset += sizeof(MacAddr);
	memcpy(buffer + offset, &type_n, sizeof(uint16_t));
	offset += sizeof(uint16_t);
    memcpy(buffer + offset, &size_n, sizeof(uint16_t));
	offset += sizeof(uint16_t);
	memcpy(buffer + offset, ethPKT->dat, ethPKT->size);
	// offset += (ethPKT->size + 1);
	// buffer[offset] = '\0';
	//memcpy(buffer + offset, "\0", 1);

}

void deserETH_PKT(const char *buffer, EtherPkt *ethPkt)
{
	unsigned long offset = 0 ;
	uint16_t type_n = 0;
	uint16_t size_n = 0;

    // Copy data from the buffer to the structure  
    memcpy(ethPkt->dst, buffer + offset, sizeof(MacAddr) );
	offset += sizeof(MacAddr);
    memcpy(ethPkt->src, buffer + offset, sizeof(MacAddr));
	offset += sizeof(MacAddr);
	memcpy(&type_n, buffer + offset, sizeof(uint16_t));
    ethPkt->type = ntohs(type_n);   // Convert fields back to host byte order
	offset += sizeof(uint16_t);
    memcpy(&size_n, buffer + offset, sizeof(uint16_t));
    ethPkt->size = ntohs(size_n);   // Convert fields back to host byte order
	offset += sizeof(uint16_t);
	ethPkt->dat = malloc(ethPkt->size + 1);
    memcpy(ethPkt->dat, buffer + offset, ethPkt->size);
	//ethPkt->dat = strdup(buffer + offset);

}




void printIP_PKT(IP_PKT *ipPkt)
{
	if (ipPkt == NULL) 
	{
        printf("Error: Null pointer passed to printIP_PKT\n");
        return;
    }
	char ipdst[32], ipsrc[32];

	IP2STR(ipPkt->dstip, ipdst);
	IP2STR(ipPkt->srcip, ipsrc);
	
	printf("*************************************************\n");
	printf("\t\tIP Packet\n");
	printf("*************************************************\n");
	printf("dstip: %s\t\tsrcip: %s\n", ipdst, ipsrc);

	if (ipPkt->data != NULL) 
	{
        printf("data:  %s\n", ipPkt->data);
    } 
	else 
	{
        printf("data:  (null)\n");
    }

	printf("*************************************************\n");



}

void printARP_PKT(ARP_PKT * arpPkt)
{
	if (arpPkt == NULL) 
	{
        printf("Error: Null pointer passed to printARP_PKT\n");
        return;
    }

	char ipdst[32], ipsrc[32], srcmac[48], dstmac[48];
	
	IP2STR(arpPkt->srcip, ipsrc);
	MAC2STR(arpPkt->srcmac, srcmac);
	IP2STR(arpPkt->dstip, ipdst);
	MAC2STR(arpPkt->dstmac, dstmac);
	
	printf("*************************************************\n");
	printf("\t\tARP Packet\n");
	printf("*************************************************\n");
	printf("op: %hd\n", arpPkt->op);
	printf("dstip: %s\t\tsrcip: %s\n", ipdst, ipsrc);
	printf("dstmac: %s\t\tsrcmac: %s\n",dstmac ,srcmac);
	printf("*************************************************\n");
}

void printETH_PKT(EtherPkt *ethPkt)
{
	if (ethPkt == NULL) 
	{
        printf("Error: Null pointer passed to printETH_PKT\n");
        return;
    }

	char srcmac[48], dstmac[48];
	MAC2STR(ethPkt->dst, dstmac);
	MAC2STR(ethPkt->src, srcmac);
	
	printf("*************************************************\n");
	printf("\t\tETHER Packet\n");
	printf("*************************************************\n");
	printf("dstmac: %s\t\tsrcmac: %s\n",dstmac ,srcmac);
	printf("type: %hd\tsize: %hd\n", ethPkt->type, ethPkt->size );
	printf("data:\n");

	char * dataBUF = malloc(ethPkt->size + 1);
	memcpy(dataBUF, ethPkt->dat, ethPkt->size);

	if(ethPkt->type == TYPE_ARP_PKT)
	{
		ARP_PKT * arp = malloc(sizeof(ARP_PKT));
		deserARP_PKT(dataBUF, arp);
		printARP_PKT(arp);
		free(arp);
	}
	else if(ethPkt->type == TYPE_IP_PKT)
	{
		IP_PKT * ip = malloc(sizeof(IP_PKT));
		deserIP_PKT(dataBUF, ip);
		printIP_PKT(ip);
		free(ip);
	}
	printf("*************************************************\n");
	free(dataBUF);
}
