IMPORTANT:
	these diffs contain a few diffs other than the ones for atari.
	these are bug fixes needed for any m68k host (and other for expressions)
	see the file POST for details.

gas-1.38 requires library PatchLevel 46 or better. of you dont have it
please get it from dsrgsun.ces.cwru.edu first.

gas-1.38 uses _stksize == -1L. please dont change it.

--
This directory contains the context diff files and additional files required
to build gcc-as for atariST Gcc or for the unix hosted cross development
system. The diff are with respect to gas v 1.38

	Use Makefile.cross to make gcc-as for the cross development system.
	(both cross-minix and cross-tos use the same gas).

	Use Makefile.cminix to make gcc-as for minix using the
	cross-development system or your native minix-gcc.

	Use Makefile.st to make gcc-as for Tos/Gemdos using the
	cross-development system/or your native tos-gcc.

Adjust $(CROSSDIR) appropriately in the make files.

INSTALLATION:
 - copy all the files from the gas v1.38 distribution in a directory GAS-SRC.
 - copy all the files from this directory into the GAS-SRC directory.
 - apply the diffs using larry walls patch program
	patch < gas-diff
 - edit the Makefiles to specify $(CROSSDIR) appropriately.
 - make a link called m68k.h to m-generic.h
 - hit make.

   Please send fixes/comments/suggestions to

--
bang:   uunet!cadence!bammi			jwahar r. bammi
domain: bammi@cadence.com
GEnie:	J.Bammi
CIS:    71515,155
