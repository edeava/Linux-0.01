#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define UTIL_IMPLEMENTATION
#include "utils.h"

int main(char *args)
{
	int argc, i, fd;
	char *opt1;
	char buff[128];
	
	read(0, opt1, 4);
	keyset(opt1, 0);
		
	while(1){
		i = (i + 1) % 100;
	}

	_exit(0);
}