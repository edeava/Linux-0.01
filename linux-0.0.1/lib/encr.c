#define __LIBRARY__
#include <unistd.h>

#include <crypt.h>

_syscall1(int,encr,int,fd)
