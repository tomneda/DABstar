/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2020
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
 *    along with Qt-TAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include  "epg-decoder.h"
#include  "glob_defs.h"
#include  <cstdio>
#include  <QDate>

//#define	 __EPG_TRACE__
#define  EPG_TAG      0X02
#define  SERVICE_TAG    0X03

//
EpgDecoder::EpgDecoder()
{
}

static u8 bitTable[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

int EpgDecoder::getBit(u8 * v, int bitnr)
{
  int bytenr = bitnr / 8;

  bitnr = bitnr % 8;
  return (v[bytenr] & bitTable[bitnr]) != 0 ? 1 : 0;
}

u32 EpgDecoder::getBits(u8 * v, int bitnr, int length)
{
  u16 res = 0;
  for (int i = 0; i < length; i++)
  {
    res <<= 1;
    res |= getBit(v, bitnr + i);
  }
  return res;
}

int EpgDecoder::process_epg(u8 * v, int e_length, u32 SId, int subType, u32 theDay)
{
  u8 tag = v[0];
  int length = v[1];
  int index = 0;

#ifdef  __EPG_TRACE__
  fprintf (stdout, "epg module is starting to process input for %x\n",SId);
#endif
  this->SId = SId;
  this->subType = subType;
  this->julianDate = theDay;
  for (int i = 0; i < 20; i++)
  {
    stringTable[i] = " ";
  }

  if ((v[0] != EPG_TAG) && (v[0] != SERVICE_TAG))
  {
    return e_length;
  }

  if (length == 0xFE)
  {
    length = (v[2] << 8) | v[3];
    index = 4;
  }
  else if (length == 0xFF)
  {
    length = (v[2] << 16) | (v[3] << 8) | v[4];
    index = 5;
  }
  else
  {
    index = 2;
  }

#ifdef  __EPG_TRACE__
                                                                                                                          fprintf (stdout, "Length for %x is %d\n",
                                   v [0], length);
#endif
  int endPoint = index + length;
  if (tag == EPG_TAG)
  {
    while (index < endPoint)
    {
#ifdef  __EPG_TRACE__
      fprintf (stdout, "encountering node %x\n", v [index]);
#endif
      switch (v[index])
      {
      case 0x04:    // process tokenTable
        index = process_tokenTable(v, index);
        break;

      case 0x20:  // process ProgramGroups
        index = process_programGroups(v, index);
        break;

      case 0x21:  // process_schedule (v, index);
        index = process_schedule(v, index);
        break;

      case 0x05:  // obsolete
        index = process_obsolete(v, index);
        break;

      case 0x06:  // default language
        index = process_defaultLanguage(v, index);
        break;

      default:
#ifdef  __EPG_TRACE__
        fprintf (stderr, "in EPG tag %x missing\n", v [index]);
#endif
        return endPoint;
      }
    }
    return endPoint;
  }

  if (tag == SERVICE_TAG)
  {  // superfluous test
#ifdef  __EPG_TRACE__
    fprintf (stdout, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x06:  // default language
      index = process_defaultLanguage(v, index);
      break;

    case 0x26:  // ensemble
      index = process_ensemble(v, index);
      break;

    case 0x28:  // process_service
      index = process_service(v, index);
      break;

    case 0x80:  // version
      index = process_483(v, index);
      break;

    case 0x81:  // creation time
      index = process_474(v, index, nullptr);
      break;

    case 0x82:  // originator
      index = process_440(v, index);
      break;

    case 0x83:  // serviceProvider
      index = process_440(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
      fprintf (stderr, "%x unsupported tag\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_programGroups(u8 * v, int index)
{
  u8 tag = v[index];
  int length = v[index + 1];

  (void)tag;
  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  int endPoint = index + length;
#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_programGroups has length %d\n", length);
#endif
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x23:  // programGroup
      index = process_programGroup(v, index);
      break;

    case 0x80:  // Version
      index = process_483(v, index);
      break;

    case 0x81:  // creation Time
      index = process_474(v, index, nullptr);
      break;

    case 0x82:  // Originator
      index = process_440(v, index);
      break;

    default:
#ifdef  __EPG_TRACE
                                                                                                                              fprintf (stderr, "in programGroups we missed %x\n",
                                                           v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_programGroup(u8 * v, int index)
{
  int length = v[index + 1];
  progDesc p;
  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "processing program group length %d\n", length);
#endif
  int endPoint = index + length;

  p.clean();
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "Encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x10:  // shortName
      index = process_shortName(v, index, &p);
      break;

    case 0x11:  // mediumName
      index = process_mediumName(v, index, &p);
      break;

    case 0x12:  // longName
      index = process_longName(v, index, &p);
      break;

    case 0x13:  // mediaDescription
      index = process_mediaDescription(v, index, &p);
      break;

    case 0x14:  // genre
      index = process_genre(v, index, &p);
      break;

    case 0x16:  // keywords
      index = process_keyWords(v, index);
      break;

    case 0x17:  // member of
      index = process_memberOf(v, index);
      break;

    case 0x18:  // link
      index = process_link(v, index);
      break;

    case 0x20:  // programGroups
      index = process_programGroups(v, index);
      break;

    case 0x80:  // id
      index = process_471(v, index);
      break;

    case 0x81:  // shortId
      index = process_472(v, index);
      break;

    case 0x82:  // version
      index = process_483(v, index);
      break;

      //	      case 0x83:	// type
      //	         index	= process_46 (v, index);
      //	         break;

      //	      case 0x84:	// number of items
      //	         index	= process_484 (v, index);
      //	         break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "in programGroup we missed %x\n", v [index]);
#endif
      return endPoint;
    }
  }
  record(&p);
  return endPoint;
}

int EpgDecoder::process_schedule(u8 * v, int index)
{
  int length = v[index + 1];
  progDesc p;

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process schedule with length %d\n", length);
#endif

  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x1C: index = process_program(v, index, nullptr);
      break;

    case 0x24:  // scope
      index = process_scope(v, index, &p);
      break;

    case 0x80:  // version
      index = process_483(v, index);
      break;

    case 0x81:  // creation time
      index = process_474(v, index, nullptr);
      break;

    case 0x82:  // originator
      index = process_440(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
      fprintf (stderr, "in schedule missed %x\n", v [index]);
#endif
      return endPoint;
    }
  }
  //	record (&p);
  return endPoint;
}

int EpgDecoder::process_program(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];
  progDesc theElement;

  (void)p;
  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_program with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x10:  // shortName
      index = process_shortName(v, index, &theElement);
      break;

    case 0x11:  // mediumName
      index = process_mediumName(v, index, &theElement);
      break;

    case 0x12:  // longName
      index = process_longName(v, index, &theElement);
      break;

    case 0x13:  // mediaDescription
      index = process_mediaDescription(v, index, &theElement);
      break;

    case 0x14:  // genre
      index = process_genre(v, index, &theElement);
      break;

    case 0x16:  // keywords
      index = process_keyWords(v, index);
      break;

    case 0x17:  // memberOf
      index = process_memberOf(v, index);
      break;

    case 0x18:  // link
      index = process_link(v, index);
      break;

    case 0x19:  // location
      index = process_location(v, index, &theElement);
      break;

    case 0x2E:  // programmeEvent
      index = process_programmeEvent(v, index);
      break;

    case 0x36:  // inDemand
      index = process_onDemand(v, index);
      break;

    case 0x80:  // id
      index = process_471(v, index);
      break;

    case 0x81:  // shortId
      index = process_472(v, index);
      break;

    case 0x82:  // version
      index = process_483(v, index);
      break;

    case 0x83:  // recommendation
      index = process_46(v, index);
      break;

    case 0x84:  // broadcast
      index = process_46(v, index);
      break;

    case 0x86:  // xml:lang
      index = process_481(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
      fprintf (stderr, "In programme we missed %x\n", v [index]);
#endif
      return endPoint;
    }
  }
  record(&theElement);
  return endPoint;
}

int EpgDecoder::process_scope(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_scope with length %d\n", length);
#endif
  int endPoint = index + length;
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x25:  // serviceScope
      index = process_serviceScope(v, index);
      break;

    case 0x80:  // start time
      index = process_474(v, index, &p->startTime);
      break;

    case 0x81:  // stop time
      index = process_474(v, index, &p->stopTime);
      break;

    default:
#ifdef  __EPG_TRACE__
      fprintf (stderr, "%x in scope not handled\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_serviceScope(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_servicescope with length %d\n", length);
#endif
  int endPoint = index + length;
  //
  //	Untested function

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // id
      index = process_476(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
      fprintf (stderr, "in process_serviceScope %d\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_mediaDescription(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_mediaDescription with length %d\n", length);
#endif
  int endPoint = index + length;
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x1A:  // shortDescription
      index = process_shortDescription(v, index, p);
      break;

    case 0x1B:  // longDescription
      index = process_longDescription(v, index, p);
      break;

    case 0x2B:  // multiMedia
      index = process_multiMedia(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr, "%x not handled in mediaDescription\n",
                                                          v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_ensemble(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_ensemble with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x10:  // shortName
      index = process_shortName(v, index, nullptr);
      break;

    case 0x11:  // mediumName
      index = process_mediumName(v, index, nullptr);
      break;

    case 0x12:  // longName
      index = process_longName(v, index, nullptr);
      break;

    case 0x13:  // mediaDescription
      index = process_mediaDescription(v, index, nullptr);
      break;

    case 0x16:  // keywords
      index = process_keyWords(v, index);
      break;

    case 0x18:  // link
      index = process_link(v, index);
      break;

    case 0x28:  // service
      index = process_service(v, index);
      break;

    case 0x80:  // id
      index = process_4171(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "Process_ensemble: missing handle %x\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_service(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_service with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x10:  // shortName
      index = process_shortName(v, index, nullptr);
      break;

    case 0x11:  // mediumName
      index = process_mediumName(v, index, nullptr);
      break;

    case 0x12:  // longname
      index = process_longName(v, index, nullptr);
      break;

    case 0x13:  // media description
      index = process_mediaDescription(v, index, nullptr);
      break;

    case 0x14:  // genre
      index = process_genre(v, index, nullptr);
      break;

    case 0x16:  // keyWords
      index = process_keyWords(v, index);
      break;

    case 0x29:  // bearer
      index = process_bearer(v, index);
      break;

    case 0x31:  // radiodns
      index = process_radiodns(v, index);
      break;

    case 0x32:  // geolocation
      index = process_geoLocation(v, index);
      break;

    case 0x80:  // version
      index = process_483(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                       "%x not handled in service desc\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_location(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_location with length %d\n", length);
#endif
  int endPoint = index + length;
  if (p == nullptr)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "Not handled now\n");
#endif
    return endPoint;
  }

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x2C:  // time
      index = process_time(v, index, &p->startTime);
      break;

    case 0x29:  // bearer
      index = process_bearer(v, index);
      break;

    case 0x2F:  // relativeTime
      process_relativeTime(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not handled in processLocation\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_bearer(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_bearer with length %d\n", length);
#endif
  int endPoint = index + length;
  //	return endPoint;
  //
  //	Thius function is not tested
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "Encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x32:  // geolocation
      index = process_geoLocation(v, index);
      break;

    case 0x80:  // id
      index = process_476(v, index);
      break;

    case 0x82:  // url
      index = process_440(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not supported as bearer\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_geoLocation(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_geolocation with length %d\n", length);
#endif
  int endPoint = index + length;
#ifndef  __EPG_TRACE__
  return endPoint;
#endif
  //
  //	Thus function is not tested
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x33:  //  country
      index = process_country(v, index);
      break;

    case 0x34:  // Point
      index = process_point(v, index);
      break;

    case 0x35:  // polygon
      index = process_polygon(v, index);
      break;

    case 0x80:  // xml:id
      index = process_440(v, index);
      break;

    case 0x81:  // ref
      index = process_440(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not supported in geolocation\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_programmeEvent(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "in process_programEvent with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x10:  // shortName
      index = process_shortName(v, index, nullptr);
      break;

    case 0x11:  // mediumName
      index = process_mediumName(v, index, nullptr);
      break;

    case 0x12:  // longName
      index = process_longName(v, index, nullptr);
      break;

    case 0x13:  // mediaDescription
      index = process_mediaDescription(v, index, nullptr);
      break;

    case 0x14:  // genre
      index = process_genre(v, index, nullptr);
      break;

    case 0x16:  // keywords
      index = process_keyWords(v, index);
      break;

    case 0x17:  // memberOf
      index = process_memberOf(v, index);
      break;

    case 0x18:  // link
      index = process_link(v, index);
      break;

    case 0x19:  // location
      index = process_location(v, index, nullptr);
      break;

    case 0x36:  // onDemand
      index = process_onDemand(v, index);
      break;

    case 0x80:  // id
      index = process_471(v, index);
      break;

    case 0x81:  // shortId
      index = process_472(v, index);
      break;

    case 0x82:  // version
      index = process_483(v, index);
      break;

    case 0x83:  // recommendation
      index = process_46(v, index);
      break;

    case 0x84:  // broadcast
      index = process_46(v, index);
      break;

    case 0x86:  // xml:lang
      index = process_481(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not supported in programmeEvent\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_onDemand(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_onDemand with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x29:  // bearer
      index = process_bearer(v, index);
      break;

    case 0x37:  // presentation time
      index = process_presentationTime(v, index);
      break;

    case 0x38:  // acquisitionTime
      index = process_acquisitionTime(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "%x not supported in onDemand\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_genre(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_genre with length %d\n", length);
#endif
  int endPoint = index + length;
  if (p == nullptr)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "Not further processed\n");
#endif
    return endPoint;
  }

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // href
      index = process_412(v, index);
      return endPoint;

    case 0x81:  // type
      index = process_46(v, index);
      return endPoint;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not supported in genre\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_keyWords(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_keyWords with length %d\n", length);
#endif
  int endPoint = index + length;
#ifndef  __EPG_TRACE__
  return endPoint;
#endif
  //
  //	This function is not tested
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // xml:lang
      index = process_481(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "in process_keyWords with %x\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_link(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_link qwith length %d\n", length);
#endif
  int endPoint = index + length;
  //
  //	this function is not tested
  //
  int t;
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // uri
      index = process_440(v, index);
      break;

    case 0x81:  // mimeValue
      index = process_473(v, index);
      break;

    case 0x82:  // xml:lang
      index = process_481(v, index);
      break;

    case 0x83:  // description
      index = process_440(v, index);
      break;

    case 0x84:  // expiryTime
      index = process_474(v, index, &t);
      fprintf(stderr, "expiry time = %d\n", t);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not supported in link\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_shortName(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_shortName with length %d\n", length);
#endif
  int endPoint = index + length;
  if (p == nullptr)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "not further handled\n");
#endif
    return endPoint;
  }

  while (index < endPoint)
  {
    QString tt;
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:index = process_481(v, index);
      break;

    case 0x01: tt = getCData(v, index, length);
      p->shortName = tt;
      return endPoint;

    default:fprintf(stderr, "unknown in shortName %x\n", v[index]);
      p->shortName = "";
      return endPoint;
    }
  }

  p->shortName = "error";
  return endPoint;
}

int EpgDecoder::process_mediumName(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_mediumName with length %d\n", length);
#endif
  int endPoint = index + length;
  if (p == nullptr)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "Not handled in this context\n");
#endif
    return endPoint;
  }

  while (index < endPoint)
  {
    QString tt;
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80: index = process_481(v, index);
      break;

    case 0x01: tt = getCData(v, index, length);
      p->mediumName = tt;
      return endPoint;

    default: fprintf(stderr, "unknown in medium name %x\n", v[index]);
      p->mediumName = "";
      return endPoint;
    }
  }
  p->mediumName = "error";
  return endPoint;
}

int EpgDecoder::process_longName(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_longName with length %d\n", length);
#endif
  int endPoint = index + length;
  if (p == nullptr)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "Not handled in this context\n");
#endif
    return endPoint;
  }

  while (index < endPoint)
  {
    QString tt;
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80: index = process_481(v, index);
      break;

    case 0x01: tt = getCData(v, index, length);
      p->longName = tt;
      return endPoint;

    default: fprintf(stderr, "unknown in long name %x\n", v[index]);
      p->longName = "";
      return endPoint;
    }
  }
  p->longName = "error";
  return endPoint;
}

int EpgDecoder::process_shortDescription(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }
#ifdef  __EPG_TRACE__
  fprintf (stderr, "processs_shortName with length %d\n", length);
#endif
  int endPoint = index + length;

  if (p == nullptr)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "not handled in this context\n");
#endif
    return endPoint;
  }

  while (index < endPoint)
  {
    QString tt;
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80: index = process_481(v, index);
      break;

    case 0x01: tt = getCData(v, index, length);
      p->shortDescription = tt;
      return endPoint;

    default: fprintf(stderr, "unknown in shortDescription %x\n", v[index]);
      p->shortDescription = "";
      return endPoint;
    }
  }
  p->shortDescription = "error";
  return endPoint;
}

int EpgDecoder::process_longDescription(u8 * v, int index, progDesc * p)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_longDescription with length %d\n", length);
#endif
  int endPoint = index + length;
  if (p == nullptr)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "not handled in this context\n");
#endif
    return endPoint;
  }

  while (index < endPoint)
  {
    QString tt;
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80: index = process_481(v, index);
      break;

    case 0x01: tt = getCData(v, index, length);
      p->longDescription = tt;
      return endPoint;

    default: fprintf(stderr, "unknown in long Description %x\n", v[index]);
      p->longDescription = "";
      return endPoint;
    }
  }
  p->longDescription = "error";
  return endPoint;
}

int EpgDecoder::process_multiMedia(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_multiMedia with length %d\n", length);
#endif

  int endPoint = index + length;
  //
  //	This function is not tested
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // mimevalue
      index = process_473(v, index);
      break;

    case 0x81:  // xml_lang
      index = process_481(v, index);
      break;

    case 0x82:  // url
      index = process_440(v, index);
      break;

    case 0x83:  // type
      index = process_46(v, index);
      break;

    case 0x84:  // width
      index = process_485(v, index);
      break;

    case 0x85: index = process_485(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "%x not supported in multimedia\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_radiodns(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_radiodns with length %d\n", length);
#endif
  int endPoint = index + length;
  //
  //	The function is untested
  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // fqdn
      index = process_440(v, index);
      break;

    case 0x81:  // serviceIdentifier
      index = process_440(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "%x has no support in radiodns\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_time(u8 * v, int index, int * t)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_time with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // time
      index = process_474(v, index, t);
      break;

    case 0x81:  // duration
      index = process_475(v, index);
      break;

    case 0x82:  // actualTime
      index = process_474(v, index, nullptr);
      break;

    case 0x83:  // actual duration
      index = process_475(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not supported in timespec\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_relativeTime(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_relativeTime with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // time
      index = process_474(v, index, nullptr);
      break;

    case 0x81:  // duration
      index = process_475(v, index);
      break;

    case 0x82:  // actualTime
      index = process_474(v, index, nullptr);
      break;

    case 0x83:  // actual duration
      index = process_475(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "%x not supported in relative time\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_memberOf(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_memberOf with length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // id
      index = process_471(v, index);
      break;

    case 0x81:  // shortId
      index = process_472(v, index);
      break;

    case 0x82:  // Index
      index = process_482(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "%x not supported in member-of\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_presentationTime(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_presentationTime with length %d\n",  length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // start
      index = process_474(v, index, nullptr);
      break;

    case 0x81:  // end
      index = process_474(v, index, nullptr);
      break;

    case 0x82:  // duration
      index = process_475(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                  "%x not supported in presentation time\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_acquisitionTime(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }
#ifdef  __EPG_TRACE__
  fprintf (stderr, "process_acquisitionTime length %d\n", length);
#endif
  int endPoint = index + length;

  while (index < endPoint)
  {
#ifdef  __EPG_TRACE__
    fprintf (stderr, "encountering node %x\n", v [index]);
#endif
    switch (v[index])
    {
    case 0x80:  // start
      index = process_474(v, index, nullptr);
      break;

    case 0x81:  // end
      index = process_474(v, index, nullptr);
      break;

    case 0x82:  // duration
      index = process_475(v, index);
      break;

    default:
#ifdef  __EPG_TRACE__
                                                                                                                              fprintf (stderr,
                      "%x not supported in acquisition time\n", v [index]);
#endif
      return endPoint;
    }
  }
  return endPoint;
}

int EpgDecoder::process_country(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
                                                                                                                          fprintf (stderr, "process_country length %d (not further handled)\n",
                                                     length);
#endif
  int endPoint = index + length;
  return endPoint;
}

int EpgDecoder::process_point(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
                                                                                                                          fprintf (stderr, "process_point length %d (not further handled)\n",
                                                     length);
#endif

  int endPoint = index + length;
  return endPoint;
}

int EpgDecoder::process_polygon(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

#ifdef  __EPG_TRACE__
                                                                                                                          fprintf (stderr, "process_polygon length %d (not further handled)\n",
                                                     length);
#endif

  int endPoint = index + length;
  return endPoint;
}

//
//////////////////////////////////////////////////////////////////////////
//
//	Attribute handlers
//
/////////////////////////////////////////////////////////////////////////////
//
int EpgDecoder::process_412(u8 * v, int index)
{
  int length = v[index + 1];
  //
  //	This function is incomplete and its value should be ignored
  return index + length;
}

int EpgDecoder::process_440(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

int EpgDecoder::process_46(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

int EpgDecoder::process_471(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

int EpgDecoder::process_472(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

int EpgDecoder::process_473(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

//
//	ETSI TS 102 371: 4.7.4 time point
int EpgDecoder::process_474(u8 * v, int index, int * t)
{
  int length = v[index + 1];
  u8 ltoFlag;
  u8 utcFlag;
  int hours;
  int minutes;
  int ltoBase;


  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }
  int endPoint = index + length;

  if (t == nullptr)
  {
    return endPoint;
  }


  u32 date = getBits(v, 8 * index + 1, 17);
  if (julianDate != date)
  {
    //	   fprintf (stderr, "%d vs schedule date %d\n",
    //	                       julianDate, date);
    *t = -1;
    return endPoint;
  }
  //
  //	we need to know whether it is today or not
  ltoFlag = getBit(v, 8 * index + 19);
  utcFlag = getBit(v, 8 * index + 20);
  ltoBase = utcFlag == 1 ? 48 : 32;

  hours = getBits(v, 8 * index + 21, 5);
  minutes = getBits(v, 8 * index + 26, 6);

  if (ltoFlag)
  {
    u16 halfHours = getBits(v, 8 * index + ltoBase, 8);
    if (halfHours & 0x20)
    {
      halfHours = -halfHours & 0x1F;
    }
    else
    {
      halfHours = halfHours & 0x1F;
    }
    minutes += halfHours * 30;
  }
  *t = hours * 60 + minutes;
  return index + length;
}

//
//	ETSI TS 102 371: 4.7.5 Duration type
int EpgDecoder::process_475(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  //	int duration	= (v [index] << 8) | v [index + 1];
  //	fprintf (stderr, "duration %d (%d)\n", duration / 60, duration % 60);
  return index + length;
}

int EpgDecoder::process_476(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;  // TODO: early return or wrong (missing!) else braces?

  u8 ensFlag = getBit(v, 8 * index + 1);
  u8 sidFlag = getBit(v, 8 * index + 3);
  // TODO: do not dare yet make the clearer
  if (ensFlag == 1)
  {
    if (sidFlag == 0)
    {
      fprintf(stderr, "SId = %X\n", getBits(v, 8 * index + 32, 16));
    }
  }
  else if (ensFlag == 0)
  {
    if (sidFlag == 0)
    {
      fprintf(stderr, "SId = %X\n", getBits(v, 8 * index + 8, 16));
    }
  }
  return index + length;
}

int EpgDecoder::process_481(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }
  //	fprintf (stderr, "481 returns %d\n", index + length);
  return index + length;
}

int EpgDecoder::process_482(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

int EpgDecoder::process_483(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  int numbers = (v[index] << 8) | v[index + 1];
  fprintf(stderr, "Version %d\n", numbers);
  return index + length;
}

int EpgDecoder::process_484(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  //	int numbers = (v [index] << 8) | v [index + 1];
  //	fprintf (stderr, "484 says %d elements\n", numbers);
  return index + length;
}

int EpgDecoder::process_485(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

int EpgDecoder::process_4171(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

int EpgDecoder::process_tokenTable(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  int endPoint = index + length;
  //	token table element, section 4.9
  while (index < endPoint)
  {
    index = process_token(v, index);
  }
  return endPoint;
}

int EpgDecoder::process_token(u8 * v, int index)
{
  int tag = v[index];
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  auto * const text = make_vla(char, length + 1);
  for (int i = 0; i < length; i++)
  {
    text[i] = v[index + i];
  }
  text[length] = 0;
  if (0 <= tag && tag <= 16)
  {
    //	   fprintf (stderr, "adding %s to %d in stringTable\n", text, tag);
    stringTable[tag] = QString::fromUtf8(text);
  }
  return index + length;
}

int EpgDecoder::process_defaultLanguage(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  //	fprintf (stderr, "default language with length %d\n", length);
  return index + length;
}

int EpgDecoder::process_obsolete(u8 * v, int index)
{
  int length = v[index + 1];

  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  return index + length;
}

void EpgDecoder::record(progDesc * theElement)
{
  if (theElement->startTime == -1)
  {
    return;
  }

  if ((theElement->mediumName == QString("")) && (theElement->shortName == QString("")) && (theElement->longName == QString("")))
  {
    return;
  }

  emit signal_set_epg_data(this->SId,
                           theElement->startTime,
                           theElement->longName != QString("") ? theElement->longName : theElement->mediumName != QString("")
                                                                                        ? theElement->mediumName
                                                                                        : theElement->shortName,
                           theElement->shortDescription);
}

QString EpgDecoder::getCData(u8 * v, int index, int /*eLength*/)
{
  int length;

  if (v[index] != 0x1)
  {
    return "";
  }

  if (v[index + 1] == 1)
  {
    if (v[index + 2] < 16)
    {
      return stringTable[v[index + 2]];
    }
    else
    {
      return "";
    }
  }

  length = v[index + 1];
  if (length == 0XFE)
  {
    length = (v[index + 2] << 8) | v[index + 3];
    index += 4;
  }
  else if (length == 0XFF)
  {
    length = (v[index + 2] << 16) | (v[index + 3] << 8) | v[index + 4];
    index += 5;
  }
  else
  {
    index += 2;
  }

  std::vector<char> text;
  for (int i = 0; i < length; i++)
  {
    if (v[index + i] < 16)
    {
      for (int k = 0; stringTable[v[index + i]].toUtf8().data()[k] != 0; k++)
      {
        char c = stringTable[v[index + i]].toUtf8().data()[k];
        text.push_back(c);
      }
    }
    else
    {
      text.push_back(v[index + i]);
    }
  }

  text.push_back(0);
  QString tt = QString::fromUtf8(text.data());
  return tt;
}
