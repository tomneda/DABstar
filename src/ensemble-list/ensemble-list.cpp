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

#include "ensemble-list.h"
#include "ui_ensemble-list.h"
#include "setting-helper.h"
#include "ensemble-list-db-handler.h"
#include "band-handler.h"
#include "gui-helpers.h"
#include <QDir>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFile>
#include <QFileDialog>


EnsembleList::EnsembleList(const QString & iDbFileName)
  : ui(new Ui_ensembleList)
  , mBandHandler("", &Settings::Storage::instance())
{
  ui->setupUi(&mFrame);
  mFrame.hide();

  Settings::EnsembleList::posAndSizeChMode.read_widget_geometry(&mFrame);

  mFrame.setWindowFlag(Qt::Tool, true);

  Settings::EnsembleList::ledPathToScan.register_widget_and_update_ui_from_setting(ui->ledPathToScan, QDir::homePath());

  ui->teScanResult->setAcceptRichText(true);
  ui->progressBar->setValue(0);
  ui->progressBar->setStyleSheet("color: lightgray");

  _setup_ui_regarding_list_mode();

  const QColor fg("white");
  ui->btnScanStart->setStyleSheet(get_bg_style_sheet(0x4846FF, fg));
  ui->btnResetDataBase->setStyleSheet(get_bg_style_sheet(0xC50F09, fg));
  ui->cbShowELNotScanned->setStyleSheet(get_bg_style_sheet(cBgColorNotScanned, fg));
  ui->cbShowELScanned->setStyleSheet(get_bg_style_sheet(cBgColorUnselected, fg));
  ui->cbShowELNoSignal->setStyleSheet(get_bg_style_sheet(cBgColorFailed, fg));

  mpDbHandler.reset(new EnsembleListDbHandler(iDbFileName, ui->tblEnsembleList));
  mpDbHandler->set_data_mode(EnsembleListDbHandler::EDataMode::Device);
  mListMode = EListMode::PlayFromDevice;

  mBandHandler.setupChannels(nullptr, BandHandler::EBand::BAND_III);
  const QString skipFileName = Settings::Config::varSkipFile.read().toString();
  mBandHandler.setup_skipList(skipFileName);

  connect(&mFrame, &CustomFrame::signal_frame_closed, []{ Settings::EnsembleList::varUiVisible.write(false); });
  connect(ui->btnResetDataBase, &QPushButton::clicked, this, &EnsembleList::_slot_handle_reset_data_base_button);
  connect(ui->btnRemoveFilesWithoutSignal, &QPushButton::clicked, this, &EnsembleList::_slot_handle_remove_invalid_entries_button);
  connect(ui->btnPathToScan, &QPushButton::clicked, this, &EnsembleList::_slot_handle_path_with_files_to_add_button);
  connect(ui->btnAddFilesInPath, &QPushButton::clicked, this, &EnsembleList::_slot_handle_add_files_in_path);
  connect(ui->btnAddSingleFile, &QPushButton::clicked, this, &EnsembleList::_slot_handle_add_single_file);
  connect(ui->btnScanStart, &QPushButton::clicked, this, &EnsembleList::_slot_handle_scan_button);
  connect(ui->tblEnsembleList, &QTableView::clicked, this, &EnsembleList::_slot_handle_table_click);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  connect(ui->cbShowSLCurChOnly, &QCheckBox::checkStateChanged, this, &EnsembleList::_slot_handle_show_current_FId_or_Ch_only);
  connect(ui->cbShowELNotScanned, &QCheckBox::checkStateChanged, this, &EnsembleList::_slot_handle_ensemble_list_filter, Qt::DirectConnection);
  connect(ui->cbShowELScanned, &QCheckBox::checkStateChanged, this, &EnsembleList::_slot_handle_ensemble_list_filter, Qt::DirectConnection);
  connect(ui->cbShowELNoSignal, &QCheckBox::checkStateChanged, this, &EnsembleList::_slot_handle_ensemble_list_filter, Qt::DirectConnection);
#else
  connect(ui->cbShowSLCurChOnly, &QCheckBox::stateChanged, this, &EnsembleList::_slot_handle_show_current_FId_or_Ch_only);
  connect(ui->cbShowELNotScanned, &QCheckBox::stateChanged, this, &EnsembleList::_slot_handle_ensemble_list_filter, Qt::DirectConnection);
  connect(ui->cbShowELScanned, &QCheckBox::stateChanged, this, &EnsembleList::_slot_handle_ensemble_list_filter, Qt::DirectConnection);
  connect(ui->cbShowELNoSignal, &QCheckBox::stateChanged, this, &EnsembleList::_slot_handle_ensemble_list_filter, Qt::DirectConnection);
#endif

  Settings::EnsembleList::cbShowELNoSignal.register_widget_and_update_ui_from_setting(ui->cbShowELNoSignal, 2);
  Settings::EnsembleList::cbShowELNotScanned.register_widget_and_update_ui_from_setting(ui->cbShowELNotScanned, 2);
  Settings::EnsembleList::cbShowELScanned.register_widget_and_update_ui_from_setting(ui->cbShowELScanned, 2);
  Settings::EnsembleList::spMinFileSizeMB.register_widget_and_update_ui_from_setting(ui->spMinFileSizeMB, 100);

  _slot_handle_ensemble_list_filter();

  if (_get_nr_rows_in_table() == 0) // init device database if not existing
  {
    _slot_handle_reset_data_base_button();
  }

  if (Settings::EnsembleList::varUiVisible.read().toBool())
  {
    mFrame.show();
  }
}

EnsembleList::~EnsembleList()
{
  mBandHandler.saveSettings();
  mBandHandler.hide();

  _write_pos_and_size();
  delete ui;
}

void EnsembleList::_add_channel_entries_to_db() const
{
  const auto channelList = mBandHandler.get_channel_entry_list();

  for (const auto & entry : channelList)
  {
    if (!entry.skip)
    {
      EnsembleListDB::SDbEntryData ed{};
      ed.key.fIdOrCh = entry.channel;
      mpDbHandler->insert_or_update_entry(ed, EnsembleListDB::EDbDataType::InsertKeyAndS0WsData); // this will overwrite existing entries in the DB
    }
  }

  _log_to_result_display(ELogType::INFOACK, QString("Added %1 channels to list").arg(_get_nr_rows_in_table()));
}

i32 EnsembleList::_get_nr_rows_in_table() const
{
  return ui->tblEnsembleList->model()->rowCount();
}

void EnsembleList::set_list_mode(const EListMode iListMode)
{
  _write_pos_and_size(); // save geometry for the outgoing mode before switching

  mListMode = iListMode;
  EnsembleListDbHandler::EDataMode dataMode;;

  switch (mListMode)
  {
  case EListMode::PlayFromFiles:  dataMode = EnsembleListDbHandler::EDataMode::Files;  break;
  case EListMode::PlayFromDevice: dataMode = EnsembleListDbHandler::EDataMode::Device; break;
  default: qFatal("Invalid list mode");
  }

  mpDbHandler->set_data_mode(dataMode);

  _read_pos_and_size(); // restore geometry for the incoming mode
  _setup_ui_regarding_list_mode();
}

void EnsembleList::_read_pos_and_size()
{
  switch (mListMode)
  {
  case EListMode::PlayFromFiles:   Settings::EnsembleList::posAndSizeFMode.read_widget_geometry(&mFrame);   break;
  case EListMode::PlayFromDevice:  Settings::EnsembleList::posAndSizeChMode.read_widget_geometry(&mFrame);  break;
  default: break;
  }
}

void EnsembleList::_write_pos_and_size()
{
  switch (mListMode)
  {
  case EListMode::PlayFromFiles:   Settings::EnsembleList::posAndSizeFMode.write_widget_geometry(&mFrame);   break;
  case EListMode::PlayFromDevice:  Settings::EnsembleList::posAndSizeChMode.write_widget_geometry(&mFrame);  break;
  default: break;
  }
}

void EnsembleList::hide()
{
  mFrame.hide();
  Settings::EnsembleList::varUiVisible.write(false);
}

void EnsembleList::show()
{
  mFrame.show();
  Settings::EnsembleList::varUiVisible.write(true);
}

void EnsembleList::init_after_connect()
{
  emit signal_show_current_FId_or_Ch_only(ui->cbShowSLCurChOnly->isChecked());
}

void EnsembleList::_log_to_result_display(const ELogType iLogType, const QString & iMessage, const i32 iAddIndent /*= 0*/) const
{
  QString indentStr;

  for (i32 i = 0; i < iAddIndent + mIndentGlobal; i++)
  {
    indentStr += "&nbsp;&nbsp;&nbsp;&nbsp;";
  }

  QString color;
  QString preMsg;

  switch (iLogType)
  {
  case ELogType::WARN:
    color = "orange";
    preMsg = "WARNING: ";
    break;
  case ELogType::ERROR2:
    color = "lightcoral";
    preMsg = "ERROR: ";
    break;
  case ELogType::INFONEUT:
    color = "lightyellow";
    preMsg = "";
    break;
  case ELogType::INFOACK:
    color = "lightblue";
    preMsg = "";
    break;
  case ELogType::INFONACK:
    color = "lightcoral";
    preMsg = "";
    break;
  }

  ui->teScanResult->append(QString("<span style='color:%1;'>%2%3%4</span>").arg(color).arg(indentStr).arg(preMsg).arg(iMessage.toHtmlEscaped()));
  ui->teScanResult->moveCursor(QTextCursor::End);
}

void EnsembleList::_setup_ui_regarding_list_mode() const
{
  const bool isFileMode = (mListMode == EListMode::PlayFromFiles);
  ui->containerFilePath->setVisible(isFileMode);
  ui->btnRemoveFilesWithoutSignal->setVisible(isFileMode);
  _update_remove_invalid_files_button_state();

  if (isFileMode)
  {
    ui->cbShowSLCurChOnly->setCheckState(Settings::EnsembleList::varShowSLCurChOnlyFMode.read().toBool() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
  }
  else
  {
    ui->cbShowSLCurChOnly->setCheckState(Settings::EnsembleList::varShowSLCurChOnlyChMode.read().toBool() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
  }
}

void EnsembleList::_setup_ui_regarding_scan_mode(bool iScanMode) const
{
  // iScanMode is also true with a single click on the ensemble list until the scan is finished
  ui->containerFilePath->setEnabled(!iScanMode);
  ui->btnResetDataBase->setEnabled(!iScanMode);
  ui->tblEnsembleList->setEnabled(!iScanMode);
}

QString EnsembleList::_add_file_to_file_scan_list(const QString & iFileName, const i64 iMinFileSize) const
{
  ui->cbShowELNotScanned->setCheckState(Qt::CheckState::Checked);

  const QFileInfo fileInfo(iFileName);

  if (fileInfo.size() < iMinFileSize) // The file size should be at least 4 MB
  {
    _log_to_result_display(ELogType::WARN, "File " + fileInfo.fileName() + " skipped as too small with " + QString::number(fileInfo.size() / (1024.0 * 1024.0), 'f', 3) + " MB");
    return {};
  }

  QFile file(iFileName);

  if (file.open(QIODevice::ReadOnly))
  {
    file.seek(3 * 1024 * 1024); // the file size should be at least 4 MB, create the hash from this place to get more sure about the uniqueness of the sample data
    const QByteArray buffer = file.read(64 * 1024);
    const QByteArray sha1 = QCryptographicHash::hash(buffer, QCryptographicHash::Sha1);
    const QString fId = sha1.left(4).toHex();

    // check if the hash already exists
    if (!mpDbHandler->is_entry_existing(fId))
    {
      EnsembleListDB::SDbEntryData ed{};
      ed.key.fIdOrCh = fId;
      ed.S0Ws.fileName = fileInfo.fileName();
      ed.S0Ws.filePath = fileInfo.path();
      ed.S0Ws.fileLengthMB = (i32)(std::round((double)fileInfo.size() / (1024.0 * 1024.0)));

      mpDbHandler->insert_or_update_entry(ed, EnsembleListDB::EDbDataType::InsertKeyAndS0WsData);

      _log_to_result_display(ELogType::INFOACK, QString("File '%1' added to list").arg(fileInfo.fileName()));
    }
    else
    {
      _log_to_result_display(ELogType::INFONACK, QString("File '%1' skipped as already in list").arg(fileInfo.fileName()));
    }
    file.close();
    return fId;
  }
  else
  {
    _log_to_result_display(ELogType::ERROR2, QString("Could not open file '%1'").arg(fileInfo.fileName()));
  }

  return {};
}

void EnsembleList::slot_select_FId_or_Ch(const QString & iFIdOrCh, const u32 iSId)
{
  i32 rowIdx = mpDbHandler->set_selector(iFIdOrCh); // mark the current processed line

  auto consider_filter = [&](QCheckBox * const ipCb)
  {
    if (rowIdx < 0 && !ipCb->isChecked())
    {
      ipCb->setCheckState(Qt::CheckState::Checked); // this calls immediately the filter update
      rowIdx = mpDbHandler->set_selector(iFIdOrCh); // mark the current processed line
      if (rowIdx < 0) ipCb->setCheckState(Qt::CheckState::Unchecked); // still not found? Reset Filter. This calls immediately the filter update
    }
  };

  // If rowIdx is < 0 then maybe the current filter inhibits showing this row, so open the filter step by step
  consider_filter(ui->cbShowELScanned);
  consider_filter(ui->cbShowELNotScanned);
  consider_filter(ui->cbShowELNoSignal);

  if (rowIdx >= 0)
  {
    _signal_FId_or_Ch_from_table_index(rowIdx, iSId);
  }
  else
  {
    _log_to_result_display(ELogType::ERROR2, QString("'%1' not found in ensemble list").arg(iFIdOrCh));
  }
}

void EnsembleList::slot_decoded_data_status(const SScanResultEL & iResult)
{
  assert(mListMode != EListMode::Invalid);

  if (mIsScanning)
  {
    assert(iResult.nrPackets > 0);
    assert(iResult.curPacketIdx < iResult.nrPackets);
    ui->progressBar->setValue((iResult.curPacketIdx + 1) * 1000 / iResult.nrPackets);
  }

  const EnsembleListDB::SDbEntryData & ed = static_cast<const EnsembleListDB::SDbEntryData &>(iResult);

  switch (iResult.infoReason)
  {
  case EInfoReason::InvalidFile:
    assert(mListMode == EListMode::PlayFromFiles);
    _log_to_result_display(ELogType::INFONACK,  "Invalid file: " + iResult.S0Ws.fileName + "  (in path: " + iResult.S0Ws.filePath + ")", 1);
    mpDbHandler->insert_or_update_entry(ed, EnsembleListDB::EDbDataType::UpdateSL1Failed);
    mCountScanFailed++;
    break;

  case EInfoReason::NewFib:
    _log_to_result_display(ELogType::INFOACK, _list_mode_str("Valid signal detected from channel " + iResult.key.fIdOrCh,
                                                             "Valid signal detected from file: " + iResult.S0Ws.fileName), 1);
    mpDbHandler->insert_or_update_entry(ed, EnsembleListDB::EDbDataType::UpdateSL2FibData);
    if (mIsScanning)
    {
      qDebug() << "Scanning is active, waiting for deferred data";
      return; // we wait for state EInfoReason::DeferredData
    }
    // mCountScanOk++;
    break;

  case EInfoReason::DeferredData:
  {
    _log_to_result_display(ELogType::INFOACK, _list_mode_str("Get extended info from channel " + iResult.key.fIdOrCh,
                                                             "Get extended info from file: " + iResult.S0Ws.fileName), 1);
    mpDbHandler->insert_or_update_entry(ed, EnsembleListDB::EDbDataType::UpdateSL3MedRunData);
    mCountScanOk++;
    break;
  }

  case EInfoReason::NewSId:
    _log_to_result_display(ELogType::INFOACK, "New SId selected: " + iResult.S0Ws.sIdPlayed, 1);
    if (iResult.S0Ws.get_sid_played() > 0)
    {
      mpDbHandler->insert_or_update_entry(ed, EnsembleListDB::EDbDataType::UpdateLastPlayedSId);
    }
    break;

  case EInfoReason::NoNullSymbDet:
  case EInfoReason::WeakSignalDet:
    if (iResult.key.fIdOrCh != mFIdOrChLast) // avoid showing message several times
    {
      mFIdOrChLast = iResult.key.fIdOrCh;
      const QString resStr = (iResult.infoReason == EInfoReason::NoNullSymbDet ? "No signal detected" : "A weak signal detected but not able to decode");
      _log_to_result_display(ELogType::INFONACK, resStr + _list_mode_str(" from channel " + iResult.key.fIdOrCh, " from file " + iResult.S0Ws.fileName), 1);
    }
    mpDbHandler->insert_or_update_entry(ed, EnsembleListDB::EDbDataType::UpdateSL1Failed);
    mCountScanFailed++;
    break;
  }

  if (mIsScanning)
  {
    const i32 rowCount = mIdentInfoListForScan.size();
    mCurrScanIdx++;

    if (mCurrScanIdx < rowCount)
    {
      _signal_ident_info(mIdentInfoListForScan[mCurrScanIdx]);
    }
    else
    {
      _stop_scan_process();
    }
  }
  else
  {
    if (mRowIdxClickOnList >= 0 && ed.S0Ws.get_sid_played() > 0)
    {
      qDebug() << "Call _signal_FId_or_Ch_from_table_index() again with SId" << ed.S0Ws.sIdPlayed;
      _signal_FId_or_Ch_from_table_index(mRowIdxClickOnList, ed.S0Ws.get_sid_played());
    }
    _setup_ui_regarding_scan_mode(false);
  }
}

void EnsembleList::_stop_scan_process()
{
  // set filter to a reasonable state after scan
  ui->cbShowELNoSignal->setCheckState(Qt::CheckState::Unchecked);
  ui->cbShowELScanned->setCheckState(Qt::CheckState::Checked);
  ui->cbShowELNotScanned->setCheckState(Qt::CheckState::Unchecked);

  mIndentGlobal = 0;
  mIsScanning = false;
  ui->progressBar->setValue(0);
  ui->btnScanStart->setText("Auto scan");
  ui->cbShowELNoSignal->setEnabled(true);
  mIdentInfoListForScan.clear();

  _setup_ui_regarding_scan_mode(false);

  _log_to_result_display(ELogType::INFONEUT, QString("Scan stopped: %1 valid ensemble of %2 searched %3 found")
                                             .arg(mCountScanOk).arg(mCountScanOk + mCountScanFailed)
                                             .arg(_list_mode_str("channels", "files")));

  mpDbHandler->set_sorting_to_FId_or_Ch(true); // restore storting to point before scan

  emit signal_start_stop_scan(false);
}

void EnsembleList::_slot_handle_scan_button()
{
  mIndentGlobal = 0;

  if (mIsScanning)
  {
    _stop_scan_process();
    return;
  }

  const i32 nrRows = _get_nr_rows_in_table();

  if (nrRows == 0)
  {
    _log_to_result_display(ELogType::WARN, "No entries in the list");
    return;
  }

  mIsScanning = true;

  _setup_ui_regarding_scan_mode(true);
  ui->btnScanStart->setText("Stop scan");

  mpDbHandler->set_selector(""); // deselect current marked row
  mpDbHandler->set_sorting_to_FId_or_Ch(false); // needed as DB content may change while scan, so sorting could be changed while scan

  assert(mListMode != EListMode::Invalid);

  // as the ensemble list will change while scan, it is necessary to make a copy of the entries which should be scanned
  mIdentInfoListForScan.clear();
  mIdentInfoListForScan.reserve(nrRows);

  for (i32 rowIdx = 0; rowIdx < nrRows; rowIdx++)
  {
    SIdentInfoEL ii;
    if (!_get_ident_info_from_row_idx(ii, rowIdx))
    {
      return;
    }
    mIdentInfoListForScan.push_back(ii);
  }

  ui->teScanResult->clear();

  mCurrScanIdx = 0;
  mCountScanOk = 0;
  mCountScanFailed = 0;

  emit signal_start_stop_scan(true);

  _log_to_result_display(ELogType::INFONEUT, _list_mode_str("Start scanning device channels ...", "Start scanning files ...") + " (this may take a while, please wait ...)");
  mIndentGlobal++;

  _signal_ident_info(mIdentInfoListForScan[0]);
}

void EnsembleList::_set_EL_filter_check_states_active() const
{
  ui->cbShowELNoSignal->setCheckState(Qt::CheckState::Checked);
  ui->cbShowELScanned->setCheckState(Qt::CheckState::Checked);
  ui->cbShowELNotScanned->setCheckState(Qt::CheckState::Checked);
}

void EnsembleList::_slot_handle_reset_data_base_button()
{
  _set_EL_filter_check_states_active();
  mpDbHandler->delete_table();
  mpDbHandler->create_new_table();
  emit signal_delete_unused_FId_or_Ch(QStringList()); // clear all entries in the service list, too
  _log_to_result_display(ELogType::INFONEUT, "Database cleared");

  if (mListMode == EListMode::PlayFromDevice)
  {
    _add_channel_entries_to_db();
  }
}

void EnsembleList::_slot_handle_remove_invalid_entries_button()
{
  (void)mpDbHandler->delete_invalid_entries();
}

void EnsembleList::_slot_handle_path_with_files_to_add_button()
{
  const QString currentPath = ui->ledPathToScan->text();
  const bool useNativeFileDialog = Settings::Config::cbUseNativeFileDialog.read().toBool();
  const QString selectedPath = QFileDialog::getExistingDirectory(&mFrame, "Select Directory to add", currentPath,
                                                                 QFileDialog::ShowDirsOnly | (useNativeFileDialog ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog));

  if (!selectedPath.isEmpty())
  {
    ui->ledPathToScan->setText(selectedPath);
  }
}

void EnsembleList::_slot_handle_add_files_in_path()
{
  const QString startPath = ui->ledPathToScan->text();

  // check if startPath exists
  const QFileInfo fileInfo(startPath);
  if (!fileInfo.exists() || !fileInfo.isDir())
  {
    _log_to_result_display(ELogType::ERROR2, QString("Path '%1' does not exist or is not a directory").arg(startPath));
    return;
  }

  QDirIterator it(startPath, QDir::Files, QDirIterator::Subdirectories);

  i64 minFileSize = ui->spMinFileSizeMB->value() * 1024 * 1024;
  if (minFileSize < cMinFileSize) minFileSize = cMinFileSize;

  while (it.hasNext())
  {
    const QString filePath = it.next();
    (void)_add_file_to_file_scan_list(filePath, minFileSize);
  }
}

void EnsembleList::_slot_handle_add_single_file()
{
  const QString currentPath = ui->ledPathToScan->text();
  const bool useNativeFileDialog = Settings::Config::cbUseNativeFileDialog.read().toBool();
  const QString selectedFileName = QFileDialog::getOpenFileName(&mFrame, "Select file to add", currentPath, "", nullptr,
                                                                (useNativeFileDialog ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog));

  if (!selectedFileName.isEmpty())
  {
    const QString fid = _add_file_to_file_scan_list(selectedFileName, cMinFileSize);

    // start the new file immediately
    if (!fid.isEmpty())
    {
      const i32 rowIdx = mpDbHandler->set_selector(fid);
      if (rowIdx >= 0)
      {
        ui->cbShowSLCurChOnly->setChecked(true);
        _signal_FId_or_Ch_from_table_index(rowIdx);
      }
    }
  }
}

void EnsembleList::_update_remove_invalid_files_button_state() const
{
  if (mListMode == EListMode::PlayFromFiles)
  {
    if (ui->cbShowELNoSignal->isChecked())
    {
      ui->btnRemoveFilesWithoutSignal->setEnabled(true);
      ui->btnRemoveFilesWithoutSignal->setStyleSheet(get_bg_style_sheet(cBgColorFailed, "white"));
    }
    else
    {
      ui->btnRemoveFilesWithoutSignal->setEnabled(false);
      ui->btnRemoveFilesWithoutSignal->setStyleSheet("");
    }
  }
}

void EnsembleList::_slot_handle_ensemble_list_filter(const int /*iState*/)
{
  // To work correctly, this slot must be called directly from the UI thread
  assert(QThread::currentThread() == thread());
  
  mDataFilter.showNoSignalEntries = ui->cbShowELNoSignal->isChecked();
  mDataFilter.showScannedEntries = ui->cbShowELScanned->isChecked();
  mDataFilter.showNotScannedEntries = ui->cbShowELNotScanned->isChecked();
  // qDebug() << "Filter changed: NoSignal=" << mDataFilter.showNoSignalEntries << ", Scanned=" << mDataFilter.showScannedEntries << ", NotScanned=" << mDataFilter.showNotScannedEntries;

  mpDbHandler->set_data_filter(mDataFilter);

  _update_remove_invalid_files_button_state();
}

void EnsembleList::_slot_handle_show_current_FId_or_Ch_only(const int iState)
{
  if (mListMode == EListMode::PlayFromFiles)
  {
    Settings::EnsembleList::varShowSLCurChOnlyFMode.write(ui->cbShowSLCurChOnly->isChecked());
  }
  else
  {
    Settings::EnsembleList::varShowSLCurChOnlyChMode.write(ui->cbShowSLCurChOnly->isChecked());
  }

  emit signal_show_current_FId_or_Ch_only(iState == Qt::CheckState::Checked);
}

bool EnsembleList::_get_ident_info_from_row_idx(SIdentInfoEL & oIdentInfo, const i32 iRowIdx)
{
  oIdentInfo.curPacketIdx = iRowIdx;
  oIdentInfo.nrPackets = _get_nr_rows_in_table();
  oIdentInfo.fIdOrCh = _get_FId_or_Ch_from_table_index(iRowIdx);

  if (oIdentInfo.nrPackets == 0)
  {
    assert(mListMode == EListMode::PlayFromFiles);
    _log_to_result_display(ELogType::WARN, QString("No entries in the list, please add one or more files to the list"));
    if (mIsScanning) _stop_scan_process();
    return false;
  }

  const EnsembleListDB::SDbEntryData dbEntry = mpDbHandler->get_entry(oIdentInfo.fIdOrCh);
  oIdentInfo.sIdToPlay = dbEntry.S0Ws.get_sid_played();
  oIdentInfo.curScanLevel = static_cast<EScanLevel>(dbEntry.S0Ws.scanLevel);
  oIdentInfo.scanLevelToAchieve = EScanLevel::SL2_FibData; // TODO:
  oIdentInfo.reReadScanLevel = false;
  oIdentInfo.absPath = dbEntry.S0Ws.get_path(); // only useful in file list mode
  return true;
}

void EnsembleList::_signal_ident_info(const SIdentInfoEL & iIdentInfo)
{
  const QString entryOfStr = (mIsScanning ? QString(" (%1 of %2)").arg(iIdentInfo.curPacketIdx+1).arg(iIdentInfo.nrPackets) : QString());
  const QString scanOrPlayStr = QString(mIsScanning ? "Scanning " : "Playing ");

  _log_to_result_display(ELogType::INFONEUT, scanOrPlayStr + _list_mode_str(QString("channel: %1").arg(iIdentInfo.fIdOrCh),
                                                                            QString("file: %1").arg(iIdentInfo.absPath)) + entryOfStr);

  emit signal_file_or_channel_to_play(iIdentInfo);
}

void EnsembleList::_signal_FId_or_Ch_from_table_index(const i32 iRowIdx, const u32 iSId /*= 0*/)
{
  SIdentInfoEL ii;
  if (!_get_ident_info_from_row_idx(ii, iRowIdx))
  {
    return;
  }

  if (iSId > 0)
  {
    ii.sIdToPlay = iSId;
  }

  mRowIdxClickOnList = (ii.sIdToPlay == 0 && !mIsScanning ? iRowIdx : -1); // triggers a reload with the first SId

  _signal_ident_info(ii);
}

QString EnsembleList::_get_FId_or_Ch_from_table_index(const i32 iRowIdx) const
{
  const QAbstractItemModel * pModel = ui->tblEnsembleList->model();
  return pModel->data(pModel->index(iRowIdx, (mListMode == EListMode::PlayFromFiles ? EnsembleListDB::CI_FId : EnsembleListDB::CI_CH))).toString();
}

const QString & EnsembleList::_list_mode_str(const QString & iTextDeviceMode, const QString & iTextFileMode) const
{
  return (mListMode == EListMode::PlayFromFiles ? iTextFileMode : iTextDeviceMode);
}

void EnsembleList::_slot_handle_table_click(const QModelIndex & index)
{
  if (!index.isValid())
  {
    return;
  }

  mIndentGlobal = 0;
  mFIdOrChLast.clear();

  _setup_ui_regarding_scan_mode(true);

  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  _signal_FId_or_Ch_from_table_index(index.row());
}
