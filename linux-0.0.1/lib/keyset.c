#define __LIBRARY__
#include <unistd.h>

#include <crypt.h>

_syscall2(int,keyset,const char *,key, int, scope)
