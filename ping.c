// icmp.cpp
// Robert Iakobashvili for Ariel uni, license BSD/MIT/Apache
//
// Sending ICMP Echo Requests using Raw-sockets.
//
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <unistd.h>
#include <resolv.h>
// IPv4 header len without options
#define IP4_HDRLEN 20
// ICMP header len for echo req
#define ICMP_HDRLEN 8
int timeCount = 0;
int pacrec = 0;  // packet recived
int pacsend = 0; // packet sends
float mintime = 99999999; // min time
float maxtime = 0; // max time
float total = 0; // total time
// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

void finish(int);

void finish(int sig)
{

    signal(sig, SIG_IGN); 
    float avr = total / (float)timeCount;//avarage time
    int pacloss = 100-((pacrec*100)/pacsend); // loos packet
    printf("\n");
    printf("%d packets transmitted, %d received, %d%c packet loss, time %fms\n"
    , pacsend, pacrec, pacloss, '%', total);
    printf("rtt min / avg / max  = %.3f / %.3f / %.3f ms\n", mintime, avr, maxtime);
    exit(0);
}

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}

int header(char packet[IP_MAXPACKET], struct icmp *icmphdr) // create header
{
    // char packetdata[IP_MAXPACKET] = *packet;
    char data[IP_MAXPACKET] = "This is the ping.\n";
    int datalen = strlen(data) + 1;
    (*icmphdr).icmp_type = ICMP_ECHO;
    (*icmphdr).icmp_code = 0;
    (*icmphdr).icmp_seq += 1;
    (*icmphdr).icmp_id = 18;
    (*icmphdr).icmp_cksum = 0;
    memcpy((packet), icmphdr, ICMP_HDRLEN);
    memcpy(packet + ICMP_HDRLEN, data, datalen);
    (*icmphdr).icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
    memcpy((packet), icmphdr, ICMP_HDRLEN);

    return datalen;
}

void ping(int sock, struct sockaddr_in *dest_in, char packet[IP_MAXPACKET], int datalen)//ping send
{

    int bytes_sent = sendto(sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)dest_in, sizeof((*dest_in)));
    pacsend++; // inc packet sent count
    if (bytes_sent == -1)
    {
        fprintf(stderr, "Send packet failes, eror: %d", errno);
        exit(0);
    }
}
ssize_t listener(int sock, struct sockaddr_in *dest_in, char packet[IP_MAXPACKET])//recive ping
{
    bzero(packet, IP_MAXPACKET);
    socklen_t len = sizeof(*dest_in);
    ssize_t bytes_received = -1;
    while ((bytes_received = recvfrom(sock, packet, IP_MAXPACKET, 0, (struct sockaddr *)dest_in, &len)))
    {
        if (bytes_received > 0)
        {

            pacrec++; // inc ping recived count
            return bytes_received;
        }
    }
    return 0;
}
int main(int count, char *argv[])
{
    if (count != 2) // set that command entered well
    {
        printf("usage: sudo ./parta <hostname>\n");
        exit(0);
    }
    int datalen = 0;
    struct icmp icmphdr; // ICMP-header
    icmphdr.icmp_seq = 0;
    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;
    dest_in.sin_addr.s_addr = inet_addr(argv[1]);
    int sock = -1;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) // create ping socket
    {
        fprintf(stderr, "Failed to creat socket: %d", errno);
        return -1;
    }
    signal(SIGINT, finish); // catch ctrl + c
    while (1)
    {
        char packet[IP_MAXPACKET] = {}; // packet buffer
        bzero(packet, IP_MAXPACKET);
        datalen = header(packet, &icmphdr); // create header
        struct timeval start, end;
        gettimeofday(&start, 0); // set start time
        ping(sock, &dest_in, packet, datalen); // send ping
        int bytes_received = listener(sock, &dest_in, packet); // recive ping
        struct iphdr *iphdr = (struct iphdr *)packet;
        struct icmp *icmpheader = (struct icmp *)(packet + (iphdr->ihl * 4));
        gettimeofday(&end, 0); // set end time
        char reply[IP_MAXPACKET];
        memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, datalen);
        float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n", bytes_received, argv[1],
               (*icmpheader).icmp_seq, iphdr->ttl, milliseconds); // print setting ping packet
        timeCount++;
        total += milliseconds;
        mintime = MIN(mintime, milliseconds);
        maxtime = MAX(maxtime, milliseconds);
        sleep(1);
    }
    // Close the raw socket descriptor.
    close(sock);
    return 0;
}
