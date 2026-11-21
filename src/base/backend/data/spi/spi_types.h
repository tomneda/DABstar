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

// Data structures of the decoded "Service and Programme Information" (SPI, formerly EPG), see ETSI TS 102 371 / TS 102 818.

#include "glob_data_types.h"
#include <QDateTime>
#include <QString>
#include <vector>

namespace Spi
{

// ETSI TS 102 371, 6.4.1 table 12: ContentSubType of ContentType 7 (application)
enum class EContentSubType : i32
{
  ServiceInformation   = 0,
  ProgrammeInformation = 1,
  GroupInformation     = 2
};

// Kind of a MOT object received in an SPI service, used for the receiving indicators (the values are used as index)
enum class EContentKind : u8
{
  ProgrammeInformation = 0, // programme and group information
  ServiceInformation   = 1,
  Logo                 = 2,
  None                 = 3
};
constexpr usize cNumContentKinds = 3; // without None

// ETSI TS 102 371, 4.7.6.1: bearerURI in the dab: domain
struct SBearer
{
  bool hasEnsemble = false;
  bool isXPad = false;
  u8 ECC = 0;
  u16 EId = 0;
  u32 SId = 0;
  u8 SCIdS = 0;

  bool operator==(const SBearer& iOther) const = default;
};

// multimedia element (mainly logos)
struct SMultimedia
{
  QString url;       // for broadcast logos this is the ContentName of the MOT object carrying the picture
  QString mimeValue;
  u8 type = 0;       // see annex F, 0 = not given
  u16 width = 0;
  u16 height = 0;
};

struct SNames
{
  QString shortName;
  QString mediumName;
  QString longName;

  [[nodiscard]] QString best() const
  {
    if (!longName.isEmpty()) return longName;
    if (!mediumName.isEmpty()) return mediumName;
    return shortName;
  }
};

struct SProgramme
{
  QString crid;               // id attribute
  u32 shortId = 0;            // shortId attribute (24 bit)
  u16 version = 0;
  SNames names;
  QString shortDescription;
  QString longDescription;
  std::vector<SMultimedia> multimedia;
  std::vector<QString> genres; // genre href, e.g. "urn:tva:metadata:cs:ContentCS:2002:3.6.1"
  QDateTime startTime;        // UTC, invalid if not given
  i32 durationSec = -1;       // -1 if not given
  QDateTime actualTime;       // UTC, invalid if not given
  i32 actualDurationSec = -1; // -1 if not given
  i32 ltoMinutes = 0;         // local time offset given for startTime
  std::vector<SBearer> bearers;
  bool isOnAir = true;        // broadcast attribute
  bool isRecommended = false; // recommendation attribute

  [[nodiscard]] QDateTime stop_time() const { return (startTime.isValid() && durationSec >= 0 ? startTime.addSecs(durationSec) : QDateTime()); }
};

struct SSchedule
{
  u16 version = 0;
  QDateTime creationTime;
  QString originator;
  QDateTime scopeStart; // UTC
  QDateTime scopeStop;  // UTC
  std::vector<SBearer> serviceScopes;
  std::vector<SProgramme> programmes;
};

struct SService
{
  u16 version = 0;
  SNames names;
  QString shortDescription;
  QString longDescription;
  std::vector<SMultimedia> multimedia;
  std::vector<QString> genres;
  std::vector<SBearer> bearers;
};

struct SServiceInformation
{
  u16 version = 0;
  QDateTime creationTime;
  QString originator;
  QString serviceProvider;
  bool hasEnsemble = false;
  u8 ECC = 0;
  u16 EId = 0;
  SNames ensembleNames;
  std::vector<SService> services;
};

enum class EObjectType
{
  Unknown,
  Epg,               // top-level element "epg" (programme or group information)
  ServiceInformation // top-level element "serviceInformation"
};

struct SResult
{
  EObjectType type = EObjectType::Unknown;
  QString defaultLanguage;
  std::vector<SSchedule> schedules;       // filled for EObjectType::Epg
  SServiceInformation serviceInformation; // filled for EObjectType::ServiceInformation
};

} // namespace Spi
