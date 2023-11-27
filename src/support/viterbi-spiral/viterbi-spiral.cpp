/*
 *    Copyright (C) 2013
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include  <cstdio>
#include  <cstdlib>
#include  "mm_malloc.h"
#include  "viterbi-spiral.h"
#include  <cstring>

#ifdef  __MINGW32__
#include	<intrin.h>
#include	<malloc.h>
#include	<windows.h>
#endif

//	It took a while to discover that the polynomes we used
//	in our own "straightforward" implementation was bitreversed!!
//	The official one is on top.
#define K 7
#define POLYS { 0155, 0117, 0123, 0155 } // octal representation with leading zeroes!
//#define POLYS { 0133, 0171, 0145, 0133 } // reversed form (defined in the standard)

#define  METRICSHIFT  0
#define  PRECISIONSHIFT  0
#define  RENORMALIZE_THRESHOLD  137


// ADDSHIFT and SUBSHIFT make sure that the thing returned is a byte.
#if (K - 1 < 8)
#define ADDSHIFT (8-(K-1))
#define SUBSHIFT 0
#elif (K - 1 > 8)
#define ADDSHIFT 0
#define SUBSHIFT ((K-1)-8)
#else
#define ADDSHIFT 0
#define SUBSHIFT 0
#endif


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



#if 0
// One could create the table above, i.e. a 256 entry odd-parity lookup table by the following function It is now precomputed
void viterbiSpiral::partab_init(void)
{
  int16_t i, cnt, ti;

  for (i = 0; i < 256; i++)
  {
    cnt = 0;
    ti = i;
    while (ti != 0)
    {
      if (ti & 1)
      {
        cnt++;
      }
      ti >>= 1;
    }
    Partab[i] = cnt & 1;
  }
}
#endif

int ViterbiSpiral::parity(int x)
{
  /* Fold down to one byte */
  x ^= (x >> 16);
  x ^= (x >> 8);
  return PARTAB[x];
}

static inline void renormalize(COMPUTETYPE * const X, const COMPUTETYPE threshold)
{
  int32_t i;

  if (X[0] > threshold)
  {
    COMPUTETYPE min = X[0];
    for (i = 0; i < NUMSTATES; i++)
    {
      if (min > X[i])
      {
        min = X[i];
      }
    }
    for (i = 0; i < NUMSTATES; i++)
    {
      X[i] -= min;
    }
  }
}

//	The main use of the viterbi decoder is in handling the FIC blocks
//	There are (in mode 1) 3 ofdm blocks, giving 4 FIC blocks
//	There all have a predefined length. In that case we use the
//	"fast" (i.e. spiral) code, otherwise we use the generic code

ViterbiSpiral::ViterbiSpiral(const int16_t iWordlength, const bool iSpiralMode) :
  mFrameBits(iWordlength),
  mSpiral(iSpiralMode)
{
  int polys[RATE] = POLYS;
  //partab_init();

  // TODO: The spiral code uses (wordLength + (K - 1) * sizeof ...
  // However, the application then crashes, so something is not OK
  // By doubling the size, the problem disappears. It is not solved though
  // and not further investigation.
#ifdef __MINGW32__
  uint32_t size = 2 * ((wordlength + (K - 1)) / 8 + 1 + 16) & ~0xF;
  data	= (uint8_t *)_aligned_malloc (size, 16);
  size = 2 * (RATE * (wordlength + (K - 1)) * sizeof(COMPUTETYPE) + 16) & ~0xF;
  symbols	= (COMPUTETYPE *)_aligned_malloc (size, 16);
  size	= 2 * (wordlength + (K - 1)) * sizeof (decision_t);
  size	= (size + 16) & ~0xF;
  vp. decisions = (decision_t  *)_aligned_malloc (size, 16);
#else
  if (posix_memalign((void **)&mpData, 16, (iWordlength + (K - 1)) / 8 + 1))
  {
    printf("Allocation of data array failed\n");
  }
  if (posix_memalign((void **)&mpSymbols, 16, RATE * (iWordlength + (K - 1)) * sizeof(COMPUTETYPE)))
  {
    printf("Allocation of symbols array failed\n");
  }
  if (posix_memalign((void **)&(mVP.decisions), 16, 2 * (iWordlength + (K - 1)) * sizeof(decision_t)))
  {
    printf("Allocation of vp decisions failed\n");
  }
#endif

  for (int16_t state = 0; state < NUMSTATES / 2; state++)
  {
    for (int16_t i = 0; i < RATE; i++)
    {
      mBranchtab[i * NUMSTATES / 2 + state] = (polys[i] < 0) ^ parity((2 * state) & abs(polys[i])) ? 255 : 0;
    }
  }

  init_viterbi(&mVP, 0);
}


ViterbiSpiral::~ViterbiSpiral()
{
#ifdef  __MINGW32__
  _aligned_free (vp. decisions);
  _aligned_free (data);
  _aligned_free (symbols);
#else
  free(mVP.decisions);
  free(mpData);
  free(mpSymbols);
#endif
}

static inline uint8_t getbit(uint8_t v, int32_t o)
{
  static const int maskTable[] = { 128, 64, 32, 16, 8, 4, 2, 1 };
  return (v & maskTable[o]) ? 1 : 0;
}

void ViterbiSpiral::deconvolve(const int16_t * const input, uint8_t * const output)
{
  init_viterbi(&mVP, 0);

  for (uint32_t i = 0; i < (uint16_t)(mFrameBits + (K - 1)) * RATE; i++)
  {
    // Note that OFDM decoder maps the softbits to -127 .. 127 we have to map that onto 0 .. 255
    int16_t temp = input[i] + VITERBI_SOFT_BIT_VALUE_MAX;
    limit_min_max<int16_t>(temp, 0, VITERBI_SOFT_BIT_VALUE_MAX * 2 + 1);
    mpSymbols[i] = temp;
  }

  if (!mSpiral)
  {
    update_viterbi_blk_GENERIC(&mVP, mpSymbols, mFrameBits + (K - 1));
  }
  else
  {
    update_viterbi_blk_SPIRAL(&mVP, mpSymbols, mFrameBits + (K - 1));
  }

  chainback_viterbi(&mVP, mpData, mFrameBits, 0);

  for (uint32_t i = 0; i < (uint16_t)mFrameBits; i++)
  {
    output[i] = getbit(mpData[i >> 3], i & 07);
  }
}

/* C-language butterfly */
void ViterbiSpiral::BFLY(int i, int s, COMPUTETYPE * syms, SMetricData * vp, decision_t * d)
{
  COMPUTETYPE metric = 0;

  for (int32_t j = 0; j < RATE; j++)
  {
    metric += (mBranchtab[i + j * NUMSTATES / 2] ^ syms[s * RATE + j]) >> METRICSHIFT;
  }

  metric = metric >> PRECISIONSHIFT;

  const COMPUTETYPE max = ((RATE * ((256 - 1) >> METRICSHIFT)) >> PRECISIONSHIFT);

  const COMPUTETYPE m0 = vp->old_metrics->t[i +             0] + metric;
  const COMPUTETYPE m1 = vp->old_metrics->t[i + NUMSTATES / 2] + (max - metric);
  const COMPUTETYPE m2 = vp->old_metrics->t[i +             0] + (max - metric);
  const COMPUTETYPE m3 = vp->old_metrics->t[i + NUMSTATES / 2] + metric;

  const int32_t decision0 = (m0 > m1 ? 1 : 0);
  const int32_t decision1 = (m2 > m3 ? 1 : 0);

  vp->new_metrics->t[2 * i + 0] = (decision0 != 0 ? m1 : m0);
  vp->new_metrics->t[2 * i + 1] = (decision1 != 0 ? m3 : m2);

  d->w[i / (sizeof(uint32_t) * 8 / 2) + s * (sizeof(decision_t) / sizeof(uint32_t))] |= (decision0 | decision1 << 1) << ((2 * i) & (sizeof(uint32_t) * 8 - 1));
}

/* Update decoder with a block of demodulated symbols
 * Note that nbits is the number of decoded data bits, not the number
 * of symbols!
 */
void ViterbiSpiral::update_viterbi_blk_GENERIC(SMetricData * vp, COMPUTETYPE * syms, int16_t nbits)
{
  decision_t * d = (decision_t *)vp->decisions;

  for (int32_t s = 0; s < nbits; s++)
  {
    memset(&d[s], 0, sizeof(decision_t));
  }

  for (int32_t s = 0; s < nbits; s++)
  {
    for (int32_t i = 0; i < NUMSTATES / 2; i++)
    {
      BFLY(i, s, syms, vp, vp->decisions);
    }

    renormalize(vp->new_metrics->t, RENORMALIZE_THRESHOLD);

    // Swap pointers to old and new metrics
    void * tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = (metric_t *)tmp;
  }
}

extern "C" {
#if defined(SSE_AVAILABLE)
void FULL_SPIRAL_sse (int, COMPUTETYPE * Y, COMPUTETYPE * X, COMPUTETYPE * syms, DECISIONTYPE * dec, COMPUTETYPE * Branchtab);
#elif defined(NEON_AVAILABLE)
void FULL_SPIRAL_neon(int, COMPUTETYPE * Y, COMPUTETYPE * X, COMPUTETYPE * syms, DECISIONTYPE * dec, COMPUTETYPE * Branchtab);
#else
void FULL_SPIRAL_no_sse(int, COMPUTETYPE * Y, COMPUTETYPE * X, COMPUTETYPE * syms, DECISIONTYPE * dec, COMPUTETYPE * Branchtab);
#endif

}

void ViterbiSpiral::update_viterbi_blk_SPIRAL(SMetricData * vp, COMPUTETYPE * syms, int16_t nbits)
{
  decision_t * d = (decision_t *)vp->decisions;
  int32_t s;

  for (s = 0; s < nbits; s++)
  {
    memset(d + s, 0, sizeof(decision_t));
  }

#if defined(SSE_AVAILABLE)
  FULL_SPIRAL_sse(nbits / 2, vp->new_metrics->t, vp->old_metrics->t, syms, d->t, mBranchtab);
#elif defined(NEON_AVAILABLE)
  FULL_SPIRAL_neon(nbits, vp->new_metrics->t, vp->old_metrics->t, syms, d->t, mBranchtab);
#else
  FULL_SPIRAL_no_sse(nbits / 2, vp->new_metrics->t, vp->old_metrics->t, syms, d->t, mBranchtab);
#endif

}

// Viterbi chainback
void ViterbiSpiral::chainback_viterbi(const SMetricData * const iVP, uint8_t * const oData, int16_t nbits, const uint16_t iEndstate)
{
  // Terminal encoder state
  const decision_t * d = iVP->decisions;

  /* Make room beyond the end of the encoder register so we can
   * accumulate a full byte of decoded data
   */
  uint16_t endstate = (iEndstate % NUMSTATES) << ADDSHIFT;

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += (K - 1); /* Look past tail */

  while (nbits-- != 0)
  {
    const uint16_t n = (endstate >> ADDSHIFT);
    const uint16_t k = (d[nbits].w[n >> 5] >> (n & 0x1F)) & 1;
    endstate = (endstate >> 1) | (k << (K - 2 + ADDSHIFT));
    oData[nbits >> 3] = endstate >> SUBSHIFT;
  }
}

/* Initialize Viterbi decoder for start of new frame */
void ViterbiSpiral::init_viterbi(SMetricData * const iP, const int16_t iStarting_state)
{
  SMetricData * vp = iP;
  int32_t i;

  for (i = 0; i < NUMSTATES; i++)
  {
    vp->metrics1.t[i] = 63;
  }

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  /* Bias known start state */
  vp->old_metrics->t[iStarting_state & (NUMSTATES - 1)] = 0;
}

