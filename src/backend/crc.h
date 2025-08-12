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

//	This is a (partial) rewrite of the GNU radio code, for use
//	within the DAB/DAB+ sdr-j receiver software
//	all rights are acknowledged.

#pragma once
#ifndef  CRC_H
#define  CRC_H

#include "glob_data_types.h"
//#include <cstdint>

u16 calc_crc(const u8 * const data, const i32 length);
bool check_crc_bytes(const u8 * const msg, const i32 len);
bool check_CRC_bits(const u8 * const iIn, const i32 iSize);

#endif
