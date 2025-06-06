#
/* -*- c++ -*- */
/*
 * Copyright 2004,2010 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

//	The Gnu radio implementation of the rsCodec
//	is included in the sdr-j dab implementation
//	Redundant code (redundant due to knowing in compiletime
//	the Galois and RS parameters) is removed.
//
#include	"rscodec.h"
#include	<cassert>
#include	<cstring>
//
//	we know that the size of the codeword is 255,
//	while the size of the parity bytes is 10
//	from input r to output d, cutlen is amount of shortening
//	we just make it into a 255 byte vector
//
#define	NUM_PARITY	10
#define	MESSAGE_LENGTH	245
#define	CODE_LENGTH	255

//	We combine a simple implementation of the galois fields
//	with the rs decoder "borrowed" from Gnu radio
//
	rscodec::rscodec() {
i16 i, pm;
i16 pinit, p1, p2, p3, p4, p5, p6, p7, p8;

//	d_m = 8;
//        d_q = 1 << 8;
//        d_p = 0435;
	pm = d_p - d_q;

	pinit = p2 = p3 = p4 = p5 = p6 = p7 = p8 = 0;
	p1 = 1;
	
	gexp [0]	= 0;
	gexp [1]	= 1;
	gexp [255]	= gexp [0];
	glog [0]	= 0;		/* shouldn't log[0] be an error? */
	
	for (i = 2; i < 256; i++) {
	   pinit = p8;
	   p8 = p7;
	   p7 = p6;
	   p6 = p5;
	   p5 = p4 ^ pinit;
	   p4 = p3 ^ pinit;
	   p3 = p2 ^ pinit;
	   p2 = p1;
	   p1 = pinit;
	   gexp [i] = p1 + p2*2 + p3*4 + p4*8 + p5*16 + p6*32 + p7*64 + p8*128;
//	   gexp [i + 255] = gexp[i];
	}

	for (i = 1; i < 256; i++) {
	i16 z;
	   for (z = 0; z < 256; z++) {
	      if (gexp [z] == i) {
	         glog [i] = z;
	         break;
              }
	   }
	}

	d_g		= new i32 [NUM_PARITY];
	create_polynomials (d_j); // construct the generator polynomial
	d_reg		= new i32 [NUM_PARITY];
	syndrome	= new i32 [NUM_PARITY];
	d_euc[0]	= new i32 [NUM_PARITY + 2];
	d_euc[1]	= new i32 [NUM_PARITY + 2];
}

	rscodec::~rscodec() {
	delete [] d_g;
	delete [] d_reg;
	delete [] syndrome;
	delete [] d_euc [0];
	delete [] d_euc [1];
}

void rscodec::create_polynomials (i32 start_j) {
i16 i;
i16 tmp_g, tmp_g2;
i16	k;

// g has degree 2t, g(0), g(1),..., g(2t-1) are its coefficients,
// g (2t) = 1 is not included

	d_g [NUM_PARITY - 1] = start_j + 1; // power representation
	for (k = 1; k < NUM_PARITY; k++){
	   tmp_g = d_g [NUM_PARITY - 1]; // preserve for later use
	   d_g [NUM_PARITY - 1] = add_power (d_g [NUM_PARITY - 1],
	                                     start_j + k + 1);
	   for (i = NUM_PARITY - 2; i >= NUM_PARITY - k; i--){
	      tmp_g2 = d_g [i];
	      d_g [i] = add_power (multiply_power (tmp_g, start_j + k + 1),
	                           d_g [i]);
	      tmp_g = tmp_g2;
	   }

	   d_g [NUM_PARITY - k - 1] =
	                multiply_power (tmp_g, start_j + k + 1);
	}
}

i16	rscodec::dec (const u8 *r, u8 *d, i16 cutlen) {
u16 rf [CODE_LENGTH];
u16 df [MESSAGE_LENGTH];
i16 i;
i16	ret;

	memset (rf, 0, cutlen * sizeof (u16));
	for (i = cutlen; i < CODE_LENGTH; i++)
	   rf [i] = (i16) r[i - cutlen];

	ret = dec_poly (rf, df);
	for (i = cutlen; i < MESSAGE_LENGTH; i++)
	   d [i - cutlen] = (u8) df [i];
	return ret;
}
//	calculate the syndrome with Horner
//	syndrone [0] is the highest degree coefficient,
//	syndrone [2t-1] is the lowest degree coefficient
//	i.e. S(X) = syndrome [0]X ^ (2t-1) + syndrome [1]X ^ (2t-2)+...
//	+ syndrome [2t-1]
i16	rscodec::dec_poly (const u16 *r, u16 *d) {
i16 i,j;

	for (i = 0; i < NUM_PARITY; i++) {
	   u16 sum = r [0];
	   u16 alpha_i = power2poly (d_j + i + 1);
// syndrome also in polynomial representation
	   for (j = 1; j < CODE_LENGTH; j++)
	      sum = add_poly (r [j], 
	                      multiply_poly (sum, alpha_i));

	   syndrome [NUM_PARITY - i - 1] = sum;
	}
// Euclidean algorithm
// step 1: initialize
// index to 'top', index to 'bottom' is !top
	i16 top	 = 0;
	i16 deg [2] = {NUM_PARITY, NUM_PARITY - 1}; // top and bottom relaxed degree
//	d_euc [top]. clear();
//	d_euc [!top].clear();
	memset (d_euc [top], 0, (NUM_PARITY + 2) * sizeof (d_euc [0][0]));
	memset (d_euc[!top], 0, (NUM_PARITY + 2) * sizeof (d_euc [0][0]));
	d_euc [top][0] = 1;
	d_euc [top][NUM_PARITY + 1] = 1;
	//d_euc [!top].set_subvector (0, syndrome);
	memcpy (d_euc [!top], syndrome, NUM_PARITY * sizeof (syndrome [0]));

// step 2: repeat 2t times
	i16 mu [2];
	for (i = 0; i < NUM_PARITY; i++) {
// step 2.a
	   mu [top] = d_euc [top][0];
	   mu[!top] = d_euc [!top][0];
// step 2.b
	   if (mu [!top] != 0 && deg [!top] < deg [top])
	      top = !top;		// swap 'top' and 'bottom'

// step 2.c
	   if (mu [!top] != 0){
	      for (j = 1; j <= deg [top]; j++){
	         d_euc [!top][j] = add_poly (
	                        multiply_poly (mu [top], d_euc [!top][j]),
                                multiply_poly (mu[!top], d_euc[top][j]));
	      }

	      for (; j <= deg [!top]; j++) {
	         d_euc [!top][j] = multiply_poly (mu[top], d_euc[!top][j]);
	      }

	      for (j = NUM_PARITY + 1; j > deg[!top]; j--) {
	         d_euc [top][j] = add_poly (multiply_poly (mu[top],
	                                                   d_euc [top][j]),
	                                    multiply_poly (mu[!top],
	                                                   d_euc[!top][j]));
	      }

	      for (; j > deg [top]; j--){
	         d_euc [top][j] = multiply_poly (mu [top], d_euc [top][j]);
	      }
	   }
// step 2.d
//	d_euc[!top].shift_left(0);
	   memmove (d_euc [!top], d_euc[!top] + 1,
	            (NUM_PARITY + 1) * sizeof (d_euc[0][0]));
	   d_euc [!top][NUM_PARITY + 1] = 0;
	   deg [!top]--;
	}
// step 3: output, evaluator=d_euc[!top][0..deg[!top]],
//	locator=d_euc[top][2t+1..deg[top]+1],
//	highest degree coefficient first
//	if deg [top] > deg [!top], then error correctable;
//	otherwise uncorrectable error pattern
	if (deg [top] <= deg[!top]){
	   // d = r.left (MESSAGE_LRNGTH);	// no correction attempt if
	   // uncorrectable error pattern
	   memcpy (d, r, MESSAGE_LENGTH * sizeof (r [0]));
	   return -1;
	}

	if (deg [top] == NUM_PARITY){ // no error
	   // d = r.left (MESSAGE_LENGTH);
	   memcpy (d, r, MESSAGE_LENGTH * sizeof (r[0]));
	   return 0;
	}

// Chien's Search
	i16 x, x2, y;
	i16 sig_high, sig_low; // sigma_high, sigma_low (temporary)
	i16 sig_even, sig_odd; // sigma_even, sigma_odd: error locator value
	i16 omega; 			// error evaluator value
	i16 err_cnt	= 0;		// error symbol counter
//	for each information symbol position i, i.e.
//	alpha ^ (-i). i represents the exponent of the received polynomial
	for (i = CODE_LENGTH - 1; i >= NUM_PARITY; i--){
	   x  = inverse_power (i + 1); // alpha^(-i). can be optimized
	   x2 = power2poly (multiply_power (x, x)); // x^2
	   x  = power2poly (x);
// calculate locator_even(x^2)
// calculate locator_odd(x^2)
	   sig_high = d_euc [top][NUM_PARITY + 1];
	   sig_low  = d_euc [top][NUM_PARITY];
	   for (j = NUM_PARITY - 1; j > deg [top]; j -= 2)
	      sig_high = add_poly (multiply_poly (sig_high, x2),
	                           d_euc [top][j]);

	   for (j = NUM_PARITY - 2; j > deg[top]; j -= 2)
	      sig_low = add_poly (multiply_poly (sig_low, x2),
	                          d_euc [top][j]);

// the last j is deg[top]+2, then sig_low is sig_odd
	   if (j == deg [top]){
	      sig_odd = sig_low;
	      sig_even = sig_high;
	   } else {
	      sig_odd = sig_high;
	      sig_even = sig_low;
	   }

// calculate locator and judge if it is an error location
	   if (add_poly (sig_even, multiply_poly (x, sig_odd)) != 0)
	      d [CODE_LENGTH - 1 - i] = r [CODE_LENGTH - 1 - i]; // not an error symbol
	   else {	// located an error
	      if (sig_odd == 0){	// non-correctable error
	         //			d = r.left (MESSAGE_LENGTH);
	         memcpy (d, r, MESSAGE_LENGTH * sizeof (r[0]));
	         return -1;
	      }
// calculate error evaluator
	      omega = d_euc [!top][0];
	      for (j = 1; j <= deg [!top]; j++)
	         omega = add_poly (multiply_poly (omega, x),
	                           d_euc [!top][j]);

// error value. Forney
	      if (d_j >= 1){
	         y = multiply_poly (divide_poly (omega, sig_odd),
	                            pow_poly (x, d_j - 1));
	      } else {
	         y = divide_poly (divide_poly (omega, sig_odd),
	                          pow_poly (x, 1 - d_j));
	      }
// error correction
	      d [CODE_LENGTH - 1 - i] = add_poly (r [CODE_LENGTH - 1 - i],  y);
	      err_cnt++; // count the errors
	   } 
	}

	return err_cnt;
}
//
//
//	The relevant operations on the galois field and the
//	polynomes

i16	rscodec::add_poly (i16 a, i16 b) {
	return a ^ b;
}

i16	rscodec::add_power (i16 a, i16 b) {
	return glog [gexp [a] ^ gexp [b]];
}

i16	rscodec::multiply_poly (i16 a, i16 b) {
	return gexp [multiply_power (glog [a], glog [b])];
}

i16	rscodec::multiply_power (i16 a, i16 b) {
	return (a == 0 || b == 0) ? 0 :
	          (a + b - 2) % (d_q - 1) + 1;
}

i16	rscodec::divide_poly (i16 a, i16 b) {
	return gexp [divide_power (glog [a], glog [b])];
}

i16	rscodec::divide_power (i16 a, i16 b) {
	assert (b != 0);
	return (a == 0) ? 0 : round_mod (a - b, d_q - 1) + 1;
}

i16	rscodec::pow_poly (i16 a, i16 n) {
	return gexp [pow_power (glog [a], n)];
}

i16	rscodec::pow_power (i16 a, i16 n) {
	return (a == 0) ? 0 : (a - 1) * n % (d_q - 1) + 1;
}

void	rscodec::poly2tuple (i16 a, u8 tuple[]) {
i16	i;
	for (i = 0; i < d_m; i++, a >>= 1)
	   tuple [i] = (u8)(a & 1);
}

void	rscodec::power2tuple (i16 a, u8 tuple []) {
	poly2tuple (gexp [a], tuple);
}

i16	rscodec::round_mod (i16 a, i16 n) {
	   return (a % n < 0)? (a % n + n) : (a % n);
}

i16	rscodec::power2poly (i16 a) {
	return gexp [a];
}

i16	rscodec::poly2power (i16 a) {
	return glog [a];
}

i16	rscodec::inverse_poly	(i16 a) {
	return divide_poly (1, a);
}

i16	rscodec::inverse_power	(i16 a) {
	return divide_power (1, a);
}
//
//	we do not really need here the encoding facility
//	it is useful for testing though
void	rscodec::enc (const u8 *u, u8 *c, i16 cutlen) {
u16 uf [MESSAGE_LENGTH]; // full length info bytes
u16 cf [CODE_LENGTH]; // full length code bytes
i16 i;

	assert (cutlen >= 0);
	for (i = 0; i < cutlen; i++)
	   uf [i] = 0;
	for (; i < MESSAGE_LENGTH; i++)
	   uf [i] = (u16)u [i - cutlen];
	enc_poly (uf,cf);
	for (i = cutlen; i < CODE_LENGTH; i++)
	   c [i - cutlen] = (u8)cf [i];
}
//
//
//	encoder is not used in the dab receiver
//
void	rscodec::enc_poly (const u16 * u, u16 * c) {
i16 i,j;
i16 fb;

	//d_reg.clear();
	memset (d_reg, 0, NUM_PARITY * sizeof (d_reg[0]));
	for (i = 0; i < MESSAGE_LENGTH; i++) {	// shift in u
	   c[i] = u[i]; // k systematic symbols
	   if (NUM_PARITY / 2 > 0){
	      fb = add_poly (u [i], d_reg [NUM_PARITY - 1]);
	      for (j = NUM_PARITY - 1; j > 0; j--){
	         d_reg[j] = add_poly (d_reg[j-1],
	                              multiply_poly (fb,
	                                power2poly (d_g[j])));
	      }

	      d_reg [0] = multiply_poly (fb, power2poly (d_g [0]));
	   }
	}
// n-k parity symbols
	for (i = MESSAGE_LENGTH; i < CODE_LENGTH; i++)
	   c[i] = d_reg [MESSAGE_LENGTH + NUM_PARITY - 1 - i];
}

