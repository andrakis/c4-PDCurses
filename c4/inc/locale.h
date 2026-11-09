// locale.h -- C4IX's libc has no locales: the demos only ask for the default.
#define LC_ALL 0
#define LC_CTYPE 0
#define setlocale(c, l) ((char *)"C")
