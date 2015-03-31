//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#define  ERR_MSG_MAX              300

#ifdef SHINO_WINDOWS
#include <windows.h>    /* for windows */
extern HANDLE hLog;
#endif
extern int use_log;


#ifndef SHINO_WINDOWS
//---------------------------------------------------------
//	バックグラウンドに移行する
//---------------------------------------------------------

int become_daemon()
{
	//chdir("/");
	umask(0);
	signal(SIGHUP, SIG_IGN);

	if (!fork()) {
		use_log = 1;
		close(0);
		close(1);
		close(2);
		/* 新セッションの開始 */
		if (setsid() < 0)
		return(-1);
	} else {
		exit(0);
	}
	return(0);
}
#endif


//---------------------------------------------------------
//	エラーメッセージを表示する
//---------------------------------------------------------

void print_err(int level, char *format, ...)
{
	va_list ap;
	char buf[ERR_MSG_MAX];
	int length;

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);

	if (use_log) {
#ifdef SHINO_WINDOWS
		WriteFile(hLog, buf, strlen(buf), &length, NULL);
#else
		syslog(level, buf);
#endif
	} else {
		fprintf(stderr, buf);
	}
}
