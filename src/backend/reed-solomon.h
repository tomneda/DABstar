/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/* Include file to configure the RS codec for character symbols
 *
 * Copyright 2002, Phil Karn, KA9Q
 * May be used under the terms of the GNU General Public License (GPL)
 */

#ifndef  REED_SOLOMON_H
#define  REED_SOLOMON_H

#include  <cstdint>
#include  "galois.h"
#include  <vector>

class ReedSolomon
{
public:
  explicit ReedSolomon(u16 symsize = 8, u16 gfpoly = 0435, u16 fcr = 0, u16 prim = 1, u16 nroots = 10);
  ~ReedSolomon() = default;

  i16 dec(const u8 * data_in, u8 * data_out, i16 cutlen);
  void enc(const u8 * data_in, u8 * data_out, i16 cutlen);

private:
  Galois myGalois;
  u16 symsize;   /* Bits per symbol */
  u16 codeLength;  /* Symbols per block (= (1<<mm)-1) */
  std::vector<u8> generator;  /* Generator polynomial */
  u16 nroots;  /* Number of generator roots = number of parity symbols */
  u8 fcr;    /* First consecutive root, index form */
  u8 prim;    /* Primitive element, index form */
  u8 iprim;    /* prim-th root of 1, index form */

  bool computeSyndromes(u8 *, u8 *);
  u8 getSyndrome(u8 *, u8);
  u16 computeLambda(u8 *, u8 *);
  i16 computeErrors(u8 *, u16, u8 *, u8 *);
  u16 computeOmega(u8 *, u8 *, u16, u8 *);
  void encode_rs(const u8 * data_in, u8 * roots);
  i16 decode_rs(u8 * data);
};

#endif
