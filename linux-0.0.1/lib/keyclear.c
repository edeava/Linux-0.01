#define __LIBRARY__
#include <unistd.h>

#include <crypt.h>

_syscall1(int,keyclear,int,scope)
