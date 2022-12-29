/*****************************************************************************/
/*** myping.c                                                              ***/
/***                                                                       ***/
/*** Use the ICMP protocol to request "echo" from destination.             ***/
/*****************************************************************************/
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#define SOURCE_IP "10.0.2.15"
// i.e the gateway or ping to google.com for their ip-address
#define DESTINATION_IP "8.8.8.8"

#define PACKETSIZE 128
struct packet
{
	struct icmphdr hdr;
	char msg[PACKETSIZE - sizeof(struct icmphdr)];
};

int pid = -1;
int loops = 25;
struct protoent *proto = NULL;
int data_bytes;


/*--------------------------------------------------------------------*/
/*--- checksum - standard 1s complement checksum                   ---*/
/*--- Calculate checksum for ICMP packet (header and data)         ---*/
/*--------------------------------------------------------------------*/
unsigned short checksum(void *b, int len)
{
	unsigned short *buf = b;
	unsigned int sum = 0;
	unsigned short result;

	for (sum = 0; len > 1; len -= 2)
		sum += *buf++;
	if (len == 1)
		sum += *(unsigned char *)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}

/*--------------------------------------------------------------------*/
/*--- display - present echo info                                  ---*/
/*--------------------------------------------------------------------*/
void display(void *buf, int bytes)
{
	int i;
	struct iphdr *ip = buf;
	struct icmphdr *icmp = buf + ip->ihl * 4;
	int flag = 1;
	struct timeval start, end;
    //gettimeofday(&start, 0);
	clock_t startt = clock();
	
	//printf("----------------\n");
	char sourceIPAddrReadable[32] = {'\0'};
	inet_ntop(AF_INET, &ip->saddr, sourceIPAddrReadable, sizeof(sourceIPAddrReadable));
	char destinationIPAddrReadable[32] = {'\0'};
	inet_ntop(AF_INET, &ip->daddr, destinationIPAddrReadable, sizeof(destinationIPAddrReadable));
	clock_t endt = clock();
	//gettimeofday(&end, 0);
	float milliseconds = (endt - startt) * 1000.0f / CLOCKS_PER_SEC; 
	// inet_ntoa(ip->saddr)
	// inet_ntoa(ip->daddr)
	// Print packet IP, seq number, time
    printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n", bytes, sourceIPAddrReadable,
	 icmp->un.echo.sequence,ip->ttl, milliseconds);
	//printf("IPv%d: hdr-size=%d pkt-size=%d protocol=%d TTL=%d src=%s ",
	//	   ip->version, ip->ihl * 4, ntohs(ip->tot_len), ip->protocol,
	//	   ip->ttl, sourceIPAddrReadable);
	//printf("dst=%s\n", destinationIPAddrReadable);
	//if (icmp->un.echo.id == pid)
	//{
	//	printf("ICMP: type[%d/%d] checksum[%d] id[%d] seq[%d]\n",
	//		   icmp->type, icmp->code, ntohs(icmp->checksum),
	//		   icmp->un.echo.id, icmp->un.echo.sequence);
	//}
}

/*--------------------------------------------------------------------*/
/*--- listener - separate process to listen for and collect messages--*/
/*--------------------------------------------------------------------*/
void listener(void)
{
	int sd;
	struct sockaddr_in addr;
	unsigned char buf[1024];

	sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
	if (sd < 0)
	{
		perror("socket");
		exit(0);
	}
	for (;;)
	{
		int bytes, len = sizeof(addr);

		bzero(buf, sizeof(buf));
		bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &len);
		if (bytes > 0){
			display(buf, bytes);
		}
		else
			perror("recvfrom");
	}
	exit(0);
}

/*--------------------------------------------------------------------*/
/*--- ping - Create message and send it.                           ---*/
/*--------------------------------------------------------------------*/
void ping(struct sockaddr_in *addr)
{
	const int val = 255;
	int i, j, sd = 1;
	int cnt = 0;
	struct packet pckt;
	struct sockaddr_in r_addr;


	sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
	if (sd < 0)
	{
		perror("socket");
		return;
	}
	if (setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
		perror("Set TTL option");
	if (fcntl(sd, F_SETFL, O_NONBLOCK) != 0)
		perror("Request nonblocking I/O");
	
	for (j = 0; j < loops; j++) // send pings 1 per second
	{
		int len = sizeof(r_addr);
		//printf("Msg #%d\n", cnt);
		if (recvfrom(sd, &pckt, sizeof(pckt), 0, (struct sockaddr *)&r_addr, &len) < 0)
			printf("Msg");
			
		bzero(&pckt, sizeof(pckt));
		pckt.hdr.type = ICMP_ECHO;
		pckt.hdr.un.echo.id = pid;
		for (i = 0; i < sizeof(pckt.msg) - 1; i++)
			pckt.msg[i] = i + '0';
		pckt.msg[i] = 0;
		pckt.hdr.un.echo.sequence = cnt++;
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));
		if (sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr *)addr, sizeof(*addr)) <= 0)
			perror("sendto");
		sleep(1);
	}
}

/*--------------------------------------------------------------------*/
/*--- main - look up host and start ping processes.                ---*/
/*--------------------------------------------------------------------*/
int main(int count, char *argv[])
{
	struct hostent *hname;
	struct sockaddr_in addr;
	//loops = 0;
	if (count != 2)
	{
		printf("usage: %s <addr> \n", argv[0]);
		exit(0);
	}
	if (count == 2) // WE HAVE SPECIFIED A MESSAGE COUNT
		//loops = atoi(argv[1]);
		loops = 25;

	int icmp_header_size = sizeof(struct icmp);
	
	printf("PING %s (%s): %d data bytes\n", argv[1], argv[1], icmp_header_size);
	if (count > 1)
	{
		
		pid = getpid();
		// printf("id main process = %d\n", pid);
		proto = getprotobyname("ICMP");
		hname = gethostbyname(argv[1]);
		bzero(&addr, sizeof(addr));
		addr.sin_family = hname->h_addrtype;
		addr.sin_port = 0;
		addr.sin_addr.s_addr = *(long *)hname->h_addr;
		if (fork() == 0) /* child process */
		{
			// printf("child process with pid  = %d\n", (int) getpid());
			listener();
		}
		else /* parent process */
		{
			// printf("parent process with pid  = %d\n", (int) getpid());
			ping(&addr);
		}
		wait(0);
	}
	else
		printf("usage: sudo ./parta <hostname>\n");
	return 0;
}
