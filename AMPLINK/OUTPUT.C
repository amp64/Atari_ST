
#include "amplink.h"

#define MAX_SECTION_NAME	99


bool ST;
char *output_name;
FILE *outfile;
word global_hnum;			/* how many there are */
word merged_secnum=-1;
word bss_secnum=-1;
long merged_offset;

struct code_list {
	struct code_list *next;
	void *where;
	long length;
	};

struct reloc {
	long *list;
	long used; long free; };
	
/* each physical output section has one of these */
struct output_section {
	struct output_section *next;
	long number;					/* starts from zero */
	long total;						/* final size in bytes */
	long sofar;						/* where we have got to */
	long sectype;
	struct code_list *code;
	long physical;					/* ST only, where it is in the owner section */
	long filephysical;				/* ST only, where from text seg start */
	char name[MAX_SECTION_NAME+1];
	struct reloc *reloclist;		/* ptr to array of relocs */
	};

/* during pass 1 they are held on a linked list. On pass 2
	then can be accessed via an array
*/
struct output_section dummy_output = { NULL, -1, 0, 0, 99, NULL, 0L, 0L, "dummy_section" };
struct output_section *output_list = &dummy_output;
struct output_section **output_array;

/* dont use this without {}s!! */
#define safewrite(a,b)	if (fwrite(a,b,1,outfile)!=1) fatal("write error on output",NULL)

#define check_word(x,y)	if ( (x>=0x8000L) || (x<-0x8000L) ) error("word overflow using '%s'",y)
#define check_byte(x,y)	if ( (x>=0x80L) || (x<-0x80L) ) error("byte overflow using '%s'",y)
#define check_uword(x,y)	if ( (x>=0x10000L) || (x<-0x10000L) ) error("unsigned word overflow using '%s'",y)
#define check_ubyte(x,y)	if ( (x>=0x100L) || (x<-0x100L) ) error("unsigned byte overflow using '%s'",y)

char *outtype(long htype)
{
static char buf[10];

	switch (htype&0x3FFFFFFFL)
		{
		case HUNK_CODE: strcpy(buf,"CODE"); break;
		case HUNK_DATA: strcpy(buf,"DATA"); break;
		case HUNK_BSS : strcpy(buf,"BSS"); break;
		default: strcpy(buf,"?");
		}
	if (htype&0x80000000)
		strcat(buf,",F");
	if (htype&0x40000000)
		strcat(buf,",C");
	return buf;
}

/* count size of sections of same type before me */
long totalsize(long type, long mask, struct output_section *me)
{
long total=0L;
struct output_section *output;
	output=output_list->next;
	while (output)
		{
		if (output==me)
			return total;
		if ( (output->sectype&mask)==type )
			total+=output->total;
		output=output->next;
		}
	return total;
}

struct {
	word magic;
	long tlen,dlen,blen,symlen,res0,res1;
	word relocflag;
	} stheader;

long st_write( char *name, ubyte len, long value, word secnum )
{
struct {
	char name[8];
	word flag;
	long value;
	char name2[14+1];
	} stsym;
long this;

	if ( (secnum<0) && link_basic )
		secnum=bss_secnum;

	if (secnum>=0)			/* dont save abs symbols */
		{
		switch (output_array[secnum]->sectype&0x3FFFFFFF)
			{
			case HUNK_CODE: stsym.flag=0xa200; break;
			case HUNK_DATA: stsym.flag=0xa400; break;
			case HUNK_BSS : stsym.flag=0xa100; break;
			default:
				fatal("INTERNAL: bad sym owner $%lx",(char*)(output_array[secnum]->sectype));
			}
		stsym.value=value+output_array[secnum]->physical;
		if (len<=8)
			{
			memcpy(stsym.name,name,len);
			memset(stsym.name+len,0,8-len);
			this=14;
			}
		else
			{
			memcpy(stsym.name,name,8);
			stsym.flag|='H';
			if (len>22)
				len=22;
			memcpy(stsym.name2,name+8,len-=8);
			memset(stsym.name2+len,0,14-len);
			this=28;
			}
		safewrite(&stsym,this);
		stheader.symlen+=this;
		}
	return 0;
}

void write_st_reloc(void)
{
struct output_section *output;
long lastreloc=0,nextreloc;
int i;
long count;
long base;
long *thislist,*new;

	/* fortunately most things are in order, which is what we need */
	output=output_list->next;
	while (output)
		{
		base=output->filephysical;
		count=0L;
		if (output->reloclist)
			{
			/* count the total fixups in this section */
			for (i=0; i<global_hnum; i++)
				count+=output->reloclist[i].used;
			}
		if (count)
			{
			/* allocate some room for them */
			new=thislist=getzmem(sizeof(long)*count);
			/* copy them all into this new area */
			for (i=0; i<global_hnum; i++)
				{
				if (output->reloclist[i].used)
					{
					memcpy(new,output->reloclist[i].list,
						output->reloclist[i].used*sizeof(long) );
					new+=output->reloclist[i].used;
					}
				}
			lqsort(new=thislist,count);
			while (count--)
				{
				nextreloc=*new++ + base;
				if (lastreloc==0)
					{
					safewrite(&nextreloc,4);
					}
				else
					{
					long diff;
					diff=nextreloc-lastreloc;
					if (diff<=0)
						fatal("INTERNAL:negative reloc",NULL);
					while (diff>254)
						{
						fputc((char)1,outfile);
						diff-=254;
						}
					fputc((char)diff,outfile);
					}
				lastreloc=nextreloc;
				}
			free(thislist);
			}
		output=output->next;
		}
	if (lastreloc==0)
		{
		safewrite(&lastreloc,4);
		}
	else
		{
		lastreloc=0;
		safewrite(&lastreloc,1);
		}
}

static word amy_secnum;

long amy_write( char *name, ubyte len, long value, word secnum )
{
long pad;
long x; long padsize;

	if (secnum==amy_secnum)
		{
		x=(len+3)>>2;			/* length in longs */
		safewrite(&x,4);
		safewrite(name,len);
		if (padsize=len&3)
			{
			pad=0;
			safewrite(&pad,4-padsize);
			}
		x=value;
		safewrite(&x,4);
		}
	return 0;
}

void dump_amy_section(struct output_section *output)
{
long header[2];
struct code_list *code;
long written;
int i;

	/* we need a header first */
	header[0]=output->sectype;
	header[1]=output->total>>2;
	safewrite(header,8);
	if ((output->sectype&0x3FFFFFFF)!=HUNK_BSS)
		{
		/* non-BSS hunks have data in them! */
		written=0;
		code=output->code;
		while (code)
			{
			safewrite(code->where,code->length);
			written+=code->length;
			code=code->next;
			}
		if (written!=output->total)
			fatal("INTERNAL: wrong code size",NULL);
			
		/* and then some reloc */
		if (output->reloclist)
			{
			header[0]=HUNK_RELOC32;
			safewrite(header,4);
			for (i=0; i<global_hnum; i++)
				{
				if (header[0]=output->reloclist[i].used)
					{
					header[1]=i;
					safewrite(header,4*2);
					safewrite(output->reloclist[i].list,header[0]*4);
					}
				}
			header[0]=0;
			safewrite(header,4);
			}
		}
	/* all may have some debug */
	if (write_debug)
		{
		header[0]=HUNK_SYMBOL;
		safewrite(header,4);
		amy_secnum=output->number;
		write_symbols(amy_write);
		header[0]=0;
		safewrite(header,4);		/* end marker */
		}
	header[0]=HUNK_END;
	safewrite(header,4);
}

void tidy_file(void)
{
	if (ST)
		{
		fseek(outfile,sizeof(stheader)+stheader.tlen+stheader.dlen,SEEK_SET);
		/* (maybe) write some debug */
		if (write_debug)
			write_symbols(st_write);
		write_st_reloc();
		if (write_debug)
			{
			fseek(outfile,0xE,SEEK_SET);
			safewrite(&stheader.symlen,4);
			}
		}
	else
		{
		/* the Amiga dumps everything at this time */
		struct output_section *output;
		output=output_list->next;
		while (output)
			{
			dump_amy_section(output);
			output=output->next;
			}
		}
}

/* if TRUE then after pass1, if FALSE then after pass 2 */
void do_output(bool pass)
{
struct output_section *output,**new;

	output=output_list->next;

	if (pass)
		{
		if (output_name==NULL)
			output_name=get_default_output();
		outfile=fopen(output_name,"wb");
		if (outfile==NULL)
			fatal("cannot create '%s'",output_name);
		new=output_array=getzmem(sizeof(struct output_section*)*global_hnum);
		
		if (ST)
			{
			long pad;
			stheader.magic=0x601a;
			stheader.tlen=totalsize(HUNK_CODE,0x3FFFFFFFL,NULL);
			stheader.dlen=totalsize(HUNK_DATA,0x3FFFFFFFL,NULL);
			stheader.blen=totalsize(HUNK_BSS,0x3FFFFFFFL,NULL);
			stheader.res1=magic_long;
			if (fwrite(&stheader,sizeof(stheader),1,outfile)!=1)
				fatal("write error on output file",NULL);
			pad=stheader.tlen+stheader.dlen;
			if (fwrite(&stheader,pad,1,outfile)!=1)
				fatal("write error on output file",NULL);
			}

		while (output)
			{
			*new++=output;			/* build array from list */
			output->sofar=0L;
			if (strcmp(output->name,"__MERGED")==0)
				{
				if ( (output->sectype&0x3FFFFFFF)==HUNK_BSS )
					{
					if (merged_secnum>=0)
						fatal("more than one MERGED section",NULL);
					if (find_linkerdb(&merged_offset,&merged_secnum))
						{
						/* we have specified a _linkerdb so use it */
						if (merged_secnum!=(word)(output->number))
							fatal("__LinkerDB defined in non-MERGED section",NULL);
						}
					else
						{
						/* use Lattice-style calculation */
						merged_secnum=(word)(output->number);
						if (output->total>0x8000L)
							merged_offset=0x00008000L;
						}
					if (show_thinking)
						printf("MERGED offset=$%lx\n",merged_offset);
					}
				}
			output->reloclist=getzmem(sizeof(struct reloc)*global_hnum);
			if (ST)
				{
				output->filephysical=output->physical=totalsize(output->sectype,0x3FFFFFFF,output);
				switch (output->sectype&0x3FFFFFFFL)
					{
					case HUNK_DATA: output->filephysical+=stheader.tlen; break;
					case HUNK_BSS: output->filephysical+=stheader.tlen+stheader.dlen; break;
					}
				}
			output=output->next;
			}

		if (link_basic)
			{
			output=output_list->next;
			while (output)
				{
				if ((output->sectype&0x3FFFFFFF)==HUNK_BSS)
					{
					bss_secnum=output->number;
					break;
					}
				output=output->next;
				}
			if (bss_secnum<0)
				fatal("INTERNAL: no BSS for BASIC",NULL);
			}

		if (!ST)
			{
			/* ADOS header required */
			long header[5]; int i;
			header[0]=HUNK_HEADER;
			header[1]=0;
			header[2]=global_hnum;
			header[3]=0;
			header[4]=global_hnum-1;
			safewrite(header,sizeof(header));
			for (i=0; i<global_hnum; i++)
				{
				/* dont forget the fast bits! */
				header[0]=output_array[i]->total>>2 |
					(output_array[i]->sectype&0xC0000000);
				safewrite(header,4);
				}
			}
		}
	else
		{
		tidy_file();
		if (fclose(outfile))
			fatal("write error on output file",NULL);
		while (output)
			{
			if (output->sofar!=output->total)
				fatal("INTERNAL: written!=total",NULL);
			output=output->next;
			}
		if (map_file)
			{
			output=output_list->next;
			printf("OUTPUT SECTION MAP\n");
			printf("==================\n");
			while (output)
				{
				printf("%8s %20s %08lX %ld ($%lx $%lx)\n",
					outtype(output->sectype),
					output->name,
					output->total,output->total,
					output->physical,output->filephysical
					);
				output=output->next;
				}
			}
		}
}

struct output_section *find_output_section(long type, char *name)
{
struct output_section *p;
	/* both name and type must match */
	p=output_list;
	while (p->next)
		{
		p=p->next;
		if ( (p->sectype==type) && (strcmp(p->name,name)==0) )
			return p;
		}
	return NULL;
}

struct output_section *create_output_section(long type, char *name)
{
struct output_section *p;
	p=output_list;
	while (p->next)
		p=p->next;
	p->next=getzmem(sizeof(struct output_section));
	p->next->number=p->number+1;
	p->next->sectype=type;
	strcpy(p->next->name,name);
	global_hnum++;
	return p->next;
}

/* add a code fragment to the list */
void add_code_list(struct output_section *output,long *mem,long len)
{
struct code_list *new,*old;

	if ( (output->sectype&0x3FFFFFFFL)==HUNK_BSS )
		{
		/* BSS sections have only 1 entry. Size is current only for this local section */
		if (output->code==NULL)
			output->code=getzmem(sizeof(struct code_list));
		output->code->length=len;
		output->code->where=BADCODE;
		}

	new=getzmem(sizeof(struct code_list));
	new->where=mem;
	new->length=len;
	
	old=output->code;
	if (old==NULL)
		output->code=new;
	else
		{
		while (old->next)
			old=old->next;
		old->next=new;
		}
}

/* called by input code to handle things on input stream */
struct output_section *current_output_section;
char last_name[MAX_SECTION_NAME+1];
void *last_mem; long last_len;

word local_hnum;

/* each file has its own ideas as to section numbers. These need to
	be remembered so we can convert them into real section numbers */

#define MAX_LOCAL_SECTIONS	100
struct { struct output_section *output;
	long sofar;
	} *local_hlist;

/* called at start/end of input files for local/global section matching */
/* should be passed the address of a local pointer */
void new_input_file(bool start, void **local)
{
	if (pass1)
		{
		if (start)
			{
			local_hnum=0;
			local_hlist=*local=getzmem(sizeof(*local_hlist)*MAX_LOCAL_SECTIONS);
			}
		else
			{
			/* shrink it down */
			*local=realloc(*local,sizeof(*local_hlist)*local_hnum);
			if (*local==NULL)
				fatal("out of memory",NULL);
			local_hlist=NULL;
			}
		}
	else
		{
		if (start)
			{
			local_hnum=0;
			local_hlist=*local;
			}
		else
			local_hlist=NULL;		
		}
}

void declare_end_section(void)
{

	if ( (current_output_section) && (!pass1) )
		{
		/* flush out the old one on the ST */
		if (ST)
			{
			long seekpos;
			seekpos=sizeof(stheader)+current_output_section->sofar;
			switch (current_output_section->sectype&0x3FFFFFFFL)
				{
				case HUNK_CODE: break;
				case HUNK_DATA: seekpos+=stheader.tlen; break;
				case HUNK_BSS: 
					current_output_section->sofar+=last_len;
					goto ret;
					break;
				default:
					fatal("INTERNAL:bad flush type",NULL);
				}
			if (fseek(outfile,seekpos,SEEK_SET)==-1)
				fatal("seek error on output file (offset $%lx)",(char*)seekpos);
			/* write the code block out now */
			safewrite(last_mem,last_len);
			}
		else
			{
			/* Amiga waits til very end */
			}
		current_output_section->sofar+=last_len;		
		}
	else if (current_output_section)
		{
		/* we can increase the total now we are finished with it */
		current_output_section->total+=last_len;
		}
ret:
	current_output_section=NULL;
	last_name[0]=0;
	last_len=0;
	last_mem=BADCODE;
}

void declare_section_name(long *mem)
{
long len;
	len=(*mem++)<<2;
	if (len>MAX_SECTION_NAME)
		len=MAX_SECTION_NAME;
	memcpy(last_name,mem,len);
	last_name[len]=0;
}

void declare_code(long *mem, long len, long htype)
{
struct output_section *output;

	if (current_output_section)
		fatal("multiple section declaration",NULL);

	if (show_thinking)
		printf("Declaring %lx bytes of type %lx\n",
			len,htype);

	output=find_output_section(htype,last_name);
	if (pass1)
		{
		if (output==NULL)
			output=create_output_section(htype,last_name);
		last_len=len;
		}
	else
		{
		if (output==NULL)
			fatal("INTERNAL: cant find output section",NULL);
		add_code_list(output,mem,len);
		last_mem=mem; last_len=len;
		}
	
	/* so we can lookup between local secnums & output ones */
	if (local_hnum>=MAX_LOCAL_SECTIONS)
		fatal("too many local sections",NULL);

	if (pass1)
		{
		local_hlist[local_hnum].sofar=output->total;
		local_hlist[local_hnum].output=output;
		}
	else
		if (local_hlist[local_hnum].output!=output)
			fatal("INTERNAL: local hlist trashed",NULL);

	local_hnum++;
	current_output_section=output;
}

void need_reloc(long offset, struct output_section *reference)
{
struct reloc *this;
#if 0
	printf("section %d refers to offset $%lx in section %d\n",
		current_output_section->number,
		offset,
		reference->number
		);
#endif
#define RCHUNK 10
	this=current_output_section->reloclist + reference->number;
	if (this->free==0)
		{
		/* need to allocate a bigger list */
		if (this->list==NULL)
			this->list=getzmem(sizeof(long)*RCHUNK);
		else
			if (
				(this->list = realloc( this->list, sizeof(long)*(this->used+RCHUNK)))
					==NULL )
				fatal("out of memory",NULL);
		this->free=RCHUNK;
		}
	this->list[this->used++]=offset;
	this->free--;
}

/* fixup the current hunk with refs to other (local) hunks */
void declare_reloc(long *mem,long count, long hnum, long rtype)
{
struct output_section *output;
long startoffset,fix;

	if (!pass1)
		{
		if (hnum<0)
			{
			error("Negative hunk number, ignoring reloc",NULL);
			return;
			}
		output=local_hlist[hnum].output;
		if (output==NULL)
			fatal("INTERNAL: null output",NULL);
		startoffset=current_output_section->sofar;
		switch (rtype)
			{
			case HUNK_RELOC32:
				while (count--)
					{
					fix=*mem++;
					if (ST)
						*(long*)((char*)last_mem+fix)+=output->filephysical+local_hlist[hnum].sofar;
					need_reloc(fix + startoffset,output);
					}
				break;
			case HUNK_DRELOC16:
				/* this is a ref to a local hunk-LinkerDB */
				if (output->number!=merged_secnum)
					{
					error("reference to unmerged data item",NULL);
					return;
					}
				while (count--)
					{
					long fixed;
					fix=*mem++;
					fixed=(long)*(word*)((char*)last_mem+fix) - merged_offset;
					check_word(fixed,"<inter hunk reference>");
					*(word*)((char*)last_mem+fix)=(word)fixed;
					}
				break;
			default:
				error("Cannot handle reloc type $%ld\n",(char*)rtype);
				break;
			}
		}
	else
		{
		#if 0
		while (count--)
			{
		printf("section %d refers to offset $%lx in local section %d type $%lx\n",
			current_output_section->number,
			*mem++,
			(int)hnum,rtype
			);
			}
		#endif
		}
}

/* delcare a symbol relative to the current section */
void declare_rel_symbol(char *name,long len, long value)
{
	if (pass1)
		{
		value+=current_output_section->total;
		add_rel_sym(name,len,value,current_output_section->number);
		}
}

void declare_abs_symbol(char *name,long len, long value)
{
	if (pass1)
		add_abs_sym(name,len,value);
}

void declare_dbg_symbol(char *name,long len, long value)
{
	if (pass1)
		{
		value+=current_output_section->total;
		add_dbg_sym(name,len,value,current_output_section->number);
		}
}




void fix_abs_long(long *list,long count, long value, char *buf, char *name)
{
long offset;
	while (count--)
		{
		offset=*list++;
		* (long*) (buf+offset) += value;
		}
}

void fix_abs_word(long *list,long count, long value, char *buf, char *name)
{
long offset;
	while (count--)
		{
		offset=*list++;
		* (word*) (buf+offset) += value;
		}
}

void fix_pc_word(long *list,long count, long value, char *buf, char *name)
{
long offset,newvalue;
	while (count--)
		{
		offset=*list++;
		newvalue=value-offset;
		check_word(newvalue,name);
		* (word*) (buf+offset) += newvalue;
		}

}

void fix_pc_byte(long *list,long count, long value, char *buf, char *name)
{
long offset,newvalue;
	while (count--)
		{
		offset=*list++;
		newvalue=value-offset;
		check_byte(newvalue,name);
		* (byte*) (buf+offset) += newvalue;
		}

}

void fix_abs_byte(long *list,long count, long value, char *buf, char *name)
{
long offset;
	while (count--)
		{
		offset=*list++;
		* (byte*) (buf+offset) += value;
		}
}


/* given a list of fixups, perform them */
void declare_import_list(char *name,long len,long *mem,long count,unsigned char ext)
{
long value; word secnum;
void (*fixup)(long*,long,long,char*,char*);
	if (!pass1)
		{
		#if 0
		if ( (strcmp(name,"COLOR")==0) && (ext==EXT_DREL16) )
			ILLEGAL;
		#endif
		value=find_symbol(name,(ubyte)len,&secnum);
		if ( (link_basic) && (secnum<0) && (ext==EXT_REF32) )
			{
			/* convert long refs to abs symbols to be relative to BSS */
			secnum=bss_secnum;
			}
		if (secnum>=0)
			{
			/* relative symbols can be complex */
			if (ext>=EXT_DREL32)
				{
				/* calculate offset into merged section */
				if (secnum!=merged_secnum)
					{
					error("symbol '%s' not in MERGED section",name);
					return;
					}
				value-=merged_offset;
				goto abs;
				}
			switch (ext)
				{
				case EXT_REF32:
					/* the current section needs refs to another section (or possibly itself) */
					while (count--)
						{
						long offset;
						offset=*mem++;
						*(long*)((long)last_mem+offset)+=value;
						if (ST)
							*(long*)((long)last_mem+offset)+=output_array[secnum]->filephysical;
						need_reloc(offset+current_output_section->sofar,
							output_array[secnum]);
						}
					return;
					break;
				case EXT_REF16:
				case EXT_REF8:
					/* this is a PC-relative reference so must be same section */
					if (secnum!=current_output_section->number)
						{
						error("PC-ref to other section re '%s'",name);
						return;
						}
					fixup= (ext==EXT_REF16) ? fix_pc_word : fix_pc_byte;
					/* the symbols value is relative to the start of the
						output section, hence: */
					value-=current_output_section->sofar;
					goto fix;
					break;
				default:
					warning("cannot fixup refs to '%s'",name);
					return;
					break;
				}
			}

		/* abs symbol fixups are pretty easy */
abs:	switch (ext)
			{
			case EXT_REF32:
			case EXT_DREL32:
				fixup=fix_abs_long; break;
			case EXT_REF16:
				fixup=fix_abs_word;
				check_uword(value,name);
				break;
			case EXT_DREL16:
				fixup=fix_abs_word;
				check_word(value,name);
				break;
			case EXT_REF8:
				fixup=fix_abs_byte;
				check_ubyte(value,name);
			case EXT_DREL8:
				fixup=fix_abs_byte;
				check_byte(value,name);
				break;
			default:
				warning("Cannot fixup abs symbol '%s'",name);
				return;
			}
fix:	(fixup)(mem,count,value,last_mem,name);
		}
}

