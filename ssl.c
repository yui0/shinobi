//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------
//	touch /etc/pki/CA/index.txt
//	echo "00" > /etc/pki/CA/serial
//	openssl req -new -newkey rsa:2048 -out ca.req -keyout ca.key
//	openssl ca -extensions v3_ca -out ca.crt -keyfile ca.key -selfsign -infiles ca.req
//---------------------------------------------------------

#ifdef SHINO_WINDOWS
#include <windows.h>    /* for windows */
#include <io.h>
#include "getopt_win.h"
#else
#include <syslog.h>	// LOG_NOTICE
#endif
#include "shinobi.h"
#include "ssl.h"


#ifndef SHINO_WINDOWS
inline
#endif
int ssl_read(sockinfo_t *s, void *buf, int num)
{
#ifdef SHINO_SSL
	return s->ssl ? SSL_read(s->ssl, buf, num) : read(s->fd, buf, num);
#else
	return read(s->fd, buf, num);
#endif
}
#ifndef SHINO_WINDOWS
inline
#endif
int ssl_write(sockinfo_t *s, void *buf, int num)
{
#ifdef SHINO_SSL
	return s->ssl ? SSL_write(s->ssl, buf, num) : write(s->fd, buf, num);
#else
	return write(s->fd, buf, num);
#endif
}

#ifdef SHINO_SSL
void ssl_open(sockinfo_t *s, int f)
{
	SSL *ssl;
	SSL_CTX *ctx;
	int ret;

	if (f<0) {
		// No SSL
		s->ssl = 0;
		s->ctx = 0;
		return;
	}

#ifndef SHINO_WINDOWS
	SSL_load_error_strings();
#endif
	SSL_library_init();
	//ctx = SSL_CTX_new(SSLv23_client_method());
	ctx = SSL_CTX_new(SSLv23_method());
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	if (ctx == NULL) {
		perror("SSL_CTX_new");
#ifndef SHINO_WINDOWS
		ERR_print_errors_fp(stderr);
#endif
		exit(1);
	}

	if (!f) {
		// 秘密鍵KEYFILEを読み込み
		ret = SSL_CTX_use_PrivateKey_file(ctx, SERVER_KEY, SSL_FILETYPE_PEM);
		if (!ret) ret = SSL_CTX_use_PrivateKey_file(ctx, SHINO_CONF SERVER_KEY, SSL_FILETYPE_PEM);
		if (!ret) {
			perror("SSL_CTX_use_PrivateKey_file");
#ifndef SHINO_WINDOWS
			ERR_print_errors_fp(stderr);
#endif
			SSL_CTX_free(ctx);
			s->ssl = 0;
			s->ctx = 0;
			return;
		}
		// 証明書CRTFILEを読み込み
		ret = SSL_CTX_use_certificate_file(ctx, SERVER_CRT, SSL_FILETYPE_PEM);
		if (!ret) ret = SSL_CTX_use_certificate_file(ctx, SHINO_CONF SERVER_CRT, SSL_FILETYPE_PEM);
		if (!ret) {
			perror("SSL_CTX_use_certificate_file");
#ifndef SHINO_WINDOWS
			ERR_print_errors_fp(stderr);
#endif
			SSL_CTX_free(ctx);
			s->ssl = 0;
			s->ctx = 0;
			return;
		}
	}

	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		perror("SSL_new");
#ifndef SHINO_WINDOWS
		ERR_print_errors_fp(stderr);
#endif
		exit(1);
	}

	ret = SSL_set_fd(ssl, s->fd);
	if (ret == 0) {
		perror("SSL_set_fd");
#ifndef SHINO_WINDOWS
		ERR_print_errors_fp(stderr);
#endif
		exit(1);
	}

	/* PRNG 初期化 (urandom がない場合) */
	/*RAND_poll();
	while (RAND_status() == 0) {
		unsigned short rand_ret = rand() % 65536;
		RAND_seed(&rand_ret, sizeof(rand_ret));
	}*/

	/* SSL で接続 */
	if (f) {
		// for client
		ret = SSL_connect(ssl);
		if (ret != 1) {
			//perror("SSL_connect");
#ifndef SHINO_WINDOWS
			ERR_print_errors_fp(stderr);
#endif
			print_err(LOG_NOTICE, "fd%d: connected by No SSL (%d/%d)\n", s->fd, ret, SSL_get_error(ssl, ret));
			//printf("fd%d: connected by No SSL (%d/%d)\n", s->fd, ret, SSL_get_error(ssl, ret));
			//exit(1);
			SSL_free(ssl);
			SSL_CTX_free(ctx);
			s->ssl = 0;
			s->ctx = 0;
			return;
		}
	} else {
		// for server
		ret = SSL_accept(ssl);
		if (ret != 1) {
			/*perror("SSL_accept");
			ERR_print_errors_fp(stderr);
			exit(1);*/
			print_err(LOG_NOTICE, "fd%d: accepted by No SSL (%d)\n", s->fd, ret);
			//printf("fd%d: accepted by No SSL (%d/%d)\n", s->fd, ret, SSL_get_error(ssl, ret));
			SSL_free(ssl);
			SSL_CTX_free(ctx);
			s->ssl = 0;
			s->ctx = 0;
			return;
		}
	}

	s->ssl = ssl;
	s->ctx = ctx;
}

void ssl_close(sockinfo_t *s)
{
	int ret;

	if (s->ssl) {
		ret = SSL_shutdown(s->ssl);
		/*if (ret != 1) {
			ERR_print_errors_fp(stderr);
			perror("SSL_shutdown");
			//exit(1);
		}*/

		SSL_free(s->ssl);
	}
	if (s->ctx) SSL_CTX_free(s->ctx);
#ifndef SHINO_WINDOWS
	ERR_free_strings();
#endif
}
#endif
