//---------------------------------------------------------
//	Shinobi (Software VPN)
//
//		©2011 YUICHIRO NAKADA
//---------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <Windows.h>

char *optarg;

int getopt(int argc, char * const argv[], const char *optstring)
{
	static int    count = 1;
	static char  *optstring_saved = NULL;
	int optstrings;
	int i;
	char *opt;

	// optstring が変わったら別の option の調査とみなす。
	if (optstring_saved != NULL && strcmp(optstring_saved, optstring) != 0) count = 1;
	optstring_saved = (char *)optstring;

	if (count > argc - 1) return(EOF);
	//printf("argc = %d, count = %d\n",argc,  count);

	optstrings = strlen(optstring);
	opt = argv[count];
	//printf("opt[0] = %c, opt[1] = %c optstrings = %d\n", opt[0], opt[1], optstrings);

	if (opt[0] == '-') {
		for (i = 0; i < optstrings; i++) {
			/* 渡されたオプションが optstring に含まれているかチェック */
			if (opt[1] == optstring[i]) {
				/* optstring に次があるかチェック。なければオプションに値は無い */
				if (i + 1 <= optstrings) {
					/* オプションに値(:)が必要とされているかどうかのチェック */
					if (optstring[i+1] == ':') {
						/* オプション値が次の argv として渡されているかどうかを確認 */
						if (count + 1 <= argc - 1) {
							/* 次の argv が '-' から始まっていないかどうかを確認 */
							if (argv[count+1][0] == '-') {
								count++;
								//printf("return 1\n");
								return(':');
							} else {
								optarg = argv[count+1];
								count = count + 2;
								//printf("return 2\n");
								return(opt[1]);
							}
						} else {
							count++;
							//printf("return 3\n");
							return(':');
						}
					} else {
						count++;
						//printf("return 4\n");
						return(opt[1]);
					}
				} else {
					count++;
					//printf("return 5\n");
					return(opt[1]);
				}
			}
			continue;
		} /* for end */
	}
	count++;
	//printf("return 6\n");

	return(':');
}
