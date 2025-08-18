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

#include "glob_data_types.h"
#include  "xml-descriptor.h"

void XmlDescriptor::printDescriptor()
{
  fprintf(stderr, "sampleRate =	%d\n", sampleRate);
  fprintf(stderr, "nrChannels	= %d\n", nrChannels);
  fprintf(stderr, "bitsperChannel = %d\n", bitsperChannel);
  fprintf(stderr, "container	= %s\n", container.toLatin1().data());
  fprintf(stderr, "byteOrder	= %s\n", byteOrder.toLatin1().data());
  fprintf(stderr, "iqOrder	= %s\n", iqOrder.toLatin1().data());
  fprintf(stderr, "nrBlocks	= %d (%d)\n", nrBlocks, (i32)(blockList.size()));
  for (i32 i = 0; i < (i32)blockList.size(); i++)
  {
    fprintf(stderr,
            ">>>   %d %lld %s %d %s\n",
            blockList.at(i).blockNumber,
            blockList.at(i).nrElements,
            blockList.at(i).typeofUnit.toLatin1().data(),
            blockList.at(i).frequency,
            blockList.at(i).modType.toLatin1().data());
  }
}

void XmlDescriptor::setSamplerate(i32 sr)
{
  this->sampleRate = sr;
}

void XmlDescriptor::setChannels(i32 nrChannels, i32 bitsperChannel, QString ct, QString byteOrder)
{
  this->nrChannels = nrChannels;
  this->bitsperChannel = bitsperChannel;
  this->container = ct;
  this->byteOrder = byteOrder;
}

void XmlDescriptor::addChannelOrder(i32 channelOrder, QString Value)
{
  if (channelOrder > 1)
  {
    return;
  }
  if (channelOrder == 0)
  {  // first element
    this->iqOrder = Value == "I" ? "I_ONLY" : "Q_ONLY";
  }
  else if ((this->iqOrder == "I_ONLY") && (Value == "Q"))
  {
    this->iqOrder = "IQ";
  }
  else if ((this->iqOrder == "Q_ONLY") && (Value == "I"))
  {
    this->iqOrder = "QI";
  }
}

void XmlDescriptor::add_dataBlock(i32 /*currBlock*/, i64 Count, i32 blockNumber, QString Unit)
{
  Blocks b;
  b.blockNumber = blockNumber;
  b.nrElements = Count;
  b.typeofUnit = Unit;
  blockList.push_back(b);
}

void XmlDescriptor::add_freqtoBlock(i32 blockno, i32 freq)
{
  blockList.at(blockno).frequency = freq;
}

void XmlDescriptor::add_modtoBlock(i32 blockno, QString modType)
{
  blockList.at(blockno).modType = modType;
}

//
//	precondition: file exists and is readable.
//	Note that after the object is filled, the
//	file pointer points to where the contents starts
XmlDescriptor::XmlDescriptor(FILE * f, bool * ok)
{
  QDomDocument xmlDoc;
  QByteArray xmlText;
  i32 zeroCount = 0;
  //
  //	set default values
  sampleRate = 2048000;
  nrChannels = 2;
  bitsperChannel = 16;
  container = "i16";
  byteOrder = QString("MSB");
  iqOrder = "IQ";
  u8 theChar;
  while (zeroCount < 500)
  {
    theChar = fgetc(f);
    if (theChar == 0)
    {
      zeroCount++;
    }
    else
    {
      zeroCount = 0;
    }

    xmlText.append(theChar);
  }

  xmlDoc.setContent(xmlText);
  QDomElement root = xmlDoc.documentElement();
  QDomNodeList nodes = root.childNodes();

  fprintf(stderr, "document has %d topnodes\n", nodes.count());
  for (i32 i = 0; i < nodes.count(); i++)
  {
    if (nodes.at(i).isComment())
    {
      continue;
    }
    QDomElement component = nodes.at(i).toElement();
    if (component.tagName() == "Recorder")
    {
      this->recorderName = component.attribute("Name", "??");
      this->recorderVersion = component.attribute("Version", "??");
    }
    if (component.tagName() == "Device")
    {
      this->deviceName = component.attribute("Name", "unknown");
      this->deviceModel = component.attribute("Model", "???");
    }

    if (component.tagName() == "Time")
    {
      this->recordingTime = component.attribute("Value", "???");
    }

    if (component.tagName() == "Sample")
    {
      QDomNodeList childNodes = component.childNodes();
      for (i32 k = 0; k < childNodes.count(); k++)
      {
        QDomElement Child = childNodes.at(k).toElement();
        if (Child.tagName() == "Samplerate")
        {
          QString SR = Child.attribute("Value", "2048000");
          QString Hz = Child.attribute("Unit", "Hz");
          i32 factor = Hz == "Hz" ? 1 : (Hz == "KHz") || (Hz == "Khz") ? 1000 : 1000000;
          setSamplerate(SR.toInt() * factor);
        }
        if (Child.tagName() == "Channels")
        {
          QString Amount = Child.attribute("Amount", "2");
          QString Bits = Child.attribute("Bits", "8");
          QString Container = Child.attribute("Container", "u8");
          QString Ordering = Child.attribute("Ordering", "N/A");
          setChannels(Amount.toInt(), Bits.toInt(), Container, Ordering);
          QDomNodeList subnodes = Child.childNodes();
          i32 channelOrder = 0;
          for (i32 k = 0; k < subnodes.count(); k++)
          {
            QDomElement subChild = subnodes.at(k).toElement();
            if (subChild.tagName() == "Channel")
            {
              QString Value = subChild.attribute("Value", "I");
              addChannelOrder(channelOrder, Value);
              channelOrder++;
            }
          }
        }
      }
    }
    if (component.tagName() == "Datablocks")
    {
      // QString Count = component.attribute ("Count", "3");
      this->nrBlocks = 0;
      i32 currBlock = 0;
      QDomNodeList nodes = component.childNodes();
      for (i32 j = 0; j < nodes.count(); j++)
      {
        if (nodes.at(j).isComment())
        {
          continue;
        }
        fprintf(stderr, "Datablocks has %d siblings\n", nodes.count());
        QDomElement Child = nodes.at(j).toElement();
        if (Child.tagName() == "Datablock")
        {
          //fprintf(stderr, "weer een block\n");
          i64 Count = (Child.attribute("Count", "100")).toLongLong();
          i32 Number = (Child.attribute("Number", "10")).toInt();
          QString Unit = Child.attribute("Channel", "Channel");
          add_dataBlock(currBlock, Count, Number, Unit);
          QDomNodeList nodeList = Child.childNodes();
          for (i32 k = 0; k < nodeList.count(); k++)
          {
            if (nodeList.at(k).isComment())
            {
              continue;
            }
            QDomElement subChild = nodeList.at(k).toElement();
            if (subChild.tagName() == "Frequency")
            {
              QString unitUcStr = subChild.attribute("Unit", "Hz").toUpper();
              i32 value = (subChild.attribute("Value", "200")).toInt();
              i32 Frequency = (unitUcStr == "HZ" ? value : (unitUcStr == "KHZ" ? value * 1'000 : value * 1'000'000)); // TODO: not found is in MHz?
              add_freqtoBlock(currBlock, Frequency);
            }
            if (subChild.tagName() == "Modulation")
            {
              QString Value = subChild.attribute("Value", "DAB");
              add_modtoBlock(currBlock, Value);
            }
          }
          currBlock++;
        }
      }
      nrBlocks = currBlock;
    }
  }
  *ok = nrBlocks > 0;
  printDescriptor();
}

