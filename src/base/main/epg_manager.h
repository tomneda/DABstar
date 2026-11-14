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

#include "dab_constants.h"
#include "mot_spi_object.h"
#include "spi_decoder.h"
#include <QObject>
#include <QPixmap>
#include <QTimer>
#include <map>
#include <vector>

// Collects the SPI (EPG) data of the current ensemble: the service information and the programme schedules.
// The objects are received via EPG services running in the background (ETSI TS 102 371).
class EpgManager : public QObject
{
  Q_OBJECT

public:
  struct SService
  {
    Spi::SNames names;
    QString shortDescription;
    QString longDescription;
    std::vector<Spi::SMultimedia> multimedia;
    std::vector<QString> genres;
  };

  explicit EpgManager(QObject* ipParent = nullptr);
  ~EpgManager() override = default;

  void clear(); // called on channel change
  void handle_spi_object(const SMotSpiObject& iObject);
  void set_service_list(const std::vector<SServiceId>& iServiceList); // service labels of the ensemble (from FIC)
  void set_current_time(const QDateTime& iUtc, i32 iLtoMinutes);      // time and local time offset of the ensemble (from FIC)

  [[nodiscard]] bool has_data() const { return !mProgrammes.empty(); }
  [[nodiscard]] std::vector<u32> get_service_ids() const; // services with programme data, sorted by label
  [[nodiscard]] QString get_service_label(u32 iSId) const;
  [[nodiscard]] const SService* get_service(u32 iSId) const;
  [[nodiscard]] std::vector<Spi::SProgramme> get_programmes(u32 iSId) const; // sorted by start time
  [[nodiscard]] QPixmap get_logo(u32 iSId, i32 iPreferredSize) const;
  [[nodiscard]] QDateTime get_current_time() const; // UTC, of the ensemble if known, else of the system
  [[nodiscard]] i32 get_lto_minutes() const; // local time offset of the ensemble if known, else of the system

signals:
  void signal_epg_changed();             // coalesced, so not each received object triggers an update
  void signal_epg_available(bool oAvailable);

private:
  static constexpr i32 cChangeNotifyDelayMs = 500;
  static constexpr i32 cKeepPastProgrammesSec = 6 * 3600; // keep programmes this long after their end
  static constexpr usize cMaxLogos = 256;

  SpiDecoder mDecoder;
  std::map<u32, SService> mServices;                              // SId -> service information
  std::map<u32, std::map<qint64, Spi::SProgramme>> mProgrammes;   // SId -> (start time in s since epoch -> programme)
  std::map<QString, QByteArray> mLogos;                           // MOT ContentName -> picture data
  std::map<u32, QString> mServiceLabels;                          // SId -> service label from FIC
  QDateTime mCurrentTimeUtc;
  i32 mLtoMinutes = 0;
  bool mHasEnsembleTime = false;
  QTimer mChangeTimer;

  void _handle_service_information(const Spi::SServiceInformation& iSI);
  void _handle_schedule(const Spi::SSchedule& iSchedule, const SMotSpiObject& iObject, bool iIsAdvanced);
  void _merge_advanced_programme(std::map<qint64, Spi::SProgramme>& ioProgrammes, const Spi::SProgramme& iProgramme) const;
  static void _fill_missing_fields(Spi::SProgramme& ioProgramme, const Spi::SProgramme& iOther);
  void _handle_logo(const SMotSpiObject& iObject);
  void _prune_old_programmes();
  void _notify_change();
  u32 _get_SId_from_name(const QString& iName) const;
  const QByteArray* _find_logo_data(const QString& iUrl) const;
};
