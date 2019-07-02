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

	argc = get_argc(args); 
	
	opt1 = get_argv(args, 1);	
	if(!strcmp(opt1, "--help")) 
	{
		printstr("Tool keyclear:\n\nThis tool is used to reset global encryption key.\n");

	}
	else
	{
		keyclear(0);
		
	}

	_exit(0);
}
