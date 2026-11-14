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
#pragma once

// Decoder of the binary encoded "Service and Programme Information" (SPI, formerly EPG), see ETSI TS 102 371 V3.2.1.
// It has no dependencies to the GUI or the DAB backend, so it can be tested on its own.

#include "spi_types.h"
#include <array>
#include <span>
#include <QByteArray>

class SpiDecoder
{
public:
  SpiDecoder() = default;
  ~SpiDecoder() = default;

  // Decodes one binary object (the body of one MOT object). Returns false if the data contain no known top-level element
  // or are corrupt. Elements which are not known are skipped. Data decoded until an error occurred are kept in oResult.
  bool decode(const std::span<const u8>& iData, Spi::SResult& oResult);

  // ETSI TS 102 371, 4.7.4: timePoint type, the result is in UTC
  static bool decode_time_point(const std::span<const u8>& iData, QDateTime& oUtc, i32& oLtoMinutes);
  // ETSI TS 102 371, 4.7.6.1: bearerURI type in the dab: domain (also used for the MOT parameter ScopeID of PI objects)
  static bool decode_bearer(const std::span<const u8>& iData, Spi::SBearer& oBearer);
  // ETSI TS 102 371, 4.12: genre href
  static QString decode_genre_href(const std::span<const u8>& iData);

private:
  struct STlv
  {
    u8 tag = 0;
    std::span<const u8> value;
  };

  // Iterates over the tag-length-value entries of an element body (ETSI TS 102 371, 4.3 and 4.4)
  class TlvReader
  {
  public:
    explicit TlvReader(const std::span<const u8>& iData) : mData(iData) {}
    bool next(STlv& oTlv);
    [[nodiscard]] bool has_error() const { return mError; }

  private:
    std::span<const u8> mData;
    usize mPos = 0;
    bool mError = false;
  };

  static constexpr usize cTokenTableSize = 0x14; // token tags are in the range 0x01..0x13
  std::array<QByteArray, cTokenTableSize> mTokenTable{};
  bool mError = false;

  void _decode_top_level_children(TlvReader& ioReader, Spi::SResult& ioResult);
  void _decode_token_table(const std::span<const u8>& iData);
  void _decode_schedule(const std::span<const u8>& iData, Spi::SSchedule& oSchedule);
  void _decode_scope(const std::span<const u8>& iData, Spi::SSchedule& ioSchedule);
  void _decode_programme(const std::span<const u8>& iData, Spi::SProgramme& oProgramme);
  void _decode_location(const std::span<const u8>& iData, Spi::SProgramme& ioProgramme);
  void _decode_time(const std::span<const u8>& iData, Spi::SProgramme& ioProgramme);
  void _decode_ensemble(const std::span<const u8>& iData, Spi::SServiceInformation& ioSI);
  void _decode_service(const std::span<const u8>& iData, Spi::SService& oService);
  void _decode_media_description(const std::span<const u8>& iData, QString& ioShortDescr, QString& ioLongDescr, std::vector<Spi::SMultimedia>& ioMultimedia);
  void _decode_multimedia(const std::span<const u8>& iData, Spi::SMultimedia& oMultimedia);
  bool _decode_bearer_element(const std::span<const u8>& iData, Spi::SBearer& oBearer);
  QString _decode_genre_element(const std::span<const u8>& iData);
  QString _decode_cdata_of_element(const std::span<const u8>& iData);
  bool _decode_name_element(const STlv& iTlv, Spi::SNames& ioNames);
  QString _decode_string(const std::span<const u8>& iData) const;
  void _check_reader(const TlvReader& iReader);

  static u32 _get_uint(const std::span<const u8>& iData);
};
