
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* these are *supposed* to be in string.h */
#ifndef max
#define max(x,y)	__builtin_max(x,y)
#define min(x,y)	__builtin_min(x,y)
#endif

/* lets have some types */
typedef short word;
typedef unsigned short uword;
typedef char byte;
typedef unsigned char ubyte;
typedef unsigned char uchar;
typedef unsigned long ulong;
typedef char bool;
typedef unsigned char schar;

#define TRUE	1
#define	FALSE	0

#define ILLEGAL	__builtin_emit(0x4afc)
void __builtin_emit(int);

/* we use a good old binary tree to store symbols, of this struct: */

struct s_sym {
	struct sym *left,*right;
	struct sym *next;		/* singly-linked list of all syms */
	long value;				/* value from start of section */
	word section;			/* section number, -1 for abs */
	ubyte len;				/* of name */
	char name[1];			/* null termed */
	};

typedef struct s_sym sym;

struct remstruct {
	sym **toadd;			/* points to left or right */
	sym *last;				/* points to last one added */
	};

#define	BADCODE	(void*)-1L

#define HUNK_UNIT	999L
#define	HUNK_NAME	1000L
#define	HUNK_CODE	1001L
#define	HUNK_DATA	1002L
#define	HUNK_BSS	1003L
#define	HUNK_RELOC32	1004L
#define	HUNK_RELOC16	1005L
#define	HUNK_RELOC8	1006L
#define	HUNK_EXT	1007L
#define	HUNK_SYMBOL	1008L
#define	HUNK_DEBUG	1009L
#define	HUNK_END	1010L
#define	HUNK_HEADER	1011L
#define	HUNK_DRELOC32	1015L
#define	HUNK_DRELOC16	1016L
#define	HUNK_DRELOC8	1017L

#define	EXT_DEF	1
#define	EXT_ABS	2
#define	EXT_REF32	129
#define	EXT_REF16	131
#define	EXT_REF8	132
#define	EXT_DREL32	133
#define	EXT_DREL16	134
#define	EXT_DREL8	135

/* from syms.c */
extern sym *global_syms,*local_syms;
extern bool symcase;				/* TRUE is sig, FALSE is not sig */
void add_rel_sym(char *name, long len, long value, long secnum);
void add_dbg_sym(char *name, long len, long value, long secnum);
void list_symbols(word);
void write_symbols( long(*each)(char*,ubyte,long,word) );
long find_symbol(char*name,ubyte len,word *sec);
void add_abs_sym(char *name, long len, long value);
bool find_linkerdb(long *value, word *secnum);

/* from main.c */
void *getzmem(long);
void fatal(char *,char*);
void error(char *,char*);
void warning(char *,char*);
extern bool pass1,show_thinking;
extern char *current_input_name;
extern bool map_file,sym_file,write_debug;
extern long magic_long;
int check_command(char*);
extern bool link_basic;

/* from input.c */
void add_input(char*);
void add_with(char*);
void do_input(bool);
char *get_default_output(void);

/* from output.c */
extern char *output_name;
extern bool ST;
void do_output(bool);
void declare_reloc(long *mem,long count, long hnum,long rtype);
void declare_rel_symbol(char *name,long len, long value);
void declare_abs_symbol(char *name,long len, long value);
void declare_dbg_symbol(char *name,long len, long value);
void declare_import_list(char *name,long len,long *mem,long value,unsigned char ext);
void declare_end_section(void);
void declare_section_name(long *);
void declare_code(long *mem, long len, long htype);
void new_input_file(bool,void**);
