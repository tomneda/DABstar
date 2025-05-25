/* K=7 r=1/4 Viterbi decoder in portable C
 * Copyright Phil Karn, KA9Q,
 * Code has been slightly modified for use with Spiral (www.spiral.net)
 * Karn's original code can be found here: http://www.ka9q.net/code/fec/
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 * see http://www.gnu.org/copyleft/lgpl.html
 */

#define BFLY(i) {\
	int decision0, decision1;\
	COMPUTETYPE m_metric, metric, m0, m1, m2, m3;\
\
    /* Form branch metrics */\
	metric = (Branchtab[i] ^ sym0) + (Branchtab[32+i] ^ sym1)\
		+ (Branchtab[64+i] ^ sym2) + (Branchtab[96+i] ^ sym3);\
    m_metric = 1020 - metric;\
\
    /* Add branch metrics to path metrics */\
    m0 = old_metrics[i] + metric;\
    m1 = old_metrics[i+32] + m_metric;\
    m2 = old_metrics[i] + m_metric;\
    m3 = old_metrics[i+32] + metric;\
\
    /* Compare and select */\
    decision0 = (int)(m0-m1) > 0;\
    decision1 = (int)(m2-m3) > 0;\
\
    /* Store surviving metrics */\
    new_metrics[2*i] = decision0 ? m1 : m0;\
    new_metrics[2*i+1] = decision1 ? m3 : m2;\
    *d |= (u64)(decision0 | (decision1 << 1)) << (i*2);\
}

#define limit_min_max(sym) {\
	i16 tmp = *syms++;\
    tmp += 127;\
    if (tmp < 0) tmp = 0;\
    else if (tmp > 255) tmp = 255;\
    sym = tmp;\
}

	u64 *d = (u64 *)decisions;
	memset(d, 0, nbits*sizeof(u64));
    COMPUTETYPE *old_metrics = metrics1;
    COMPUTETYPE *new_metrics = metrics2;
    const COMPUTETYPE *Branchtab = Branchtable;
    const i16 *syms = input;

	while(nbits--)
    {
		COMPUTETYPE sym0, sym1, sym2, sym3;
		limit_min_max(sym0);
		limit_min_max(sym1);
		limit_min_max(sym2);
		limit_min_max(sym3);
		BFLY(0);
		BFLY(1);
		BFLY(2);
		BFLY(3);
		BFLY(4);
		BFLY(5);
		BFLY(6);
		BFLY(7);
		BFLY(8);
		BFLY(9);
		BFLY(10);
		BFLY(11);
		BFLY(12);
		BFLY(13);
		BFLY(14);
		BFLY(15);
		BFLY(16);
		BFLY(17);
		BFLY(18);
		BFLY(19);
		BFLY(20);
		BFLY(21);
		BFLY(22);
		BFLY(23);
		BFLY(24);
		BFLY(25);
		BFLY(26);
		BFLY(27);
		BFLY(28);
		BFLY(29);
		BFLY(30);
		BFLY(31);
        d++;

		// Swap pointers to old and new metrics
		COMPUTETYPE *temp = new_metrics;
		new_metrics = old_metrics;
		old_metrics = temp;
    }
