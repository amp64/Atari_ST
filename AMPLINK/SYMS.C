/* another ***ing linker by AMP. This one takes Lattice linkable
	code and creates Amiga output
	Symbol table handling based on Monxx's
*/

#include "amplink.h"

/* global variables */
sym *global_syms,*local_syms;
bool symcase=TRUE;				/* TRUE is sig, FALSE is not sig */

/* return the length, subject to a max */
ubyte strnlen(char *t, ubyte max)
{
char *s;

	s=t;
	while (*s++)
		if (max--==0)
			break;
	return (ubyte)(--s-t);
}

#define SLOWSYMS 1
#ifdef SLOWSYMS

sym *get_last_sym(sym *global)
{
sym *p;
	if ( (p=global)==NULL)
		return NULL;
	while (p->next)
		p=p->next;
	return p;
}

/* find a given symbol in a given table */
/* returns pointer to struct, or NULL if not there */
/* remember can be used to add it subsequently, or can be NULL */
sym *find_sym(sym **global,char *symname, ubyte symlen, struct remstruct *remember,bool casesig)
{
sym *s;
int cmp;

	if ( (s=*global)==NULL )
		{ /* very first symbol */
		if (remember)
			{
			remember->toadd=global;
			remember->last=NULL;
			}
		return NULL;
		}
	for( ; ; )
		{ /* scan the tree */
		/* sorting is in true AMP tradition of length/ASCII */
		if (symlen < s->len)
			goto goleft;
		else if (symlen > s->len)
			goto goright;
		if (casesig)
			cmp=strncmp(symname,s->name,(unsigned)symlen);
		else
			cmp=strnicmp(symname,s->name,(unsigned)symlen);
		if (cmp>0)
			goto goright;
		else if (cmp==0)
		/* ahah - we have found the beast */
			return s;
		/* look in left branch */
goleft:
		if (s->left==NULL)
			{
			if (remember)
				{
				remember->toadd=&(s->left);
				remember->last=get_last_sym(*global);
				}
			return NULL;
			}
		else
			{
			s=s->left;
			continue;
			}
goright:
		if (s->right==NULL)
			{
			if (remember)
				{
				remember->toadd=&(s->right);
				remember->last=get_last_sym(*global);
				}
			return NULL;
			}
		else
			{
			s=s->right;
			continue;
			}
		}
}
#else
sym *find_sym(sym **,char *, ubyte, struct remstruct *,bool);
#endif

/* after a call to the above, this adds a symbol to a tree */
sym *add_sym(char *symname, ubyte symlen, struct remstruct *remember)
{
word l;
sym *newsym;

	l=sizeof(struct s_sym)+symlen+1;
	newsym=(sym*)getzmem(l);
	newsym->len=symlen;
	strncpy(newsym->name,symname,symlen);
	newsym->name[symlen]=0;					/* null term */
	newsym->next=NULL;
	*(remember->toadd)=newsym;
	if (remember->last)
		(remember->last)->next=newsym;
	return newsym;
}


long foreachsym( sym *s,long (*each)(sym *))
{
long ret;
sym *next;

	if (s==NULL)
		return 0L;

	do
		{
		next=s->next;			/* in case we're freeing! */
		if (ret=(*each)(s))
			return ret;
		}
	while (s=next);
	return 0;
	
#if 0
the old recursive version. Bad news for large GenST symbol tables.
	if (s->left)
		if (ret=foreachsym(s->left,(each)))
			return ret;
	if (ret=(*each)(s))
		return ret;
	if (s->right)
		if (ret=foreachsym(s->right,(each)))
			return ret;
	return 0L;
#endif
}

void add_rel_sym(char *name, long len, long value, long secnum)
{
sym *s;
struct remstruct remember;
char buf[256];

	if (len>255)
		len=255;

	s=find_sym(&global_syms,name,(ubyte)len,&remember,symcase);
	if (s)
		{
		stccpy(buf,name,len);
		fatal("symbol '%s' defined twice",buf);
		}
	s=add_sym(name,(ubyte)len,&remember);
	s->section=(word)secnum;
	s->value=value;
}

void add_dbg_sym(char *name, long len, long value, long secnum)
{
sym *s;
struct remstruct remember;
char buf[256];

	if (len>255)
		len=255;

	s=find_sym(&local_syms,name,(ubyte)len,&remember,symcase);
	if (s)
		{
		stccpy(buf,name,len);
		warning("debug symbol '%s' defined twice, 2nd definition ignored",buf);
		}
	else
		{
		s=add_sym(name,(ubyte)len,&remember);
		s->section=(word)secnum;
		s->value=value;
		}
}

void add_abs_sym(char *name, long len, long value)
{
	add_rel_sym(name,len,value,-1);
}

long find_symbol(char*name,ubyte len,word *section)
{
sym *s;
	s=find_sym(&global_syms,name,len,NULL,symcase);
	if (s==NULL)
		{
		error("symbol '%s' not defined",name);
		*section=0;
		return 0;
		}
	else
		{
		*section=s->section;
		return s->value;
		}
}

#define LINKERDB "__LinkerDB"

bool find_linkerdb(long *value, word *secnum)
{
sym *s;
	s=find_sym(&global_syms,LINKERDB,strlen(LINKERDB),NULL,symcase);
	if (s==NULL)
		return FALSE;
	*value=s->value;
	*secnum=s->section;
	return TRUE;
}

long list_sym(sym *s)
{
	printf("%08lx %02d %s\n",s->value,s->section,s->name);
	return 0;
}

void list_symbols(word flag)
{
	if (flag&1)
		foreachsym(global_syms,list_sym);
	if (flag&2)
		foreachsym(local_syms,list_sym);
}

long (*write_proc)(char*,ubyte,long,word);

static bool filter;

long write_sym(sym *s)
{
	/* dont dump debug symbols if also exports */
	if (filter && find_sym(&global_syms,s->name,s->len,NULL,symcase) )
		return 0;
	/* dont dump linkerdb itself */
	if ( (s->len==strlen(LINKERDB)) && (strcmp(s->name,LINKERDB)==0) )
		return 0;
	return (*write_proc)(s->name,s->len,s->value,s->section);
}

void write_symbols( long (*each)(char*,ubyte,long,word) )
{
	write_proc=each;
	filter=FALSE;
	foreachsym(global_syms,write_sym);
	filter=TRUE;
	foreachsym(local_syms,write_sym);
}
