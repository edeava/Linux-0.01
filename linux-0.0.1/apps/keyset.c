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
	if(argc < 1)
	{
		printerr("No program args, use option --help\n");
		_exit(1);
	}
	
	if(argc == 1)
	{
		char *string;
		visibilitySwitch();
		fgets(string,514,0);
		visibilitySwitch();
		string[strlen(string)-1] = '\0';
		keyset(string, 1);
	}else{

		opt1 = get_argv(args, 1);	
		if(!strcmp(opt1, "--help")) 
		{
			printstr("Tool keygen:\n\nThis tool is used to generate a random global encryption key.\nOptions:\n\t1 - Generates key of length 4\n\t2 - Generates key of length 8\n\t3 - Generates key of length 16\n");

		}
		else
		{
			int len = strlen(opt1);
			while(len > 0){
				if(len % 2 == 1 || len >= 1024){
					printerr("Argument has to be a power of 2 length!\n");
					_exit(0);
				}
				len = len >> 2;	
				if(len == 1) break;	
			}
			keyset(opt1, 1);
		
		}
	}

	_exit(0);
}
