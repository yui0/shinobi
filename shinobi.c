//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------
//	仮想 NIC のユーザプロセスのデーモン。
//	デバイスドライバ（TUN/TAP）からのデータを仮想ハブ（shinobi_hub）
//	に転送する。また、仮想ハブからのデータをデバイスドライバに渡す。
//
//	gcc shinobi.c shinobi_socket.c utils.c -o shinobi
//
// HOST A: (192.168.100.1)
//	modprobe tun
//	shinobi_hub -d 4
//	shinobi -d 4
//	ifconfig shino0 192.168.1.1 up
//	#ifconfig shino0 ether 8:0:20:0:0:1 (MAC)
// HOST B:
//	shinobi -h 192.168.100.1:80 -d 4
//	ifconfig shino0 192.168.1.2 up
//	ping 192.168.1.1 (Firewall 解除しておく)
//
// Usage: shinobi [ -i instance] [-h hub[:port]] [ -p proxy[:port]] [-d level]
//  引数:
//	-i instance	shino デバイスのインスタンス番号
//			指定されなければ、デフォルトで 0(=shino0)。
//
//	-h hub[:port]	仮想ハブが動作するホストを指定する。
//			指定されなければ、デフォルトで localhost:80。
//			コロン(:)の後にポート番号が指定されていれば
//			そのポート番号に接続にいく。デフォルトは 80。
//
//	-p proxy[:port]	経由するプロキシサーバを指定する。
//			デフォルトではプロキシサーバは使われない。
//			コロン(:)の後にポート番号が指定されていれば
//			そのポート番号に接続にいく。デフォルトは 80。
//
//	-d level	デバッグレベル。1 以上にした場合はフォアグランド
//			で実行され、標準エラー出力にデバッグ情報が
//			出力される。デフォルトは 0。
//---------------------------------------------------------

#ifdef SHINO_WINDOWS
#include <winsock2.h>   /* for windows */
#include <windows.h>    /* for windows */
#include <winioctl.h>   /* for windows */
#include <io.h>
#include "getopt_win.h"
#else
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include "shinobi.h"
#include "ssl.h"	// for ssl_close ONLY

#ifdef  SHINO_WINDOWS
HANDLE  hLog;		// デバッグログ用のファイルハンドル
#define IFNAMSIZ 16	// from net/if.h
WSAEVENT EventArray[2];	// socket とドライバ用の 2 つの Event 配列
#endif

int debuglevel = 0;	// デバッグレベル。1 以上にした場合はフォアグランドで実行される
int use_log = 0;	// メッセージを STDERR でなく、syslog に出力する


#ifdef  SHINO_WINDOWS
#pragma warning(disable: 4996)
#pragma comment(lib, "Advapi32.lib")

#define TAP_CONTROL_CODE(request,method) \
  CTL_CODE (FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

#define TAP_IOCTL_SET_MEDIA_STATUS \
  TAP_CONTROL_CODE (6, METHOD_BUFFERED)

#define BUFMAX 2048

// ネットワークデバイス表示名からデバイス GUID 文字列を検索
CHAR *GetNetWorkDeviceGuid(CONST CHAR *pDisplayName, CHAR *pszBuf, DWORD cbBuf)
{
	#define BUFSZ 256
	CONST CHAR *SUBKEY = "SYSTEM\\CurrentControlSet\\Control\\Network";

	// HKLM\SYSTEM\\CurrentControlSet\\Control\\Network\{id1]\{id2}\Connection\Name が
	// ネットワークデバイス名（ユニーク）の格納されたエントリであり、{id2} がこのデバイスの GUID である
	HKEY hKey1, hKey2, hKey3;
	LONG nResult;
	DWORD dwIdx1, dwIdx2;
	CHAR szData[64], *pKeyName1, *pKeyName2, *pKeyName3, *pKeyName4;
	DWORD dwSize, dwType = REG_SZ;
	BOOL bDone = FALSE;
	FILETIME ft;

	hKey1 = hKey2 = hKey3 = NULL;
	pKeyName1 = pKeyName2 = pKeyName3 = pKeyName4 = NULL;

	// 主キーのオープン
	nResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, SUBKEY, 0, KEY_ALL_ACCESS, &hKey1);
	if (nResult != ERROR_SUCCESS) {
		printf("GetNetWorkDeviceGuid: open key err HKLM%s\n", SUBKEY);
		return NULL;
	}
	pKeyName1 = (CHAR*)malloc(BUFSZ);
	pKeyName2 = (CHAR*)malloc(BUFSZ);
	pKeyName3 = (CHAR*)malloc(BUFSZ);
	pKeyName4 = (CHAR*)malloc(BUFSZ);

	dwIdx1 = 0;
	while (bDone != TRUE) { // {id1} を列挙するループ
		dwSize = BUFSZ;
		nResult = RegEnumKeyEx(hKey1, dwIdx1++, pKeyName1,
				&dwSize, NULL, NULL, NULL, &ft);
		if (nResult == ERROR_NO_MORE_ITEMS) break;

		// SUBKEY\{id1} キーをオープン
		sprintf(pKeyName2, "%s\\%s", SUBKEY, pKeyName1);
		nResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pKeyName2,
				0, KEY_ALL_ACCESS, &hKey2);
		if (nResult != ERROR_SUCCESS) continue;
		dwIdx2 = 0;
		while (1) { // {id2} を列挙するループ
			dwSize = BUFSZ;
			nResult = RegEnumKeyEx(hKey2, dwIdx2++, pKeyName3,
					&dwSize, NULL, NULL, NULL, &ft);
			if (nResult == ERROR_NO_MORE_ITEMS) break;
			if (nResult != ERROR_SUCCESS) continue;

			// SUBKEY\{id1}\{id2]\Connection キーをオープン
			sprintf(pKeyName4, "%s\\%s\\%s",
					pKeyName2, pKeyName3, "Connection");
			nResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					pKeyName4, 0, KEY_ALL_ACCESS, &hKey3);
			if (nResult != ERROR_SUCCESS) continue;

			// SUBKEY\{id1}\{id2]\Connection\Name 値を取得
			dwSize = sizeof(szData);
			nResult = RegQueryValueEx(hKey3, "Name",
					0, &dwType, (LPBYTE)szData, &dwSize);

			if (nResult == ERROR_SUCCESS) {
				if (stricmp(szData, pDisplayName) == 0) {
					strcpy(pszBuf, pKeyName3);
					bDone = TRUE;
					break;
				}
			}
			RegCloseKey(hKey3);
			hKey3 = NULL;
		}
		RegCloseKey(hKey2);
		hKey2 = NULL;
	}

	if (hKey1) { RegCloseKey(hKey1); }
	if (hKey2) { RegCloseKey(hKey2); }
	if (hKey3) { RegCloseKey(hKey3); }

	if (pKeyName1) { free(pKeyName1); }
	if (pKeyName2) { free(pKeyName2); }
	if (pKeyName3) { free(pKeyName3); }
	if (pKeyName4) { free(pKeyName4); }

	// GUID を発見できず
	if (bDone != TRUE) return NULL;
	return pszBuf;
}

int tun_reader(shinostat_t *s)
{
	int res;
	OVERLAPPED olpd;

	olpd.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	while (1) {
		olpd.Offset = 0;
		olpd.OffsetHigh = 0;
		res = ReadFile(s->ste_fd, s->rdatabuf, STRBUFSIZE, (LPDWORD)&s->datalen, &olpd);
		if (!res) {
			WaitForSingleObject(olpd.hEvent, INFINITE);
			res = GetOverlappedResult(s->ste_fd, &olpd, (LPDWORD)&s->datalen, FALSE);
			if (read_drv(s) < 0) {
				/* socket にエラーが発生した模様。再接続に行く */
				/*CLOSE(s->sock.fd);
				s->sock.fd = -1;
				if ((s->sock.fd = open_socket(s, hub, proxy)) < 0) {*/
					print_err(LOG_ERR, "failed to re-open connection with hub\n");
				/*	goto err;
				}*/
			}
		}
	}

	return 0;
}

//---------------------------------------------------------
//	TAP ドライバー
//---------------------------------------------------------

int tun_open(char *dev)
{
	HANDLE hTap = NULL;
	UCHAR Buf[BUFMAX];
	CHAR szDevicePath[256];
	DWORD dwLen;
	ULONG status = TRUE;

	// 指定された表示名から TAP の GUID を得る
	if (!GetNetWorkDeviceGuid(dev, Buf, BUFMAX)) {
		print_err(LOG_ERR, "TAP-Win32: [%s] GUID is not found\n", dev);
		return -1;
	}
	print_err(LOG_NOTICE, "TAP-Win32: [%s] GUID = %s\n", dev, Buf);
	sprintf(szDevicePath, DEVICE_PATH_FMT, Buf);

	// TAP デバイスを開く
	hTap = CreateFile(szDevicePath, GENERIC_READ | GENERIC_WRITE,
		0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
	if (hTap == INVALID_HANDLE_VALUE) {
		print_err(LOG_ERR, "TAP-Win32: Failed to open [%s]", szDevicePath);
		return -1;
	}

	// TAP デバイスをアクティブに
	status = TRUE;
	if (!DeviceIoControl(hTap, TAP_IOCTL_SET_MEDIA_STATUS,
		&status, sizeof(status), &status, sizeof(status), &dwLen, NULL)) {
		print_err(LOG_ERR, "TAP-Win32: TAP_IOCTL_SET_MEDIA_STATUS err\n");
		CloseHandle(hTap);
		return -1;
	}

	// socket およびドライバのデータ受信通知用のイベントオブジェクトを作成
	EventArray[0] = CreateEvent(NULL, FALSE, FALSE, NULL); // Socket 用
	//EventArray[1] = CreateEvent(NULL, FALSE, FALSE, NULL); // ドライバ用

	//printf("%d",_open_osfhandle((long)hTap, _O_BINARY));
	return (int)hTap;
}
#else
/*#include <netpacket/packet.h>
int eth_fd;
//---------------------------------------------------------
//	実デバイスを開く
//---------------------------------------------------------

int OpenEth(char *dev)
{
	struct ifreq ifr;
	struct sockaddr_ll sa;
	int s;

	if ((s = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
		perror("socket");
		return -1;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	strcpy(ifr.ifr_name, dev);
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl SIOCGIFINDEX");
		close(s);
		return -1;
	}

	//sockA_ifidx = ifr.ifr_ifindex;
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(ETH_P_ALL);
	sa.sll_ifindex = ifr.ifr_ifindex;
	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind");
		close(s);
		return -1;
	}

	return s;
}*/


//---------------------------------------------------------
//	TAP ドライバー
//---------------------------------------------------------

int tun_open(char *dev)
{
	struct ifreq ifr;
	int fd, err;

	if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
		//perror("open");
		return -1;
	}

	/* Flag: IFF_TUN   - TUN device ( no ether header )
	 *       IFF_TAP   - TAP device
	 *       IFF_NO_PI - no packet information
	 */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP/*IFF_TUN*/ | IFF_NO_PI;
	if (*dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
		perror("TUNSETIFF");
		close(fd);
		return err;
	}
	strcpy(dev, ifr.ifr_name);
	return fd;
}
#endif


//---------------------------------------------------------
//	Ethernet Ⅱフレームのタイプ
//---------------------------------------------------------

char *EthII_Type(int t1, int t2)
{
	switch (t1*256+t2) {
	case 0x0800:
			return "IPv4(0x0800)";
	case 0x0806:
			return "ARP(0x0806)";
	case 0x86dd:
			return "IPv6(0x86dd)";
	}
	return "Unknown";
}


//---------------------------------------------------------
//	shino ドライバからデータを読み込み、仮想 HUB に転送する
//---------------------------------------------------------

int read_drv(shinostat_t *stedstat)
{
	stehead_t steh;
	int flags = 0;
	int pad = 0;	/* パディング */
	int remain = 0;	/* 全データ長を 4 で割った余り */
	int ret;
	uchar_t *sendbuf  = stedstat->sendbuf;	/* Socket 送信バッファ*/
	uchar_t *rdatabuf = stedstat->rdatabuf;	/* ドライバからの読み込み用バッファ */
	uchar_t *sendp;	/* Socket 送信バッファの書き込み位置ポインタ */
	int readsize;

	sendp = sendbuf + stedstat->sendbuflen;
#ifdef SHINO_WINDOWS
	readsize = stedstat->datalen;
#else
	readsize = read(stedstat->ste_fd, rdatabuf, STRBUFSIZE);
#endif

	if (debuglevel > 1) {
		print_err(LOG_DEBUG, "========= from shino %d bytes ==================\n", readsize);
		if (debuglevel > 3) {
			print_err(LOG_DEBUG, "Type %s, UDP+DNS %d, TTL %x, PROTOCOL %x\n", EthII_Type(rdatabuf[12], rdatabuf[13]), rdatabuf[16]*256+rdatabuf[17], rdatabuf[22], rdatabuf[23]);
			if (rdatabuf[12]*256+rdatabuf[13] == 0x0806) {
				print_err(LOG_DEBUG, "[%d.%d.%d.%d]->[%d.%d.%d.%d]\n", rdatabuf[28], rdatabuf[29], rdatabuf[30], rdatabuf[31], rdatabuf[38], rdatabuf[39], rdatabuf[40], rdatabuf[41]);
			} else {
				print_err(LOG_DEBUG, "[%d.%d.%d.%d:%d]->[%d.%d.%d.%d:%d]\n", rdatabuf[26], rdatabuf[27], rdatabuf[28], rdatabuf[29], rdatabuf[34]*256+rdatabuf[35], rdatabuf[30], rdatabuf[31], rdatabuf[32], rdatabuf[33], rdatabuf[36]*256+rdatabuf[37]);
			}
			print_err(LOG_DEBUG, "      |-Send to (MAC)-| |-- Recv from --| |SAP|");
		}
		if (debuglevel > 2) {
			int i;
			for (i = 0; i < readsize; i++) {
				if ((i)%16 == 0) print_err(LOG_DEBUG, "\n%04d: ", i);
				print_err(LOG_DEBUG, "%02x ", rdatabuf[i] & 0xff);
			}
			print_err(LOG_DEBUG, "\n\n");
		}
	}

	if (remain = ( sizeof(stehead_t) + readsize ) % 4 ) pad = 4 - remain;
	steh.len = htonl(readsize + pad);
	steh.orglen = htonl(readsize);
	if (debuglevel > 1) {
		print_err(LOG_DEBUG, "head.len    = %d\n", ntohl(steh.len));
		print_err(LOG_DEBUG, "head.pad    = %d\n", pad);
		print_err(LOG_DEBUG, "head.orglen = %d\n", ntohl(steh.orglen));
	}
	memcpy(sendp, &steh, sizeof(stehead_t));	// ヘッダー追加
	memcpy(sendp + sizeof(stehead_t), /*rdata.buf*/rdatabuf, readsize);
	memset(sendp + sizeof(stehead_t) + readsize, 0x0, pad);
	stedstat->sendbuflen += sizeof(stehead_t) + readsize + pad;

	// ste から受け取ったサイズが ETHERMAX(1514byte)より小さいか、
	// 送信バッファへの書き込み済みサイズが SENDBUF_THRESHOLD 以上になったら送信する
	if (readsize < ETHERMAX || stedstat->sendbuflen > SENDBUF_THRESHOLD) {
		if (debuglevel > 1) {
			print_err(LOG_DEBUG, "readsize = %d, sendbuflen = %d\n",
				readsize, stedstat->sendbuflen);
		}
		if (write_socket(stedstat) < 0) return(-1);
	}
	return(0);
}


//---------------------------------------------------------
//	shino ドライバにデータを書き込む
//---------------------------------------------------------

int write_drv(shinostat_t *s)
{
#ifndef SHINO_WINDOWS
	if (write(s->ste_fd, s->wdatabuf, s->orgdatalen) < 0) {
		perror("write_drv");
		return -1;
	}
	/*if (write(eth_fd, s->wdatabuf, s->orgdatalen) < 0) {
		perror("write_drv");
		return -1;
	}*/
#else
	int written, res;
	OVERLAPPED olpd;

	olpd.Offset = 0;
	olpd.OffsetHigh = 0;
	olpd.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	res = WriteFile(s->ste_fd, s->wdatabuf, s->orgdatalen, &written, &olpd);
	if (!res && GetLastError() == ERROR_IO_PENDING) {
		WaitForSingleObject(olpd.hEvent, INFINITE);
		res = GetOverlappedResult(s->ste_fd, &olpd, &written, FALSE);
		if (written != s->orgdatalen) return -1;
	}
#endif
	/*if (debuglevel > 3) {
		int i;
		for (i=0; i < stedstat->orgdatalen; i++) {
			if ((i)%16 == 0) print_err(LOG_DEBUG, "\n%04d: ", i);
			print_err(LOG_DEBUG, "%02x ", stedstat->wdatabuf[i] & 0xff);
		}
		print_err(LOG_DEBUG, "\n\n");
	}*/
	return 0;
}


//---------------------------------------------------------
//	使い方を表示する
//---------------------------------------------------------

void print_usage(char *argv)
{
	printf("Usage: %s [ -i instance] [-h hub[:port]] [ -p proxy[:port]] [-d level]\n", argv);
	printf("\t-i instance     : Instance number of the snb device\n");
	printf("\t-h hub[:port]   : Virtual HUB and its port number\n");
	printf("\t-p proxy[:port] : Proxy server and its port number\n");
	printf("\t-d level        : Debug level[0-3]\n");
	exit(0);
}


//---------------------------------------------------------
//	メインループ
//---------------------------------------------------------

int /*WINAPIV*/ main(int argc, char *argv[])
{
	int ste_fd, sock_fd;
	int c, ret;
	fd_set fds;
	char dev[IFNAMSIZ];
	int instance = 0;  /* インターフェースのインスタンス番号 */
	int hub_port = 0;  /* 仮想ハブのポート番号 */
	char *hub = NULL;
	char *proxy = NULL;
	char localhost[] = "localhost:80";
	char dummy;
	struct timeval timeout;
	shinostat_t stedstat[1];

	while ((c = getopt(argc, argv, "d:i:h:p:")) != EOF) {
		switch (c) {
		case 'i':
			instance = atoi(optarg);
			break;
		case 'h':
			hub = optarg;
			break;
		case 'p':
			proxy = optarg;
			break;
		case 'd':
			debuglevel = atoi(optarg);
			break;
		default:
			print_usage(argv[0]);
		}
	}
	if (!hub) hub = localhost;

#ifdef SHINO_WINDOWS
	// Windows の場合はログファイルをオープンする。
	hLog = CreateFile(LOG_FILE,
		GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ| FILE_SHARE_WRITE,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	SetFilePointer(hLog, 0, NULL, FILE_END);
#else
	// syslog のための設定。Facility は　LOG_USER とする
	openlog((char*)basename(argv[0]), LOG_PID, LOG_USER);
#endif

	// shino オープン
	strcpy(dev, SHINO_DEV);
#ifndef SHINO_WINDOWS
	dev[SHINO_DEV_LEN] = 0x30+instance;
	dev[SHINO_DEV_LEN+1] = 0;
#endif
	if ((ste_fd = tun_open(dev)) < 0) {
		print_err(LOG_ERR, "Failed to open %s\n", dev);
		goto err;
	}
#ifndef SHINO_WINDOWS
	stedstat->ste_fd = ste_fd;
	//eth_fd = OpenEth("eth0");
#else
	stedstat->ste_fd = (HANDLE)ste_fd;
#endif

	// HUB と接続
	if ((sock_fd = open_socket(stedstat, hub, proxy)) < 0) {
		print_err(LOG_ERR, "failed to open connection with hub\n");
		goto err;
	}

	// ここまではとりあえず、フォアグラウンドで実行。
	// ここからは、デバッグレベル 0 （デフォルト）なら、バックグラウンド
	// で実行し、そうでなければフォアグラウンド続行。
	if (debuglevel == 0) {
#ifndef SHINO_WINDOWS
		print_err(LOG_NOTICE, "Going to background mode\n");
		if (become_daemon() != 0) {
			print_err(LOG_ERR, "can't become daemon\n");
			goto err;
		}
#else
		use_log = 1;
#endif
	}

#ifdef SHINO_WINDOWS
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)tun_reader, stedstat, 0, NULL);
#endif
	timeout.tv_sec = 0;
	timeout.tv_usec = SELECT_TIMEOUT;
	FD_ZERO(&fds);
	while (1) {
#ifndef SHINO_WINDOWS
		FD_SET(ste_fd, &fds);
#endif
		FD_SET(sock_fd, &fds);

		if ((ret = select(FD_SETSIZE, &fds, NULL, NULL, &timeout)) < 0) {
			SET_ERRNO();
			print_err(LOG_ERR, "select: %s\n", strerror(errno));
			goto err;
		} else if (ret == 0 && stedstat->sendbuflen > 0) {
			// SELECT_TIMEOUT 間に送受信がなければ、送信バッファーのデータを送信する。
			if (debuglevel > 1) {
				print_err(LOG_DEBUG, "select timeout(sendbuflen = %d)\n", stedstat->sendbuflen);
			}
			if (write_socket(stedstat) < 0) goto err;
			continue;
		}
		/* HUB からのデータ */
		if (FD_ISSET(sock_fd, &fds)) {
			if (read_socket(stedstat) < 0) {
				/* socket にエラーが発生した模様。再接続に行く */
				CLOSE(sock_fd);
				stedstat->sock.fd = -1;
				if ((sock_fd = open_socket(stedstat, hub, proxy)) < 0) {
					print_err(LOG_ERR, "failed to re-open connection with hub\n");
					goto err;
				}
			}
			continue;
		}
#ifndef SHINO_WINDOWS
		/* ドライバからのデータ */
		if (FD_ISSET(ste_fd, &fds)) {
			if (read_drv(stedstat) < 0) {
				/* socket にエラーが発生した模様。再接続に行く */
				CLOSE(sock_fd);
				stedstat->sock.fd = -1;
				if ((sock_fd = open_socket(stedstat, hub, proxy)) < 0) {
					print_err(LOG_ERR, "failed to re-open connection with hub\n");
					goto err;
				}
			}
			continue;
		}
#endif
	} /* main loop end */

	err:
	ssl_close(&stedstat->sock);
	// もし /dev/ste をまだオープンしているなら、まず登録解除してから終了する。
	/*if (ste_fd > 0)
		strioctl(ste_fd, UNREGSVC, -1, sizeof(int), (char *)&dummy);*/
	print_err(LOG_ERR, "Stopped\n");
	exit(1);
}
