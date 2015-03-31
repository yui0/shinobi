#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <sys/time.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

#define  PRINT_MAC_ADDR(ether_addr_octet)     {         \
        int i;                                          \
        for ( i =0; i < 6; i++){                        \
            printf("%02x",(ether_addr_octet[i]));       \
            if(i != 5)                                  \
                printf(":");                            \
        }                                               \
    }

struct ethhdr_vlan
{
  unsigned char   h_dest[6];    /* destination eth addr */
  unsigned char   h_source[6];  /* source ether addr    */
  unsigned int    h_vlan;       /* VLAN */
  unsigned short  h_proto;      /* packet type ID field */
};


void print_header(char *frame);
void forward_frame();
int sockB;
int sockA;
int sockA_ifidx;
int sockB_ifidx;

//#define DEBUG
//#define DEBUG_IP_LEVEL
//#define DEBUG_TCP_LEVEL

//Frame forwarding procedure
void forward_frame()
{
  int rll_size;
  struct sockaddr_ll rll;
  fd_set fds, readfds;
  char buffer[8000];
  ssize_t frame_len, send_len;
  struct sockaddr_ll sll;
  int max_fd;

  FD_ZERO(&readfds);
  FD_SET(sockA, &readfds);
  FD_SET(sockB, &readfds);

  max_fd = max(sockA, sockB) + 1;

  while(1){
    memcpy(&fds, &readfds, sizeof(fd_set));

    if(select(max_fd, &fds , NULL, NULL, NULL) < 0 ){
      perror("select");
      exit(0);
    }

    // Check the buffer of sockA
    if(FD_ISSET(sockA, &fds)){
      memset(&rll, 0, sizeof(rll));
      rll_size = sizeof(rll);

      // Read one frame from sockA
      if((frame_len=recvfrom(sockA, &buffer, sizeof(buffer), 0, (struct sockaddr *)&rll, &rll_size))<0){
	perror("recvfrom");
	exit(1);
      }

      // Forward the frame if the frame is not sent by the same interface
      if(rll.sll_pkttype!=PACKET_OUTGOING){
	memset(&sll, 0, sizeof(sll));
	sll.sll_family=AF_PACKET;
	sll.sll_halen=ETH_ALEN;
	sll.sll_ifindex = sockB_ifidx;
	if((send_len=sendto(sockB, &buffer, frame_len, 0, (struct sockaddr *)&sll, sizeof(sll)))<0){
	  perror("send sockB :");
	  exit(1);
	}else{
	  #ifdef DEBUG
	  printf("forward %d bytes to IF%d\n", send_len, sockB_ifidx);
	  #endif
	}
      }
    }

    // Check the buffer of sockB
    if(FD_ISSET(sockB, &fds)){
      memset(&rll, 0, sizeof(rll));
      rll_size = sizeof(rll);

      // Read one frame from sockB
      if((frame_len=recvfrom(sockB, &buffer, sizeof(buffer), 0, (struct sockaddr *)&rll, &rll_size))<0){
	perror("recvfrom");
	exit(1);
      }

      // Forward the frame if the frame is not sent by the same interface
      if(rll.sll_pkttype!=PACKET_OUTGOING){
	memset(&sll, 0, sizeof(sll));
	sll.sll_family=AF_PACKET;
	sll.sll_halen=ETH_ALEN;
	sll.sll_ifindex = sockA_ifidx;
	if((send_len=sendto(sockA, &buffer, frame_len, 0, (struct sockaddr *)&sll, sizeof(sll)))<0){
	  perror("send AockA");
	  exit(1);
	}else{
	  #ifdef DEBUG
	  printf("forward %d bytes to IF%d\n", send_len, sockA_ifidx);
	  #endif
	}
      }
    }

    #ifdef DEBUG

    printf("FromInterface = %d\n", rll.sll_ifindex);
    printf("PacketType = %d\n", rll.sll_pkttype);
    printf("HeaderType = %d\n", rll.sll_hatype);
    printf("Frame Size = %d\n", frame_len);

    print_header(&buffer);

    #endif
  }
}



void print_header(char *frame)
{
  struct ethhdr *p_ether;
  struct iphdr *p_ip;
  struct tcphdr *p_tcp;
  struct in_addr insaddr,indaddr;

  p_ether=(struct ethhdr *)frame;
  p_ip=(struct iphdr *)(frame + sizeof(struct ethhdr));
  p_tcp=(struct tcphdr *)(frame + sizeof(struct ethhdr) + sizeof(struct iphdr));

  printf("Ether SrcAdr = ");
  PRINT_MAC_ADDR(p_ether->h_source);
  puts("");
  printf("Ether DstAdr = ");
  PRINT_MAC_ADDR(p_ether->h_dest);
  puts("");
  printf("Ether Type = %d\n", ntohs(p_ether->h_proto));

  insaddr.s_addr = p_ip->saddr;
  indaddr.s_addr = p_ip->daddr;

  /*
    char *tmp;
    unsigned int t;
    tmp=(char *)&buffer;

    for(t=0;t<30;t++)
    printf("%d, %x\n", t, tmp[t]);

    tmp=(char *)p_ip;

    puts("--------------");

    for(t=0;t<20;t++)
    printf("%d, %x\n", t, tmp[t]);

    exit(1);
  */

  #ifdef DEBUG_IP_LEVEL
  printf("----IP Header--------------------\n");
  printf("version     : %u\n",p_ip->version);
  printf("ihl         : %u\n",p_ip->ihl);
  printf("tos         : %u\n",p_ip->tos);
  printf("tot length  : %u\n",ntohs(p_ip->tot_len));
  printf("id          : %u\n",ntohs(p_ip->id));
  printf("frag_off    : %u\n",p_ip->frag_off & 8191);
  printf("ttl         : %u\n",p_ip->ttl);
  printf("protocol    : %u\n",p_ip->protocol);
  printf("check       : 0x%x\n",ntohs(p_ip->check));
  printf("saddr       : %s\n",inet_ntoa(insaddr));
  printf("daddr       : %s\n",inet_ntoa(indaddr));

  #ifdef DEBUG_TCP_LEVEL
  if(p_ip->protocol == IPPROTO_TCP){
    printf("----TCP Header-------------------\n");
    printf("source port : %u\n",ntohs(p_tcp->source));
    printf("dest port   : %u\n",ntohs(p_tcp->dest));
    printf("sequence    : %u\n",ntohl(p_tcp->seq));
    printf("ack seq     : %u\n",ntohl(p_tcp->ack_seq));
    printf("frags       :");
    p_tcp->fin ? printf(" FIN") : 0 ;
    p_tcp->syn ? printf(" SYN") : 0 ;
    p_tcp->rst ? printf(" RST") : 0 ;
    p_tcp->psh ? printf(" PSH") : 0 ;
    p_tcp->ack ? printf(" ACK") : 0 ;
    p_tcp->urg ? printf(" URG") : 0 ;
    printf("\n");
    printf("window      : %u\n",ntohs(p_tcp->window));
    printf("check       : 0x%x\n",ntohs(p_tcp->check));
    printf("urt_ptr     : %u\n\n\n",p_tcp->urg_ptr);
  }
  #endif
  #endif
}

main(int argc, char *argv[]) {
  struct ifreq ifr;
  struct packet_mreq mreq;
  struct sockaddr_ll sa;

  if(argc < 3){
    printf("Usage: %s interface1 interface2\n", argv[0]);
    printf("Exmaple: %s eth0 eth2\n", argv[0]);
    exit(1);
  }

  // SocketA
  if((sockA = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0 ){
    perror("socket");
    exit(0);
  }

  // Bind
  memset(&ifr, 0, sizeof(struct ifreq));
  strcpy(ifr.ifr_name, argv[1]);
  if(ioctl(sockA, SIOCGIFINDEX, &ifr) < 0 ){
    perror("ioctl SIOCGIFINDEX");
    exit(0);
  }

  sockA_ifidx=ifr.ifr_ifindex;
  sa.sll_family=AF_PACKET;
  sa.sll_protocol=htons(ETH_P_ALL);
  sa.sll_ifindex=ifr.ifr_ifindex;
  if(bind(sockA, (struct sockaddr *)&sa, sizeof(sa)) < 0){
    //Error
    perror("bind 1");
    close(sockA);
    exit(0);
  }

  // Set promiscuous mode
  mreq.mr_type = PACKET_MR_PROMISC;
  mreq.mr_ifindex = ifr.ifr_ifindex;
  mreq.mr_alen = 0;
  mreq.mr_address[0] ='\0';
  if(setsockopt(sockA, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0){
    perror("setsockopt");
    exit(0);
  }


  // SocketB
  if ((sockB = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0 ){
    perror("socket");
    exit(0);
  }

  // Bind
  memset(&ifr, 0, sizeof(struct ifreq));
  strcpy(ifr.ifr_name, argv[2]);
  if(ioctl(sockB, SIOCGIFINDEX, &ifr) < 0 ){
    perror("ioctl SIOCGIFINDEX");
    exit(0);
  }

  sockB_ifidx=ifr.ifr_ifindex;
  sa.sll_family=AF_PACKET;
  sa.sll_protocol=htons(ETH_P_ALL);
  sa.sll_ifindex=ifr.ifr_ifindex;
  if(bind(sockB, (struct sockaddr *)&sa, sizeof(sa)) < 0){
    //Error
    perror("bind 0");
    close(sockB);
    exit(0);
  }

  // Set promiscuous mode
  mreq.mr_type = PACKET_MR_PROMISC;
  mreq.mr_ifindex = ifr.ifr_ifindex;
  mreq.mr_alen = 0;
  mreq.mr_address[0] ='\0';
  if(setsockopt(sockB, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0){
    perror("setsockopt");
    exit(0);
  }

  printf("Interface1 (SockA) : %s (index %d)\n", argv[1], sockA_ifidx);
  printf("Interface2 (SockB) : %s (index %d)\n", argv[2], sockB_ifidx);

  forward_frame();
}
