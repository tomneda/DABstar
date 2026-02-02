#
/*
 *    Copyright (C) 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computer
 *
 *    This file is part of Qt-DAB
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
 *
 *      rds encoding is heavily based on the "fmstation"
 *      from Jan Roemisch (github.com/maxx23), all rights acknowledged
 */

#include  <cstdio>
#include  <cstring>
#include  <signal.h>
#include  <stdlib.h>
#include  <stdint.h>
#include  <math.h>
#include  <atomic>
#include  <sys/time.h>
#include  <unistd.h>
#include  <vector>
#include  "up-filter.h"
#include  "dab-streamer.h"
#include  "pluto-rxtx-handler.h"

static inline i64 getMyTime(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return ((i64)tv.tv_sec * 1000000 + (i64)tv.tv_usec);
}

dabStreamer::dabStreamer(i32 inRate, i32 outRate, plutoHandler * generator) :
  lowPassFilter(21, 13000, inRate),
  lmrFilter(25, 38000 - 12000, 38000 + 12000, outRate),
  rdsFilter(45, 57000 - 1500, 57000 + 1500, outRate),
  pcmBuffer(8 * 32768),
  rdsBuffer(2 * 256)
{
  this->inRate = inRate;
  this->outRate = outRate;
  this->generator = generator;

  oscillatorTable = new cf32[outRate];
  for (i32 i = 0; i < outRate; i++)
  {
    oscillatorTable[i] = cf32(cos(2 * M_PI * (f32)i / outRate), sin(2 * M_PI * (f32)i / outRate));
  }

  running.store(false);
  pos = 0;
  nextPhase = 0;

  sinTable = new f32[outRate];
  for (i32 i = 0; i < outRate; i++)
  {
    sinTable[i] = sin((f32)i / outRate * 2 * M_PI);
  }

  rt_pos = 0;
  m = 0;
  e = 0;
  prev_e = 0;
  bpos = 0;
  symclk_p = 0.0;
  symclk_b = 1;

  rds_info.pi = 0x10f0;
  rds_info.af[0] = 0xE210;
  rds_info.af[1] = 0x20E0;
  rds_info.di = RDS_GROUP_0A_DI_STEREO;
  rds_info.pty = 0x00;
  memcpy(&rds_info.ps_name[0], "<home>", 7);
  memcpy(&rds_info.radiotext[0], "no transmission yet\r", 19);
  rds_init_groups(&rds_info);
  group = rds_group_schedule();
  messageIn.store(false);
  threadHandle = std::thread(&dabStreamer::run, this);
  running.store(true);
}

dabStreamer::~dabStreamer(void)
{
  stop();

  delete sinTable;
  delete oscillatorTable;
}

void dabStreamer::stop(void)
{
  if (!running.load())
  {
    return;
  }
  running.store(false);
  threadHandle.join();
}

void dabStreamer::audioOutput(f32 * v, i32 amount)
{
  auto * const lBuffer = make_vla(f32, 2 * amount);

  while (pcmBuffer.GetRingBufferWriteAvailable() < 2 * amount)
  {
    usleep(1000);
  }
  for (i32 i = 0; i < amount; i++)
  {
    cf32 x = cf32(v[2 * i], v[2 * i + 1]);
    x = lowPassFilter.Pass(x);
    lBuffer[2 * i + 0] = real(x);
    lBuffer[2 * i + 1] = imag(x);
  }
  pcmBuffer.putDataIntoBuffer(lBuffer, 2 * amount);
}

void dabStreamer::addRds(const std::string v)
{
  if (messageIn.load())
  {    // still processing?
    return;
  }
  rdsBuffer.putDataIntoBuffer(v.c_str(), v.size());
  messageIn.store(true);
}

void dabStreamer::addName(const std::string n)
{
}

void dabStreamer::run()
{
  i32 outputLimit = 2 * outRate / 5;  // in single values
  upFilter theFilter(15, inRate, outRate);
  u64 nextStop;

  //	the rds text will be the name of the file that is being played
  rds_info.pi = 0x10f0;
  rds_info.af[0] = 0xE210;
  rds_info.af[1] = 0x20E0;
  rds_info.di = RDS_GROUP_0A_DI_STEREO;
  rds_info.pty = 0x00;
  memset(&rds_info.ps_name[0], 0, RDS_PS_LEN);
  memcpy(&rds_info.ps_name[0], "<DAB>", 6);
  memcpy(&rds_info.radiotext[0], "no rds yet\r", 12);
  rds_init_groups(&rds_info);
  group = rds_group_schedule();

  running.store(true);

  auto * const lBuf = make_vla(f32, inRate / 5);
  while (running.load())
  {
    i32 readCount;
    //	   u64 currentTime      = getMyTime ();
    //
    //	check to see if  we need/can update the rds text
    i32 rdsCount = rdsBuffer.GetRingBufferReadAvailable();
    if ((rdsCount >= 10) && (rt_pos == 0))
    {
      auto * const rds = make_vla(char, rdsCount);
      rdsBuffer.getDataFromBuffer(rds, rdsCount);
      messageIn.store(false);
      for (i32 i = 0; i < rdsCount; i++)
      {
        rds_info.radiotext[i] = rds[i];
      }
      rds_info.radiotext[rdsCount] = '\r';
      rds_info.radiotext[rdsCount + 1] = 0;
      rds_init_groups(&rds_info);
      group = rds_group_schedule();
    }

    while (running.load() && (pcmBuffer.GetRingBufferReadAvailable() < inRate / 10))
    {
      //	      cf32 filler [48];
      //	      memset (filler, 0, 48 * sizeof (cf32));
      usleep(10000);
    }

    if (!running.load())
    {
      break;
    }
    readCount = pcmBuffer.getDataFromBuffer(lBuf, inRate / 10);
    for (i32 i = 0; i < readCount / 2; i++)
    {
      cf32 v = cf32(4 * lBuf[2 * i], 4 * lBuf[2 * i + 1]);
      auto * const lbuf = make_vla(cf32, outRate / inRate);
      theFilter.Filter(v, lbuf);
      modulateData((f32 *)lbuf, outRate / inRate, 2);
    }
  }
}

//
//	Preemphasis filter derived from the one by Jonti (Jonathan Olds)
static f64 a0 = 5.309858008;
static f64 a1 = -4.794606188;
static f64 b1 = 0.4847481783;

f32 sample;
struct sample_i
{
  f32 l;
  f32 r;
};

struct sample_i samp_s;

struct sample_i preemp(struct sample_i in)
{
  static f64 x_l = 0;
  static f64 x_r = 0;
  static f64 y_l = 0;
  static f64 y_r = 0;
  struct sample_i res;

  y_l = a0 * in.l + a1 * x_l - b1 * y_l;
  x_l = in.l;
  y_r = a0 * in.r + a1 * x_r - b1 * y_r;
  x_r = in.r;
  res.l = y_l;
  res.r = y_r;
  return res;
}

f32 preemp(f32 in)
{
  static f64 x = 0;
  static f64 y = 0;

  y = a0 * in + a1 * x - b1 * y;
  x = in;
  return y;
}

f32 lowPass(f32 s)
{
  static f64 out = 0;
  out = out + 0.05 * (s - out);
  return out;
}

void dabStreamer::modulateData(f32 * bo, i32 amount, i32 channels)
{
  i32 i;

  for (i = 0; i < amount; i++)
  {
    //	   f64 clock		= 2 * M_PI * (f64)pos * 19000.0 / outRate;

    //	taking 3468 as period works fine
    i32 ind_1 = (i32)(((i64)pos * 19000) % outRate);
    i32 ind_2 = (i32)(((i64)pos * 19000 * 2) % outRate);
    i32 ind_3 = (i32)(((i64)pos * 19000 * 3) % outRate);
    i32 ind_4 = (i32)(((i64)pos * 19000 / 16) % outRate);
    f64 pilot = sinTable[ind_1];
    f64 carrier = sinTable[ind_2];
    f64 rds_carrier = 4.0 * sinTable[ind_3];
    f64 symclk = sinTable[ind_4];
    f64 bit_d = 0;

    if ((symclk_p <= 0) && (symclk > 0))
    {
      e = group->bits[bpos] ^ prev_e;
      m = MANCH_ENCODE (symclk_b, e);
      bit_d = NRZ (m);
      bpos = (bpos + 1) % RDS_GROUP_LEN;
      prev_e = e;
      symclk_b ^= 1;

      if (bpos == 0)
      {
        group = rds_group_schedule();
      }
      if (pos > 3648)
      {
        pos = 0;
      }
    }

    if ((symclk_p >= 0) && (symclk < 0))
    {
      bit_d = NRZ (m >> 1);
      symclk_b ^= 1;
    }
    //
    //	finally, fetch the (left part of the) sample
    samp_s.l = bo[i * channels];
    if ((channels == 2) && (outRate >= 192000))
    {
      samp_s.r = bo[i * channels + 1];
      //
      //	sound elements are preempted
      samp_s = preemp(samp_s);
      f64 lpr = (samp_s.l + samp_s.r);
      f64 lmr = (samp_s.l - samp_s.r);

      //	the rds signal
      //
      //	and build up the resulting sample
      f32 sym = lowPass(fabs(symclk) * bit_d);
      sample = 0.50 * lpr + 0.05 * pilot + 0.35 * lmr * carrier +
               //	                          lmrFilter. Pass (0.85	* lmr * carrier) +
               10 * sym * rds_carrier;

      symclk_p = symclk;
    }
    else if (channels == 2)
    {
      sample = preemp(samp_s.l + bo[i * 2]) / 2;
    }
    else
    {
      sample = preemp(samp_s.l);
    }

    nextPhase += 5 * sample;
    if (nextPhase >= 2 * M_PI)
    {
      nextPhase -= 2 * M_PI;
    }
    if (nextPhase < 0)
    {
      nextPhase += 2 * M_PI;
    }
    i32 index = nextPhase / (2 * M_PI) * outRate;
    //
    //	there might be an issue with the resulting index, nextPhase
    //	may be nan, inf, or just too large
    i32 aa = index % outRate;
    if ((0 <= aa) && (aa < outRate))
    {
      generator->sendSample(oscillatorTable[aa], sample);
    }
    pos++;
  }
}

u16 dabStreamer::rds_crc(u16 in)
{
  i32 i;
  u16 reg = 0;
  static const u16 rds_poly = 0x5B9;

  for (i = 16; i > 0; i--)
  {
    reg = (reg << 1) | ((in >> (i - 1)) & 0x1);
    if (reg & (1 << 10))
    {
      reg = reg ^ rds_poly;
    }
  }

  for (i = 10; i > 0; i--)
  {
    reg = reg << 1;
    if (reg & (1 << 10))
    {
      reg = reg ^ rds_poly;
    }
  }
  return (reg & ((1 << 10) - 1));
}

void dabStreamer::rds_bits_to_values(char * out, u16 in, i32 len)
{
  i32 n = len;
  u16 mask;

  while (n--)
  {
    mask = 1 << n;
    out[len - 1 - n] = ((in & mask) >> n) & 0x1;
  }
}

void dabStreamer::rds_serialize(struct rds_group_s * group, char flags)
{
  i32 n = 4;
  static const u16 rds_checkwords[] = { 0xFC, 0x198, 0x168, 0x1B4, 0x350, 0x0 };

  while (n--)
  {
    i32 start = RDS_BLOCK_LEN * n;
    u16 block = group->blocks[n];
    u16 crc;

    if (!((flags >> n) & 0x1))
    {
      continue;
    }

    crc = rds_crc(block);
    crc ^= rds_checkwords[n];
    rds_bits_to_values(&group->bits[start], block, RDS_BLOCK_DATA_LEN);
    rds_bits_to_values(&group->bits[start + RDS_BLOCK_DATA_LEN], crc, RDS_BLOCK_CRC_LEN);
  }
}

//
//
void dabStreamer::rds_init_groups(struct rds_info_s * info)
{

  group_0a.info_block = info;
  group_0a.blocks[index_A] = info->pi;
  group_0a.blocks[index_B] = RDS_GROUP (RDS_GROUP_TYPE(0, 0)) | RDS_TP | RDS_PTY (info->pty) | RDS_GROUP_0A_MUSIC;

  group_0a.info = ((info->af[0] >> 8) & 0xff) - 0xE0;
  rds_serialize(&group_0a, RDS_BLOCK_A);

  group_2a.info_block = info;
  group_2a.blocks[index_A] = info->pi;
  group_2a.blocks[index_B] = RDS_GROUP (RDS_GROUP_TYPE(2, 0)) | RDS_TP | RDS_PTY (info->pty);

  group_2a.info = strnlen(info->radiotext, RDS_RT_MAX);
  //	fprintf (stderr, "length of rds message %d\n", group_2a. info);
  rds_serialize(&group_2a, RDS_BLOCK_A);

  group_3a.info_block = info;
  group_3a.blocks[index_A] = info->pi;
  group_3a.blocks[index_B] = RDS_GROUP (RDS_GROUP_TYPE(3, 0)) | RDS_TP | RDS_PTY (info->pty) | RDS_TMC_GROUP;

  group_3a.blocks[index_D] = RDS_TMC_AID;
  rds_serialize(&group_3a, RDS_BLOCK_A | RDS_BLOCK_B | RDS_BLOCK_D);

  group_8a.info_block = info;
  group_8a.blocks[index_A] = info->pi;
  group_8a.blocks[index_B] = RDS_GROUP (RDS_GROUP_TYPE(8, 0)) | RDS_TP | RDS_PTY (info->pty) | RDS_TMC_F | RDS_TMC_DP(0);

  group_8a.blocks[index_C] = RDS_TMC_D | RDS_TMC_PN | RDS_TMC_EXTENT (1) | RDS_TMC_EVENT (1478);
  group_8a.blocks[index_D] = RDS_TMC_LOCATION (12693);
  rds_serialize(&group_8a, RDS_BLOCK_A | RDS_BLOCK_B | RDS_BLOCK_C | RDS_BLOCK_D);
}

//
//	group 0: deals with af and ps name
void dabStreamer::rds_group_0A_update(void)
{
  static i32 af_pos = 0, ps_pos = 0;
  u16 di = (group_0a.info_block->di >> (ps_pos >> 1)) & 0x1;

  group_0a.blocks[index_B] = (group_0a.blocks[index_B] & 0xfff8) | (di << 2) | (ps_pos >> 1);
  group_0a.blocks[index_C] = group_0a.info_block->af[af_pos];
  group_0a.blocks[index_D] = group_0a.info_block->ps_name[ps_pos] << 8 | group_0a.info_block->ps_name[ps_pos + 1];

  rds_serialize(&group_0a, RDS_BLOCK_B | RDS_BLOCK_C | RDS_BLOCK_D);

  af_pos = (af_pos + 1) % group_0a.info;
  ps_pos = (ps_pos + 2) & (RDS_PS_LEN - 1);
}

//
//	group 2: deals with the tekst
void dabStreamer::rds_group_2A_update(void)
{
  static i32 b_pos = 0;
  group_2a.blocks[index_B] = (group_2a.blocks[index_B] & 0xffe0) | b_pos;
  group_2a.blocks[index_C] = group_2a.info_block->radiotext[rt_pos + 0] << 8 | group_2a.info_block->radiotext[rt_pos + 1];
  group_2a.blocks[index_D] = group_2a.info_block->radiotext[rt_pos + 2] << 8 | group_2a.info_block->radiotext[rt_pos + 3];

  rds_serialize(&group_2a, RDS_BLOCK_B | RDS_BLOCK_C | RDS_BLOCK_D);

  b_pos = (b_pos + 1) & 0xf;
  if ((rt_pos += 4) > group_2a.info)
  {
    rt_pos = 0, b_pos = 0;
  }
}

void dabStreamer::rds_group_3A_update(void)
{
  static i32 toggle = 1;

  if (toggle)
  {
    group_3a.blocks[index_C] = RDS_TMC_VAR (0) | RDS_TMC_LTN (1) | RDS_TMC_N | RDS_TMC_R;
  }
  else
  {
    group_3a.blocks[index_C] = RDS_TMC_VAR (1) | RDS_TMC_GAP (0) | RDS_TMC_SID (0) | RDS_TMC_TA (0) | RDS_TMC_TW (0) | RDS_TMC_TD (0);
  }

  rds_serialize(&group_3a, RDS_BLOCK_C);
  toggle ^= 1;
}

void dabStreamer::rds_group_8A_update(void)
{
}

struct rds_group_s * dabStreamer::rds_group_schedule(void)
{
  static i32 ps = 1;
  struct rds_group_s * group = NULL;

  switch (ps)
  {
  case 0: rds_group_0A_update();
    group = &group_0a;
    break;

  case 1: rds_group_2A_update();
    group = &group_2a;
    break;

  case 2: rds_group_3A_update();
    group = &group_3a;
    break;

  case 3: rds_group_8A_update();
    group = &group_8a;
    break;
  }

  ps = (ps + 1) % 4;
  return group;
}
