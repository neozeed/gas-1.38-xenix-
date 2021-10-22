/* write.c - emit .o file - Copyright(C)1986 Free Software Foundation, Inc.
   Copyright (C) 1986,1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "as.h"
#include "md.h"
#include "subsegs.h"
#include "obstack.h"
#include "struc-symbol.h"
#include "write.h"
#include "symbols.h"

#include "xenixomf.h"
#include "msomf.h"

#if __STDC__
#include <stddef.h>
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

void	append();

/*
 * In: length of relocation (or of address) in chars: 1, 2 or 4.
 * Out: OMF LOC type.
 */

static unsigned char nbytes_r_length [] = {
  15, LOC_LOBYTE, LOC_OFFSET, 15, LOC_OFFSET32
  };


static struct frag *	text_frag_root;
static struct frag *	data_frag_root;

static struct frag *	text_last_frag;	/* Last frag in segment. */
static struct frag *	data_last_frag;	/* Last frag in segment. */

static struct exec	the_exec;

static long int string_byte_count;
static long int stab_symbol_count;

void	relax_segment();
long int	fixup_segment();

static void threads(type, trgt_thrd, frm_thrd)
int type;
int *trgt_thrd, *frm_thrd;
{
	  switch (type)
	  {
	    case PRIVDATA:	*trgt_thrd = DATA_TGT_THREAD;
				*frm_thrd = DGROUP_FRAME_THREAD;
				break;
	    case PRIVBSS:	*trgt_thrd = BSS_TGT_THREAD;
				*frm_thrd = DGROUP_FRAME_THREAD;
				break;
	    case PRIVTEXT:	*trgt_thrd = TEXT_TGT_THREAD;
				*frm_thrd = TEXT_FRAME_THREAD;
				break;
	    case PUBLICDATA:	*trgt_thrd = DATA_TGT_THREAD;
				*frm_thrd = DGROUP_FRAME_THREAD;
				break;
	    case PUBLICTEXT:	*trgt_thrd = TEXT_TGT_THREAD;
				*frm_thrd = TEXT_FRAME_THREAD;
				break;
	    default:		*trgt_thrd = CONST_TGT_THREAD;
				*frm_thrd = DGROUP_FRAME_THREAD;
				break;
	  }
}

void
write_object_file()
{
  register struct frchain *	frchainP; /* Track along all frchains. */
  register fragS *		fragP;	/* Track along all frags. */
  register struct frchain *	next_frchainP;
  register fragS * *		prev_fragPP;
  register char *		name;
  register symbolS *		symbolP;
  register symbolS **		symbolPP;
  /* register fixS *		fixP; JF unused */
  unsigned long
  	text_siz,
	data_siz,
	syms_siz,
	tr_siz,
	dr_siz;
  void output_file_create();
  void output_file_append();
  void output_file_close();
  extern long omagic;		/* JF magic # to write out.  Is different for
				   Suns and Vaxen and other boxes */

  /*
   * After every sub-segment, we fake an ".align ...". This conforms to BSD4.2
   * brane-damage. We then fake ".fill 0" because that is the kind of frag
   * that requires least thought. ".align" frags like to have a following
   * frag since that makes calculating their intended length trivial.
   */
#define SUB_SEGMENT_ALIGN (2)
  for ( frchainP=frchain_root; frchainP; frchainP=frchainP->frch_next )
    {
      subseg_new (frchainP -> frch_seg, frchainP -> frch_subseg);
      frag_align (SUB_SEGMENT_ALIGN, 0);
				/* frag_align will have left a new frag. */
				/* Use this last frag for an empty ".fill". */
      /*
       * For this segment ...
       * Create a last frag. Do not leave a "being filled in frag".
       */
      frag_wane (frag_now);
      frag_now -> fr_fix	= 0;
      know( frag_now -> fr_next == NULL );
      /* know( frags . obstack_c_base == frags . obstack_c_next_free ); */
      /* Above shows we haven't left a half-completed object on obstack. */
    }

  /*
   * From now on, we don't care about sub-segments.
   * Build one frag chain for each segment. Linked thru fr_next.
   * We know that there is at least 1 text frchain & at least 1 data frchain.
   */
  prev_fragPP = &text_frag_root;
  for ( frchainP=frchain_root; frchainP; frchainP=next_frchainP )
    {
      know( frchainP -> frch_root );
      * prev_fragPP = frchainP -> frch_root;
      prev_fragPP = & frchainP -> frch_last -> fr_next;
      if (   ((next_frchainP = frchainP->frch_next) == NULL)
	  || next_frchainP == data0_frchainP)
	{
	  prev_fragPP = & data_frag_root;
	  if ( next_frchainP )
	    {
	      text_last_frag = frchainP -> frch_last;
	    }
	  else
	    {
	      data_last_frag = frchainP -> frch_last;
	    }
	}
    }				/* for(each struct frchain) */

  relax_segment (text_frag_root, SEG_TEXT);
  relax_segment (data_frag_root, SEG_DATA);
  /*
   * Now the addresses of frags are correct within the segment.
   */

  text_siz=text_last_frag->fr_address;

  /*
   *
   * Determine a_data [length of data segment].
   */
  if (data_frag_root)
      data_siz=data_last_frag->fr_address;
  else
      data_siz = 0;

  bss_address_frag . fr_address = 0;
	      
  /*
   *
   * Crawl the symbol chain.
   *
   * For each symbol whose value depends on a frag, take the address of
   * that frag and subsume it into the value of the symbol.
   * After this, there is just one way to lookup a symbol value.
   * Values are left in their final state for object file emission.
   * We adjust the values of 'L' local symbols, even if we do
   * not intend to emit them to the object file, because their values
   * are needed for fix-ups.
   *
   * Unless we saw a -L flag, remove all symbols that begin with 'L'
   * from the symbol chain.
   *
   * Count the (length of the nlists of the) (remaining) symbols.
   * Assign a symbol number to each symbol.
   * Count the number of string-table chars we will emit.
   *
   */
  string_byte_count = sizeof( string_byte_count );

  /* JF deal with forward references first. . . */
  for(symbolP=symbol_rootP;symbolP;symbolP=symbolP->sy_next) {
  	if(symbolP->sy_forward) {
		symbolP->sy_value+=symbolP->sy_forward->sy_value+symbolP->sy_forward->sy_frag->fr_address;
		symbolP->sy_forward=0;
	}
  }
  symbolPP = & symbol_rootP;	/* -> last symbol chain link. */
  {
    register long int		symbol_number;

    symbol_number = 1;  stab_symbol_count = 0;

    while (symbolP  = * symbolPP)
      {
	name = symbolP -> sy_name;
	symbolP -> sy_value += symbolP -> sy_frag -> fr_address;
	if ( !name || (symbolP->sy_nlist.n_type&N_STAB)
	    || (name[0]!='\001' && (flagseen ['L'] || name [0] != 'L' )))
	  {
	    if (EXTDEF(symbolP->sy_nlist.n_type))
	    	symbolP -> sy_number = symbol_number ++;
	    else
	    	symbolP -> sy_number = 0;
	    if (symbolP->sy_nlist.n_type&N_STAB)
		stab_symbol_count++;
	    if (name && symbolP->sy_nlist.n_type&N_STAB)
	      {			/* Ordinary case. */
		symbolP -> sy_name_offset = string_byte_count;
		string_byte_count += strlen (symbolP  -> sy_name) + 1;
	      }
	    else			/* .Stabd case. */
		symbolP -> sy_name_offset = 0;
	    symbolPP = & (symbolP -> sy_next);
	  }
	else
	    * symbolPP = symbolP -> sy_next;
      }				/* for each symbol */

    syms_siz = sizeof( struct nlist) * symbol_number;
  }

  /*
   * Addresses of frags now reflect addresses we use in the object file.
   * Symbol values are correct.
   * Scan the frags, converting any ".org"s and ".align"s to ".fill"s.
   * Also converting any machine-dependent frags using md_convert_frag();
   */
  subseg_change( SEG_TEXT, 0);

  for (fragP = text_frag_root;  fragP;  fragP = fragP -> fr_next)
    {
      switch (fragP -> fr_type)
	{
	case rs_align:
	case rs_org:
	  fragP -> fr_type = rs_fill;
	  know( fragP -> fr_var == 1 );
	  know( fragP -> fr_next );
	  fragP -> fr_offset
	    =     fragP -> fr_next -> fr_address
	      -   fragP -> fr_address
		- fragP -> fr_fix;
	  break;

	case rs_fill:
	  break;

	case rs_machine_dependent:
	  md_convert_frag (fragP);
	  /*
	   * After md_convert_frag, we make the frag into a ".space 0".
	   * Md_convert_frag() should set up any fixSs and constants
	   * required.
	   */
	  frag_wane (fragP);
	  break;

	default:
	  BAD_CASE( fragP -> fr_type );
	  break;
	}			/* switch (fr_type) */
    }				/* for each frag. */


  subseg_change( SEG_DATA, 0);

  for (fragP = data_frag_root;  fragP;  fragP = fragP -> fr_next)
    {
      switch (fragP -> fr_type)
	{
	case rs_align:
	case rs_org:
	  fragP -> fr_type = rs_fill;
	  know( fragP -> fr_var == 1 );
	  know( fragP -> fr_next );
	  fragP -> fr_offset
	    =     fragP -> fr_next -> fr_address
	      -   fragP -> fr_address
		- fragP -> fr_fix;
	  break;

	case rs_fill:
	  break;

	case rs_machine_dependent:
	  md_convert_frag (fragP);
	  /*
	   * After md_convert_frag, we make the frag into a ".space 0".
	   * Md_convert_frag() should set up any fixSs and constants
	   * required.
	   */
	  frag_wane (fragP);
	  break;

	default:
	  BAD_CASE( fragP -> fr_type );
	  break;
	}			/* switch (fr_type) */
    }				/* for each frag. */

  subseg_change( SEG_TEXT, 0);

  fixup_segment (text_fix_root, N_TEXT);
  fixup_segment (data_fix_root, N_DATA);
  output_file_create (out_file_name);
  omf_initialize_a_out(module_name, text_siz, data_siz,
		       (long) local_bss_counter, 0L,
		       flagseen['g'], stab_symbol_count * sizeof(struct nlist),
		       string_byte_count);

/*  Traverse the symbol chain emitting external symbol definitions
 *  for external, public and common symbols.
 */

  for (   symbolP = symbol_rootP;   symbolP;   symbolP = symbolP -> sy_next   )
  {
      if (EXTDEF(symbolP->sy_type))
      {
          if (COMMDEF(symbolP->sy_type))
          {
              omf_start_comdef();
              omf_comdef(symbolP->sy_name, TD_CNEAR, symbolP->sy_value, 0L);
              omf_end_comdef();
          }
          else
              omf_extdef(symbolP->sy_name, 0);
      }
  }

/*  Traverse the symbol chain emitting public definitions
 */

  for (   symbolP = symbol_rootP;   symbolP;   symbolP = symbolP -> sy_next   )
  {
    if (PUBLIC(symbolP->sy_type))
    {
        unsigned int group = 0, segment = 0;

        if (symbolP->sy_type == PUBLICTEXT)
            segment = SDEF_TEXT;
        if (symbolP->sy_type == PUBLICDATA)
        {
            group = GDEF_DGROUP;  segment = SDEF_DATA;
        }
        
        omf_start_pubdef(I386, group, segment, 0);
        omf_pubdef(I386, symbolP->sy_name, symbolP->sy_value, 0);
        omf_end_pubdef();
    }
  }
  

  /* Setup threads for DGROUP and the segments ready for relocation */

  omf_start_thread(I386);
  omf_thread(CONST_TGT_THREAD, TARGET_THREAD, FRM_SI, SDEF_CONST);
  omf_thread(DATA_TGT_THREAD, TARGET_THREAD, FRM_SI, SDEF_DATA);
  omf_thread(TEXT_TGT_THREAD, TARGET_THREAD, FRM_SI, SDEF_TEXT);
  omf_thread(BSS_TGT_THREAD, TARGET_THREAD, FRM_SI, SDEF_BSS);
  omf_thread(TEXT_FRAME_THREAD, FRAME_THREAD, FRM_SI, SDEF_TEXT);
  omf_thread(DGROUP_FRAME_THREAD, FRAME_THREAD, FRM_GI, GDEF_DGROUP);
  omf_end_thread();
  
  /*
   * Emit code.
   */

  for (fragP = data_frag_root;  fragP;  fragP = fragP -> fr_next)
  {
      register long int count;
      register char * fill_literal;
      register long int fill_size;
      register fixS * fixP;
      register symbolS * symbolP;
      int started_fixup;

      know( fragP -> fr_type == rs_fill );

      if (fragP->fr_fix != 0 || fragP->fr_offset != 0)
      {
          omf_start_ledata(I386, SDEF_DATA, fragP->fr_address);
          if (fragP->fr_fix != 0)
              omf_ledata(fragP->fr_literal, (int)fragP->fr_fix);
          fill_literal = fragP->fr_literal + fragP -> fr_fix;
          fill_size = fragP->fr_var;
          count = fragP -> fr_offset;
	  if ((fragP->fr_fix + count * fill_size) < 1016)
	  {
              for ( ; count; count--)
                  omf_ledata(fill_literal, (int)fill_size);
	  }
	  else  /* filling will exceed the 1024 byte record size */
	  {
	      /* This code assumes that fill_size <= 8 bytes.
	       * This is infact the case because of
	       * compatability with other assemblers 
	       */
	      register int fcount;

	      fcount = (1016 - fragP->fr_fix) / fill_size;
	      count -= fcount;
	      for ( ; fcount > 0; fcount--)
                  omf_ledata(fill_literal, (int)fill_size);
	      
	  }
          omf_end_ledata();

          /* Emit the relocations for this frag
           */

          fixP = data_fix_root;  started_fixup = 0;
          for ( ;  fixP;  fixP = fixP -> fx_next)
          {
              if (fixP->fx_frag == fragP && (symbolP = fixP->fx_addsy))
              {
		  int trgt_thrd, frm_thrd;
		  int external = EXTDEF(symbolP->sy_type);

		  threads(symbolP->sy_type, &trgt_thrd, &frm_thrd);
		  if (fixP->fx_pcrel)
			frm_thrd = TEXT_FRAME_THREAD;

                  if (!started_fixup)
                  {
                      omf_start_fixup(I386);
                      started_fixup++;
                  }
                  omf_fixup(I386, !fixP->fx_pcrel,
		        nbytes_r_length [fixP->fx_size], fixP->fx_where,
    			external ? 0 : 1,
    			external ? FRM_TRGT : frm_thrd,
    			external ? 0 : 1,
    			external ? TGT_EI : trgt_thrd,
    			1, /* No offset */
			0, /* No frame required */
    			EXTDEF(symbolP->sy_type) ? symbolP->sy_number : 0,
			0);
	      }
          }
    	  if (started_fixup)
              omf_end_fixup();
	  
	  while (count > 0)	/* still sum fill data to emit */
	  {
	      unsigned long address;
	      unsigned int len;

              address = fragP->fr_address + fragP->fr_fix +
			  (fragP->fr_offset - count) * fill_size;
              omf_start_ledata(I386, SDEF_DATA, address);
              for ( len = 0; count && (len < 1000); count--, len += fill_size)
                  omf_ledata(fill_literal, (int)fill_size);
              omf_end_ledata();
	  }
      }
  }

  for (fragP = text_frag_root;  fragP;  fragP = fragP -> fr_next)
  {
      register long int count;
      register char * fill_literal;
      register long int fill_size;
      register fixS * fixP;
      register symbolS * symbolP;
      int started_fixup;

      know( fragP -> fr_type == rs_fill );

      if (fragP->fr_fix != 0 || fragP->fr_offset != 0)
      {
          omf_start_ledata(I386, SDEF_TEXT, fragP->fr_address);
          if (fragP->fr_fix != 0)
              omf_ledata(fragP->fr_literal, (int)fragP->fr_fix);
          fill_literal = fragP->fr_literal + fragP -> fr_fix;
          fill_size = fragP->fr_var;
          for (count = fragP -> fr_offset;  count;  count --)
              omf_ledata(fill_literal, (int)fill_size);
          omf_end_ledata();

          /* Emit the relocations for this frag
           */

          fixP = text_fix_root;  started_fixup = 0;
          for ( ;  fixP;  fixP = fixP -> fx_next)
          {
              if (fixP->fx_frag == fragP && (symbolP = fixP->fx_addsy))
              {
		  int trgt_thrd, frm_thrd;
		  int external = EXTDEF(symbolP->sy_type);

		  threads(symbolP->sy_type, &trgt_thrd, &frm_thrd);
		  if (fixP->fx_pcrel)
			frm_thrd = TEXT_FRAME_THREAD;

                  if (!started_fixup)
                  {
                      omf_start_fixup(I386);
                      started_fixup++;
                  }
                  omf_fixup(I386, !fixP->fx_pcrel,
		        nbytes_r_length [fixP->fx_size], fixP->fx_where,
    			external ? 0 : 1,
    			external ? FRM_TRGT : frm_thrd,
    			external ? 0 : 1,
    			external ? TGT_EI : trgt_thrd,
    			1, /* No offset */
			0, /* No frame required */
    			external ? symbolP->sy_number : 0,
			0);
	      }
          }
    	  if (started_fixup)
              omf_end_fixup();
      }
  }

/*  Traverse the symbol chain emitting stabs.
 */

#define VAL_OFF offsetof(struct nlist, n_value)

  if (flagseen['g'] && stab_symbol_count)
  {   
      long count = 0, ncount = 0;
      symbolS *last_startP;
      char *temp;
      symbolS *fsymP;
      int i;
      int start_fixup = 0;

      omf_start_ledata(I386, SDEF_SYMBOLS, 0);
      last_startP = symbol_rootP;
      for (symbolP = symbol_rootP; symbolP; symbolP = symbolP -> sy_next)
      {
	 if (symbolP->sy_nlist.n_type & N_STAB)
	 {
            if ((ncount + 1) * sizeof(struct nlist) >= 1016)
            {
               omf_end_ledata();

               /* emit relocations */

               start_fixup = 0;
	       for (i = 0, fsymP=last_startP; i < ncount; fsymP=fsymP->sy_next)
	       {
	          if (fsymP->sy_nlist.n_type & N_STAB)
		  {
                     if (fsymP->sy_type & N_TYPE) /* if relocation required */
		     {
		       int trgt_thrd, frm_thrd;
		       int external = EXTDEF(fsymP->sy_type & N_TYPE);

		       threads(fsymP->sy_type & N_TYPE,&trgt_thrd,&frm_thrd);
		       if (!start_fixup)
		       {
                           omf_start_fixup(I386);
			   start_fixup++;
		       }
		       omf_fixup(I386, 1, /* segment relative */
		          LOC_OFFSET32, i * sizeof(struct nlist) + VAL_OFF,
    			  external ? 0 : 1,
    			  external ? FRM_TRGT : frm_thrd,
    			  external ? 0 : 1,
    			  external ? TGT_EI : trgt_thrd,
    			  1, /* No offset */
			  0, /* No frame required */
    			  external ? fsymP->sy_number : 0,
			  0);
		     }
		     i++;
		  }
	       }
	       if (start_fixup)
                  omf_end_fixup();

               /* start new segment */

               ncount = 0;  last_startP = symbolP;
               omf_start_ledata(I386, SDEF_SYMBOLS, count*sizeof(struct nlist));
            }
            temp = symbolP->sy_nlist.n_un.n_name;
            symbolP->sy_nlist.n_un.n_strx = symbolP->sy_name_offset;
            omf_ledata(&(symbolP->sy_nlist), sizeof(struct nlist));
            symbolP->sy_nlist.n_un.n_name = temp;
   	    ncount++;  count++;
	 }
      }
      omf_end_ledata();
      start_fixup = 0;
      for (i = 0, fsymP = last_startP; i < ncount; fsymP = fsymP->sy_next)
      {
         if (fsymP->sy_nlist.n_type & N_STAB)
	 {
            if (fsymP->sy_type & N_TYPE) /* if relocation required */
	    {
	      int trgt_thrd, frm_thrd;
	      int external = EXTDEF(fsymP->sy_type & N_TYPE);

	      threads(fsymP->sy_type & N_TYPE,&trgt_thrd,&frm_thrd);
	      if (!start_fixup)
              {
                 omf_start_fixup(I386);
                 start_fixup++;
              }
	      omf_fixup(I386, 1, /* segment relative */
	         LOC_OFFSET32, i * sizeof(struct nlist) + VAL_OFF,
   		  external ? 0 : 1,
   		  external ? FRM_TRGT : frm_thrd,
   		  external ? 0 : 1,
   		  external ? TGT_EI : trgt_thrd,
   		  1, /* No offset */
                  0, /* No frame required */
   		  external ? fsymP->sy_number : 0,
		  0);
	    }
	    i++;
	 }
      }
      if (start_fixup)
         omf_end_fixup();

      count = sizeof(string_byte_count);  ncount = count;
      omf_start_ledata(I386, SDEF_TYPES, 0);
      omf_ledata(&string_byte_count, sizeof(string_byte_count));
      for (symbolP = symbol_rootP; symbolP; symbolP = symbolP -> sy_next)
      {
         if (symbolP->sy_nlist.n_type & N_STAB && symbolP -> sy_name)
	 {
	    int len = strlen(symbolP -> sy_name) + 1;

	    if (ncount + len > 1016)
	    {
	       omf_end_ledata();
               omf_start_ledata(I386, SDEF_TYPES, count);
	       ncount = 0;
	    }
            omf_ledata(symbolP->sy_name, len);
	    count += len;  ncount += len;
	 }
      }
      omf_end_ledata();
  }
  

  omf_modend(I386);
  output_file_close (out_file_name);

}				/* write_object_file() */
