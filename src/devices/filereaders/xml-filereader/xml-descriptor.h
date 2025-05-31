/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB 
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
 */

#ifndef  XML_DESCRIPTOR_H
#define  XML_DESCRIPTOR_H

#include <QtXml>
#include <QString>
#include <cstdint>
#include <cstdio>

class Blocks
{
public:
  Blocks() = default;
  ~Blocks() = default;

  i32 blockNumber = 0;
  i32 nrElements = 0;
  QString typeofUnit;
  i32 frequency = 0;
  QString modType;
};

class XmlDescriptor
{
public:
  QString deviceName;
  QString deviceModel;
  QString recorderName;
  QString recorderVersion;
  QString recordingTime;
  i32 sampleRate;
  i32 nrChannels;
  i32 bitsperChannel;
  QString container;
  QString byteOrder;
  QString iqOrder;
  i32 nrBlocks;
  std::vector<Blocks> blockList;

  XmlDescriptor(FILE *, bool *);
  ~XmlDescriptor() = default;

  void printDescriptor();
  void setSamplerate(i32 sr);
  void setChannels(i32 nrChannels, i32 bitsperChannel, QString ct, QString byteOrder);
  void addChannelOrder(i32 channelOrder, QString Value);
  void add_dataBlock(i32 currBlock, i32 Count, i32 blockNumber, QString Unit);
  void add_freqtoBlock(i32 blockno, i32 freq);
  void add_modtoBlock(i32 blockno, QString modType);
};

#endif
