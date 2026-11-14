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
#include "epg_manager.h"
#include "mot_content_types.h"
#include <QLoggingCategory>
#include <QTimeZone>
#include <algorithm>
#include <limits>

Q_LOGGING_CATEGORY(sLogEpgManager, "EpgManager", QtInfoMsg)

namespace
{
constexpr u8 cProfileIdBasic = 0x01; // ETSI TS 102 371, table 13

QString last_path_element(const QString& iPath)
{
  return iPath.section('/', -1).toLower();
}
}

EpgManager::EpgManager(QObject* const ipParent)
  : QObject(ipParent)
{
  mChangeTimer.setSingleShot(true);
  mChangeTimer.setInterval(cChangeNotifyDelayMs);
  connect(&mChangeTimer, &QTimer::timeout, this, [this]()
  {
    emit signal_epg_changed();
    emit signal_epg_available(has_data());
  });
}

void EpgManager::clear()
{
  mServices.clear();
  mProgrammes.clear();
  mLogos.clear();
  mServiceLabels.clear();
  mCurrentTimeUtc = QDateTime();
  mLtoMinutes = 0;
  mHasEnsembleTime = false;
  mChangeTimer.stop();
  emit signal_epg_changed();
  emit signal_epg_available(false);
}

void EpgManager::set_service_list(const std::vector<SServiceId>& iServiceList)
{
  std::map<u32, QString> labels;
  for (const auto& sl : iServiceList)
  {
    labels[sl.SId] = sl.serviceLabel.trimmed();
  }

  if (labels != mServiceLabels)
  {
    mServiceLabels = std::move(labels);
    _notify_change();
  }
}

void EpgManager::set_current_time(const QDateTime& iUtc, const i32 iLtoMinutes)
{
  if (!iUtc.isValid())
  {
    return;
  }

  mCurrentTimeUtc = iUtc;
  mLtoMinutes = iLtoMinutes;
  mHasEnsembleTime = true;
}

QDateTime EpgManager::get_current_time() const
{
  return (mHasEnsembleTime ? mCurrentTimeUtc : QDateTime::currentDateTimeUtc());
}

i32 EpgManager::get_lto_minutes() const
{
  return (mHasEnsembleTime ? mLtoMinutes : QDateTime::currentDateTime().offsetFromUtc() / 60);
}

void EpgManager::handle_spi_object(const SMotSpiObject& iObject)
{
  if (iObject.isEncrypted)
  {
    qCDebug(sLogEpgManager) << "Encrypted SPI object" << iObject.contentName << "ignored";
    return;
  }

  const MOTContentBaseType baseType = getContentBaseType(static_cast<MOTContentType>(iObject.contentType));

  if (baseType == MOTBaseTypeImage)
  {
    _handle_logo(iObject);
    return;
  }

  if (baseType != MOTBaseTypeApplication)
  {
    qCDebug(sLogEpgManager) << "SPI object" << iObject.contentName << "with content type" << Qt::hex << iObject.contentType << "ignored";
    return;
  }

  if (iObject.isCompressed)
  {
    // only "Advanced" profile objects can be compressed (ETSI TS 102 371, 6.4.4), the "Basic" ones contain the main data
    qCDebug(sLogEpgManager) << "Compressed SPI object" << iObject.contentName << "is not supported and ignored";
    return;
  }

  const bool isAdvanced = !iObject.profileSubset.isEmpty() && !iObject.profileSubset.contains((char)cProfileIdBasic);
  const std::span<const u8> data(reinterpret_cast<const u8*>(iObject.body.constData()), (usize)iObject.body.size());
  Spi::SResult result;

  if (!mDecoder.decode(data, result))
  {
    qCWarning(sLogEpgManager) << "Could not decode SPI object" << iObject.contentName << "with" << iObject.body.size() << "bytes";
    return;
  }

  switch (result.type)
  {
  case Spi::EObjectType::ServiceInformation:
    _handle_service_information(result.serviceInformation);
    break;
  case Spi::EObjectType::Epg:
    for (const auto& schedule : result.schedules)
    {
      _handle_schedule(schedule, iObject, isAdvanced);
    }
    break;
  case Spi::EObjectType::Unknown:
    break;
  }

  _prune_old_programmes();
  _notify_change();
}

void EpgManager::_handle_service_information(const Spi::SServiceInformation& iSI)
{
  for (const auto& service : iSI.services)
  {
    for (const auto& bearer : service.bearers)
    {
      SService& s = mServices[bearer.SId];
      s.names = service.names;
      s.shortDescription = service.shortDescription;
      s.longDescription = service.longDescription;
      s.multimedia = service.multimedia;
      s.genres = service.genres;
    }
  }

  qCDebug(sLogEpgManager) << "Service information with" << iSI.services.size() << "services received";
}

void EpgManager::_handle_schedule(const Spi::SSchedule& iSchedule, const SMotSpiObject& iObject, const bool iIsAdvanced)
{
  // Find the service the schedule belongs to. Prefer the serviceScope, then the MOT parameter ScopeID (bearerURI for PI),
  // then the bearers of each programme and at last the content name (older encoders named the objects with the SId).
  u32 scheduleSId = 0;

  if (!iSchedule.serviceScopes.empty())
  {
    scheduleSId = iSchedule.serviceScopes.front().SId;
  }
  else if (Spi::SBearer bearer; !iObject.scopeId.isEmpty()
           && SpiDecoder::decode_bearer(std::span(reinterpret_cast<const u8*>(iObject.scopeId.constData()), (usize)iObject.scopeId.size()), bearer))
  {
    scheduleSId = bearer.SId;
  }

  std::map<u32, std::vector<const Spi::SProgramme*>> programmesOfSId;
  i32 numDropped = 0;

  for (const auto& programme : iSchedule.programmes)
  {
    if (!iIsAdvanced && !programme.startTime.isValid())
    {
      ++numDropped;
      continue;
    }

    if (scheduleSId != 0)
    {
      programmesOfSId[scheduleSId].push_back(&programme);
    }
    else if (!programme.bearers.empty())
    {
      for (const auto& bearer : programme.bearers)
      {
        programmesOfSId[bearer.SId].push_back(&programme);
      }
    }
    else if (const u32 SId = _get_SId_from_name(iObject.contentName);
             SId != 0)
    {
      programmesOfSId[SId].push_back(&programme);
    }
    else
    {
      ++numDropped;
    }
  }

  if (numDropped > 0)
  {
    qCDebug(sLogEpgManager) << numDropped << "programmes without time or service of object" << iObject.contentName << "dropped";
  }

  for (const auto& [SId, programmes] : programmesOfSId)
  {
    auto& programmeMap = mProgrammes[SId];

    if (iIsAdvanced)
    {
      for (const auto* const pProgramme : programmes)
      {
        _merge_advanced_programme(programmeMap, *pProgramme);
      }
      continue;
    }

    // a "Basic" programme information object replaces all programmes within its scope
    QDateTime scopeStart = iSchedule.scopeStart;
    QDateTime scopeStop = iSchedule.scopeStop;
    i32 lto = 0;

    if (!scopeStart.isValid() && !iObject.scopeStart.isEmpty())
    {
      SpiDecoder::decode_time_point(std::span(reinterpret_cast<const u8*>(iObject.scopeStart.constData()), (usize)iObject.scopeStart.size()), scopeStart, lto);
    }
    if (!scopeStop.isValid() && !iObject.scopeEnd.isEmpty())
    {
      SpiDecoder::decode_time_point(std::span(reinterpret_cast<const u8*>(iObject.scopeEnd.constData()), (usize)iObject.scopeEnd.size()), scopeStop, lto);
    }

    qint64 eraseFrom = std::numeric_limits<qint64>::max();
    qint64 eraseTo = std::numeric_limits<qint64>::min();
    for (const auto* const pProgramme : programmes)
    {
      eraseFrom = std::min(eraseFrom, pProgramme->startTime.toSecsSinceEpoch());
      eraseTo = std::max(eraseTo, pProgramme->startTime.toSecsSinceEpoch() + 1);
    }
    if (scopeStart.isValid())
    {
      eraseFrom = std::min(eraseFrom, scopeStart.toSecsSinceEpoch());
    }
    if (scopeStop.isValid())
    {
      eraseTo = std::max(eraseTo, scopeStop.toSecsSinceEpoch());
    }

    // keep the replaced programmes to be able to take over data of "Advanced" objects which could have been received before
    std::map<qint64, Spi::SProgramme> replaced;
    for (auto it = programmeMap.lower_bound(eraseFrom); it != programmeMap.end() && it->first < eraseTo;)
    {
      replaced.insert(programmeMap.extract(it++));
    }

    for (const auto* const pProgramme : programmes)
    {
      auto& entry = programmeMap[pProgramme->startTime.toSecsSinceEpoch()];
      entry = *pProgramme;

      for (const auto& [start, old] : replaced)
      {
        if (old.shortId != 0 && old.shortId == entry.shortId)
        {
          _fill_missing_fields(entry, old);
          break;
        }
      }
    }

    qCDebug(sLogEpgManager) << "Schedule with" << programmes.size() << "programmes for SId" << Qt::hex << SId << "received";
  }
}

void EpgManager::_merge_advanced_programme(std::map<qint64, Spi::SProgramme>& ioProgrammes, const Spi::SProgramme& iProgramme) const
{
  // ETSI TS 102 371, 5.4.2: "Basic" and "Advanced" programmes are merged by the shortId
  for (auto& [start, p] : ioProgrammes)
  {
    if ((iProgramme.shortId != 0 && p.shortId == iProgramme.shortId) || (iProgramme.shortId == 0 && !iProgramme.crid.isEmpty() && p.crid == iProgramme.crid))
    {
      _fill_missing_fields(p, iProgramme);
      return;
    }
  }
}

void EpgManager::_fill_missing_fields(Spi::SProgramme& ioProgramme, const Spi::SProgramme& iOther)
{
  if (ioProgramme.crid.isEmpty())             { ioProgramme.crid = iOther.crid; }
  if (ioProgramme.names.shortName.isEmpty())  { ioProgramme.names.shortName = iOther.names.shortName; }
  if (ioProgramme.names.mediumName.isEmpty()) { ioProgramme.names.mediumName = iOther.names.mediumName; }
  if (ioProgramme.names.longName.isEmpty())   { ioProgramme.names.longName = iOther.names.longName; }
  if (ioProgramme.shortDescription.isEmpty()) { ioProgramme.shortDescription = iOther.shortDescription; }
  if (ioProgramme.longDescription.isEmpty())  { ioProgramme.longDescription = iOther.longDescription; }
  if (ioProgramme.genres.empty())             { ioProgramme.genres = iOther.genres; }
  if (ioProgramme.multimedia.empty())         { ioProgramme.multimedia = iOther.multimedia; }
  if (ioProgramme.durationSec < 0)            { ioProgramme.durationSec = iOther.durationSec; }
}

void EpgManager::_handle_logo(const SMotSpiObject& iObject)
{
  if (iObject.contentName.isEmpty() || iObject.body.isEmpty())
  {
    return;
  }

  if (mLogos.size() >= cMaxLogos && !mLogos.contains(iObject.contentName))
  {
    return;
  }

  mLogos[iObject.contentName] = iObject.body;
  _notify_change();
}

void EpgManager::_prune_old_programmes()
{
  if (!mHasEnsembleTime) // with a recorded file the system time could be far away from the time of the programmes
  {
    return;
  }

  const qint64 limit = mCurrentTimeUtc.toSecsSinceEpoch() - cKeepPastProgrammesSec;

  for (auto itSId = mProgrammes.begin(); itSId != mProgrammes.end();)
  {
    auto& programmeMap = itSId->second;

    for (auto it = programmeMap.begin(); it != programmeMap.end();)
    {
      const qint64 stop = it->first + std::max(it->second.durationSec, 0);
      if (stop < limit)
      {
        it = programmeMap.erase(it);
      }
      else
      {
        break; // sorted by start time
      }
    }

    itSId = (programmeMap.empty() ? mProgrammes.erase(itSId) : std::next(itSId));
  }
}

void EpgManager::_notify_change()
{
  if (!mChangeTimer.isActive())
  {
    mChangeTimer.start();
  }
}

u32 EpgManager::_get_SId_from_name(const QString& iName) const
{
  for (const auto& [SId, label] : mServiceLabels)
  {
    if (iName.contains(QString::number(SId, 16), Qt::CaseInsensitive))
    {
      return SId;
    }
  }
  return 0;
}

std::vector<u32> EpgManager::get_service_ids() const
{
  std::vector<u32> SIds;
  SIds.reserve(mProgrammes.size());
  for (const auto& [SId, programmes] : mProgrammes)
  {
    SIds.push_back(SId);
  }

  std::sort(SIds.begin(), SIds.end(), [this](const u32 iA, const u32 iB)
  {
    return get_service_label(iA).compare(get_service_label(iB), Qt::CaseInsensitive) < 0;
  });
  return SIds;
}

QString EpgManager::get_service_label(const u32 iSId) const
{
  if (const auto it = mServiceLabels.find(iSId); it != mServiceLabels.end() && !it->second.isEmpty())
  {
    return it->second;
  }

  if (const auto it = mServices.find(iSId); it != mServices.end())
  {
    if (const QString name = it->second.names.best(); !name.isEmpty())
    {
      return name;
    }
  }

  return QString("SId %1").arg(iSId, 0, 16).toUpper();
}

const EpgManager::SService* EpgManager::get_service(const u32 iSId) const
{
  const auto it = mServices.find(iSId);
  return (it != mServices.end() ? &it->second : nullptr);
}

std::vector<Spi::SProgramme> EpgManager::get_programmes(const u32 iSId) const
{
  std::vector<Spi::SProgramme> programmes;

  if (const auto it = mProgrammes.find(iSId); it != mProgrammes.end())
  {
    programmes.reserve(it->second.size());
    for (const auto& [start, programme] : it->second)
    {
      programmes.push_back(programme);
    }
  }
  return programmes;
}

const QByteArray* EpgManager::_find_logo_data(const QString& iUrl) const
{
  if (iUrl.isEmpty())
  {
    return nullptr;
  }

  if (const auto it = mLogos.find(iUrl); it != mLogos.end())
  {
    return &it->second;
  }

  const QString fileName = last_path_element(iUrl);
  for (const auto& [name, data] : mLogos)
  {
    if (last_path_element(name) == fileName)
    {
      return &data;
    }
  }
  return nullptr;
}

QPixmap EpgManager::get_logo(const u32 iSId, const i32 iPreferredSize) const
{
  const SService* const pService = get_service(iSId);

  if (pService == nullptr)
  {
    return {};
  }

  // choose the available logo with the size nearest to the preferred one, square logos are preferred
  const QByteArray* pBest = nullptr;
  i32 bestScore = std::numeric_limits<i32>::max();

  for (const auto& mm : pService->multimedia)
  {
    const QByteArray* const pData = _find_logo_data(mm.url);
    if (pData == nullptr)
    {
      continue;
    }

    const i32 size = std::max<i32>(mm.width, mm.height);
    i32 score = (size > 0 ? std::abs(size - iPreferredSize) : 10000);
    if (mm.width != mm.height)
    {
      score += 1000;
    }

    if (score < bestScore)
    {
      bestScore = score;
      pBest = pData;
    }
  }

  QPixmap pixmap;
  if (pBest != nullptr)
  {
    pixmap.loadFromData(*pBest);
  }
  return pixmap;
}
