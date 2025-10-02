/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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

#ifndef  DAB_CONSTANTS_H
#define  DAB_CONSTANTS_H

#include "glob_defs.h"
#include  <QString>
#include  <cmath>
#include  <unistd.h>

#if defined(__MINGW32__) || defined(_WIN32)
  #include	"windows.h"
#else
  #include  "dlfcn.h"
  using HINSTANCE = void *;
#endif

#ifndef  M_PI
  #define M_PI           3.14159265358979323846
#endif

#define  Hz(x)     (x)
#define  kHz(x)    (x * 1000)
#define  MHz(x)    (kHz (x) * 1000)

constexpr i32 INPUT_RATE = 2048000;
constexpr i32 LIGHT_SPEED_MPS = 299792458;

constexpr f32 F_2_M_PI = (f32)(2 * M_PI);
constexpr f32 F_M_PI   = (f32)M_PI;
constexpr f32 F_M_PI_4 = (f32)M_PI_4;
constexpr f32 F_M_PI_2 = (f32)M_PI_2;
constexpr f32 F_SQRT1_2 = 0.70710678118654752440084436210485f;
constexpr f32 F_RAD_PER_DEG = (f32)(M_PI / 180.0);
constexpr f32 F_DEG_PER_RAD = (f32)(180.0 / M_PI);
constexpr i16 VITERBI_SOFT_BIT_VALUE_MAX = 127;
constexpr f32 F_VITERBI_SOFT_BIT_VALUE_MAX = (f32)VITERBI_SOFT_BIT_VALUE_MAX;

constexpr u8 BAND_III = 0100;
constexpr u8 L_BAND   = 0101;
constexpr u8 A_BAND   = 0102;

constexpr char sSettingSampleStorageDir[]  = "saveDirSampleDump";
constexpr char sSettingAudioStorageDir[]   = "saveDirAudioDump";
constexpr char sSettingContentStorageDir[] = "saveDirContent";

enum class EProcessFlag
{
  Foreground,
  Background
};

enum ETMId // TransportMechanismMode
{
  StreamModeAudio = 0x0,
  StreamModeData  = 0x1,
  // Reserved        = 0x2,
  PacketModeData  = 0x3
};

enum EMapType
{
  MAP_RESET      = 0,
  MAP_FRAME      = 1,
  MAP_MAX_TRANS  = 2,
  MAP_NORM_TRANS = 4
};

template<typename T>
struct SpecViewLimits
{
  struct SMaxMin { T Max; T Min; };
  // the sequence is in typical level order
  SMaxMin Glob {-30, -90};
  SMaxMin Loc  {-31, -50};
};

struct SEpgElement
{
  i32 theTime;
  QString theText;
  QString theDescr;
};

struct SServiceId
{
  QString name;
  u32 SId;
  bool isAudioChannel;
  bool hasSpiEpgData;  
};

struct SDescriptorType
{
  bool isDefined = false;
  ETMId TMId;
  QString serviceName;
  QString serviceNameShort;
  u32 SId;
  i32 SCIdS;
  i16 subchId;
  i16 startAddr;
  bool shortForm;
  i16 protLevel;
  i16 length;
  i16 bitRate;
  QString channel;
};

//	for service handling we define
struct SPacketData : SDescriptorType
{
  i16 DSCTy;
  i16 FEC_scheme;
  i16 DGflag;
  std::vector<i16> appTypeVec;
  // i16 compnr;
  i16 packetAddress;
  SPacketData() { TMId = ETMId::PacketModeData; }
};

struct SAudioData : SDescriptorType
{
  i16 ASCTy;
  // i16 language;
  // i16 programType;
  // i16 compnr;
  // i32 fmFrequency = -1;
  SAudioData() { TMId = ETMId::StreamModeAudio; }
};

struct SAudioDataAddOns // "slower" addons
{
  std::optional<i16> language = std::nullopt;
  std::optional<i16> programType = std::nullopt;;
  // i32 fmFrequency = -1;
};

struct SChannelData
{
  bool in_use;
  i16 id;
  i16 start_cu;
  u8 uepFlag;
  i16 protlev;
  i16 size;
  i16 bitrate;
  i16 ASCTy;
};

#endif
