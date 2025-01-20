/* K=7 r=1/4 Viterbi decoder,
 * Copyright Phil Karn, KA9Q,
 * Code has been slightly modified for use with Spiral (www.spiral.net)
 * Karn's original code can be found here: http://www.ka9q.net/code/fec/
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 * see http://www.gnu.org/copyleft/lgpl.html
 */

#include "viterbi-spiral.h"


#if defined(NEON_AVAILABLE)
#include "sse2neon.h"
#endif

#if defined(SSE_AVAILABLE)
#include <emmintrin.h>
#endif

#if defined(NEON_AVAILABLE) || defined(SSE_AVAILABLE)
#define COMPUTETYPE short
#else
#define COMPUTETYPE int
#endif

#define K 7
#define RATE 4
#define POLYS { 109, 79, 83, 109 }


#define ALIGN(a) __attribute__ ((aligned(a)))

ALIGN(16) const COMPUTETYPE Branchtable[RATE*NUMSTATES/2]{
    0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0,//0
    255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255,//1
    0, 255, 255, 0, 255, 0, 0, 255, 0, 255, 255, 0, 255, 0, 0, 255,//2
    0, 255, 255, 0, 255, 0, 0, 255, 0, 255, 255, 0, 255, 0, 0, 255,//2
    0, 255, 0, 255, 0, 255, 0, 255, 255, 0, 255, 0, 255, 0, 255, 0,//3
    0, 255, 0, 255, 0, 255, 0, 255, 255, 0, 255, 0, 255, 0, 255, 0,//3
    0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0,//0
    255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255 //1
};


typedef struct {
	COMPUTETYPE t[NUMSTATES];
} metric_t;

ALIGN(16) static metric_t metrics1, metrics2;

static const uint8_t PARTAB[256] =
{
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};


ViterbiSpiral::ViterbiSpiral(const int16_t iWordlength, const bool iSpiralMode) :
  mFrameBits(iWordlength),
  mSpiral(iSpiralMode)
{
  const int nbits = mFrameBits + (K - 1);

  decisions = (decision_t *)malloc(nbits * sizeof(decision_t));
  mpSymbols = (int *)malloc(RATE * nbits * sizeof(int));
}


ViterbiSpiral::~ViterbiSpiral()
{
  free(mpSymbols);
  free(decisions);
}

int ViterbiSpiral::parity(int x)
{
  /* Fold down to one byte */
  x ^= (x >> 16);
  x ^= (x >> 8);
  return PARTAB[x];
}

void ViterbiSpiral::deconvolve(const int16_t * const input, uint8_t * const output)
{
  /* Initialize Viterbi decoder for start of new frame */
  for (int i = 0; i<NUMSTATES; i++)
    metrics1.t[i] = 1000;

  metrics1.t[0] = 0; /* Bias known start state */

  uint32_t nbits = mFrameBits + (K - 1);

  for (uint32_t i = 0; i < nbits * RATE; i++)
  {
    // Note that OFDM decoder maps the softbits to -127 .. 127 we have to map that onto 0 .. 255
    short temp = input[i] + VITERBI_SOFT_BIT_VALUE_MAX;
    limit_min_max<short>(temp, 0, VITERBI_SOFT_BIT_VALUE_MAX * 2 + 1);
    mpSymbols[i] = temp;
  }

#if defined(NEON_AVAILABLE) || defined(SSE_AVAILABLE)
#include "viterbi_8way.h"
#else
#include "viterbi_scalar.h"
#endif

  /* Do Viterbi chainback */
  unsigned int endstate = 0; /* Terminal encoder state */
  unsigned int framebits = mFrameBits;
  decision_t *dec = decisions;

  dec += (K - 1); /* Look past tail */
  while (framebits--)
  {
	int k = (dec[framebits].w[(endstate >> 2) / 32] >> ((endstate >> 2) % 32)) & 1;
	endstate = (endstate >> 1) | (k << K);
    output[framebits] = k;
  }
}

void ViterbiSpiral::calculate_BER(const int16_t * const input, uint8_t *punctureTable, uint8_t const *output, int &bits, int &errors)
{
  int i;
  int sr = 0;
  int polys[RATE] = POLYS;

  for(i=0; i<mFrameBits; i++)
  {
    sr = ((sr << 1) | output[i]) & 0xff;
    for(int j=0; j<RATE; j++)
    {
      uint8_t b = parity(sr & polys[j]);
      if (punctureTable[i*RATE+j])
      {
        bits++;
        if ((input[i*RATE+j] > 0) != b)
          errors++;
      }
    }
  }

  //Now the residue bits. Empty the registers by shifting in zeros
  for(i=mFrameBits; i<mFrameBits+6; i++)
  {
    sr = (sr << 1) & 0xff;
    for(int j=0; j<RATE; j++)
    {
      uint8_t b = parity(sr & polys[j]);
      if (punctureTable[i*RATE+j])
      {
        bits++;
        if ((input[i*RATE+j] > 0) != b)
          errors++;
      }
    }
  }
}

