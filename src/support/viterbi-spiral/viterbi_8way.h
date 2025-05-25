/* K=7 r=1/4 Viterbi decoder for SSE2
 * Copyright Phil Karn, KA9Q,
 * Code has been slightly modified for use with Spiral (www.spiral.net)
 * Karn's original code can be found here: http://www.ka9q.net/code/fec/
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 * see http://www.gnu.org/copyleft/lgpl.html
 */

#define RENORMALIZE_THRESHOLD 30000

#define renormalize {\
    if (metrics2[0] > RENORMALIZE_THRESHOLD) {\
	    m0 = new_metrics[0];\
	    for (int i = 1; i < 8; i++)\
	        m0 = _mm_min_epi16(m0, new_metrics[i]);\
	    m0 = _mm_min_epi16(_mm_srli_si128(m0, 8), m0);\
    	m0 = _mm_min_epi16(_mm_srli_epi64(m0, 32), m0);\
    	m0 = _mm_min_epi16(_mm_srli_epi64(m0, 16), m0);\
    	m0 = _mm_shufflelo_epi16(m0, 0);\
    	m0 = _mm_unpacklo_epi64(m0, m0);\
	    for (int i = 0; i < 8; i++)\
    	    new_metrics[i] = _mm_subs_epi16(new_metrics[i], m0);\
	}\
}

#define BFLY(i) {\
    /* Form branch metrics */\
    __m128i metric = (Branchtab[i] ^ sym0) + (Branchtab[4] ^ sym1)\
                 + (Branchtab[i+8] ^ sym2) + (Branchtab[i] ^ sym3);\
   	__m128i m_metric = m1020 - metric;\
\
    /* Add branch metrics to path metrics */\
    m0 = _mm_adds_epi16(old_metrics[i], metric);\
    m1 = _mm_adds_epi16(old_metrics[i+4], m_metric);\
    m2 = _mm_adds_epi16(old_metrics[i], m_metric);\
    m3 = _mm_adds_epi16(old_metrics[i+4], metric);\
\
    /* Compare and select */\
    const __m128i decision0 = _mm_cmpgt_epi16(m0, m1);\
    const __m128i survivor0 = _mm_min_epi16(m1, m0);\
    const __m128i decision1 = _mm_cmpgt_epi16(m2, m3);\
    const __m128i survivor1 = _mm_min_epi16(m3, m2);\
\
    /* Store surviving metrics */\
    new_metrics[2*i] = _mm_unpacklo_epi16(survivor0, survivor1);\
    new_metrics[2*i+1] = _mm_unpackhi_epi16(survivor0, survivor1);\
\
    /* Pack each set of decisions into 8 8-bit bytes, then interleave them and compress into 16 bits */\
	m0 = _mm_packs_epi16(decision0, _mm_setzero_si128());\
	m1 = _mm_packs_epi16(decision1, _mm_setzero_si128());\
    *d++ = _mm_movemask_epi8(_mm_unpacklo_epi8(m0, m1));\
}

    __m128i *old_metrics = (__m128i *)metrics1;
    __m128i *new_metrics = (__m128i *)metrics2;
    const __m128i *Branchtab = (__m128i *)Branchtable;
   	const __m128i *syms = (const __m128i *)input;
    u16 *d = (u16 *)decisions;
    const __m128i m1020 = _mm_set1_epi16(1020);
    const __m128i m127 = _mm_set1_epi16(127);
    const __m128i m255 = _mm_set1_epi16(255);

	nbits /= 2;
	while(nbits--)
    {
		__m128i m0, m1, m2, m3, m4;
		__m128i sym0, sym1, sym2, sym3;
		__m128i *tmp;

        m0 = _mm_loadu_si128(syms++);

        // add 127 and limit to 0, 255
	    m0 = _mm_adds_epi16(m0, m127);
        m0 = _mm_min_epi16(m0, m255);
		m0 = _mm_max_epi16(m0, _mm_setzero_si128());
        m4 = m0;

	    /* Splat the 0th symbol across sym0, the 1st symbol across sym1, etc */
        m0 = _mm_unpacklo_epi16(m0, m0);
        sym0 = _mm_shuffle_epi32(m0, 0x00);
        sym1 = _mm_shuffle_epi32(m0, 0x55);
        sym2 = _mm_shuffle_epi32(m0, 0xaa);
        sym3 = _mm_shuffle_epi32(m0, 0xff);

		BFLY(0);
		BFLY(1);
		BFLY(2);
		BFLY(3);

	    /* Swap pointers to old and new metrics */
	    tmp = old_metrics;
	    old_metrics = new_metrics;
	    new_metrics = tmp;

	    /* Splat the 0th symbol across sym0, the 1st symbol across sym1, etc */
        m0 = _mm_unpackhi_epi16(m4, m4);
        sym0 = _mm_shuffle_epi32(m0, 0x00);
        sym1 = _mm_shuffle_epi32(m0, 0x55);
        sym2 = _mm_shuffle_epi32(m0, 0xaa);
        sym3 = _mm_shuffle_epi32(m0, 0xff);

		BFLY(0);
		BFLY(1);
		BFLY(2);
		BFLY(3);

       	renormalize;

	    /* Swap pointers to old and new metrics */
	    tmp = old_metrics;
	    old_metrics = new_metrics;
	    new_metrics = tmp;
    }

