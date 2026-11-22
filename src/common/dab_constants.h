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

#pragma once

#include "glob_defs.h"
#include "qt_compat.h"
#include <QString>
#include <cmath>
#include <unistd.h>

#if defined(_WIN32)
  #include	"windows.h"
#else
  #include "dlfcn.h"
  using HINSTANCE = void*;
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

constexpr char sSettingSampleStorageDir[]  = "saveDirSampleDump";
constexpr char sSettingAudioStorageDir[]   = "saveDirAudioDump";
constexpr char sSettingContentStorageDir[] = "saveDirContent";

enum class EProcessFlag
{
  Primary,
  Secondary,
  Global     // background service of the ensemble (like SPI/EPG), independent of the selected service
};

enum ETMId // TransportMechanismMode
{
  StreamModeAudio = 0x0,
  StreamModeData  = 0x1,
  // Reserved        = 0x2,
  PacketModeData  = 0x3
};

template<typename T>
struct SpecViewLimits
{
  struct SMaxMin { T Max; T Min; };
  // the sequence is in typical level order
  SMaxMin Glob {-30, -90};
  SMaxMin Loc  {-31, -50};
};

struct SServiceId
{
  QString serviceLabel;
  u32 SId;
  bool isAudioChannel;
  bool hasSpiEpgData;
};

struct SDescriptorType
{
  bool isDefined = false;
  ETMId TMId;
  u32 SId;
  i16 SubChId;
  i16 CuStartAddr;
  i16 CuSize;
  bool shortForm;
  i16 protLevel;
  i16 bitRate;

  // these could be initialized later when FIG 1/x data are loaded (slower)
  // QString channel;
  QString serviceLabel;
  QString serviceLabelShort;
};

//	for service handling we define
struct SPacketData : SDescriptorType
{
  i16 DSCTy;
  i16 FEC_scheme;
  i16 DGflag;
  std::vector<i16> appTypeVec;
  i16 PacketAddress;
  i32 SCIdS;
  std::optional<i16> language = std::nullopt;
  SPacketData() { TMId = ETMId::PacketModeData; }
};

struct SAudioData : SDescriptorType
{
  i16 ASCTy;
  // i16 programType;
  // i16 compnr;
  // i32 fmFrequency = -1;
  SAudioData() { TMId = ETMId::StreamModeAudio; }
};

constexpr i16 cUserAppTypeSlideShow = 0x002; // ETSI TS 101 756, table 16: MOT Slideshow
constexpr i16 cUserAppTypeSpi = 0x007; // ETSI TS 101 756, table 16: SPI (Service and Programme Information, formerly EPG)
constexpr i16 cDSCTyMot = 60;          // ETSI TS 101 756, table 2b: Multimedia Object Transfer (MOT)

struct SXPadUserApp // user application carried in the X-PAD of an audio service component (signalled via FIG 0/13)
{
  i16 UserAppType = -1; // e.g. 0x44A for Journaline
  i16 XPadAppTy = -1;   // X-PAD application type for the start of X-PAD data group, continuation uses XPadAppTy + 1
  i16 DSCTy = -1;
  bool DataGroupsUsed = false; // MSC data groups are used to transport the user application
};

// user applications whose MSC data groups are collected from the X-PAD and handled like in a packet mode service
enum class EXPadDataGroupApp : u8
{
  Journaline = 0,
  Spi        = 1  // SPI (EPG) via MOT
};
constexpr usize cNumXPadDataGroupApps = 2;

struct SAudioDataAddOns // "slower" addons
{
  std::optional<i16> language = std::nullopt;
  std::optional<i16> programType = std::nullopt;
  std::optional<u16> announcementSupportFlags = std::nullopt; // FIG 0/18 ASu flags, b0: Alarm, b1: Road Traffic flash, ...
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

