#include <string.h>
#include <stdio.h>
#define M_XENIX 1
#include "xenixomf.h"
#include "msomf.h"


#ifdef M_XENIX
#  undef i386
#endif

#define MAXRECORDSIZE	(14 * 1024)	/* 1k data => 1024 fixups */
#define I386	1

extern char *out_file_name;

static char chksum = 0;
static unsigned char recordbuff[MAXRECORDSIZE];
static unsigned char *record_ptr;

static void copy_bytes_to_record(p, count)
unsigned char *p;
int count;
{
	while (count--)
	{
		*record_ptr++ = *p;
		chksum += *p++;
	}
}

static void start_record(type)
unsigned char type;
{
	chksum = 0;
	record_ptr = recordbuff;
	copy_bytes_to_record(&type, 1);
	record_ptr += 2;	/* leave space for the record size */
}

static void output_record()
{
	int length;
	unsigned char b;

	length = (record_ptr - recordbuff) + 1; /* 1 for chksum */

	/* patch the record length into the header and put -chksum
	 * at the end of the record.
	 */

	b = (length - 3) & 0xff;  /* -3 as record length excludes type + len */
	chksum += b;  recordbuff[1] = b;
	b = ((length - 3) >> 8) & 0xff;
	chksum += b;  recordbuff[2] = b;
	*record_ptr = (unsigned char)(-chksum);

	output_file_append (recordbuff, length, out_file_name);
}

static void copy_string_to_record(s)
char *s;
{
	unsigned char len;

	len = strlen(s);
	if (len > OMFNAMELENGTH)
	{
		char tname[OMFNAMELENGTH+2];

		strncpy(tname, s, OMFNAMELENGTH);
		tname[OMFNAMELENGTH] = '\0';
		fprintf(stderr, "Identifier truncated to %s (%d chars)\n",
				tname, OMFNAMELENGTH);
		len = OMFNAMELENGTH;
		copy_bytes_to_record(&len, 1);
		copy_bytes_to_record(s, OMFNAMELENGTH);
	}
	else
	{
		copy_bytes_to_record(&len, 1);
		if (len > 0)
			copy_bytes_to_record(s, len);
	}
}

static void copy_index_to_record(index)
unsigned int index;
{
	unsigned char b;

	if (index < 128)
	{
		b = (unsigned char)(index & 0xff);
		copy_bytes_to_record(&b, 1);
	}
	else
	{
		b = (unsigned char)(((index >> 8) & 0xff) | 0x80);
		copy_bytes_to_record(&b, 1);
		b = (unsigned char)(index & 0xff);
		copy_bytes_to_record(&b, 1);
	}
}

static void copy_vint_to_record(value, count)
long value;
int count;
{
	unsigned char b;

	while (count--)
	{
		b = (unsigned char)(value & 0xff);
		copy_bytes_to_record(&b, 1);
		value >>= 8;
	}
}

static void copy_word_to_record(value)
unsigned int value;
{
	copy_vint_to_record((long)value, 2);
}

static void copy_offset_to_record(i386, value)
int i386;
long value;
{
	copy_vint_to_record(value, i386 ? 4 : 2);
}

static void copy_comsize_to_record(value)
long value;
{
	unsigned char b;

	if (value < 128)
		copy_vint_to_record((long)value, 1);
	else if (value < 65536L)
	{
		b = 0x81;
		copy_bytes_to_record(&b, 1);
		copy_vint_to_record((long)value, 2);
	}
	else if (value < 16777216L)
	{
		b = 0x84;
		copy_bytes_to_record(&b, 1);
		copy_vint_to_record((long)value, 3);
	}
	else
	{
		b = 0x88;
		copy_bytes_to_record(&b, 1);
		copy_vint_to_record((long)value, 4);
	}

}

void omf_theadr(name)
unsigned char *name;
{
	start_record(MTHEADR);
	copy_string_to_record(name);
	output_record();
}

void omf_coment(p, count, class)
unsigned char *p;
int count;
unsigned char class;
{
	unsigned char attrib;

	start_record(MCOMENT);
	attrib = 0;		/* Purge and List attributes */
	copy_bytes_to_record(&attrib, 1);
	copy_bytes_to_record(&class, 1);
	copy_bytes_to_record(p, count);
	output_record();
}

void omf_extdef(name, type)
unsigned char *name;
{
	start_record(MEXTDEF);
	copy_string_to_record(name);
	copy_bytes_to_record(&type, 1);
	output_record();
}

void omf_start_pubdef(i386, group, segment, frame)
int i386;
int group, segment, frame;
{
	if (i386)
		start_record(MPUB386);
	else
		start_record(MPUBDEF);

	copy_index_to_record(group);
	copy_index_to_record(segment);
	if (group == 0 && segment == 0)
		copy_word_to_record(frame);
}

void omf_pubdef(i386, name, offset, type)
int i386;
unsigned char *name;
int type;
long offset;
{
	copy_string_to_record(name);
	copy_offset_to_record(i386, offset);
	copy_index_to_record(type);
}

void omf_end_pubdef()
{
	output_record();
}

void omf_start_lnames()
{
	start_record(MLNAMES);
}

void omf_lnames(name)
unsigned char *name;
{
	copy_string_to_record(name);
}

void omf_end_lnames()
{
	output_record();
}

void omf_segdef(i386, acbp, frame, offset, seglen, segname, segclass)
int i386;
unsigned char acbp;
unsigned int frame;
unsigned int offset;
long seglen;
unsigned int segname, segclass;
{
	unsigned int ovlindex = 0;
	if (i386)
		start_record(MSEG386);
	else
		start_record(MSEGDEF);

	copy_bytes_to_record(&acbp, 1);
	if ((acbp & SD_ALIGN) == SD_ABS)
	{
		copy_word_to_record(frame);
		copy_word_to_record(offset);  /* should be byte - Fix Me */
	}
	copy_offset_to_record(i386, seglen);
	copy_index_to_record(segname);
	copy_index_to_record(segclass);
	copy_index_to_record(ovlindex);
	output_record();
}

void omf_start_grpdef(grpname)
unsigned int grpname;
{
	start_record(MGRPDEF);
	copy_index_to_record(grpname);
}

void omf_grpdef(segindex)
unsigned int segindex;
{
	unsigned char b = 0xff;

	copy_bytes_to_record(&b, 1);
	copy_index_to_record(segindex);
}

void omf_end_grpdef()
{
	output_record();
}

void omf_start_comdef()
{
	start_record(MCOMDEF);
}

void omf_comdef(name, dataseg_type, length, el_size)
unsigned char *name;
unsigned char dataseg_type;
{
	unsigned char type = 0;

	copy_string_to_record(name);
	copy_bytes_to_record(&type, 1);
	copy_bytes_to_record(&dataseg_type, 1);

	copy_comsize_to_record(length);
	if (dataseg_type == TD_CFAR)
	{
		copy_comsize_to_record(el_size);
	}
}

void omf_end_comdef()
{
	output_record();
}

void omf_start_linnum(i386, segindex)
{
	unsigned char grpindex = 0;

	if (i386)
		start_record(MLIN386);
	else
		start_record(MLINNUM);

	copy_bytes_to_record(&grpindex, 1);
	copy_index_to_record(segindex);
}

void omf_linnum(i386, line, offset)
int i386;
unsigned int line;
long offset;
{
	copy_word_to_record(line);
	copy_offset_to_record(i386, offset);
}

void omf_end_linnum()
{
	output_record();
}

void omf_start_thread(i386)
{
	if (i386)
		start_record(MFIX386);
	else
		start_record(MFIXUPP);
}

void omf_thread(thread, framethread, method, index)
unsigned char thread;
int framethread;
unsigned char method;
unsigned int index;
{
	unsigned char thread_data;

	thread_data = thread | (method << TRD_MTHDSHFT);
	if (framethread)
		thread_data |= TRD_FRAME;
	copy_bytes_to_record(&thread_data, 1);

	if (framethread)
	{
		switch (method)
		{
		case FRM_SI:
		case FRM_GI:
		case FRM_EI:
		case FRM_ABS:
			copy_index_to_record(index);
			break;
		case FRM_LOC:
		case FRM_TRGT:
			break;
		}
	}
	else
		copy_index_to_record(index);
}

void omf_end_thread()
{
	output_record();
}

void omf_start_fixup(i386)
{
	if (i386)
		start_record(MFIX386);
	else
		start_record(MFIXUPP);
}

static void generate_fixdat(i386, f_thrd, frame, t_thrd, trgt, t_sec,
	       		    frame_index, target_index, offset)
int i386;
int f_thrd;
unsigned int frame;
int t_thrd;
unsigned int trgt;
int t_sec;
unsigned int frame_index, target_index;
long offset;
{
	unsigned char fixdat, b;

	fixdat = (f_thrd ? FIXDAT_FTHRD : 0) | (frame << FIXDAT_FRSHFT);
	fixdat |= (t_thrd ? FIXDAT_TTHRD : 0) | trgt | (t_sec ? FIXDAT_TSCND:0);
	copy_bytes_to_record(&fixdat, 1);

	if (!f_thrd)
	{
		switch (frame)
		{
		case FRM_SI:
		case FRM_GI:
		case FRM_EI:
		case FRM_ABS:
			copy_index_to_record(frame_index);
			break;
		case FRM_LOC:
		case FRM_TRGT:
			break;
		}
	}

	if (!t_thrd)
		copy_index_to_record(target_index);
	
	if (!t_sec)
		copy_offset_to_record(i386, offset);
}

void omf_fixup(i386, segrel, loc, data_off, f_thrd, frame, t_thrd, trgt, t_sec,
	       frame_index, target_index, offset)
int i386, segrel;
unsigned char loc;
unsigned int data_off;
int f_thrd;
unsigned int frame;
int t_thrd;
unsigned int trgt;
int t_sec;
unsigned int frame_index, target_index;
long offset;
{
	unsigned int locat = 0x8000;
	unsigned char fixdat, b;

	locat |= (segrel ? FIX_SEG : 0) | (loc << FIX_LOCSHFT) |
		 (data_off & FIX_DATAOFF);
	b = (locat >> 8) & 0xff;  copy_bytes_to_record(&b, 1);
	b = locat & 0xff;  copy_bytes_to_record(&b, 1);
	generate_fixdat(i386, f_thrd, frame, t_thrd, trgt, t_sec, frame_index,
			target_index, offset);
}

void omf_end_fixup()
{
	output_record();
}

void omf_start_ledata(i386, segindex, start_offset)
int i386;
unsigned int segindex;
long start_offset;
{
	if (i386)
		start_record(MLED386);
	else
		start_record(MLEDATA);

	copy_index_to_record(segindex);
	copy_offset_to_record(i386, start_offset);
}

void omf_ledata(data, count)
unsigned char *data;
int count;
{
	copy_bytes_to_record(data, count);
	
}

void omf_end_ledata()
{
	output_record();
}

void omf_main_modend(i386, f_thrd, frame, t_thrd, trgt, t_sec, frame_index,
		     target_index, offset)
int i386;
int f_thrd;
unsigned int frame;
int t_thrd;
unsigned int trgt;
int t_sec;
unsigned int frame_index, target_index;
long offset;
{
	unsigned char mtype = 0xc1;

	if (i386)
		start_record(M386END);
	else
		start_record(MMODEND);

	copy_bytes_to_record(&mtype, 1);

	generate_fixdat(i386, f_thrd, frame, t_thrd, trgt, t_sec, frame_index,
			target_index, offset);
	output_record();
}

void omf_modend(i386)
int i386;
{
	unsigned char mtype = 0;

	if (i386)
		start_record(M386END);
	else
		start_record(MMODEND);

	copy_bytes_to_record(&mtype, 1);
	output_record();
}

/** Definitions required to produce a standard Microsoft .o file
 */

/** LNAMES, SEGDEF, GRPDEF and COMENT stuff
 *
 *  NB the following defines must reflect the position of the string in
 *  the lnames_tab table.
 */

#define	L_BLANK		1
#define	L_DGROUP	2
#define L_UTEXT		3
#define L_CODE		4
#define L_UDATA		5
#define L_DATA		6
#define L_CONST		7
#define L_UBSS		8
#define L_BSS		9
#define L_TYPES		10
#define L_DEBTYP	11
#define L_SYMBOLS	12
#define L_DEBSYM	13
#define L_TSIZE		9
#define L_G_TSIZE	13

char *lnames_tab[] = {
	"",
	"",
	"DGROUP",
	"_TEXT",
	"CODE",
	"_DATA",
	"DATA",
	"CONST",
	"_BSS",
	"BSS",
	"$$TYPES",
	"DEBTYP",
	"$$SYMBOLS",
	"DEBSYM"
};

struct segtable {
	unsigned char attrib;
	long length;
	unsigned nameindex;
	unsigned classindex;
};

/* SEGDEF's */

static struct segtable segt[] = {
	{0, NULL, 0, 0},
	{SD_DWORD|SD_PUBLIC|SD_PGRES, 0, L_UTEXT, L_CODE},
	{SD_DWORD|SD_PUBLIC|SD_PGRES, 0, L_UDATA, L_DATA},
	{SD_DWORD|SD_PUBLIC|SD_PGRES, 0, L_CONST, L_CONST},
	{SD_DWORD|SD_PUBLIC|SD_PGRES, 0, L_UBSS, L_BSS},
	{SD_BYTE|SD_PGRES, 0, L_SYMBOLS, L_DEBSYM},
	{SD_BYTE|SD_PGRES, 0, L_TYPES, L_DEBTYP}
};

/* GRPDEF */

#define GRPTABSIZ 3

unsigned int group_tab[GRPTABSIZ] = {3, 4, 2};

/* COMENTs */

struct comment {
	unsigned char class;
	unsigned char count;
	unsigned char *data;
};

#define NUMFIXEDCOMMENTS 4

static unsigned char gas_comment[] = "gas-1.38.1a";
static unsigned char lib_comment[] = "LIBCRT";
static unsigned char model_comment[] = "3s";
static unsigned char msext_comment[] = {1, 0x43, 0x56};

struct comment fix_cmnt[NUMFIXEDCOMMENTS] = {
	{0, 8, gas_comment},
	{0x9f, 6, lib_comment},
	{0x9d, 2, model_comment},
	{0xa1, 3, msext_comment}
};

void omf_initialize_a_out(name, text_size, data_size, bss_size, const_size,
			  gdb, syms_size, str_size)
unsigned char *name;
long text_size, data_size, bss_size, const_size;
int gdb;
long syms_size, str_size;
{
	int i;

	omf_theadr(name);

	/* static comments */

	for (i = 0; i < NUMFIXEDCOMMENTS; i++)
	  omf_coment(fix_cmnt[i].data, fix_cmnt[i].count, fix_cmnt[i].class);

	/* LNAMES */

	omf_start_lnames();
	for (i = 1; i <= (gdb ? L_G_TSIZE : L_TSIZE); i++)
		omf_lnames(lnames_tab[i]);
	omf_end_lnames();

	/* SEGDEFS */

	segt[SDEF_TEXT].length = text_size;
	segt[SDEF_DATA].length = data_size;
	segt[SDEF_BSS].length = bss_size;
	segt[SDEF_CONST].length = const_size;
	if (gdb)
	{
		segt[SDEF_SYMBOLS].length = syms_size;
		segt[SDEF_TYPES].length = str_size;
	}
	for (i = 1; i <= (gdb ? SDEF_G_SIZE : SDEF_SIZE); i++)
		omf_segdef(I386, segt[i].attrib, 0, 0L, segt[i].length,
			   segt[i].nameindex, segt[i].classindex);
	/* DGROUP */

	omf_start_grpdef(L_DGROUP);
	for (i = 0; i < GRPTABSIZ; i++)
		omf_grpdef(group_tab[i]);
	omf_end_grpdef();
}
