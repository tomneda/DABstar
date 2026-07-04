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
#include "tii_codes.h"
#include "tii_detector.h"
#include "tii_list_display.h"
#include <QObject>
#include <QTimer>
#include <QString>
#include <complex>
#include <vector>

class Configuration;
class DabProcessor;
class MapHttpServer;
class QComboBox;
class QLabel;
class QWidget;

class TiiManager : public QObject
{
Q_OBJECT
public:
  struct SResourceConfig
  {
    Configuration * pConfig;
    QComboBox     * pCmbTiiList;
    QLabel        * pLblTii;
    QWidget       * pParentWidget;  // for QMessageBox in load_table()
  };

  explicit TiiManager(const SResourceConfig & cfg, QObject * parent = nullptr);
  ~TiiManager() override = default;

  // State update methods called by DabRadio when state changes
  void set_dab_processor(DabProcessor * p)  { mpDabProcessor = p; }
  void set_http_handler(MapHttpServer * p)   { mpHttpHandler = p; }
  void set_channel_running(bool isRunning)   { mIsChannelRunning = isRunning; }
  void set_file_mode(bool isFileMode)        { mIsFileMode = isFileMode; }
  void set_local_pos(const cf32 & localPos)  { mLocalPos = localPos; }
  void set_channel_info(const QString & fIdOrCh, u32 eid, const QString & countryName, const QString & typeInfo);

  // Called once to initialize (load) TII database from settings/resources
  bool init_tii_file();
  // Called by slot_load_table and on reinit to load a specific file
  bool fill_tii_cache(const QString & fileName);

  void hide_tii_display();
  void clear_tii_list_and_label();
  const std::vector<STiiResult> & get_transmitter_ids() const { return mTransmitterIds; }

  // Called from DabRadio::slot_load_table (for QMessageBox)
  void load_table();

  // Called from DabRadio wrapper (not a slot — invoked directly)
  void show_tii(const std::vector<STiiResult> & iTiiList);

public slots:
  // Connected from Configuration widgets
  void slot_use_strongest_peak(bool iIsChecked);
  void slot_handle_tii_collisions(bool iIsChecked);
  void slot_handle_tii_threshold(i32 trs);
  void slot_handle_tii_subid(i32 subid);
  // Connected from btnTii button
  void slot_handle_tii_button();
  // Connected from TiiListDisplay signal_frame_closed
  void slot_handle_tii_viewer_closed();

signals:
  void signal_tii_file_status(bool valid, const QString & toolTip);  // for btnHttpServer enabled/tooltip

private:
  // Non-owned resources (from config)
  Configuration * const mpConfig;
  QComboBox     * const mpCmbTiiList;
  QLabel        * const mpLblTii;
  QWidget       * const mpParentWidget;

  // Non-owned, updated via set_*() methods
  DabProcessor  * mpDabProcessor = nullptr;
  MapHttpServer * mpHttpHandler  = nullptr;

  // Owned objects
  TiiHandler    mTiiHandler{};
  TiiListDisplay mTiiListDisplay;
  QTimer        mTiiIndexCntTimer;

  // State
  std::vector<STiiResult> mTransmitterIds;
  cf32    mLocalPos{};
  QString mCurFIdOrCh;
  QString mCurCountryName;
  QString mCurTypeInfo;
  u32     mCurEid = 0;
  u32     mTiiIndex = 0;
  bool    mIsChannelRunning = false;
  bool    mIsFileMode       = false;
  bool    mShowTiiListWindow = false;

private slots:
  void _slot_tii_index_cnt_timeout();
};
