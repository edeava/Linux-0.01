#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define UTIL_IMPLEMENTATION
#include "utils.h"

char filenamebuff[256];

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
		printstr("Tool enrypt:\n\nThis tool is used to encrypt content of a file.\nOptions:\n\tFile name\n");

	}
	else
	{
		opt1 = get_argv(args, 1);
		strcpy(filenamebuff, get_argv(args, ARG_PWD)); /* Uzimamo CWD korisnika. */
		if(strlen(filenamebuff) != 1) strcat(filenamebuff, "/"); /* Ako je CWD razlicit od /. */
		if(opt1[0] == '/') /* Proveravamo da li je korisnik zadao apsolutnu putanju. */
			strcpy(filenamebuff, opt1);
		else
			strcat(filenamebuff, opt1);
		fd = open(filenamebuff, O_RDONLY);
		if(fd < 0)
		{
			printerr("Could not open file!\n");
			printerr("Tried to open: ");
			printerr(filenamebuff);
			_exit(1);
		}
		int test = encr(fd);
		char *buffer;
		itoa(test, buffer);

		if(test < 0)
			printerr("Doslo je do greske!");
		close(fd);

	}
	_exit(0);
}
