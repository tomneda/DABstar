#include  <cstdio>
#include  <stdint.h>

/*
 * If DABstar shows errors like: "libpng warning: iCCP: known incorrect sRGB profile"
 * try to remove the sRGB profile in the PNG picture with ImageMagick with command:
 * convert <InImage.png> <OutImage.png>
 * or
 * mogrify <Image.png>  (mogrify *.png)
 *
 * see https://community.magento.com/t5/Magento-2-x-Technical-Issues/libpng-warning-iCCP-known-incorrect-sRGB-profile/td-p/127379
 */


int main(int argc, char ** argv)
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage converter infile outfile:\n %s <PNG File> <Outfile without .h/.cpp>\n", argv[0]);
    return 0;
  }

  char fnhpp[255];
  char fncpp[255];

  snprintf(fnhpp, 254, "%s.h", argv[2]);
  snprintf(fncpp, 254, "%s.cpp", argv[2]);

  // create cpp
  {
    FILE * fin = fopen(argv[1], "r");

    if (fin == 0)
    {
      fprintf(stderr, "Error opening file %s\n", argv[1]);
      return -1;
    }

    FILE * fout =fopen(fncpp, "w");

    int teller = 0;
    uint8_t element;

    // first a header for the out file
    fprintf(fout, "// generated with %s\n\n", argv[0]);
    fprintf(fout, "#include \"%s\"\n\n", fnhpp);
    fprintf(fout, "const uint8_t PAUSESLIDE[] =\n{\n  ");

    while ((fread(&element, 1, 1, fin)) > 0)
    {
      fprintf(fout, "0x%02x, ", element);
      teller++;
      if (teller >= 16)
      {
        fprintf(fout, "\n  ");
        teller = 0;
      }
    }

    fprintf(fout, "0x00\n};\n\n"); // fill with zero to conclude the last comma
    fprintf(fout, "const int32_t PAUSESLIDE_SIZE = sizeof(PAUSESLIDE) - 1; // ignore last artificial zero\n\n");

    fclose(fout);
    fclose(fin);
  }

  // create header
  {
    FILE * fout = fopen(fnhpp, "w");

    fprintf(fout, "// generated with %s\n\n", argv[0]);
    fprintf(fout, "#include <stdint.h>\n\n");
    fprintf(fout, "extern const uint8_t PAUSESLIDE[];\n");
    fprintf(fout, "extern const int32_t PAUSESLIDE_SIZE;\n\n");

    fclose(fout);
  }
}
