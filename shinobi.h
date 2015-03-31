//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------

#ifndef __SHINO_H
#define __SHINO_H

#include "utils.h"

#ifdef  SHINO_WINDOWS
#define SHINO_DEV	"TAP01"		// デバイス名
#define DEVICE_PATH_FMT	"\\\\.\\Global\\%s.tap"
#define LOG_FILE	"shinobi.log"	// ログファイル
#define HUB_LOG_FILE	"shinobi_hub.log"	// ログファイル
#else
#define SHINO_DEV	"shino"		// デバイス名
#define SHINO_DEV_LEN	5
#endif

// Windows の場合 SAGetLastError() を使って errno にエラー番号をセットする
// また、Windows の場合 socket の close には closesocket() を使う。
#ifdef SHINO_WINDOWS
#define SET_ERRNO()	errno = WSAGetLastError()
#define CLOSE(fd)	closesocket(fd)
#else
#define SET_ERRNO()
#define CLOSE(fd)	close(fd)
#endif

/*******************************************************
 * o 仮想 NIC デーモンが利用する各種パラメータ
 *
 *  CONNECT_REQ_SIZE     proxy に対する CONNECT 要求の文字長
 *  CONNECT_REQ_TIMEOUT  Proxy から CONNECT のレスポンスを受け取るタイムアウト
 *  STRBUFSIZE           getmsg(9F),putmsg(9F) 用のバッファのサイズ
 *  PORT_NO              デフォルトの仮想ハブのポート番号
 *  SOCKBUFSIZE          recv(), send() 用のバッファサイズ
 *  ERR_MSG_MAX          syslog や、STDERR に出力するメッセージのサイズ
 *  SENDBUF_THRESHOLD    送信一時バッファのデータを送信するしきい値。
 *  SELECT_TIMEOUT       select() 用のタイムアウト（Solaris 用)
 *  HTTP_STAT_OK         HTTP のステータスコード OK
 *  MAXHOSTNAME          ホスト名（HUBやProxy）の最大長
 *  GETMSG_MAXWAIT       getmsg(9F) のタイムアウト値（Solaris 用)
 ********************************************************/
#define  CONNECT_REQ_SIZE	200
#define  CONNECT_REQ_TIMEOUT	10
#define  STRBUFSIZE		32768
#define  PORT_NO		80
#define  SOCKBUFSIZE		32768
#define  SENDBUF_THRESHOLD	3028    // ETHERMAX(1514) x 2
#define  SELECT_TIMEOUT		400000  // 400m sec = 0.4 sec
#define  HTTP_STAT_OK		200
#define  MAXHOSTNAME		30
//#define  GETMSG_MAXWAIT		15
#define  STE_MAX_DEVICE_NAME	30

#define  ETHERMAX		1514

#ifdef SHINO_SSL
#include <openssl/ssl.h>
#endif
typedef struct sock_info
{
#ifdef SHINO_SSL
	SSL	*ssl;
	SSL_CTX	*ctx;
#endif
	int	fd;	// HUB または Proxy との通信に使う
} sockinfo_t;

/*
 * 仮想 NIC デーモン sted と、仮想ハブデーモン stehub が通信を
 * 行う際、送受信する Ethernet フレームのデータに付加されるヘッダ。
 */
typedef struct stehead
{
	int len;	// パディング後のデータサイズ
	int orglen;	// パディングする前のサイズ
} stehead_t;

/*
 * sted デーモンが使う sted の管理用構造体
 * HUB との通信の情報や、仮想 NIC ドライバの情報を持っている。
 */
typedef struct shino_stat
{
	/* Socket 通信用情報 */
	sockinfo_t	sock;
	//int           sock_fd;                 /* HUB または Proxy との通信につかう FD  */
	char          hub_name[MAXHOSTNAME];   /* 仮想ハブ名 */
	int           hub_port;                /* 仮想ハブのポート番号 */
	char          proxy_name[MAXHOSTNAME]; /* プロキシーサーバ名   */
	int           proxy_port;              /* プロキシーサーバのポート番号  */
	int           sendbuflen;              /* 送信バッファへの現在の書き込みサイズ  */
	int           datalen;                 // パッドを含む Ethernet フレームのサイズ
	int           orgdatalen;              /* 元の Ethernet フレームのサイズ        */
	int           dataleft;                /* 未受信の Ethernet フレームのサイズ    */
	stehead_t     dummyhead;               /* 受信途中の stehead のコピー           */
	int           dummyheadlen;            /* 受信済みの stehead のサイズ           */
	//int           use_syslog;              /* メッセージを STDERR でなく、syslog に出力する */
	unsigned char sendbuf[SOCKBUFSIZE];    /* Socket 送信用バッファ */
	unsigned char recvbuf[SOCKBUFSIZE];    /* Socket 受信用バッファ */
	/* ste ドライバ用情報 */
#ifdef SHINO_WINDOWS
	HANDLE		ste_fd;			// 仮想 NIC デバイスのハンドル
#else
	int		ste_fd;			// 仮想 NIC デバイス
#endif
	unsigned char	wdatabuf[STRBUFSIZE];	// ドライバへの書き込み用バッファ
	unsigned char	rdatabuf[STRBUFSIZE];	// ドライバからの読み込み用バッファ
} shinostat_t;

int write_drv(shinostat_t *stedstat);

// shinobi_socket.c
int open_socket(shinostat_t *stedstat, char *hub, char *proxy);
int read_socket(shinostat_t *stedstat);
int write_socket(shinostat_t *stedstat);

typedef unsigned char	uchar_t;

#endif /* #ifndef __SHINO_H */
