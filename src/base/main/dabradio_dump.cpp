//
// Dump management: ETI/source dump handling and dump timer display.
//
#include "dabradio.h"
#include "ui_dabradio.h"
#include "setting_helper.h"
#include "configuration.h"
#include "audio_manager.h"
#include "gui_helpers.h"

QString DabRadio::_seconds_to_timestring(const u32 iTimer) const
{
  // compact format to fit into the small dump buttons: "mm:ss", or "1h05" from one hour on
  if (iTimer < 3600)
  {
    return QString::asprintf("%02d:%02d", iTimer / 60, iTimer % 60);
  }
  return QString::asprintf("%dh%02d", iTimer / 3600, (iTimer / 60) % 60);
}

void DabRadio::_set_dump_button_emphasized(IndicatorButton * const ipButton, const bool iEmphasized, const u32 iBaseColor) const
{
  ipButton->setStyleSheet(iEmphasized ? get_active_dump_style_sheet() : get_bg_style_sheet(iBaseColor));
  ipButton->setFixedSize(QSize(32, 32));
}

void DabRadio::_slot_update_time_display()
{
  if (!mIsChannelRunning)
  {
    return;
  }

#if 0 && !defined(NDEBUG)
  if (mResetRingBufferCnt > 5) // wait 5 seconds to start
  {
    sRingBufferFactoryUInt8.print_status(false);
    sRingBufferFactoryInt16.print_status(false);
    sRingBufferFactoryCmplx16.print_status(false);
    sRingBufferFactoryFloat.print_status(false);
    sRingBufferFactoryCmplx.print_status(false);
  }
  else
  {
    // reset min/max state
    sRingBufferFactoryUInt8.print_status(true);
    sRingBufferFactoryInt16.print_status(true);
    sRingBufferFactoryCmplx16.print_status(true);
    sRingBufferFactoryFloat.print_status(true);
    sRingBufferFactoryCmplx.print_status(true);

    ++mResetRingBufferCnt;
  }
#endif
  if (mDumpStatus.rawDumpActive)
  {
    ui->btnDumpRaw->set_sub_text(_seconds_to_timestring(mRawDumpTimer++));
  }
  if (mDumpStatus.etiDumpActive)
  {
    ui->btnDumpEti->set_sub_text(_seconds_to_timestring(mEtiDumpTimer++));
  }
  mpAudioManager->update_dump_timers();
}

void DabRadio::_start_source_dumping(const SChannelDescriptor & iChannelDesc, SDumpStatus & ioDumpStatus)
{
  if (mIsScanning)
  {
    return;
  }

  const bool useNativeIqFormat = Settings::Config::cbUseNativeIqFormat.read().toBool();
  if (mpInputDevice->hasDump() && useNativeIqFormat)
  {
    ioDumpStatus.rawDumpActive = mpInputDevice->startDumping();
    if (!ioDumpStatus.rawDumpActive)
    {
      return;
    }
  }
  else
  {
    ioDumpStatus.pRawDumper = mOpenFileDialog.open_raw_dump_sndfile_ptr(mpInputDevice->deviceName(), iChannelDesc.get_fId_or_ch_descr());
    if (ioDumpStatus.pRawDumper == nullptr)
    {
      return;
    }
    mpDabProcessor->start_dumping(ioDumpStatus.pRawDumper);
  }
  ioDumpStatus.rawDumpActive = true;
  mRawDumpTimer = 0;
  _set_dump_button_emphasized(ui->btnDumpRaw, true, cRawDumpColor);
}

void DabRadio::_stop_source_dumping(SDumpStatus & ioDumpStatus) const
{
  if (!ioDumpStatus.rawDumpActive)
  {
    return;
  }

  mpInputDevice->stopDumping();
  if (ioDumpStatus.pRawDumper != nullptr)
  {
    mpDabProcessor->stop_dumping();
    sf_close(ioDumpStatus.pRawDumper);
  }
  ioDumpStatus.rawDumpActive = false;
  ioDumpStatus.pRawDumper = nullptr;
  _set_dump_button_emphasized(ui->btnDumpRaw, false, cRawDumpColor);
  ui->btnDumpRaw->set_sub_text({});
}

void DabRadio::_slot_handle_source_dump_button()
{
  if (!mIsChannelRunning || mIsScanning)
  {
    return;
  }

  if (mDumpStatus.rawDumpActive)
  {
    _stop_source_dumping(mDumpStatus);
  }
  else
  {
    _start_source_dumping(mChannelDesc, mDumpStatus);
  }
}

void DabRadio::_slot_handle_eti_button()
{
  if (!mIsChannelRunning || mIsScanning)
  {
    return;
  }
  if (mpDabProcessor == nullptr)
  {  // should not happen
    return;
  }

  if (mDumpStatus.etiDumpActive)
  {
    _stop_eti_handler(mDumpStatus);
  }
  else
  {
    _start_eti_handler(mChannelDesc, mDumpStatus);
  }
}

void DabRadio::_start_eti_handler(const SChannelDescriptor & iChannelDesc, SDumpStatus & ioDumpStatus)
{
  if (ioDumpStatus.etiDumpActive)
  {
    return;
  }

  const QString etiFile = mOpenFileDialog.get_eti_file_name(iChannelDesc.ensembleName, iChannelDesc.get_fId_or_ch());

  if (etiFile.isEmpty())
  {
    return;
  }

  ioDumpStatus.etiDumpActive = mpDabProcessor->start_eti_generator(etiFile);

  if (ioDumpStatus.etiDumpActive)
  {
    _set_dump_button_emphasized(ui->btnDumpEti, true, cEtiDumpColor);
    mEtiDumpTimer = 0;
  }
}

void DabRadio::_stop_eti_handler(SDumpStatus & ioDumpStatus) const
{
  if (!ioDumpStatus.etiDumpActive)
  {
    return;
  }

  mpDabProcessor->stop_eti_generator();
  ioDumpStatus.etiDumpActive = false;

  _set_dump_button_emphasized(ui->btnDumpEti, false, cEtiDumpColor);
  ui->btnDumpEti->set_sub_text({});
}
