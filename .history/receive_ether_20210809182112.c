#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <unistd.h>

/*
Destination MAC : it should be defined accroding to situation
*/
#define DEST_MAC0	0xbe
#define DEST_MAC1	0x81
#define DEST_MAC2	0xbe
#define DEST_MAC3	0x9a
#define DEST_MAC4	0xb9
#define DEST_MAC5	0x9f

/* Default type of ethernet frame */
#define ETHER_TYPE	0x0900

#define DEFAULT_IF	"eth0"
#define BUF_SIZ		1500
#define DATA_SIZ 	720

void printf2(uint16_t n) {
    uint16_t i = 0;
    for(i = 0; i < 16; i++) {
        if(n & (0x8000) >> i) {
            printf("1");
        }else {
            printf("0");
        }
    }
    printf("\n");
}

void write_data_recv(uint16_t data[], int numbytes) {
    FILE *fw = fopen("data_recv.bin", "wb");
    for (int i = 0; i < numbytes; i++)
    {   
        fwrite((data+i), sizeof(uint16_t), 1, fw);
    }

    fclose(fw);
}

int main(int argc, char *argv[])
{
	char sender[INET6_ADDRSTRLEN];
	int sockfd, ret, i;
	int sockopt;
	ssize_t numbytes;       /* numbers of bytes */
	struct ifreq ifopts;	/* set promiscuous mode */
	struct ifreq if_ip;	    /* get ip addr */
	struct sockaddr_storage their_addr;
	uint8_t buf[BUF_SIZ] = {0};   /* storage receive bytes frome socket*/
	uint16_t data_recv[DATA_SIZ] = {0};	/* storage data after byte combination */
	char ifName[IFNAMSIZ];  /* which interface it receive */
	
	/* Get interface name (for paticular interface) */
	if (argc > 1)
		strcpy(ifName, argv[1]);
	else
		strcpy(ifName, DEFAULT_IF);

	/* Header structures */
	struct ether_header *eh = (struct ether_header *) buf;
	struct iphdr *iph = (struct iphdr *) (buf + sizeof(struct ether_header));
	struct udphdr *udph = (struct udphdr *) (buf + sizeof(struct iphdr) + sizeof(struct ether_header));

	memset(&if_ip, 0, sizeof(struct ifreq));

	/* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */

	// printf("open PF_PACKET socket fd:%d\n",sockfd);

	if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETHER_TYPE))) == -1) {
		perror("listener: socket");	
		return -1;
	}

	/* Set interface to promiscuous mode - Maybe it's useless to do this every time */

	// printf("set interface to p mode\n");

	strncpy(ifopts.ifr_name, ifName, IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);
    
	/* Allow the socket to be reused - incase connection is closed prematurely */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
		perror("setsockopt");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	/* Bind to device */
	if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifName, IFNAMSIZ-1) == -1)	{
		perror("SO_BINDTODEVICE");
		close(sockfd);
		exit(EXIT_FAILURE);
	}

repeat:	
    
    printf("listener: Waiting to recvfrom...\n");
	// printf("sockfd: %d\n",sockfd);

	numbytes = recvfrom(sockfd, buf, BUF_SIZ, 0, NULL, NULL);
	// printf("numbytes:%ld\n",numbytes);
	printf("listener: got packet %lu bytes\n", numbytes);

	/* Check the packet is for me */
	if (eh->ether_dhost[0] == DEST_MAC0 &&
			eh->ether_dhost[1] == DEST_MAC1 &&
			eh->ether_dhost[2] == DEST_MAC2 &&
			eh->ether_dhost[3] == DEST_MAC3 &&
			eh->ether_dhost[4] == DEST_MAC4 &&
			eh->ether_dhost[5] == DEST_MAC5) {
		printf("Correct destination MAC address\n");
	} else {
		printf("Wrong destination MAC: %x:%x:%x:%x:%x:%x\n",
						eh->ether_dhost[0],
						eh->ether_dhost[1],
						eh->ether_dhost[2],
						eh->ether_dhost[3],
						eh->ether_dhost[4],
						eh->ether_dhost[5]);
		ret = -1;
		goto done;
	}

	/* Get source IP */
	((struct sockaddr_in *)&their_addr)->sin_addr.s_addr = iph->saddr;
	inet_ntop(AF_INET, &((struct sockaddr_in*)&their_addr)->sin_addr, sender, sizeof sender);

	/* Look up my device IP addr if possible */
	strncpy(if_ip.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFADDR, &if_ip) >= 0) { /* if we can't check then don't */
		printf("Source IP: %s\n My IP: %s\n", sender, 
				inet_ntoa(((struct sockaddr_in *)&if_ip.ifr_addr)->sin_addr));
		/* ignore if I sent it */
		if (strcmp(sender, inet_ntoa(((struct sockaddr_in *)&if_ip.ifr_addr)->sin_addr)) == 0)	{
			printf("but I sent it :(\n");
			ret = -1;
			goto done;
		}
	}

	/* UDP payload length */
	ret = ntohs(udph->len) - sizeof(struct udphdr);

	// for (i=0; i<numbytes; i++) {
	// 	buf[i] = (buf[i]&0XFF)*256 + (buf[i]>>8);
	// }

	/* Print packet */
	printf("\tData:");
	for(i=0; i<DATA_SIZ - 1; i++) {
		data_recv[i] = buf[16+i*2] + buf[17+i*2]*256;
		printf2(data_recv[i]);
	}
	printf("\n");
	

	/* Write Data */
	write_data_recv(data_recv, DATA_SIZ-1);

done:	goto repeat;

	close(sockfd);
	return ret;
}