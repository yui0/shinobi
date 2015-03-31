//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------

#ifndef __SHINO_SSL_H
#define __SHINO_SSL_H

#ifdef SHINO_SSL
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#define SHINO_CONF	"/etc/shinobi/"
#define SERVER_KEY	"server.key"
#define SERVER_CRT	"server.crt"
//#define SERVER_KEY	"ca.key"
//#define SERVER_CRT	"ca.crt"

#ifdef SHINO_SSL
void ssl_open(sockinfo_t *s, int f);
void ssl_close(sockinfo_t *s);
#else
#define ssl_open(a, b)
#define ssl_close(a)
#endif

#ifndef SHINO_WINDOWS
inline
#endif
int ssl_read(sockinfo_t *s, void *buf, int num);
#ifndef SHINO_WINDOWS
inline
#endif
int ssl_write(sockinfo_t *s, void *buf, int num);

#endif /* #ifndef __SHINO_SSL_H */
