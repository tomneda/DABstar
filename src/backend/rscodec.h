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

//
//	In the SDR-J DAB+ software, we use a - slighty altered -
//	version of the dabp_rscodec as found in GnuRadio.
//	For the Galois fields, we use a different implementation
#ifndef	RSCODEC
#define	RSCODEC

#include	<cstdint>
#include	<cstdint>
#include	<cstring>


#define	GALOIS_DEGREE	8
#define	ERROR_CORRECT	5
#define	START_J		0

class rscodec {
public:
	// GF(2^m), error correction capability t,
	// start_j is the starting exponent in the generator polynomial
	rscodec();
	 ~rscodec();
// decode shortened code
	i16 dec (const u8 *r, u8 *d, i16 cutlen = 135);
//	encode shortened code, cutlen bytes were shortened
//	not used for the DAB+ decoding
	void enc (const u8 *u, u8 *c, i16 cutlen = 135);
private:
	static	const i16 d_m	= 8;		// m
	static	const i16 d_q	= 1 << 8;	// q = 2 ^ m
// primitive polynomial to generate GF(q)
	static	const i16 d_p	= 0435;
// starting exponent for generator polynomial, j
	static	const i16	d_j	= 0;
//
//	LUT translating power form to polynomial form
	i32 gexp [512];
//	LUT translating polynomial form to power form
	i32 glog [512];
	
// all in power representations
 	i16	add_poly	(i16 a, i16 b);
	i16 add_power	(i16 a, i16 b);
	i16 multiply_poly	(i16 a, i16 b); // a*b
	i16 multiply_power	(i16 a, i16 b);
	i16 divide_poly	(i16 a, i16 b); 	// a/b
	i16 divide_power	(i16 a, i16 b);
	i16 pow_poly	(i16 a, i16 n);		// a^n
	i16 pow_power	(i16 a, i16 n);
	i16 power2poly	(i16 a);
	i16 poly2power	(i16 a);
	i16	inverse_poly	(i16 a);
	i16	inverse_power	(i16 a);

// convert a polynomial representation to m-tuple representation
// returned in tuple, lowest degree first.
// tuple must have size d_m at minimum
	void poly2tuple		(i16 a, u8 tuple[]);
	void power2tuple	(i16 a, u8 tuple[]);

// round mod algorithm, calculate a % n, n > 0, a is any integer, a % n >= 0
	i16 round_mod (i16 a, i16 n);

// u and c are in polynomial representation. This is more common in practice
	void enc_poly (const u16 * u, u16 * c);
	   
// r and d are in polynomial representation. This is more common in practice
	i16 dec_poly (const u16 *r, u16 *d);

//	i32 d_m;		// GF(2^m)
//	dabp_galois d_gf;	// Galois field

// d_g[0] is the lowest exponent coefficient,
// d_g[2t-1] is the highest exponent coefficient, d_g[2t]=1 is not included
	i32 *d_g;		// generator g.

// error correcting capability t, info length k, codeword length n,
// all measured in unit of GF(2^m) symbols
	
	i32 *d_reg;	// registers for encoding
	i32 *syndrome;	// syndrome
	i32 *d_euc [2];	// data structure for Euclidean computation
	
	void create_polynomials (i32 start_j); // initialize the generator polynomial g
};

#endif		// DABP_RSCODE

