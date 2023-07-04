
#include "amplink.h"
#include <fcntl.h>

/* I would like to include dos.h but it redefines byte so I do these manually */
#define FNSIZE 13	/* maximum file node size */
#define FMSIZE 128	/* maximum file name size */
#define FESIZE 4	/* maximum file extension size */

char *firstname;

struct input_list {
	struct input_list *next;
	char *filename;
	long length;
	void *buffer;			/* NULL until loaded */
	void *local;
	};

struct input_list *input_list,dummy_input_list={ NULL, "crap_filename", NULL };

/* add a filename to the input list (NULL means init) */
void add_input(char *filename)
{
struct input_list *last,*new;

	if (filename)
		{
		last=input_list;
		while (last->next)
			last=last->next;
		new=getzmem(sizeof(struct input_list));
		new->filename=getzmem(strlen(filename)+1);
		strcpy(new->filename,filename);
		/* next & buffers will be zero */
		last->next=new;
		if (firstname==NULL)
			firstname=filename;
		}
	else
		input_list = &dummy_input_list;
}

/* find the next space. Cannot cope with >1 space
	and junks trailing spaces (only at end) */

char *find_space(char *p)
{
char c,*q;
	while (c=*p++)
		{
		if ( (c==' ') || (c=='\t') )
			{
			q=p--;
			while (c=*p++)
				{
				if (c!=' ')
					break;
				}
			if (c==0)
				{
				*q=0;
				return NULL;		/* kill trailing space */
				}
			else
				return q;
			}
		}
	return NULL;
}

/* do a WITH file (must be re-entrant) */
void add_with(char *filename)
{
FILE *fp;
#define LINELEN 255
char buf[LINELEN+1],*p;
char *space;

	if (firstname==NULL)
		firstname=filename;
	fp=fopen(filename,"ra");
	if (fp==NULL)
		fatal("cannot open WITH file '%s'",filename);
	buf[LINELEN]=0;
	for(;;)
		{
		p=fgets(buf,LINELEN,fp);
		if (p==NULL)
			break;
		if (*(long*)p==HUNK_UNIT)
			fatal("cannot use object file '%s' as WITH file!",filename);
		if ( (*p==0) || (*p=='\n') )
			continue;
		p+=strlen(p)-1;
		if (*p=='\n')
			*p=0;
		if (buf[0]==';')
			continue;
		if (space=find_space(buf)) *space=0;
		switch (check_command(buf))
			{
			case 0:
				break;
			case 1:
				if (space==NULL)
					fatal("WITH needs a filename",NULL);
				add_with(space+1);
				break;
			case 2:
				if (space==NULL)
					fatal("TO needs a filename",NULL);
				output_name=getzmem(strlen(space+1));
				strcpy(output_name,space+1);
				break;
			default:
				p=buf;
				for(;;)
					{
					add_input(p);
					if (space==NULL)
						break;
					p=space+1;
					space=find_space(p);
					}
			}
		}
	fclose(fp);
}

/* return pointer to default output name (based on first input name) */
char *get_default_output(void)
{
char drive[3],path[FMSIZE],node[FMSIZE],ext[FMSIZE];
char *q;

	strsfn(firstname,drive,path,node,ext);
	q=getzmem(FMSIZE);
	strmfn(q,drive,path,node,ST ? "PRG" : "");
	return q;
}

long *do_reloc(long *mem, long rtype)
{
long n1;
long hnum;
	for(;;)
		{
		n1=*mem++;
		if (n1==0L)
			break;
		hnum=*mem++;
		declare_reloc(mem,n1,hnum,rtype);
		mem+=n1;
		}
	return mem;
}


long *do_ext(long *mem)
{
unsigned char ext;
long x; long slen;
char *p;
long value;

	for(;;)
		{
		x=*mem++;
		if (x==0)
			break;
		ext=(x>>24)&0xFF;
		x&=0x00FFFFFFL;
		slen=x<<2;
		p=(char*)((long)mem+slen-3);
		if (*p++==0)
			slen-=3;
		else if (*p++==0)
			slen-=2;
		else if (*p++==0)
			slen--;
		p=(char*)mem;			/* point to start of symbol */
		mem+=x;
		value=*mem++;
		switch (ext)
			{
			case EXT_DEF:
				declare_rel_symbol(p,slen,value);
				break;
			case EXT_ABS:
				declare_abs_symbol(p,slen,value);
				break;
			case EXT_REF32: case EXT_REF16: case EXT_REF8:
			case EXT_DREL32: case EXT_DREL16: case EXT_DREL8:
				declare_import_list(p,slen,mem,value,ext);
				mem+=value;
				break;
			default:
				fatal("unknown EXT type %ld",(char*)(long)ext);
				break;
			}
		}
	return mem;
}

long *do_sym(long *mem)
{
long x;
long slen;
char *p;
long value;
	for (;;)
		{
		x=(*mem++)&0x00FFFFFFL;		/* important to strip high byte */
		if (x==0L)
			break;
		slen=x<<2;
		p=(char*)((long)mem+slen-3);
		if (*p++==0)
			slen-=3;
		else if (*p++==0)
			slen-=2;
		else if (*p++==0)
			slen--;
		p=(char*)mem;			/* point to start of symbol */
		mem+=x;
		value=*mem++;
		declare_dbg_symbol(p,slen,value);
		}
	return mem;
}

/* parse a hunk, return TRUE after hunk_end */
bool parse_hunk(long **x)
{
long *mem;
long len,htype;
bool ret=FALSE;

	mem=*x;
	htype=*mem++;
	len=*mem++;
	switch (htype&0x3FFFFFFFL)
		{
		case HUNK_NAME:
			declare_section_name(mem-1);
			mem+=len;
			break;
		case HUNK_CODE:
		case HUNK_DATA:
			declare_code(mem,len<<2,htype);
			mem+=len;
			break;
		case HUNK_BSS:
			declare_code(NULL,len<<2,htype);
			break;
		case HUNK_RELOC32:
		case HUNK_RELOC16:
		case HUNK_RELOC8:
		case HUNK_DRELOC32:
		case HUNK_DRELOC16:
		case HUNK_DRELOC8:
			mem=do_reloc(--mem,htype);
			break;
		case HUNK_EXT:
			mem=do_ext(--mem);
			break;
		case HUNK_SYMBOL:
			mem=do_sym(--mem);
			break;
		case HUNK_DEBUG:
			mem+=len;
			break;
		case HUNK_END:
			declare_end_section();
			mem--;
			ret=TRUE;
			break;
		default:
			fatal("invalid hunk type $%lx",(char*)htype);
			break;
		}
	*x=mem;
	return ret;
}

/* given a RAM image of the file, parse it */
void parse_file(char *name, long *mem, long len, void**local)
{
char *unit_name;
long x;
long end=(long)mem+len;

	if ( (len<4) || (*mem++!=HUNK_UNIT) )
		fatal("invalid file '%s'",name);
	if (len&3)
		fatal("file '%s' not a multiple of 4 bytes",name);
	current_input_name=name;
	x=*mem++;
	unit_name=getzmem( (x<<2)+1 );
	memcpy(unit_name,mem,(x<<2)+1);
	mem+=x;
	new_input_file(TRUE,local);
	declare_end_section();
	for (;;)
		{
		if ( ((long)mem)>end)
			fatal("HUNK_END missing",NULL);
		if ( parse_hunk(&mem) && ((long)mem==end) )
			break;
		}
	declare_end_section();
	new_input_file(FALSE,local);
	current_input_name=NULL;
}

void read_file(struct input_list *input)
{
FILE *fp;

	if (show_thinking)
		printf("Reading: %s\n",input->filename);
	if (input->buffer==NULL)
		{
		/* read the file into RAM */
		fp=fopen(input->filename,"rb");
		if (fp==NULL)
			fatal("file '%s' not found",input->filename);
		input->length=filelength(fileno(fp));
		if (input->length==0)
			fatal("file '%s' is zero bytes in length",input->filename);
		input->buffer=getzmem(input->length);
		if (fread(input->buffer,input->length,1,fp)!=1)
			fatal("read error on '%s'",input->filename);
		fclose(fp);
		}
	parse_file(input->filename,input->buffer,input->length,
		&(input->local) );
}

/* do something with each filename. Called once for each pass */
void do_input(bool pass)
{
struct input_list *input;
	pass1=pass;
	input=&dummy_input_list;
	if (input->next==NULL)
		fatal("no input files specified!",NULL);
	while (input->next)
		{
		input=input->next;
		read_file(input);
		}
	if ( link_basic )
		{
		if (!ST)
			link_basic=FALSE;			/* cannot do amigas yet */
		else
			{
			/* we must have a (dummy) BSS section */
			static void *basic_local;
			current_input_name="dummy_basic_bss";
			new_input_file(TRUE,&basic_local);
			declare_end_section();
			declare_code(NULL,0L,HUNK_BSS);
			declare_end_section();
			new_input_file(FALSE,&basic_local);
			current_input_name=NULL;
			}
		}
}
