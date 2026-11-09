// signal.h -- C4IX's libc has no signals, and nothing sends a demo one.
#define SIGINT 2
#define SIGTERM 15
#define signal(n, h) ((void)0)
