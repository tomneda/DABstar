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

#include "glob_data_types.h"
#include <QByteArray>
#include <QMetaType>
#include <QString>

// A complete MOT object received via an SPI (EPG) service, with the MOT parameters of ETSI TS 102 371, 6.4
struct SMotSpiObject
{
  QByteArray body;
  QString contentName;
  i32 contentType = 0;       // as MOTContentType
  QByteArray scopeId;        // ScopeID (0x27): ECC + EId for SI/GI objects, bearerURI for PI objects
  QByteArray scopeStart;     // ScopeStart (0x25) as timePoint
  QByteArray scopeEnd;       // ScopeEnd (0x26) as timePoint
  QByteArray profileSubset;  // ProfileSubset (0x21): list of profile IDs, empty means all profiles
  bool isCompressed = false; // CompressionType (0x11) is given
  bool isEncrypted = false;  // CAInfo (0x23) is given
};

Q_DECLARE_METATYPE(SMotSpiObject)
