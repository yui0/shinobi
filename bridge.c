//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------
//	gcc bridge.c -o bridge
//---------------------------------------------------------

#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <linux/if_tun.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#include <sys/types.h>
#include <sys/socket.h>	// IFNAMSIZ
#include <netinet/in.h>
//#include <sys/ioctl.h>
#include <net/if.h>	// ifreq
#include <syslog.h>	// LOG_ERR

int use_log = 0;	// メッセージを STDERR でなく、syslog に出力する


int OpenEth(char *dev)
{
	struct ifreq ifr;
	struct sockaddr_in addr;
	int s;

	s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s < 0) return /*INVALID_SOCKET*/-1;

	memset(&ifr, 0, sizeof(ifr));
	if (*dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		print_err(LOG_ERR, "ioctl: %s\n", strerror(errno));
		close(s);
		return 0;
	}

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = PF_PACKET;
	addr.sin_port = htons(ETH_P_ALL);
	//addr.sin_ifindex = ifr.ifr_ifindex;
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		print_err(LOG_ERR, "bind: %s\n", strerror(errno));
		close(s);
		return 0;
	}

	/*if (local == false)
	{
		// プロミスキャスモードに設定する
		Zero(&ifr, sizeof(ifr));
		StrCpy(ifr.ifr_name, sizeof(ifr.ifr_name), name);
		if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
		{
			// 失敗
			closesocket(s);
			return NULL;
		}

		ifr.ifr_flags |= IFF_PROMISC;

		if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
		{
			// 失敗
			closesocket(s);
			return NULL;
		}
	}*/

	// accept() でブロックされるのを防ぐため、non-blocking mode に設定
#ifndef SHINO_WINDOWS
	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0)
#else
	if (ioctlsocket(s, FIONBIO, &param) < 0)
#endif
	{
//		SET_ERRNO();
		print_err(LOG_ERR, "Failed to set nonblock: %s (%d)\n", strerror(errno), errno);
		exit(1);
	}

	if (listen(s, 5) < 0) {
//		SET_ERRNO();
		print_err(LOG_ERR, "listen: %s\n", strerror(errno));
		exit(1);
	}

printf("[%d]\n",s);
	return s;
}


#define max(a,b)	((a)>(b) ? (a):(b))

int main(int argc, char *argv[])
{
	char buf[1600];
	int f1, f2, l, fm;
	fd_set fds;

	if (argc < 2) {
		printf("Usage: %s dev1 dev2\n", argv[0]);
		exit(1);
	}

	f1 = OpenEth(argv[1]);
	f2 = OpenEth(argv[2]);

	fm = max(f1, f2) + 1;

	while (1) {
		FD_ZERO(&fds);
		FD_SET(f1, &fds);
		FD_SET(f2, &fds);

		select(fm, &fds, NULL, NULL, NULL);

		if (FD_ISSET(f1, &fds)) {
			l = read(f1, buf, sizeof(buf));
			write(f2, buf, l);
			printf("%d\n", l);
		}
		if (FD_ISSET(f2, &fds)) {
			l = read(f2, buf, sizeof(buf));
			write(f1, buf, l);
			printf("%d\n", l);
		}
	}
}
