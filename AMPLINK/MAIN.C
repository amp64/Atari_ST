
#include "amplink.h"

bool pass1,show_thinking;
char *current_input_name;
bool map_file,sym_file;
long magic_long=7;			/* l m fast */
bool write_debug=TRUE;
bool link_basic;

void do_error(char *msg, char *x, char *y)
{
	fprintf(stderr,msg);
	fprintf(stderr,x,y);
	if (current_input_name)
		fprintf(stderr," in file '%s'",current_input_name);
	fprintf(stderr,"\n");
	fflush(stderr);
}

void fatal(char *x, char *y)
{
	do_error("Fatal error: ",x,y);
	exit(10);
}

void error(char *x, char *y)
{
	do_error("Error: ",x,y);
}

void warning(char *x,char*y)
{
	do_error("Warning: ",x,y);
}
	
/* this gets memory which is zeroed and aborts if fails */
void *getzmem(long size)
{
void *x;
	x=malloc(size);
	if (x==NULL)
		fatal("out of memory",NULL);
	memset(x, 0, size);
	return x;
}

char *command_list[] = { 
	"WITH",		/* 1 */
	"TO", 	/* 2 */
	"MAP", 	/* (3) */
	"SYM", 	/* (4) */
	"AMP", 	/* (5) */
	"ATARI",	/* (6) */
	"AMIGA",	/* (7) */
	"XDEBUG",	/* (8) */
	"NODEBUG",	/* (9) */
	"BASIC"		/* (10) */
	 };

/* if something is a simple command then do it & return 0
	if it is special, return >0
	if it is not a command, return <0
*/
int check_command(char *p)
{
int i;
	for (i=0; i<sizeof(command_list)/sizeof(char*); i++)
		{
		if (stricmp(command_list[i],p)==0)
			{
			switch (++i)
				{
				case 3: map_file=TRUE; i=0; break;
				case 4: sym_file=TRUE; i=0; break;
				case 5: show_thinking=TRUE; i=0; break;
				case 6: ST=TRUE; i=0; break;
				case 7: ST=FALSE; i=0; break;
				case 8: write_debug=TRUE; i=0; break;
				case 9: write_debug=FALSE; i=0; break;
				case 10: link_basic=TRUE; i=0; break;
				}
			return i;
			}
		}
	return -1;	
}

int main(int argc, char *argv[])
{
int c;
char *p;
int cmd;

	printf("AMPLINK Version 0.41\n");

	add_input(NULL);				/* initialise */
	c=1;
	while (c<argc)
		{
		p=argv[c++];
		switch (check_command(p))
			{
			case 0:
				break;
			case 1:
				if (c>=argc)
					goto usage;
				add_with(argv[c++]);
				break;
			case 2:
				if (c>=argc)
					goto usage;
				if (output_name)
					goto usage;
				output_name=argv[c++];
				break;
			default:
			/* must be an input filename */
				add_input(p);
				break;
			}
		}

	if (show_thinking)
		printf("Pass 1\n");

	do_input(TRUE);
	do_output(TRUE);

	if (show_thinking)
		printf("Pass 2\n");
	
	do_input(FALSE);
	do_output(FALSE);
	if (sym_file)
		list_symbols(3);
	fprintf(stderr,"Finished OK\n");
	return 0;
usage:
	fprintf(stderr,"usage: AMPLINK inputname(s)\n");
	fflush(stderr);
	return 10;
}
