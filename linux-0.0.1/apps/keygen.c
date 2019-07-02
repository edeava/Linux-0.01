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
	if(argc < 2)
	{
		printerr("No program args, use option --help\n");
		_exit(1);
	}
	
	opt1 = get_argv(args, 1);	
	if(!strcmp(opt1, "--help")) 
	{
		printstr("Tool keygen:\n\nThis tool is used to generate a random global encryption key.\nOptions:\n\t1 - Generates key of length 4\n\t2 - Generates key of length 8\n\t3 - Generates key of length 16\n");

	}
	else
	{
		
		opt1 = get_argv(args, 1);
		
		if(!strcmp(opt1, "1")){
			keygen(1);
		}
		else if(!strcmp(opt1, "2"))
		{
			keygen(2);
		}
		else if(!strcmp(opt1, "3"))
		{
			keygen(3);
		}
		else{
			printerr("Invaluid program args!\n");
			_exit(1);
		}
		
	}

	_exit(0);
}
