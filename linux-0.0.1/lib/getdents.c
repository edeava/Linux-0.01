#define __LIBRARY__
#include <unistd.h>
#include <dirent.h>

_syscall3(int,getdents,unsigned int,fd, struct dirent *,dirp,unsigned int,count)

