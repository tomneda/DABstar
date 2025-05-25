/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */



#include	<cstdio>
#include	<stdint.h>


int	main (int argc, char **argv) {
FILE	*fout	= stdout;
FILE	*fin	= stdin;
int	teller	= 0;
int	element;

	if (argc < 2) {
	   fprintf (stderr, "Usage converter infile outfile\n");
	   return 0;
	}

	fin	= fopen (argv [1], "r");
	fout	= fopen (argv [2], "w");

//
//	first a header for the out file
	fprintf (fout, "#\n#include <stdint.h>\n");
	fprintf (fout, "// generated map \n");
	fprintf (fout, "static u8 qt_map [] = {\n");
	while ((element = fgetc (fin)) >= 0) {
	   fprintf (fout, "%d, ", (u8) element);
	   teller ++;
	   if (teller > 14) {
	      fprintf (fout, "\n\t");
	      teller = 0;
	   }
	}
	if (teller != 0) 
	   fprintf (fout, "\n\t 0};\n");
	else
	   fprintf (fout, "0};\n");
	fclose (fout);
}

	      
	   
	
