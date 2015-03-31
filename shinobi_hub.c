//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------
//	仮想ハブ。仮想 NIC デーモンからの Ethernet フレームを受け取り、
//	他の仮想 NIC デーモンへ転送する役割を持つユーザプロセス。
//
//	gcc shinobi_hub.c utils.c -o shinobi_hub
//	wine c:\\lcc\\bin\\lcc -O shinobi_hub.c -DSHINO_WINDOWS
//
// Usage: shinobi_hub [ -p port] [-d level]
//  引数:
//	-p port	仮想 NIC デーモンからの接続を待ち受けるポート。
//		指定されなければ、80 が使われる。
//---------------------------------------------------------

#ifdef SHINO_WINDOWS
#include <winsock2.h>   /* for windows */
#include <windows.h>    /* for windows */
#include <winioctl.h>   /* for windows */
#include "getopt_win.h"
#else
#define  WINAPIV        /* for solaris */
#include <strings.h>    /* for solaris */
#include <unistd.h>     /* for solaris */
#include <sys/socket.h> /* for solaris */
#include <netinet/in.h> /* for solaris */
#include <netdb.h>      /* for solaris */
#include <syslog.h>     /* for solaris */
#include <libgen.h>     /* for solaris */
#include <arpa/inet.h>  /* for solaris */
#include <sys/time.h>   /* for solaris */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "shinobi.h"
#include "ssl.h"

#ifdef  FD_SETSIZE
#undef  FD_SETSIZE
#endif
#define FD_SETSIZE	1024

#ifdef  SHINO_WINDOWS
HANDLE  hLog;	/* デバッグログ用のファイルハンドル */
#endif

// 各 TCP 接続管理
struct conn_stat {
	struct conn_stat *next;
	sockinfo_t s;
	struct in_addr addr;	// 接続してきている仮想 NIC デーモンのアドレス
};

struct conn_stat conn_stat_head[1];

int           use_log = 0;      /* メッセージを STDERR でなく、syslog に出力する */
static int    debuglevel = 0;   /* デバッグレベル。 1 以上ならフォアグラウンドで実行 */


//---------------------------------------------------------
//	conn_stat 構造体のリンクリストに追加する
//---------------------------------------------------------
//	引数：
//		fd: 新規コネクションの socket 番号
//		addr: 接続してきたホストのアドレス
//---------------------------------------------------------

void add_conn_stat(int fd, struct in_addr addr)
{
	struct conn_stat *conn, *conn_stat_new;
	int i = 0;

	for (conn = conn_stat_head; conn->next != NULL; conn = conn->next);

	conn_stat_new = (struct conn_stat *)malloc(sizeof(struct conn_stat));
	conn_stat_new->s.fd = fd;
	conn_stat_new->addr = addr;
	conn_stat_new->next = NULL;

	conn->next = conn_stat_new;
	ssl_open(&conn_stat_new->s, 0/*server*/);
}


//---------------------------------------------------------
//	conn_stat 構造体のリンクリストから削除する
//---------------------------------------------------------
//	引数：
//		fd: 削除する socket 番号
//---------------------------------------------------------

void delete_conn_stat(int fd)
{
	struct conn_stat *conn, *conn_stat_delete;
	int i = 0;

	conn = conn_stat_head;
	while ( conn != NULL) {
		if (conn->next->s.fd == fd) {
			ssl_close(&conn->next->s);
			conn_stat_delete = conn->next;
			conn->next = conn_stat_delete->next;
			free(conn_stat_delete);
			return;
		}
		conn = conn->next;
	}
}


//---------------------------------------------------------
//	conn_stat 構造体のリンクリストから探す
//---------------------------------------------------------
//	引数：
//		fd: 検索する socket 番号
//	戻り値：
//		conn_stat 構造体のポインタ
//---------------------------------------------------------

struct conn_stat *find_conn_stat(int fd)
{
	struct conn_stat *conn;
	int i = 0;

	conn = conn_stat_head;
	while (conn != NULL) {
		if (conn->next->s.fd == fd) {
			return(conn->next);
		}
		conn = conn->next;
	}
	return ((struct conn_stat *)NULL);
}


//---------------------------------------------------------
//	使い方を表示する
//---------------------------------------------------------

void print_usage(char *argv)
{
	printf("Usage: %s [ -p port] [-d level]\n", argv);
	printf("\t-p port   : Port nubmer\n");
	printf("\t-d level  : Debug level[0-2]\n");
	exit(0);
}


//---------------------------------------------------------
//	メインループ
//---------------------------------------------------------

int WINAPIV main(int argc, char *argv[])
{
	int                 listener_fd, new_fd;
	int                 remotelen;
	int                 port = PORT_NO;
	int                 c, on;
	struct sockaddr_in  local_sin, remote_sin;
	static fd_set       fdset, fdset_saved;
	struct conn_stat   *rconn, *wconn;
#ifdef SHINO_WINDOWS
	u_long param = 0;	// FIONBIO コマンドのパラメータ Non-Blocking ON
	int nRtn;
	WSADATA wsaData;
	nRtn = WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif

	while ((c = getopt(argc, argv, "p:d:")) != EOF) {
		switch (c) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			debuglevel = atoi(optarg);
			break;
		default:
			print_usage(argv[0]);
		}
	}

	conn_stat_head->next = NULL;
	conn_stat_head->s.fd = 0;

	if ((listener_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		SET_ERRNO();
		print_err(LOG_ERR, "socket: %s (%d)\n", strerror(errno), errno);
		exit(1);
	}

	on = 1;
	if ((setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on))) < 0) {
		SET_ERRNO();
		print_err(LOG_ERR, "setsockopt: %s\n", strerror(errno));
		exit(1);
	}

	memset((char *)&remote_sin, 0x0, sizeof(struct sockaddr_in));
	memset((char *)&local_sin, 0x0, sizeof(struct sockaddr_in));
	local_sin.sin_port = htons((short)port);
	local_sin.sin_family = AF_INET;
	local_sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listener_fd, (struct sockaddr *)&local_sin, sizeof(struct sockaddr_in)) < 0) {
		SET_ERRNO();
		print_err(LOG_ERR, "bind: %s\n", strerror(errno));
		exit(1);
	}


	// accept() でブロックされるのを防ぐため、non-blocking mode に設定
#ifndef SHINO_WINDOWS
	if (fcntl(listener_fd, F_SETFL, O_NONBLOCK) < 0)
#else
	if (ioctlsocket(listener_fd, FIONBIO, &param) < 0)
#endif
	{
		SET_ERRNO();
		print_err(LOG_ERR, "Failed to set nonblock: %s (%d)\n", strerror(errno), errno);
		exit(1);
	}

	if (listen(listener_fd, 5) < 0) {
		SET_ERRNO();
		print_err(LOG_ERR, "listen: %s\n", strerror(errno));
		exit(1);
	}

	FD_ZERO(&fdset_saved);
	FD_SET(listener_fd, &fdset_saved);

#ifdef SHINO_WINDOWS
	// Windows の場合はログファイルをオープンする。
	hLog = CreateFile(HUB_LOG_FILE,
		GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ| FILE_SHARE_WRITE,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	SetFilePointer(hLog, 0, NULL, FILE_END);
#else
	// syslog のための設定。Facility は　LOG_USER とする
	openlog(basename(argv[0]), LOG_PID, LOG_USER);
#endif


	// ここからは、デバッグレベル 0 （デフォルト）なら、バックグラウンド
	// で実行し、そうでなければフォアグラウンド続行。
	// Windows の場合は、フォアグラウンドで実行し、
	// デバッグレベル 1 以上の場合はログをファイルに書く。
	if (debuglevel == 0) {
#ifndef SHINO_WINDOWS
		print_err(LOG_NOTICE, "Going to background mode\n");
		if (become_daemon() != 0) {
			print_err(LOG_ERR, "can't become daemon\n");
			print_err(LOG_ERR, "Exit\n");
			exit(1);
		}
#else
		use_log = 1;
#endif
	}
	print_err(LOG_NOTICE, "Started\n");

	// メインループ
	// 仮想 NIC デーモンからの接続要求を待ち、接続後は仮想 NIC デーモン
	// からのデータを待つ。１つの仮想デーモンからのデータを他方に転送する。
	while (1) {
		fdset = fdset_saved;
		if (select(FD_SETSIZE, &fdset, NULL, NULL, NULL) < 0) {
			SET_ERRNO();
			print_err(LOG_ERR, "select: %s\n", strerror(errno));
		}

		if (FD_ISSET(listener_fd, &fdset)) {
			// 新規接続
			remotelen = sizeof(struct sockaddr_in);
			if ((new_fd = accept(listener_fd, (struct sockaddr *)&remote_sin, &remotelen)) < 0) {
				SET_ERRNO();
				if (errno == EINTR || errno == EWOULDBLOCK || errno == ECONNABORTED) {
					print_err(LOG_NOTICE, "accept: %s\n", strerror(errno));
					continue;
				} else {
					print_err(LOG_ERR, "accept: %s\n", strerror(errno));
					return(-1);
				}
			}

			FD_SET(new_fd, &fdset_saved);
			print_err(LOG_NOTICE, "fd%d: connection from %s\n", new_fd, inet_ntoa(remote_sin.sin_addr));
			add_conn_stat(new_fd, remote_sin.sin_addr);

			//recv() でブロックされるのを防ぐため、non-blocking mode に設定
#ifndef SHINO_WINDOWS
			if (fcntl(new_fd, F_SETFL, O_NONBLOCK) < 0)
#else
			if (ioctlsocket(new_fd, FIONBIO, &param) < 0)
#endif
			{
				SET_ERRNO();
				print_err(LOG_ERR, "fd%d: Failed to set nonblock: %s (%d)\n",
					new_fd, strerror(errno), errno);
				return(-1);
			}
			continue;
		}

		// 各仮想 NIC からの要求確認
		for (rconn = conn_stat_head->next; rconn != NULL; rconn = rconn->next) {
			int rfd, wfd;

			rfd = rconn->s.fd;
			if (FD_ISSET(rfd, &fdset)) {
				int   rsize;
				char  databuf[SOCKBUFSIZE];
				char *bufp;
				int   datalen;

				bufp = (char*)databuf;
				//rsize = recv(rfd, bufp, SOCKBUFSIZE, 0);
				rsize = ssl_read(&rconn->s, bufp, SOCKBUFSIZE);
				if (rsize == 0) {
					// コネクションが切断。socket を close してループを抜ける
					print_err(LOG_ERR, "fd%d: Connection closed by %s\n", rfd, inet_ntoa(rconn->addr));
					CLOSE(rfd);
					print_err(LOG_ERR, "fd%d: closed\n", rfd);
					FD_CLR(rfd, &fdset_saved);
					delete_conn_stat(rfd);
					break;
				}
				if (rsize < 0) {
					SET_ERRNO();
					// 致命的でない error の場合は無視してループを継続
					if (errno == EINTR || errno == EWOULDBLOCK) {
						print_err(LOG_NOTICE, "fd%d: recv: %s\n", rfd, strerror(errno));
						continue;
					}

					// エラーが発生。socket を close して forループを抜ける
					print_err(LOG_ERR, "fd%d: recv: %s\n", rfd, strerror(errno));
					CLOSE(rfd);
					print_err(LOG_ERR, "fd%d: closed\n", rfd);
					FD_CLR(rfd, &fdset_saved);
					delete_conn_stat(rfd);
					break;
				}

				// 他の仮想 NIC にパケットを転送する。
				// 「待ち」が発生すると、パフォーマンスに影響があるので、EWOULDBLOCK の場合は配送をあきらめる。
				for (wconn = conn_stat_head->next; wconn != NULL; wconn = wconn->next) {
					wfd = wconn->s.fd;
					if (rfd == wfd) continue;

					if (debuglevel > 1) {
						print_err(LOG_ERR, "fd%d(%s) ==> ", rfd, inet_ntoa(rconn->addr));
						print_err(LOG_ERR, "fd%d(%s)\n", wfd, inet_ntoa(wconn->addr));
					}

					//if (send(wfd, bufp, rsize, 0) < 0) {
					if (ssl_write(&wconn->s, bufp, rsize) < 0) {
						SET_ERRNO();
						if (errno == EINTR || errno == EWOULDBLOCK) {
							print_err(LOG_NOTICE, "fd%d: send: %s\n", wfd, strerror(errno));
							continue;
						} else {
							print_err(LOG_ERR, "fd%d: send: %s (%d)\n", wfd, strerror(errno), errno);
							CLOSE(wfd);
							print_err(LOG_ERR, "fd%d: closed\n", wfd);
							FD_CLR(wfd, &fdset_saved);
							delete_conn_stat(wfd);
							break;
						}
					}
				} /* End of loop for send()ing */
			}
		} /* End of loop for each connection */
	} /* End of main loop */
}
