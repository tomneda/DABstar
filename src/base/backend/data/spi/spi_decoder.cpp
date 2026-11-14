/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "spi_decoder.h"
#include <QTimeZone>

namespace
{
// ETSI TS 102 371, annex D: element tags
namespace ETag
{
constexpr u8 CData              = 0x01;
constexpr u8 Epg                = 0x02;
constexpr u8 ServiceInformation = 0x03;
constexpr u8 TokenTable         = 0x04;
constexpr u8 DefaultLanguage    = 0x06;
constexpr u8 ShortName          = 0x10;
constexpr u8 MediumName         = 0x11;
constexpr u8 LongName           = 0x12;
constexpr u8 MediaDescription   = 0x13;
constexpr u8 Genre              = 0x14;
constexpr u8 Location           = 0x19;
constexpr u8 ShortDescription   = 0x1A;
constexpr u8 LongDescription    = 0x1B;
constexpr u8 Programme          = 0x1C;
constexpr u8 Schedule           = 0x21;
constexpr u8 Scope              = 0x24;
constexpr u8 ServiceScope       = 0x25;
constexpr u8 Ensemble           = 0x26;
constexpr u8 Service            = 0x28;
constexpr u8 ServiceBearer      = 0x29; // bearer as child of service (former serviceID)
constexpr u8 Multimedia         = 0x2B;
constexpr u8 Time               = 0x2C;
constexpr u8 LocationBearer     = 0x2D; // bearer as child of location or onDemand
}

// ETSI TS 102 371, annex E: attribute tags (only unique within the parent element)
constexpr u8 cAttr0 = 0x80;
constexpr u8 cAttr1 = 0x81;
constexpr u8 cAttr2 = 0x82;
constexpr u8 cAttr3 = 0x83;
constexpr u8 cAttr4 = 0x84;
constexpr u8 cAttr5 = 0x85;

// ETSI TS 102 371, 4.12: classification schemes
constexpr std::array<const char*, 9> cClassificationScheme =
{
  nullptr, "IntentionCS", "FormatCS", "ContentCS", "IntendedAudienceCS", "OriginationCS", "ContentAlertCS", "MediaTypeCS", "AtmosphereCS"
};

constexpr u32 cMjdToJulianDay = 2400001; // QDate::fromJulianDay(2400001) is MJD 0 (1858-11-17)
}

bool SpiDecoder::TlvReader::next(STlv& oTlv)
{
  if (mError || mPos >= mData.size())
  {
    return false;
  }

  if (mData.size() - mPos < 2)
  {
    mError = true;
    return false;
  }

  oTlv.tag = mData[mPos];
  usize length = mData[mPos + 1];
  usize headerSize = 2;

  if (length == 0xFE)
  {
    if (mData.size() - mPos < 4)
    {
      mError = true;
      return false;
    }
    length = ((usize)mData[mPos + 2] << 8) | mData[mPos + 3];
    headerSize = 4;
  }
  else if (length == 0xFF)
  {
    if (mData.size() - mPos < 5)
    {
      mError = true;
      return false;
    }
    length = ((usize)mData[mPos + 2] << 16) | ((usize)mData[mPos + 3] << 8) | mData[mPos + 4];
    headerSize = 5;
  }

  if (mData.size() - mPos - headerSize < length)
  {
    mError = true;
    return false;
  }

  oTlv.value = mData.subspan(mPos + headerSize, length);
  mPos += headerSize + length;
  return true;
}

bool SpiDecoder::decode(const std::span<const u8>& iData, Spi::SResult& oResult)
{
  oResult = {};
  mTokenTable = {};
  mError = false;

  TlvReader reader(iData);
  STlv top;

  if (!reader.next(top))
  {
    return false;
  }

  TlvReader childReader(top.value);

  if (top.tag == ETag::Epg)
  {
    oResult.type = Spi::EObjectType::Epg;
  }
  else if (top.tag == ETag::ServiceInformation)
  {
    oResult.type = Spi::EObjectType::ServiceInformation;
  }
  else
  {
    return false;
  }

  _decode_top_level_children(childReader, oResult);
  _check_reader(childReader);

  return !mError;
}

void SpiDecoder::_decode_top_level_children(TlvReader& ioReader, Spi::SResult& ioResult)
{
  const bool isSI = (ioResult.type == Spi::EObjectType::ServiceInformation);
  Spi::SServiceInformation& si = ioResult.serviceInformation;
  STlv tlv;

  while (ioReader.next(tlv))
  {
    switch (tlv.tag)
    {
    case ETag::TokenTable:
      _decode_token_table(tlv.value);
      break;

    case ETag::DefaultLanguage:
      ioResult.defaultLanguage = _decode_string(tlv.value);
      break;

    case ETag::Schedule:
      if (!isSI)
      {
        _decode_schedule(tlv.value, ioResult.schedules.emplace_back());
      }
      break;

    case ETag::Ensemble:
      if (isSI)
      {
        _decode_ensemble(tlv.value, si);
      }
      break;

    case ETag::Service: // services without ensemble element (DRM)
      if (isSI)
      {
        _decode_service(tlv.value, si.services.emplace_back());
      }
      break;

    case cAttr0:
      if (isSI)
      {
        si.version = (u16)_get_uint(tlv.value);
      }
      break;

    case cAttr1:
      if (isSI)
      {
        i32 lto = 0;
        decode_time_point(tlv.value, si.creationTime, lto);
      }
      break;

    case cAttr2:
      if (isSI)
      {
        si.originator = _decode_string(tlv.value);
      }
      break;

    case cAttr3:
      if (isSI)
      {
        si.serviceProvider = _decode_string(tlv.value);
      }
      break;

    default: // e.g. programmeGroups (group information) or unknown elements are skipped
      break;
    }
  }
}

void SpiDecoder::_decode_token_table(const std::span<const u8>& iData)
{
  // ETSI TS 102 371, 4.9.1: tokens use an 8 bit length field only
  usize pos = 0;

  while (pos + 2 <= iData.size())
  {
    const u8 tokenTag = iData[pos];
    const usize tokenLength = iData[pos + 1];
    pos += 2;

    if (pos + tokenLength > iData.size())
    {
      mError = true;
      return;
    }

    if (tokenTag < cTokenTableSize)
    {
      mTokenTable[tokenTag] = QByteArray(reinterpret_cast<const char*>(iData.data() + pos), (qsizetype)tokenLength);
    }
    pos += tokenLength;
  }

  if (pos != iData.size())
  {
    mError = true;
  }
}

void SpiDecoder::_decode_schedule(const std::span<const u8>& iData, Spi::SSchedule& oSchedule)
{
  TlvReader reader(iData);
  STlv tlv;
  i32 lto = 0;

  while (reader.next(tlv))
  {
    switch (tlv.tag)
    {
    case ETag::Scope:     _decode_scope(tlv.value, oSchedule); break;
    case ETag::Programme: _decode_programme(tlv.value, oSchedule.programmes.emplace_back()); break;
    case cAttr0:          oSchedule.version = (u16)_get_uint(tlv.value); break;
    case cAttr1:          decode_time_point(tlv.value, oSchedule.creationTime, lto); break;
    case cAttr2:          oSchedule.originator = _decode_string(tlv.value); break;
    default: break;
    }
  }
  _check_reader(reader);
}

void SpiDecoder::_decode_scope(const std::span<const u8>& iData, Spi::SSchedule& ioSchedule)
{
  TlvReader reader(iData);
  STlv tlv;
  i32 lto = 0;

  while (reader.next(tlv))
  {
    switch (tlv.tag)
    {
    case cAttr0: decode_time_point(tlv.value, ioSchedule.scopeStart, lto); break;
    case cAttr1: decode_time_point(tlv.value, ioSchedule.scopeStop, lto); break;
    case ETag::ServiceScope:
    {
      Spi::SBearer bearer;
      if (_decode_bearer_element(tlv.value, bearer))
      {
        ioSchedule.serviceScopes.emplace_back(bearer);
      }
      break;
    }
    default: break;
    }
  }
  _check_reader(reader);
}

void SpiDecoder::_decode_programme(const std::span<const u8>& iData, Spi::SProgramme& oProgramme)
{
  TlvReader reader(iData);
  STlv tlv;

  while (reader.next(tlv))
  {
    if (_decode_name_element(tlv, oProgramme.names))
    {
      continue;
    }

    switch (tlv.tag)
    {
    case ETag::MediaDescription: _decode_media_description(tlv.value, oProgramme.shortDescription, oProgramme.longDescription, oProgramme.multimedia); break;
    case ETag::Location:         _decode_location(tlv.value, oProgramme); break;
    case ETag::Genre:
    {
      const QString genre = _decode_genre_element(tlv.value);
      if (!genre.isEmpty())
      {
        oProgramme.genres.emplace_back(genre);
      }
      break;
    }
    case cAttr0: oProgramme.crid = _decode_string(tlv.value); break;
    case cAttr1: oProgramme.shortId = _get_uint(tlv.value); break;
    case cAttr2: oProgramme.version = (u16)_get_uint(tlv.value); break;
    case cAttr3: oProgramme.isRecommended = (!tlv.value.empty() && tlv.value[0] == 0x02); break;
    case cAttr4: oProgramme.isOnAir = (tlv.value.empty() || tlv.value[0] != 0x02); break;
    default: break; // e.g. programmeEvent, onDemand, keywords, memberOf, link
    }
  }
  _check_reader(reader);
}

void SpiDecoder::_decode_location(const std::span<const u8>& iData, Spi::SProgramme& ioProgramme)
{
  TlvReader reader(iData);
  STlv tlv;

  while (reader.next(tlv))
  {
    switch (tlv.tag)
    {
    case ETag::Time:
      _decode_time(tlv.value, ioProgramme);
      break;

    case ETag::LocationBearer:
    {
      Spi::SBearer bearer;
      if (_decode_bearer_element(tlv.value, bearer))
      {
        ioProgramme.bearers.emplace_back(bearer);
      }
      break;
    }
    default: break; // e.g. relativeTime
    }
  }
  _check_reader(reader);
}

void SpiDecoder::_decode_time(const std::span<const u8>& iData, Spi::SProgramme& ioProgramme)
{
  TlvReader reader(iData);
  STlv tlv;
  QDateTime time;
  QDateTime actualTime;
  i32 lto = 0;
  i32 actualLto = 0;
  i32 duration = -1;
  i32 actualDuration = -1;

  while (reader.next(tlv))
  {
    switch (tlv.tag)
    {
    case cAttr0: decode_time_point(tlv.value, time, lto); break;
    case cAttr1: duration = (tlv.value.size() >= 2 ? (i32)_get_uint(tlv.value.first(2)) : -1); break;
    case cAttr2: decode_time_point(tlv.value, actualTime, actualLto); break;
    case cAttr3: actualDuration = (tlv.value.size() >= 2 ? (i32)_get_uint(tlv.value.first(2)) : -1); break;
    default: break;
    }
  }
  _check_reader(reader);

  // the time elements of a programme are sorted chronologically (ETSI TS 102 371, 5.2.2), so take the first one
  if (!ioProgramme.startTime.isValid() && time.isValid())
  {
    ioProgramme.startTime = time;
    ioProgramme.ltoMinutes = lto;
    ioProgramme.durationSec = duration;
    ioProgramme.actualTime = actualTime;
    ioProgramme.actualDurationSec = actualDuration;
  }
}

void SpiDecoder::_decode_ensemble(const std::span<const u8>& iData, Spi::SServiceInformation& ioSI)
{
  TlvReader reader(iData);
  STlv tlv;

  while (reader.next(tlv))
  {
    if (_decode_name_element(tlv, ioSI.ensembleNames))
    {
      continue;
    }

    switch (tlv.tag)
    {
    case ETag::Service:
      _decode_service(tlv.value, ioSI.services.emplace_back());
      break;

    case cAttr0: // ETSI TS 102 371, 4.17.1: ECC (8 bit) and EId (16 bit)
      if (tlv.value.size() >= 3)
      {
        ioSI.hasEnsemble = true;
        ioSI.ECC = tlv.value[0];
        ioSI.EId = (u16)_get_uint(tlv.value.subspan(1, 2));
      }
      break;

    default: break;
    }
  }
  _check_reader(reader);
}

void SpiDecoder::_decode_service(const std::span<const u8>& iData, Spi::SService& oService)
{
  TlvReader reader(iData);
  STlv tlv;

  while (reader.next(tlv))
  {
    if (_decode_name_element(tlv, oService.names))
    {
      continue;
    }

    switch (tlv.tag)
    {
    case ETag::MediaDescription: _decode_media_description(tlv.value, oService.shortDescription, oService.longDescription, oService.multimedia); break;
    case ETag::Genre:
    {
      const QString genre = _decode_genre_element(tlv.value);
      if (!genre.isEmpty())
      {
        oService.genres.emplace_back(genre);
      }
      break;
    }
    case ETag::ServiceBearer:
    case ETag::LocationBearer:
    {
      Spi::SBearer bearer;
      if (_decode_bearer_element(tlv.value, bearer))
      {
        oService.bearers.emplace_back(bearer);
      }
      break;
    }
    case cAttr0: oService.version = (u16)_get_uint(tlv.value); break;
    default: break; // e.g. keywords, link, radiodns, geolocation
    }
  }
  _check_reader(reader);
}

void SpiDecoder::_decode_media_description(const std::span<const u8>& iData, QString& ioShortDescr, QString& ioLongDescr, std::vector<Spi::SMultimedia>& ioMultimedia)
{
  TlvReader reader(iData);
  STlv tlv;

  while (reader.next(tlv))
  {
    switch (tlv.tag)
    {
    case ETag::ShortDescription:
      if (ioShortDescr.isEmpty())
      {
        ioShortDescr = _decode_cdata_of_element(tlv.value);
      }
      break;

    case ETag::LongDescription:
      if (ioLongDescr.isEmpty())
      {
        ioLongDescr = _decode_cdata_of_element(tlv.value);
      }
      break;

    case ETag::Multimedia:
      _decode_multimedia(tlv.value, ioMultimedia.emplace_back());
      break;

    default: break;
    }
  }
  _check_reader(reader);
}

void SpiDecoder::_decode_multimedia(const std::span<const u8>& iData, Spi::SMultimedia& oMultimedia)
{
  TlvReader reader(iData);
  STlv tlv;

  while (reader.next(tlv))
  {
    switch (tlv.tag)
    {
    case cAttr0: oMultimedia.mimeValue = _decode_string(tlv.value); break;
    case cAttr2: oMultimedia.url = _decode_string(tlv.value); break;
    case cAttr3: oMultimedia.type = (tlv.value.empty() ? 0 : tlv.value[0]); break;
    case cAttr4: oMultimedia.width = (u16)_get_uint(tlv.value); break;
    case cAttr5: oMultimedia.height = (u16)_get_uint(tlv.value); break;
    default: break; // e.g. xml:lang
    }
  }
  _check_reader(reader);
}

bool SpiDecoder::_decode_bearer_element(const std::span<const u8>& iData, Spi::SBearer& oBearer)
{
  TlvReader reader(iData);
  STlv tlv;
  bool found = false;

  while (reader.next(tlv))
  {
    if (tlv.tag == cAttr0 && decode_bearer(tlv.value, oBearer)) // the url (0x82) of the http: domain is not of interest here
    {
      found = true;
    }
  }
  _check_reader(reader);
  return found;
}

QString SpiDecoder::_decode_genre_element(const std::span<const u8>& iData)
{
  TlvReader reader(iData);
  STlv tlv;
  QString genre;

  while (reader.next(tlv))
  {
    if (tlv.tag == cAttr0)
    {
      genre = decode_genre_href(tlv.value);
    }
  }
  _check_reader(reader);
  return genre;
}

QString SpiDecoder::_decode_cdata_of_element(const std::span<const u8>& iData)
{
  TlvReader reader(iData);
  STlv tlv;
  QString text;

  while (reader.next(tlv))
  {
    if (tlv.tag == ETag::CData)
    {
      text += _decode_string(tlv.value);
    }
  }
  _check_reader(reader);
  return text;
}

bool SpiDecoder::_decode_name_element(const STlv& iTlv, Spi::SNames& ioNames)
{
  QString* pName = nullptr;

  switch (iTlv.tag)
  {
  case ETag::ShortName:  pName = &ioNames.shortName; break;
  case ETag::MediumName: pName = &ioNames.mediumName; break;
  case ETag::LongName:   pName = &ioNames.longName; break;
  default: return false;
  }

  const QString name = _decode_cdata_of_element(iTlv.value);

  if (pName->isEmpty()) // there could be names in several languages, keep the first one
  {
    *pName = name;
  }
  return true;
}

QString SpiDecoder::_decode_string(const std::span<const u8>& iData) const
{
  QByteArray utf8;
  utf8.reserve((qsizetype)iData.size());

  for (const u8 b : iData)
  {
    if (b > 0 && b < cTokenTableSize && b != 0x09 && b != 0x0A && b != 0x0D) // a token tag (ETSI TS 102 371, 4.9.1)
    {
      utf8.append(mTokenTable[b]);
    }
    else
    {
      utf8.append((char)b);
    }
  }

  return QString::fromUtf8(utf8);
}

void SpiDecoder::_check_reader(const TlvReader& iReader)
{
  if (iReader.has_error())
  {
    mError = true;
  }
}

u32 SpiDecoder::_get_uint(const std::span<const u8>& iData)
{
  u32 value = 0;
  for (const u8 b : iData.first(std::min<usize>(iData.size(), 4)))
  {
    value = (value << 8) | b;
  }
  return value;
}

bool SpiDecoder::decode_time_point(const std::span<const u8>& iData, QDateTime& oUtc, i32& oLtoMinutes)
{
  if (iData.size() < 4)
  {
    return false;
  }

  // Rfa(1) Date(17) Rfa(1) LTO flag(1) UTC flag(1) Hours(5) Minutes(6) [Seconds(6) Rfa(10)] [Rfa(2) Sign(1) Half-hours(5)]
  const u32 word = _get_uint(iData.first(4));
  const u32 mjd = (word >> 14) & 0x1FFFF;
  const bool ltoFlag = ((word >> 12) & 0x1) != 0;
  const bool utcLongForm = ((word >> 11) & 0x1) != 0;
  const i32 hours = (i32)((word >> 6) & 0x1F);
  const i32 minutes = (i32)(word & 0x3F);
  i32 seconds = 0;
  usize idx = 4;

  if (utcLongForm)
  {
    if (iData.size() < 6)
    {
      return false;
    }
    seconds = iData[4] >> 2;
    idx = 6;
  }

  i32 lto = 0;
  if (ltoFlag)
  {
    if (iData.size() < idx + 1)
    {
      return false;
    }
    const u8 ltoByte = iData[idx];
    lto = (ltoByte & 0x1F) * 30;
    if ((ltoByte & 0x20) != 0)
    {
      lto = -lto;
    }
  }

  if (hours > 23 || minutes > 59 || seconds > 59)
  {
    return false;
  }

  oUtc = QDateTime(QDate::fromJulianDay(mjd + cMjdToJulianDay), QTime(hours, minutes, seconds), QTimeZone::UTC);
  oLtoMinutes = lto;
  return oUtc.isValid();
}

bool SpiDecoder::decode_bearer(const std::span<const u8>& iData, Spi::SBearer& oBearer)
{
  if (iData.empty())
  {
    return false;
  }

  // Rfa(1) Ens flag(1) X-PAD flag(1) SId flag(1) SCIdS(4) [ECC(8) EId(16)] SId(16 or 32) [Rfa(3) X-PAD AppTy(5)]
  const u8 flags = iData[0];
  Spi::SBearer bearer;
  bearer.hasEnsemble = (flags & 0x40) != 0;
  bearer.isXPad = (flags & 0x20) != 0;
  const bool longSId = (flags & 0x10) != 0;
  bearer.SCIdS = flags & 0x0F;

  usize idx = 1;

  if (bearer.hasEnsemble)
  {
    if (iData.size() < idx + 3)
    {
      return false;
    }
    bearer.ECC = iData[idx];
    bearer.EId = (u16)_get_uint(iData.subspan(idx + 1, 2));
    idx += 3;
  }

  const usize sidSize = (longSId ? 4 : 2);
  if (iData.size() < idx + sidSize)
  {
    return false;
  }
  bearer.SId = _get_uint(iData.subspan(idx, sidSize));

  oBearer = bearer;
  return true;
}

QString SpiDecoder::decode_genre_href(const std::span<const u8>& iData)
{
  if (iData.empty())
  {
    return {};
  }

  const u8 cs = iData[0] & 0x0F;

  if (cs < 1 || cs >= cClassificationScheme.size()) // undefined classification schemes shall be ignored
  {
    return {};
  }

  QString href = QString("urn:tva:metadata:cs:%1:2005:%2").arg(cClassificationScheme[cs]).arg(cs);

  for (const u8 level : iData.subspan(1, std::min<usize>(iData.size() - 1, 3)))
  {
    href += QString(".%1").arg(level);
  }
  return href;
}
