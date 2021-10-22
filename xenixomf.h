
/* Defines to map a.out definitions to OMF style definitions
 */
#define	PUBLICDATA	(N_DATA|N_EXT)
#define	PUBLICTEXT	(N_TEXT|N_EXT)
#define	PUBLICABS	(N_ABS|N_EXT)
#define COMMDEF(x)	((x) == (N_UNDF|N_EXT))
#define PRIVDATA	(N_DATA)
#define PRIVTEXT	(N_TEXT)
#define PRIVBSS		(N_BSS)
#define EXTERNAL	(N_UNDF)
#define PUBLIC(x)	((x)==PUBLICDATA || (x)==PUBLICTEXT || (x)==PUBLICABS)
#define EXTDEF(x)	((x)==EXTERNAL || COMMDEF(x))
#define PRIVATE(x)	((x)==PRIVDATA || (x)==PRIVTEXT || (x)==PRIVBSS)


#define MTHEADR		0x80	/* module header */
#define MCOMENT		0x88	/* comment */
#define MMODEND		0x8a	/* module end */
#define M386END		0x8b	/* 32 bit module end */
#define MEXTDEF		0x8c	/* external definition */
#define MTYPDEF		0x8e	/* type definition */
#define MPUBDEF		0x90	/* public definition */
#define MPUB386		0x91	/* 32 bit public definition */
#define MLINNUM		0x94	/* source line number */
#define MLIN386		0x95	/* 32 bit source line number */
#define MLNAMES		0x96	/* name list */
#define MSEGDEF		0x98	/* segment definition */
#define MSEG386		0x99	/* 32 bit segment definition */
#define MGRPDEF		0x9a	/* group definition */
#define MFIXUPP		0x9c	/* fix up previous data image */
#define MFIX386		0x9d	/* fix up previous 32 bit data image */
#define MLEDATA		0xa0	/* logical data image */
#define MLED386		0xa1	/* 32 bit logical data image */
#define	MCOMDEF		0xb0	/* communal names definition */

/* The maximum length of an identifier.
 */

#define OMFNAMELENGTH	127

/* alignment required
 */

#define SD_ABS		0x00		/* absolute */
#define SD_BYTE		0x20		/* byte */
#define SD_WORD		0x40		/* word */
#define SD_PARA		0x60		/* paragraph */
#define SD_PAGE		0x80		/* page */
#define SD_DWORD	0xa0		/* double word */
#define SD_LTL		0xc0		/* load-time locatable */
#define SD_ALIGN	0xe0		/* alignment mask */
#define SD_ASHIFT	5		/* alignment shift */

/* segment combine classes */

#define SD_PRIV		0x00		/* private, can't be combined */
#define SD_HCOMM	0x04		/* common, place in high mem */
#define SD_PUBLIC	0x08		/* public, sequential */
#define SD_BAD		0x0c		/* undefined */
#define SD_C4		0x10		/* not used */
#define SD_STACK	0x14		/* stack segment */
#define SD_COMM		0x18		/* common segment */
#define SD_RCOMM	0x1c		/* not used, reverse common segment */
#define SD_COMBO	0x1c		/* combine mask */
#define SD_CSHIFT	2		/* combine shift */
#define SD_PGRES	0x01		/* page resident */
#define SD_64K		0x02		/* segment size is exactly 64k */

/* BSSDEF record definitions
 */

#define	TD_CNEAR	0x62		/* near .comm variable */
#define	TD_CFAR		0x61		/* far .comm variable */

/* COMENT record definitions
 */

#define	 CMT_PURGE	0x80		/* comment can be purged */
#define  CMT_LIST	0x40		/* don't list when listing comments */

/* FIXUP record definitions
 */

#define  FIX_FIXUP	0x80		/* fixup is a fixup (else thread def) */
#define  TRD_FRAME	0x40		/* thread def for a frame (else tgt) */
#define  TRD_MTHDSHFT	2		/* frame / target method shift */
#define  TRD_MTHDMSK	0x1c		/* frame / target method mask */
#define  TRD_THRED	0x03		/* thread number mask */

/* Target method defines.  0 - 3 are primary, they include an offset, while 
 * 4 - 7 or secondary, the offset 0 and not specified.
 */

#define  TGT_SI		0		/* target is Segment Index(N) + M */
#define  TGT_GI		1		/* target is Group Index(N) + M */
#define  TGT_EI		2		/* target is External Index(N) + M */
#define  TGT_ABS	3		/* target is absolute frame N + N */
#define  TGT_SI_0	4		/* target is Segment Index(N) + 0 */
#define  TGT_GI_0	5		/* target is Group Index(N) + 0 */
#define  TGT_EI_0	6		/* target is External Index(N) + 0 */
#define  TGT_ABS_0	7		/* target is Absolute Segmnent N + 0 */

/* Frame fixup method
 */

#define  FRM_SI		0		/* Frame is Segment index(N) */
#define  FRM_GI		1		/* Frame is Group index(N) */
#define  FRM_EI		2		/* Frame is External index(N) */
#define  FRM_ABS	3		/* Frame is Absolute frame(N) */
#define  FRM_LOC	4		/* Frame is LSEG of LOCATION */
#define  FRM_TRGT	5		/* Frame is the frame of the target */

/* FIXUP fixdat field definitions
 */

#define  FIX_SEG	0x4000		/* Fixup is seg relative (else self) */
#define  FIX_24BIT	0x2000		/* Fixup has 24b tgt disp (NOT USED) */
#define  FIX_LOCMSK	0x3c00		/* Fixup location type mask */
#define  FIX_LOCSHFT	10		/* Fixup location type shift */
#define  FIX_DATAOFF	0x3ff		/* location in prev data rec of fixup */
#define  FIXDAT_FTHRD	0x80		/* Fixup frame by thread else direct */
#define  FIXDAT_FRAME	0x70		/* Frame thread or method mask */
#define  FIXDAT_FRSHFT	4		/* Frame thread or method shift */
#define  FIXDAT_TTHRD	0x08		/* Fixup target by thread else direct */
#define  FIXDAT_TSCND	0x04		/* Primary or secondary target method */
#define  FIXDAT_TRGT	0x03		/* Target thread or method mask */

/* Location type definitions
 */

#define  LOC_LOBYTE	0		/* low 8 bits */
#define  LOC_OFFSET	1		/* 16 bit offset */
#define  LOC_BASE	2		/* 16 bit frame number */
#define  LOC_POINTER	3		/* 32 bit pointer */
#define  LOC_HIBTE	4		/* second 8 bits */
#define  LOC_OFFSETL	5		/* 16 bit offset (liner resolved) */
#define  LOC_OFFSET32	9		/* 32 bit offset */
#define  LOC_POINTER48	11		/* 48 bit pointer */
#define  LOC_OFFSETL32	13		/* 32 bit offset (liner resolved) */
