/* K=7 r=1/4 Viterbi decoder for AVX2
 * Copyright Phil Karn, KA9Q,
 * Code has been slightly modified for use with Spiral (www.spiral.net)
 * Karn's original code can be found here: http://www.ka9q.net/code/fec/
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 * see http://www.gnu.org/copyleft/lgpl.html
 */

#define RENORMALIZE_THRESHOLD 60000

#define renormalize {\
    if (metrics2[0] > RENORMALIZE_THRESHOLD) {\
        __m128i *mm_metrics  = (__m128i *)new_metrics;\
	    __m128i m0 = mm_metrics[0];\
	    for (int i = 1; i < 8; i++)\
	        m0 = _mm_min_epu16(m0, mm_metrics[i]);\
	    m0 = _mm_min_epu16(_mm_srli_si128(m0, 8), m0);\
    	m0 = _mm_min_epu16(_mm_srli_epi64(m0, 32), m0);\
    	m0 = _mm_min_epu16(_mm_srli_epi64(m0, 16), m0);\
    	m0 = _mm_shufflelo_epi16(m0, 0);\
    	m0 = _mm_unpacklo_epi64(m0, m0);\
	    for (int i = 0; i < 8; i++)\
    	    mm_metrics[i] = _mm_subs_epu16(mm_metrics[i], m0);\
	}\
}

#define BFLY(i) {\
    /* Form branch metrics */\
    __m256i metric = (Branchtab[i] ^ sym0) + (Branchtab[2] ^ sym1)\
                   + (Branchtab[4] ^ sym2) + (Branchtab[i] ^ sym3);\
   	__m256i m_metric = m1020 - metric;\
\
    /* Add branch metrics to path metrics */\
    __m256i m0 = _mm256_adds_epu16(old_metrics[i  ], metric);\
    __m256i m1 = _mm256_adds_epu16(old_metrics[i+2], m_metric);\
    __m256i m2 = _mm256_adds_epu16(old_metrics[i  ], m_metric);\
    __m256i m3 = _mm256_adds_epu16(old_metrics[i+2], metric);\
\
    /* Compare and select */\
    const __m256i survivor0 = _mm256_min_epu16(m0, m1);\
    const __m256i survivor1 = _mm256_min_epu16(m2, m3);\
    const __m256i decision0 = _mm256_cmpeq_epi16(survivor0, m1);\
    const __m256i decision1 = _mm256_cmpeq_epi16(survivor1, m3);\
\
    /* Store surviving metrics */\
    const __m256i new_metric_lo = _mm256_unpacklo_epi16(survivor0, survivor1);\
    const __m256i new_metric_hi = _mm256_unpackhi_epi16(survivor0, survivor1);\
    new_metrics[2*i  ] = _mm256_permute2x128_si256(new_metric_lo, new_metric_hi, 0x20);\
    new_metrics[2*i+1] = _mm256_permute2x128_si256(new_metric_lo, new_metric_hi, 0x31);\
\
    /* Pack each set of decisions into 16 8-bit bytes, then interleave them and compress into 32 bits */\
	m0 = _mm256_packs_epi16(decision0, _mm256_setzero_si256());\
	m1 = _mm256_packs_epi16(decision1, _mm256_setzero_si256());\
    *d++ = _mm256_movemask_epi8(_mm256_unpacklo_epi8(m0, m1));\
}

    __m256i *old_metrics = (__m256i *)metrics1;
    __m256i *new_metrics = (__m256i *)metrics2;
    const __m256i *Branchtab = (__m256i *)Branchtable;
   	const __m128i *syms = (const __m128i *)input;
    u32 *d = (u32 *)decisions;
    const __m256i m1020 = _mm256_set1_epi16(1020);
    const __m128i m127 = _mm_set1_epi16(127);
    const __m128i m255 = _mm_set1_epi16(255);

	nbits /= 2;
	while(nbits--)
    {
		__m256i sym0, sym1, sym2, sym3;
		__m256i *tmp;
        __m128i m0 = _mm_load_si128(syms++);

        // add 127 and limit to 0, 255
	    m0 = _mm_adds_epi16(m0, m127);
        m0 = _mm_min_epi16(m0, m255);
		m0 = _mm_max_epi16(m0, _mm_setzero_si128());

	    /* Splat the 0th symbol across sym0, the 1st symbol across sym1, etc */
		sym0 = _mm256_broadcastw_epi16(m0);
        m0 = _mm_srli_si128(m0, 2);
        sym1 = _mm256_broadcastw_epi16(m0);
        m0 = _mm_srli_si128(m0, 2);
        sym2 = _mm256_broadcastw_epi16(m0);
        m0 = _mm_srli_si128(m0, 2);
        sym3 = _mm256_broadcastw_epi16(m0);
        m0 = _mm_srli_si128(m0, 2);

		BFLY(0);
		BFLY(1);

	    /* Swap pointers to old and new metrics */
	    tmp = old_metrics;
	    old_metrics = new_metrics;
	    new_metrics = tmp;

	    /* Splat the 0th symbol across sym0, the 1st symbol across sym1, etc */
        sym0 = _mm256_broadcastw_epi16(m0);
        m0 = _mm_srli_si128(m0, 2);
        sym1 = _mm256_broadcastw_epi16(m0);
        m0 = _mm_srli_si128(m0, 2);
        sym2 = _mm256_broadcastw_epi16(m0);
        m0 = _mm_srli_si128(m0, 2);
        sym3 = _mm256_broadcastw_epi16(m0);

		BFLY(0);
		BFLY(1);

       	renormalize;

	    /* Swap pointers to old and new metrics */
	    tmp = old_metrics;
	    old_metrics = new_metrics;
	    new_metrics = tmp;
    }

