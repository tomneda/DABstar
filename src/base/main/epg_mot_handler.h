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
#include "openfiledialog.h"
#include "epgdec.h"
#include "epg_decoder.h"
#include <QObject>
#include <QTimer>
#include <QString>
#include <QByteArray>
#include <QScopedPointer>
#include <vector>

class Configuration;
class DabProcessor;
class QLabel;
class QPushButton;
class QPixmap;

class EpgMotHandler : public QObject
{
  Q_OBJECT

public:
  struct SResourceConfig
  {
    Configuration * pConfig;
    QLabel * pPictureLabel;
    QPushButton * pBtnOpenPicFolder;
    OpenFileDialog * pOpenFileDialog;
  };

  explicit EpgMotHandler(const SResourceConfig & iCfg, QObject * ipParent = nullptr);
  ~EpgMotHandler() override;

  // State update methods called by DabRadio when channel/service state changes
  void set_channel_info(const QString & iFIdOrChDescr, const QString & iEnsembleName);
  void set_service_label(const QString & iLabel) { mCurServiceLabel = iLabel; }
  void set_channel_running(bool iIsRunning) { mIsChannelRunning = iIsRunning; }
  void set_dab_processor(DabProcessor * ipDabProcessor) { mpDabProcessor = ipDabProcessor; }
  void set_service_list(const std::vector<SServiceId> * ipServiceList) { mpServiceList = ipServiceList; }

  // Called from DabRadio::_stop_services() to clear per-service/channel state
  void on_stop_services();

  void show_pause_slide() const;
  void close_dl_text_file();

  // Called from DabRadio::_slot_handle_open_pic_folder_button
  QString get_pic_folder() const;
  // Called from DabRadio::slot_show_label for DL text file writing
  FILE * get_dl_text_file() const { return mpDlTextFile; }

  // Called from DabRadio wrappers (not slots — invoked directly)
  void handle_mot_object(const QByteArray & iResult, const QString & iObjectName, i32 iContentType, bool iDirElement);
  void trigger_mot_indicator();

private:
  // Non-owned resources (from config, valid for lifetime of EpgMotHandler)
  Configuration * const mpConfig;
  QLabel * const mpPictureLabel;
  QPushButton * const mpBtnOpenPicFolder;
  OpenFileDialog * const mpOpenFileDialog;

  // Non-owned, updated via set_dab_processor()
  DabProcessor * mpDabProcessor = nullptr;

  // Owned objects
  QScopedPointer<CEPGDecoder> mpEpgHandler;
  QScopedPointer<EpgDecoder> mpEpgProcessor;
  QTimer mTimerMotReceived;

  // Path state (read from settings in init_paths())
  QString mEpgPath;
  QString mPicPath;
  QString mMotPath;

  // Per-channel/-service state
  QString mMotPicPathLast;
  QString mCurFIdOrChDescr;
  QString mCurEnsembleName;
  QString mCurServiceLabel;
  bool mIsChannelRunning = false;

  // Non-owned, updated via set_service_list()
  const std::vector<SServiceId> * mpServiceList = nullptr;

  // Counters
  i32 mMotObjectCnt = 0;

  // File handles
  FILE * mpDlTextFile = nullptr;

  bool _save_mot_epg_data(const QByteArray & iResult, const QString & iObjectName, i32 iContentType) const;
  void _save_mot_object(const QByteArray & iResult, const QString & iName);
  void _save_mot_text(const QByteArray & iResult, i32 iContentType, const QString & iName) const;
  void _show_mot_image(const QByteArray & iData, i32 iContentType, const QString & iPictureName, i32 iDirs);
  void _write_picture(const QPixmap & iPixMap) const;
  QString _check_and_create_dir(const QString & iPath) const;
  void _create_directory(const QString & iDirOrPath, bool iContainsFileName) const;
  QString _generate_unique_file_path_from_hash(const QString & iBasePath, const QString & iFileExt, const QByteArray & iData, i32 iDirLevel) const;
  QString _generate_file_path(const QString & iBasePath, const QString & iFileName, i32 iDirLevel) const;
  u32 _extract_epg(const QString & iName, u32 iEnsembleId) const;

public slots:
  void slot_init_paths(); // Called once after construction to load paths from settings
  void slot_handle_mot_saving_selector(i32 iIdx); // Connected from configuration combobox
  void slot_set_epg_data(i32 iSId, i32 iTheTime, const QString & iTheText, const QString & iTheDescr); // Connected from EpgDecoder
  void slot_handle_dl_text_button(); // Connected from dlTextButton

private slots:
  void _slot_mot_timer_timeout();

signals:
  void signal_mot_indicator(bool);   // DabRadio connects: _set_status_info_status(mStatusInfo.MOT, b)
  void signal_epg_indicator(bool);   // DabRadio connects: _show_epg_label(b)
};
