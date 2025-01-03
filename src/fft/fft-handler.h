/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015 .. 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
 *
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
#ifndef  FFT_HANDLER_H
#define  FFT_HANDLER_H

#include "glob_defs.h"
#include <vector>

#ifdef  __FFTW3__
  #include  <fftw3.h>
#elif __KISS_FFT__
  #include "kiss_fft.h"
#elif __NAYUKI__
#else
  #error "FFT declaration missing!"
#endif

class FftHandler
{
public:
  FftHandler(int size, bool);
  ~FftHandler();

  void fft(std::vector<cmplx> & ioV) const;
  void fft(cmplx * const ioV) const;

  void fft(float * const ioV) const;

private:
  int size;
  bool dir;
#ifdef  __KISS_FFT__
  kiss_fft_cfg plan;
  kiss_fft_cpx *fftVector_in;
  kiss_fft_cpx *fftVector_out;
#elif  __FFTW3__
  fftwf_plan planCmplx;
  fftwf_plan planFloat;
  cmplx * fftCmplxVector = nullptr;
  float * fftFloatVector = nullptr;
#endif
};

#endif
