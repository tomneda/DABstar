/*
 *    Copyright (C) 2014 .. 2020
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
//
//	Common definitions and includes for
//	the DAB decoder

#ifndef  DAB_CONSTANTS_H
#define  DAB_CONSTANTS_H

#include "glob_defs.h"
#include  <QString>
#include  <cmath>
#include  <cstdlib>
#include  <cstdio>
#include  <complex>
#include  <limits>
#include  <cstring>
#include  <unistd.h>

#ifndef  __FREEBSD__
//#include	<malloc.h>
#endif

#ifdef  __MINGW32__
//#include	"iostream.h"
  #include	"windows.h"
#else
  #ifndef  __FREEBSD__
//#include	"alloca.h"
  #endif

  #include  "dlfcn.h"

typedef void * HINSTANCE;
#endif

#ifndef  M_PI
# define M_PI           3.14159265358979323846  /* pi */
#endif

#define  Hz(x)     (x)
#define  kHz(x)    (x * 1000)
#define  MHz(x)    (kHz (x) * 1000)

#define    AUDIO_SERVICE  0101
#define    PACKET_SERVICE  0102
#define    UNKNOWN_SERVICE  0100

#define    INPUT_RATE  2048000
#define    BANDWIDTH  1536000

#define    MAP_RESET  0
#define    MAP_FRAME  1
#define    MAP_MAX_TRANS  2
#define    MAP_NORM_TRANS  4

typedef struct
{
  int theTime;
  QString theText;
  QString theDescr;
} epgElement;

class serviceId
{
public:
  QString name;
  uint32_t SId;
  uint16_t subChId;
  bool isAudio;
  int16_t programType;
  QString channel;        // just for presets
};

//	order by id order by name
#define ID_BASED        1
#define  SUBCH_BASED  2
#define  ALPHA_BASED  3

//
//	40 up shows good results
#define    DIFF_LENGTH  60

static inline bool isIndeterminate(float x)
{
  return x != x;
}

static inline bool isInfinite(float x)
{
  return x == std::numeric_limits<float>::infinity();
}

static inline float get_db(float x)
{
  return 20 * log10((x + 0.005) / (float)(256));
}
//

template<typename T> inline void limit(T & ioVal, const T iLimitLeft, const T iLimitRight)
{
  if (ioVal < iLimitLeft)
  {
    ioVal = iLimitLeft;
  }
  else if (ioVal > iLimitRight)
  {
    ioVal = iLimitRight;
  }
}

static inline float jan_abs(cmplx z)
{
  float re = real(z);
  float im = imag(z);

  if (re < 0)
  {
    re = -re;
  }
  if (im < 0)
  {
    im = -im;
  }
  if (re > im)
  {
    return re + 0.5 * im;
  }
  else
  {
    return im + 0.5 * re;
  }
}

template<typename T> static inline T fixround(float v)
{
  return static_cast<T>(std::roundf(v));
}

#define    BAND_III  0100
#define    L_BAND    0101
#define    A_BAND    0102

#define    FORE_GROUND  0000
#define    BACK_GROUND  0100

class DescriptorType
{
public:
  uint8_t type;
  bool defined;
  QString serviceName;
  int32_t SId;
  int SCIds;
  int16_t subchId;
  int16_t startAddr;
  bool shortForm;
  int16_t protLevel;
  int16_t length;
  int16_t bitRate;
  QString channel;  // just for presets
public:
  DescriptorType()
  {
    defined = false;
    serviceName = "";
  }

  virtual    ~DescriptorType()
  {}
};

//	for service handling we define
class packetdata : public DescriptorType
{
public:
  int16_t DSCTy;
  int16_t FEC_scheme;
  int16_t DGflag;
  int16_t appType;
  int16_t compnr;
  int16_t packetAddress;

  packetdata()
  {
    type = PACKET_SERVICE;
  }
};

class Audiodata : public DescriptorType
{
public:
  int16_t ASCTy;
  int16_t language;
  int16_t programType;
  int16_t compnr;
  int32_t fmFrequency;

  Audiodata()
  {
    type = AUDIO_SERVICE;
    fmFrequency = -1;
  }
};

typedef struct
{
  bool in_use;
  int16_t id;
  int16_t start_cu;
  uint8_t uepFlag;
  int16_t protlev;
  int16_t size;
  int16_t bitrate;
  int16_t ASCTy;
} channel_data;



#endif
