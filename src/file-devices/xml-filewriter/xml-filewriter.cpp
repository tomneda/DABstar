#
/*
 *    Copyright (C) 2014 .. 2019
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB (formerly SDR-J, JSDR).
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
#include	"xml-filewriter.h"
#include	<cstdio>
#include	<time.h>

struct kort_woord {
  u8 byte_1;
  u8 byte_2;
};

XmlFileWriter::XmlFileWriter(FILE *f,
                             i32 nrBits,
                             QString container,
                             i32 sampleRate,
                             i32 frequency,
                             QString deviceName,
                             QString deviceModel,
                             QString recorderVersion)
{
  u8 t	= 0;
  xmlFile	= f;
  this->nrBits = nrBits;
  this->container = container;
  this->sampleRate = sampleRate;
  this->frequency = frequency;
  this->deviceName = deviceName;
  this->deviceModel = deviceModel;
  this->recorderVersion = recorderVersion;

  for (i32 i = 0; i < 2048; i ++)
    fwrite(&t, 1, 1, f);
  i16 testWord = 0xFF;

  struct kort_woord *p = (struct kort_woord *)(&testWord);
  if (p->byte_1 == 0xFF)
    byteOrder = "LSB";
  else
    byteOrder = "MSB";
  nrElements = 0;
}

XmlFileWriter::~XmlFileWriter()
{
}

void XmlFileWriter::computeHeader()
{
  QString s;
  QString	topLine = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
  if (xmlFile == nullptr)
    return;
  s = create_xmltree();
  fseek(xmlFile, 0, SEEK_SET);
  fprintf(xmlFile, "%s", topLine.toLatin1().data());
  char * cs = s.toLatin1().data();
  i32 len = strlen (cs);
  fwrite (cs, 1, len, xmlFile);
}

#define	BLOCK_SIZE	8192
static i16 buffer_int16[BLOCK_SIZE];
static i32 bufferP_int16 = 0;
void XmlFileWriter::add(std::complex<i16> * data, i32 count)
{
  nrElements	+= 2 * count;
  for (i32 i = 0; i < count; i ++)
  {
    buffer_int16 [bufferP_int16 ++] = real (data [i]);
    buffer_int16 [bufferP_int16 ++] = imag (data [i]);
    if (bufferP_int16 >= BLOCK_SIZE)
    {
      fwrite (buffer_int16, sizeof (i16), BLOCK_SIZE, xmlFile);
      bufferP_int16 = 0;
    }
  }
}

static u8 buffer_uint8[BLOCK_SIZE];
static i32 bufferP_uint8	= 0;
void XmlFileWriter::add(std::complex<u8> * data, i32 count)
{
  nrElements	+= 2 * count;
  for (i32 i = 0; i < count; i ++)
  {
    buffer_uint8[bufferP_uint8 ++] = real(data[i]);
    buffer_uint8[bufferP_uint8 ++] = imag(data[i]);
    if (bufferP_uint8 >= BLOCK_SIZE)
    {
      fwrite(buffer_uint8, sizeof (u8), BLOCK_SIZE, xmlFile);
      bufferP_uint8 = 0;
    }
  }
}

static i8 buffer_int8[BLOCK_SIZE];
static i32 bufferP_int8	= 0;
void XmlFileWriter::add(std::complex<i8> * data, i32 count)
{
  nrElements	+= 2 * count;
  for (i32 i = 0; i < count; i ++)
  {
    buffer_int8[bufferP_int8 ++] = real(data[i]);
    buffer_int8[bufferP_int8 ++] = imag(data[i]);
    if (bufferP_int8 >= BLOCK_SIZE) {
      fwrite (buffer_int8, sizeof (i8), BLOCK_SIZE, xmlFile);
      bufferP_int8 = 0;
    }
  }
}

QString	XmlFileWriter::create_xmltree()
{
  QDomDocument theTree;
  QDomElement root = theTree.createElement("SDR");

  time_t rawtime;
  struct tm *timeinfo;
  time (&rawtime);
  timeinfo = localtime(&rawtime);

  theTree. appendChild(root);
  QDomElement theRecorder = theTree.createElement("Recorder");
  theRecorder.setAttribute("Name", PRJ_NAME);
  theRecorder.setAttribute("Version", recorderVersion);
  root.appendChild(theRecorder);
  QDomElement theDevice = theTree.createElement("Device");
  theDevice.setAttribute("Name", deviceName);
  theDevice.setAttribute("Model", deviceModel);
  root.appendChild(theDevice);
  QDomElement theTime = theTree. createElement("Time");
  theTime.setAttribute("Unit", "UTC");
  char help[256];
  strcpy(help, asctime (timeinfo));
  help[strlen(help)] = 0;	// get rid of \n
  theTime.setAttribute("Value", QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss"));
  root. appendChild(theTime);
  QDomElement theSample = theTree.createElement("Sample");
  QDomElement theRate = theTree.createElement("Samplerate");
  theRate.setAttribute("Unit", "Hz");
  theRate.setAttribute("Value", QString::number (sampleRate));
  theSample.appendChild (theRate);
  QDomElement theChannels = theTree. createElement("Channels");
  theChannels.setAttribute("Bits", QString::number(nrBits));
  theChannels.setAttribute("Container", container);
  theChannels.setAttribute("Ordering", byteOrder);
  QDomElement I_Channel = theTree. createElement ("Channel");
  I_Channel.setAttribute("Value", "I");
  theChannels.appendChild(I_Channel);
  QDomElement Q_Channel = theTree.createElement ("Channel");
  Q_Channel.setAttribute("Value", "Q");
  theChannels.appendChild(Q_Channel);
  theSample.appendChild(theChannels);
  root.appendChild(theSample);

  QDomElement theDataBlocks = theTree. createElement("Datablocks");
  QDomElement theDataBlock = theTree. createElement("Datablock");
  theDataBlock.setAttribute("Count", QString::number(nrElements));
  theDataBlock.setAttribute("Number", "1");
  theDataBlock.setAttribute("Unit",  "Channel");
  QDomElement theFrequency = theTree.createElement("Frequency");
  theFrequency.setAttribute("Value", QString::number(frequency / 1000));
  theFrequency.setAttribute("Unit", "kHz");
  theDataBlock.appendChild(theFrequency);
  QDomElement theModulation = theTree. createElement("Modulation");
  theModulation.setAttribute("Value", "DAB");
  theDataBlock.appendChild(theModulation);
  theDataBlocks.appendChild(theDataBlock);
  root. appendChild(theDataBlocks);

  return theTree.toString();
}
