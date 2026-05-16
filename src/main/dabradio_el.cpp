//
// Created by tomneda on 2025-12-30.
//
#include "configuration.h"
#include "dabradio.h"
#include "service-list-handler.h"
#include "ui_dabradio.h"
#include "ensemble-list.h"
#include "setting-helper.h"
#include "dab-tables.h"
#include "itu-regions.h"

void DabRadio::_initialize_ensemble_list()
{
  connect(mpEnsembleList.get(), &EnsembleList::signal_file_or_channel_to_play, this, &DabRadio::_slot_file_or_channel_to_play);
  connect(mpEnsembleList.get(), &EnsembleList::signal_start_stop_scan, this, &DabRadio::_slot_start_stop_scan);
  connect(mpEnsembleList.get(), &EnsembleList::signal_delete_unused_FId_or_Ch, this, [this](const QStringList & iUsedChOrFIdList) {mpServiceListHandler->delete_not_existing_channel(iUsedChOrFIdList); });
  connect(mpEnsembleList.get(), &EnsembleList::signal_show_current_FId_or_Ch_only, this, &DabRadio::_slot_show_current_FId_or_Ch_only);
  connect(this, &DabRadio::signal_fib_data_status, mpEnsembleList.get(), &EnsembleList::slot_decoded_data_status);
  connect(this, &DabRadio::signal_FId_or_Ch_selected, mpEnsembleList.get(), &EnsembleList::slot_select_FId_or_Ch);
  connect(ui->btnEnsembleList, &QPushButton::clicked, this, &DabRadio::_slot_handle_ensemble_list_button);

  mpEnsembleList->init_after_connect();
}

// This is called by the EnsembleList
void DabRadio::_slot_file_or_channel_to_play(const SIdentInfoEL & iIdentInfo)
{
  qDebug() << "Playing channel/FId" << iIdentInfo.fIdOrCh << "with SId" << QString("%1").arg(iIdentInfo.sIdToPlay, 4, 16, QChar('0')) << "file path" << iIdentInfo.absPath;

  const bool fIdOrChHasChanged = mChannelDesc.set_ident_info(iIdentInfo);

  if (mIsFileMode)
  {
    if (mpInputDevice == nullptr || mpDabProcessor == nullptr || fIdOrChHasChanged) // if the file has changed
    {
      qDebug() << "New file selected with FId" << iIdentInfo.fIdOrCh << "and path" << iIdentInfo.absPath;
      _create_new_input_device_and_dab_processor(iIdentInfo.absPath);
    }
  }
  else  // device mode
  {
    if (mIsChannelRunning && fIdOrChHasChanged)
    {
      _stop_channel(); // stops also the DAB processor (but not deleted it), mIsChannelRunning -> false
      mChannelDesc.clean_channel();
      assert(!mIsChannelRunning);
    }
  }

  if (mpInputDevice == nullptr)
  {
    if (mIsFileMode)
    {
      _inform_ensemble_list(mChannelDesc.get_ident_info(), EInfoReason::InvalidFile);
      return;
    }
    qWarning() << "No input device found. Is a device selected?";
    return;
  }
  assert(mpDabProcessor != nullptr);

  mScanSecurityTimer.start();

  _start_playing(iIdentInfo.fIdOrCh, iIdentInfo.sIdToPlay, !fIdOrChHasChanged);

  if (mIsScanning)
  {
    ui->lblDynLabel->setText(_get_scan_message(false));
  }
}

// This is called by the EnsembleList to start or stop a scan (both for device and file scan)
void DabRadio::_slot_start_stop_scan(const bool iIsScanning)
{
  if (!mIsChannelRunning)
  {
    return;
  }

  if (iIsScanning && !mIsScanning)
  {
    _start_scanning(mChannelDesc);
  }
  else if (!iIsScanning && mIsScanning)
  {
    _stop_scanning(mChannelDesc);
  }
}

// This is called (repeatedly) after receiving FIB data, it checks if addition waiting time is necessary to gain further data
void DabRadio::_slot_ensemble_list_retrigger_timeout()
{
  // First send the already collected FIB data
  if (!mChannelDesc.fibDataSentToEL)
  {
    mChannelDesc.fibDataSentToEL = true;
    _collect_fib_data_and_emit_to_ensemble_list(mChannelDesc);
  }

  // Second, try to send deferred data
  if (!_collect_deferred_data_and_emit_to_ensemble_list(mChannelDesc, false))
  {
    // still waiting for data... wait a bit longer for FIB data
    mEnsListRetriggerCnt++;
    if (mEnsListRetriggerCnt > 5)
    {
      qWarning() << "FIB data timeout, giving up";
      (void)_collect_deferred_data_and_emit_to_ensemble_list(mChannelDesc, true);
      return;
    }
    mEnsListRetriggerTimer.start(1000); // wait a bit longer for FIB data
  }
}

void DabRadio::_collect_fib_data_and_emit_to_ensemble_list(const SChannelDescriptor & iChannelDesc) const
{
  const auto * const fibDec = mpDabProcessor->get_fib_decoder();
  const auto & idi = iChannelDesc.get_ident_info();
  assert(fibDec != nullptr);
  assert(idi.nrPackets > 0);
  assert(idi.curPacketIdx < idi.nrPackets);

  const std::vector<SServiceId> serviceList = fibDec->get_service_list();

  u32 curUsedSId = mCurPrimaryAudioService.SId;

  if (serviceList.empty())
  {
    qWarning() << "FIB service list is empty";
  }
  else if (curUsedSId == 0)
  {
    qWarning() << "FIB service list is not empty but no primary audio service is set";
    // search for first audio SId in serviceList
    for (const auto & sl : serviceList)
    {
      if (sl.isAudioChannel)
      {
        curUsedSId = sl.SId;
        break;
      }
    }
  }

  SScanResultEL sr{};
  sr.infoReason = EInfoReason::NewFib;
  sr.curPacketIdx = idi.curPacketIdx;
  sr.nrPackets = idi.nrPackets;
  sr.key.fIdOrCh = iChannelDesc.get_fId_or_ch();
  const QFileInfo fileInfo(iChannelDesc.get_ident_info().absPath);
  sr.S0Ws.fileName = fileInfo.fileName();
  sr.S0Ws.filePath = fileInfo.absolutePath();
  sr.S0Ws.set_sid_played(curUsedSId);

  i32 numDab = 0;
  i32 numDabPlus = 0;
  QSet<QString> protLevSet;
  std::set<i16> dataRateSet; // std::set is sorted instead of QSet
  std::set<u32> dscTyAppTySet; // store DSCTy and AppTY in one set for common sorting

  for (const auto & sl : serviceList)
  {
    if (sl.isAudioChannel)
    {
      SAudioData ad;
      fibDec->get_data_for_audio_service(sl.SId, ad);

      if (ad.isDefined == true)
      {
        if (ad.ASCTy == 0x3F) numDabPlus++;
        else                  numDab++;
        
        protLevSet.insert(getProtectionLevel(ad.shortForm, ad.protLevel));
        dataRateSet.insert(ad.bitRate);
      }
    }
    else // data packet
    {
      std::vector<SPacketData> pdVec;
      fibDec->get_data_for_packet_service(sl.SId, pdVec);

      for (const auto & pd : pdVec)
      {
        protLevSet.insert(getProtectionLevel(pd.shortForm, pd.protLevel));
        dscTyAppTySet.insert((pd.DSCTy << 16) | pd.appTypeVec.front()); // only us first AppType element (is usually only one)
      }
    }
  }

  QStringList protLevList = protLevSet.values();
  protLevList.sort();
  sr.S1Fib.errorProtection = protLevList.join("|");

  QStringList dataRateList;
  for (const auto & rate : dataRateSet) dataRateList.append(QString::number(rate));
  sr.S1Fib.audioDataRates = dataRateList.join("|");

  QStringList dscTyAppTyList;
  for (const auto & dscTyAppTy : dscTyAppTySet) dscTyAppTyList.append(QString("%1[%2]").arg(dscTyAppTy >> 16).arg(dscTyAppTy & 0xFFFF));
  sr.S1Fib.dscTyAppTy = dscTyAppTyList.join("|");
  if (sr.S1Fib.dscTyAppTy.isEmpty()) sr.S1Fib.dscTyAppTy = "-";

  sr.S1Fib.numDabDabPlus = QString("%1|%2").arg(numDab, 2, 10, QChar('0')).arg(numDabPlus, 2, 10, QChar('0'));

  emit signal_fib_data_status(sr); // this fills up the EnsembleList database
}

bool DabRadio::_collect_deferred_data_and_emit_to_ensemble_list(const SChannelDescriptor & iChannelDesc, const bool iForceSend) const
{
  if (!iChannelDesc.deferredData.countryName.has_value() ||
      !iChannelDesc.deferredData.snr.has_value() ||
      !iChannelDesc.deferredData.mer.has_value() ||
      !iChannelDesc.deferredData.nomFreqkHz.has_value() ||
      !iChannelDesc.deferredData.bbOffset.has_value())
  {
    qDebug() << "Deferred data is not set yet, wait a bit longer for FIB data for FId/Ch" << iChannelDesc.get_fId_or_ch() << "( count" << mEnsListRetriggerCnt << ")";
    if (!iForceSend) return false;
  }

  const auto * const fibDec = mpDabProcessor->get_fib_decoder();
  assert(fibDec != nullptr);

  const u32 EId = fibDec->get_EId();
  const QString ensembleName = fibDec->get_ensemble_name();

  if (EId == 0 || ensembleName.isEmpty())
  {
    qDebug() << "FIB ensemble name is empty, wait a bit longer for FIB data";
    if (!iForceSend) return false;
  }

  const u32 mjd = fibDec->get_mod_julian_date();

  if (mjd == 0)
  {
    qDebug() << "FIB date is not set yet, wait a bit longer for FIB data";
    if (!iForceSend) return false;
  }

  const auto & idi = iChannelDesc.get_ident_info();
  assert(idi.nrPackets > 0);
  assert(idi.curPacketIdx < idi.nrPackets);

  SScanResultEL sr{};
  sr.infoReason = EInfoReason::DeferredData;
  sr.curPacketIdx = idi.curPacketIdx;
  sr.nrPackets = idi.nrPackets;
  sr.key.fIdOrCh = iChannelDesc.get_fId_or_ch();
  const QFileInfo fileInfo(iChannelDesc.get_ident_info().absPath);
  sr.S0Ws.fileName = fileInfo.fileName();
  sr.S0Ws.filePath = fileInfo.absolutePath();

  sr.S2MedRun.ensembleName = ensembleName;
  sr.S2MedRun.ensembleId = QString("%1").arg(EId, 4, 16, QChar('0'));
  sr.S2MedRun.ituCountry = iChannelDesc.deferredData.countryName.value_or("-");
  sr.S2MedRun.mer = std::round(iChannelDesc.deferredData.mer.value_or(iChannelDesc.merLast) * 10) / 10.0;
  sr.S2MedRun.snr = std::round(iChannelDesc.deferredData.snr.value_or(iChannelDesc.snrLast) * 10) / 10.0;
  sr.S2MedRun.basebandOffset = iChannelDesc.deferredData.bbOffset.value_or(0);
  sr.S2MedRun.nomFreqkHz = iChannelDesc.deferredData.nomFreqkHz.value_or(0);

  // get UTC date from FIB
  if (mjd == 0)
  {
    qDebug() << "FIB date is not set";
    sr.S2MedRun.dateUtc = "-";
  }
  else
  {
    i32 year, month, day;
    _get_YMD_from_mod_julian_date(year, month, day, mjd);
    sr.S2MedRun.dateUtc = QDate(year, month, day).toString("yyyy-MM-dd");
  }

  qDebug() << "Sending deferred data to ensemble list, ForceSend" << iForceSend;
  emit signal_fib_data_status(sr); // this fills up the EnsembleList database

  return true;
}

// This is called if there was a failure while scanning
void DabRadio::_inform_ensemble_list(const SIdentInfoEL & iIdentInfoEL, const EInfoReason iInfoReason)
{
  assert(!mIsScanning || iIdentInfoEL.nrPackets > 0);
  assert(!mIsScanning || iIdentInfoEL.curPacketIdx < iIdentInfoEL.nrPackets);
  mEnsListRetriggerTimer.stop();

  SScanResultEL sr{};
  sr.infoReason = iInfoReason;
  sr.curPacketIdx = iIdentInfoEL.curPacketIdx;
  sr.nrPackets = iIdentInfoEL.nrPackets;
  sr.key.fIdOrCh = iIdentInfoEL.fIdOrCh;
  const QFileInfo fileInfo(iIdentInfoEL.absPath);
  sr.S0Ws.fileName = fileInfo.fileName();
  sr.S0Ws.filePath = fileInfo.absolutePath();
  sr.S0Ws.set_sid_played(mCurPrimaryAudioService.SId);

  emit signal_fib_data_status(sr);
}

void DabRadio::_slot_handle_ensemble_list_button() const
{
  if (mpEnsembleList == nullptr)
  {
    return;
  }

  mpEnsembleList->setVisible(mpEnsembleList->is_hidden());
}

void DabRadio::_slot_show_current_FId_or_Ch_only(const bool iShowOnlyCurrentFIdOrCh) const
{
  mpServiceListHandler->set_sort_to_FId_Or_Ch(iShowOnlyCurrentFIdOrCh);
  mpServiceListHandler->set_selector(mChannelDesc.get_fId_or_ch(), mChannelDesc.get_sId_next());
}

void DabRadio::_slot_service_list_src_change(int iIdClicked)
{
  assert(mpEnsembleList != nullptr);
  // const EServiceListSrc src = static_cast<EServiceListSrc>(qobject_cast<QButtonGroup *>(sender())->checkedId());
  mEnsListServiceListSrc = static_cast<EServiceListSrc>(iIdClicked);

  // Handle the service list source change based on the selected radio button
  switch (mEnsListServiceListSrc)
  {
  case EServiceListSrc::DEVICE_PLAYER:
  {
    // Switch to device mode
    mIsFileMode = false;
    mpTiiManager->set_file_mode(false);
    // mChannelDesc.set_to_file_mode(false);
    _adapt_gui_for_device_or_file_play(true);
    _create_new_input_device_and_dab_processor(mChannelDesc.get_device_or_file_name());
    ui->cmbDeviceSelect->setEnabled(true);
    connect(ui->cmbDeviceSelect, &QComboBox::textActivated, this, &DabRadio::_slot_device_selected);
    mpEnsembleList->set_list_mode(EnsembleList::EListMode::PlayFromDevice);
    mpServiceListHandler->set_data_mode(ServiceListHandler::EDataMode::DevicePlayer);
    const QString ch = Settings::Main::varPresetCh.read().toString();
    const u32 sIdNext = Settings::Main::varPresetCSId.read().toUInt();
    emit signal_FId_or_Ch_selected(ch, sIdNext);
    break;
  }

  case EServiceListSrc::FILE_PLAYER:
  {
    // Switch to file player mode
    mIsFileMode = true;
    mpTiiManager->set_file_mode(true);
    _adapt_gui_for_device_or_file_play(false);
    disconnect(ui->cmbDeviceSelect, &QComboBox::textActivated, this, &DabRadio::_slot_device_selected);
    // _create_new_input_device_and_dab_processor is called later as the file path is not known yet, but we can stop current processes
    _clean_up_dab_processor_and_input_device();
    mpEnsembleList->set_list_mode(EnsembleList::EListMode::PlayFromFiles);
    mpServiceListHandler->set_data_mode(ServiceListHandler::EDataMode::FilePlayer);
    const QString fId = Settings::Main::varPresetFId.read().toString();
    const u32 sIdNext = Settings::Main::varPresetFSId.read().toUInt();
    emit signal_FId_or_Ch_selected(fId, sIdNext);
    break;
  }
  }

  Settings::Main::varDeviceFilePlayerId.write(iIdClicked);
}
