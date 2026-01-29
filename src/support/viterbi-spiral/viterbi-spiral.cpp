/* K=7 r=1/4 Viterbi decoder,
 * Copyright Phil Karn, KA9Q,
 * Code has been slightly modified for use with Spiral (www.spiral.net)
 * Karn's original code can be found here: http://www.ka9q.net/code/fec/
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 * see http://www.gnu.org/copyleft/lgpl.html
 */

#include "viterbi-spiral.h"

#if defined(HAVE_VITERBI_AVX2)
  #include <immintrin.h>
  using COMPUTETYPE = u16;
#elif defined(HAVE_VITERBI_SSE2)
  #include <immintrin.h>
  using COMPUTETYPE = u16;
#elif defined(HAVE_VITERBI_NEON)
  #include "sse2neon.h"
  using COMPUTETYPE = u16;
#else
  using COMPUTETYPE = i32;
#endif

#define K        7
#define RATE     4

#if defined(_MSC_VER)
  #define ALIGN(a)  _VCRT_ALIGN(a)
#else
  #define ALIGN(a) __attribute__ ((aligned(a)))
#endif

ALIGN(32) static const COMPUTETYPE Branchtable[RATE * NUMSTATES / 2]
{
  0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, // 0
  255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, // 1
  0, 255, 255, 0, 255, 0, 0, 255, 0, 255, 255, 0, 255, 0, 0, 255, // 2
  0, 255, 255, 0, 255, 0, 0, 255, 0, 255, 255, 0, 255, 0, 0, 255, // 2
  0, 255, 0, 255, 0, 255, 0, 255, 255, 0, 255, 0, 255, 0, 255, 0, // 3
  0, 255, 0, 255, 0, 255, 0, 255, 255, 0, 255, 0, 255, 0, 255, 0, // 3
  0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, // 0
  255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 255, 255  // 1
};


ALIGN(32) static COMPUTETYPE metrics1[NUMSTATES], metrics2[NUMSTATES];

static const u8 PARTAB[256] =
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


ViterbiSpiral::ViterbiSpiral(const i16 iWordlength, const bool iSpiralMode)
  : mFrameBits(iWordlength)
  , mSpiral(iSpiralMode)
{
#if defined(HAVE_VITERBI_AVX2)
  qInfo("Using AVX2 for Viterbi spiral decoder");
#elif defined(HAVE_VITERBI_SSE2)
  qInfo("Using SSE2 for Viterbi spiral decoder");
#elif defined(HAVE_VITERBI_NEON)
  qInfo("Using NEON for Viterbi spiral decoder");
#else
  qInfo("Using Scalar for Viterbi spiral decoder");
#endif

  const i32 nbits = mFrameBits + (K - 1);
  decisions = (decision_t *)malloc(nbits * sizeof(decision_t));
}


ViterbiSpiral::~ViterbiSpiral()
{
  free(decisions);
}

i32 ViterbiSpiral::parity(i32 x)
{
  /* Fold down to one byte */
  x ^= (x >> 16);
  x ^= (x >> 8);
  return PARTAB[x];
}

/*u32 v;  // word value to compute the parity of
v ^= v >> 16;
v ^= v >> 8;
v ^= v >> 4;
v &= 0xf;
return (0x6996 >> v) & 1;*/

void ViterbiSpiral::deconvolve(const i16 * const input, u8 * const output)
{
  /* Initialize Viterbi decoder for start of new frame */
  for (i32 i = 0; i < NUMSTATES; i++)
    metrics1[i] = 1000;

  metrics1[0] = 0; /* Bias known start state */

  u32 nbits = mFrameBits + (K - 1);

#if defined(HAVE_VITERBI_AVX2)
  #include "viterbi_16way.h"
#elif defined(HAVE_VITERBI_NEON) || defined(HAVE_VITERBI_SSE2)
  #include "viterbi_8way.h"
#else
  #include "viterbi_scalar.h"
#endif

  /* Do Viterbi chainback */
  u32 endstate = 0; /* Terminal encoder state */
  u32 framebits = mFrameBits;
  decision_t * dec = decisions;

  dec += (K - 1); /* Look past tail */
  while (framebits--)
  {
    i32 k = (dec[framebits].w[(endstate >> 2) / 32] >> ((endstate >> 2) % 32)) & 1;
    endstate = (endstate >> 1) | (k << K);
    output[framebits] = k;
  }
}

void ViterbiSpiral::calculate_BER(const i16 * const input, u8 * punctureTable, u8 const * output, i32 & bits, i32 & errors)
{
  i32 i;
  i32 sr = 0;
  const i32 polys[RATE] = { 109, 79, 83, 109 };

  for (i = 0; i < mFrameBits; i++)
  {
    sr = ((sr << 1) | output[i]) & 0xff;
    for (i32 j = 0; j < RATE; j++)
    {
      u8 b = parity(sr & polys[j]);
      if (punctureTable[i * RATE + j])
      {
        bits++;
        if ((input[i * RATE + j] > 0) != b)
          errors++;
      }
    }
  }

  //Now the residue bits. Empty the registers by shifting in zeros
  for (i = mFrameBits; i < mFrameBits + 6; i++)
  {
    sr = (sr << 1) & 0xff;
    for (i32 j = 0; j < RATE; j++)
    {
      u8 b = parity(sr & polys[j]);
      if (punctureTable[i * RATE + j])
      {
        bits++;
        if ((input[i * RATE + j] > 0) != b)
          errors++;
      }
    }
  }
}
