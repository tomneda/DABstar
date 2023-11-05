/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2014 .. 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    Copyright (C) 2019 Amplifier, antenna and ppm correctors
 *    Fabio Capozzi
 *
 *    This file is part of Qt-DAB
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation version 2 of the License.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <QThread>
#include <QSettings>
#include <QTime>
#include <QDate>
#include <QLabel>
#include <QDebug>
#include <QFileDialog>
#include "hackrf-handler.h"
#include "xml-filewriter.h"
#include "device-exceptions.h"
#include <string.h>

#define CHECK_ERR_RETURN(x_)              if (!check_err(x_, __FUNCTION__, __LINE__)) return
#define CHECK_ERR_RETURN_FALSE(x_)        if (!check_err(x_, __FUNCTION__, __LINE__)) return false
#define LOAD_METHOD_RETURN_FALSE(m_, n_)  if (!load_method(m_, n_, __LINE__)) return false

constexpr int32_t DEFAULT_LNA_GAIN = 16;
constexpr int32_t DEFAULT_VGA_GAIN = 30;

HackRfHandler::HackRfHandler(QSettings * iSetting, const QString & iRecorderVersion) :
  mpHackrfSettings(iSetting),
  mRecorderVersion(iRecorderVersion)
{
  mpHackrfSettings->beginGroup("hackrfSettings");
  int x = mpHackrfSettings->value("position-x", 100).toInt();
  int y = mpHackrfSettings->value("position-y", 100).toInt();
  mpHackrfSettings->endGroup();

  setupUi(&myFrame);

  myFrame.move(QPoint(x, y));
  myFrame.show();

#ifdef  __MINGW32__
  const char *libraryString = "libhackrf.dll";
#elif __linux__
  const char * libraryString = "libhackrf.so";
#elif __APPLE__
  const char *libraryString = "libhackrf.dylib";
#endif

  mpHandle = new QLibrary(libraryString);
  mpHandle->load();

  if (!mpHandle->isLoaded())
  {
    throw (hackrf_exception("failed to open " + std::string(libraryString)));
  }

  if (!load_hackrf_functions())
  {
    delete mpHandle;
    throw (hackrf_exception("could not find one or more library functions"));
  }

  //	From here we have a library available

  mVfoFreqHz = kHz(220000);

  //	See if there are settings from previous incarnations
  mpHackrfSettings->beginGroup("hackrfSettings");
  sliderLnaGain->setValue(mpHackrfSettings->value("hack_lnaGain", DEFAULT_LNA_GAIN).toInt());
  sliderVgaGain->setValue(mpHackrfSettings->value("hack_vgaGain", DEFAULT_VGA_GAIN).toInt());
  bool isChecked = mpHackrfSettings->value("hack_AntEnable", false).toBool();
  btnBiasTEnable->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
  isChecked = mpHackrfSettings->value("hack_AmpEnable", false).toBool();
  btnAmpEnable->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
  ppm_correction->setValue(mpHackrfSettings->value("hack_ppmCorrection", 0).toInt());
  save_gain_settings = mpHackrfSettings->value("save_gainSettings", 1).toInt() != 0;
  mpHackrfSettings->endGroup();

  mRefFreqErrFac = 1.0f + (float)ppm_correction->value() / 1.0e6f;

  check_err_throw(mHackrf.init());
  check_err_throw(mHackrf.open(&theDevice));
  check_err_throw(mHackrf.set_sample_rate(theDevice, OVERSAMPLING * (2048000.0 * mRefFreqErrFac)));
  check_err_throw(mHackrf.set_baseband_filter_bandwidth(theDevice, 1750000)); // the filter must be set after the sample rate to be not overwritten
  check_err_throw(mHackrf.set_freq(theDevice, 220000000));

  slot_set_lna_gain(sliderLnaGain->value());
  slot_set_vga_gain(sliderVgaGain->value());
  slot_enable_bias_t(1); // value is a dummy
  slot_enable_amp(1);    // value is a dummy

  if (hackrf_device_list_t const * deviceList = mHackrf.device_list();
      deviceList != nullptr)
  {
    std::stringstream swInfo;
    swInfo << "Ver: " << mHackrf.library_version() << ", " << "Rel: " << mHackrf.library_release();
    lblSwInfo->setText(swInfo.str().c_str());

    char const * pSerial = deviceList->serial_numbers[0];
    while (*pSerial == '0') ++pSerial; // remove leading '0'

    uint8_t revNo = 0;
    check_err_throw(mHackrf.board_rev_read(theDevice, &revNo));
    mRevNo = (hackrf_board_rev)revNo;

    std::stringstream devInfo;
    devInfo << "SN: " << pSerial << ", Rev: " << mHackrf.board_rev_name(mRevNo);
    lblDeviceInfo->setText(devInfo.str().c_str());

    enum hackrf_usb_board_id board_id = deviceList->usb_board_ids[0];
    lblDeviceName->setText(mHackrf.usb_board_id_name(board_id));
  }

  //	and be prepared for future changes in the settings
  connect(sliderLnaGain, &QSlider::valueChanged, this, &HackRfHandler::slot_set_lna_gain);
  connect(sliderVgaGain, &QSlider::valueChanged, this, &HackRfHandler::slot_set_vga_gain);
  connect(btnBiasTEnable, &QCheckBox::stateChanged, this, &HackRfHandler::slot_enable_bias_t);
  connect(btnAmpEnable, &QCheckBox::stateChanged, this, &HackRfHandler::slot_enable_amp);
  connect(ppm_correction, qOverload<int>(&QSpinBox::valueChanged), this, &HackRfHandler::slot_set_ppm_correction);
  connect(dumpButton, &QPushButton::clicked, this, &HackRfHandler::slot_xml_dump);

  connect(this, &HackRfHandler::signal_new_ant_enable, btnBiasTEnable, &QCheckBox::setChecked);
  connect(this, &HackRfHandler::signal_new_amp_enable, btnAmpEnable, &QCheckBox::setChecked);
  connect(this, &HackRfHandler::signal_new_vga_value, sliderVgaGain, &QSlider::setValue);
  connect(this, &HackRfHandler::signal_new_vga_value, vgagainDisplay, qOverload<int>(&QLCDNumber::display));
  connect(this, &HackRfHandler::signal_new_lna_value, sliderLnaGain, &QSlider::setValue);
  connect(this, &HackRfHandler::signal_new_lna_value, lnagainDisplay, qOverload<int>(&QLCDNumber::display));

  mpXmlDumper = nullptr;
  mDumping.store(false);
  mRunning.store(false);
}

HackRfHandler::~HackRfHandler()
{
  stopReader();
  myFrame.hide();
  mpHackrfSettings->beginGroup("hackrfSettings");
  mpHackrfSettings->setValue("position-x", myFrame.pos().x());
  mpHackrfSettings->setValue("position-y", myFrame.pos().y());
  mpHackrfSettings->setValue("hack_lnaGain", sliderLnaGain->value());
  mpHackrfSettings->setValue("hack_vgaGain", sliderVgaGain->value());
  mpHackrfSettings->setValue("hack_AntEnable", btnBiasTEnable->checkState() == Qt::Checked);
  mpHackrfSettings->setValue("hack_AmpEnable", btnAmpEnable->checkState() == Qt::Checked);
  mpHackrfSettings->setValue("hack_ppmCorrection", ppm_correction->value());
  mpHackrfSettings->endGroup();

  mHackrf.close(theDevice);
  mHackrf.exit();
}

void HackRfHandler::setVFOFrequency(int32_t newFrequency)
{
  CHECK_ERR_RETURN(mHackrf.set_freq(theDevice, newFrequency * mRefFreqErrFac));

  //	It seems that after changing the frequency, the preamp is switched off
  //	(tomneda: do not see this, I guess the bug is already fixed, but leave the workaround)
  if (btnAmpEnable->checkState() == Qt::Checked)
  {
    slot_enable_amp(1);
  }

  mVfoFreqHz = newFrequency;
}

int32_t HackRfHandler::getVFOFrequency()
{
  return mVfoFreqHz;
}

void HackRfHandler::slot_set_lna_gain(int newGain)
{
  if (newGain >= 0 && newGain <= 40)
  {
    CHECK_ERR_RETURN(mHackrf.set_lna_gain(theDevice, newGain));
    lnagainDisplay->display(newGain);
  }
}

void HackRfHandler::slot_set_vga_gain(int newGain)
{
  if (newGain >= 0 && newGain <= 62)
  {
    CHECK_ERR_RETURN(mHackrf.set_vga_gain(theDevice, newGain));
    vgagainDisplay->display(newGain);
  }
}

void HackRfHandler::slot_enable_bias_t(int d)
{
  (void)d;
  CHECK_ERR_RETURN(mHackrf.set_antenna_enable(theDevice, btnBiasTEnable->isChecked() ? 1 : 0));
}

void HackRfHandler::slot_enable_amp(int a)
{
  (void)a;
  CHECK_ERR_RETURN(mHackrf.set_amp_enable(theDevice, btnAmpEnable->isChecked() ? 1 : 0));
}

void HackRfHandler::slot_set_ppm_correction(int32_t ppm)
{
  // writing in the si5351 (Ref clock generator) register does not seem to work, so do it other way...
  mRefFreqErrFac = 1.0f + (float)ppm / 1.0e6f;

  CHECK_ERR_RETURN(mHackrf.set_sample_rate(theDevice, OVERSAMPLING * (2048000.0 * mRefFreqErrFac)));
  CHECK_ERR_RETURN(mHackrf.set_baseband_filter_bandwidth(theDevice, 1750000)); // the filter must be set after the sample rate to be not overwritten
  stopReader();
  restartReader(getVFOFrequency());
}

//	we use a static large buffer, rather than trying to allocate a buffer on the stack
static std::array<std::complex<int8_t>, 32 * 32768> buffer;

static int callback(hackrf_transfer * transfer)
{
  auto * ctx = static_cast<HackRfHandler *>(transfer->rx_ctx);
  const int8_t * const p = reinterpret_cast<const int8_t * const>(transfer->buffer);
  HackRfHandler::TRingBuffer * q = &(ctx->mRingBuffer);
  int bufferIndex = 0;

  for (int i = 0; i < transfer->valid_length / 2; ++i)
  {
    const cmplx x = cmplx(p[2 * i + 0], p[2 * i + 1]);
    cmplx y;
#ifdef HAVE_HBF
    if (!ctx->mHbf.decimate(x, y))
    {
      continue;
    }
#else
    y = x;
#endif
    assert(bufferIndex < (signed)buffer.size());
    buffer[bufferIndex] = std::complex<int8_t>((int8_t)(y.real()), (int8_t)(y.imag()));
    ++bufferIndex;
  }
  q->putDataIntoBuffer(buffer.data(), bufferIndex);
  return 0;
}

bool HackRfHandler::restartReader(int32_t iFreqHz)
{
  if (mRunning.load())
  {
    return true;
  }

  mVfoFreqHz = iFreqHz;

  if (save_gain_settings)
  {
    update_gain_settings(iFreqHz / MHz(1));
  }

  CHECK_ERR_RETURN_FALSE(mHackrf.set_lna_gain(theDevice, sliderLnaGain->value()));
  CHECK_ERR_RETURN_FALSE(mHackrf.set_vga_gain(theDevice, sliderVgaGain->value()));
  CHECK_ERR_RETURN_FALSE(mHackrf.set_amp_enable(theDevice, btnAmpEnable->isChecked() ? 1 : 0));
  CHECK_ERR_RETURN_FALSE(mHackrf.set_antenna_enable(theDevice, btnBiasTEnable->isChecked() ? 1 : 0));

  CHECK_ERR_RETURN_FALSE(mHackrf.set_freq(theDevice, iFreqHz * mRefFreqErrFac));
  CHECK_ERR_RETURN_FALSE(mHackrf.start_rx(theDevice, callback, this));
  mRunning.store(mHackrf.is_streaming(theDevice));
  return mRunning.load();
}

void HackRfHandler::stopReader()
{
  if (!mRunning.load())
  {
    return;
  }

  CHECK_ERR_RETURN(mHackrf.stop_rx(theDevice));

  if (save_gain_settings)
  {
    record_gain_settings(mVfoFreqHz / MHz (1));
  }

  mRunning.store(false);
}

//	The brave old getSamples. For the hackrf, we get
//	size still in I/Q pairs
int32_t HackRfHandler::getSamples(cmplx * V, int32_t size)
{
  std::complex<int8_t> temp[size];
  int amount = mRingBuffer.getDataFromBuffer(temp, size);

  for (int i = 0; i < amount; i++)
  {
    V[i] = cmplx((float)temp[i].real() / 127.0f, (float)temp[i].imag() / 127.0f);
  }

  if (mDumping.load())
  {
    mpXmlWriter->add(temp, amount);
  }

  return amount;
}

int32_t HackRfHandler::Samples()
{
  return mRingBuffer.GetRingBufferReadAvailable();
}

void HackRfHandler::resetBuffer()
{
  mRingBuffer.FlushRingBuffer();
}

int16_t HackRfHandler::bitDepth()
{
  return 8;
}

QString HackRfHandler::deviceName()
{
  return "hackRF";
}

bool HackRfHandler::load_hackrf_functions()
{
  LOAD_METHOD_RETURN_FALSE(mHackrf.init, "hackrf_init");
  LOAD_METHOD_RETURN_FALSE(mHackrf.open, "hackrf_open");
  LOAD_METHOD_RETURN_FALSE(mHackrf.close, "hackrf_close");
  LOAD_METHOD_RETURN_FALSE(mHackrf.exit, "hackrf_exit");
  LOAD_METHOD_RETURN_FALSE(mHackrf.start_rx, "hackrf_start_rx");
  LOAD_METHOD_RETURN_FALSE(mHackrf.stop_rx, "hackrf_stop_rx");
  LOAD_METHOD_RETURN_FALSE(mHackrf.device_list, "hackrf_device_list");
  LOAD_METHOD_RETURN_FALSE(mHackrf.set_baseband_filter_bandwidth, "hackrf_set_baseband_filter_bandwidth");
  LOAD_METHOD_RETURN_FALSE(mHackrf.set_lna_gain, "hackrf_set_lna_gain");
  LOAD_METHOD_RETURN_FALSE(mHackrf.set_vga_gain, "hackrf_set_vga_gain");
  LOAD_METHOD_RETURN_FALSE(mHackrf.set_freq, "hackrf_set_freq");
  LOAD_METHOD_RETURN_FALSE(mHackrf.set_sample_rate, "hackrf_set_sample_rate");
  LOAD_METHOD_RETURN_FALSE(mHackrf.is_streaming, "hackrf_is_streaming");
  LOAD_METHOD_RETURN_FALSE(mHackrf.error_name, "hackrf_error_name");
  LOAD_METHOD_RETURN_FALSE(mHackrf.usb_board_id_name, "hackrf_usb_board_id_name");
  LOAD_METHOD_RETURN_FALSE(mHackrf.set_antenna_enable, "hackrf_set_antenna_enable");
  LOAD_METHOD_RETURN_FALSE(mHackrf.set_amp_enable, "hackrf_set_amp_enable");
  LOAD_METHOD_RETURN_FALSE(mHackrf.si5351c_read, "hackrf_si5351c_read");
  LOAD_METHOD_RETURN_FALSE(mHackrf.si5351c_write, "hackrf_si5351c_write");
  LOAD_METHOD_RETURN_FALSE(mHackrf.board_rev_read, "hackrf_board_rev_read");
  LOAD_METHOD_RETURN_FALSE(mHackrf.board_rev_name, "hackrf_board_rev_name");
  LOAD_METHOD_RETURN_FALSE(mHackrf.version_string_read, "hackrf_version_string_read");
  LOAD_METHOD_RETURN_FALSE(mHackrf.library_release, "hackrf_library_release");
  LOAD_METHOD_RETURN_FALSE(mHackrf.library_version, "hackrf_library_version");

  fprintf(stdout, "HackRf functions loaded\n");
  return true;
}


void HackRfHandler::slot_xml_dump()
{
  if (mpXmlDumper == nullptr)
  {
    if (setup_xml_dump())
    {
      dumpButton->setText("writing");
    }
  }
  else
  {
    close_xml_dump();
    dumpButton->setText("Dump");
  }
}

static inline bool isValid(QChar c)
{
  return c.isLetterOrNumber() || (c == '-');
}

bool HackRfHandler::setup_xml_dump()
{
  QString saveDir = mpHackrfSettings->value("saveDir_xmlDump", QDir::homePath()).toString();

  if ((saveDir != "") && (!saveDir.endsWith("/")))
  {
    saveDir += "/";
  }

  QString channel = mpHackrfSettings->value("channel", "xx").toString();
  QString timeString = QDate::currentDate().toString() + "-" + QTime::currentTime().toString();

  for (int i = 0; i < timeString.length(); i++)
  {
    if (!isValid(timeString.at(i)))
    {
      timeString.replace(i, 1, '-');
    }
  }

  QString suggestedFileName = saveDir + "hackrf" + "-" + channel + "-" + timeString;
  QString fileName = QFileDialog::getSaveFileName(nullptr, tr("Save file ..."), suggestedFileName + ".uff", tr("Xml (*.uff)"));
  fileName = QDir::toNativeSeparators(fileName);
  mpXmlDumper = fopen(fileName.toUtf8().data(), "w");
  if (mpXmlDumper == nullptr)
  {
    return false;
  }

  mpXmlWriter = new xml_fileWriter(mpXmlDumper, 8, "int8", 2048000, getVFOFrequency(), "Hackrf", "--", mRecorderVersion);
  mDumping.store(true);

  QString dumper = QDir::fromNativeSeparators(fileName);
  int x = dumper.lastIndexOf("/");
  saveDir = dumper.remove(x, dumper.count() - x);
  mpHackrfSettings->setValue("saveDir_xmlDump", saveDir);

  return true;
}

void HackRfHandler::close_xml_dump()
{
  if (mpXmlDumper == nullptr)
  {  // this can happen !!
    return;
  }
  mDumping.store(false);
  usleep(1000);
  mpXmlWriter->computeHeader();
  delete mpXmlWriter;
  fclose(mpXmlDumper);
  mpXmlDumper = nullptr;
}

void HackRfHandler::show()
{
  myFrame.show();
}

void HackRfHandler::hide()
{
  myFrame.hide();
}

bool HackRfHandler::isHidden()
{
  return myFrame.isHidden();
}

void HackRfHandler::record_gain_settings(int freq)
{
  int vgaValue;
  int lnaValue;
  int ampEnable;
  QString theValue;

  vgaValue = sliderVgaGain->value();
  lnaValue = sliderLnaGain->value();
  ampEnable = btnAmpEnable->isChecked() ? 1 : 0;
  theValue = QString::number(vgaValue) + ":";
  theValue += QString::number(lnaValue) + ":";
  theValue += QString::number(ampEnable);

  mpHackrfSettings->beginGroup("hackrfSettings");
  mpHackrfSettings->setValue(QString::number(freq), theValue);
  mpHackrfSettings->endGroup();
}

void HackRfHandler::update_gain_settings(int iFreqMHz)
{
  int vgaValue;
  int lnaValue;
  int ampEnable;
  QString theValue = "";

  mpHackrfSettings->beginGroup("hackrfSettings");
  theValue = mpHackrfSettings->value(QString::number(iFreqMHz), "").toString();
  mpHackrfSettings->endGroup();

  if (theValue == QString(""))
  {
    return;
  }    // or set some defaults here

  QStringList result = theValue.split(":");
  if (result.size() != 3)
  {  // should not happen
    return;
  }

  vgaValue = result.at(0).toInt();
  lnaValue = result.at(1).toInt();
  ampEnable = result.at(2).toInt();

  sliderVgaGain->blockSignals(true);
  emit signal_new_vga_value(vgaValue);
  while (sliderVgaGain->value() != vgaValue)
  {
    usleep(1000);
  }
  vgagainDisplay->display(vgaValue);
  sliderVgaGain->blockSignals(false);

  sliderLnaGain->blockSignals(true);
  emit signal_new_lna_value(lnaValue);
  while (sliderLnaGain->value() != lnaValue)
  {
    usleep(1000);
  }
  lnagainDisplay->display(lnaValue);
  sliderLnaGain->blockSignals(false);

  btnAmpEnable->blockSignals(true);
  emit signal_new_amp_enable(ampEnable == 1);
  while (btnAmpEnable->isChecked() != (ampEnable == 1))
  {
    usleep(1000);
  }
  btnAmpEnable->blockSignals(false);
}

void HackRfHandler::check_err_throw(int32_t iResult) const
{
  if (iResult != HACKRF_SUCCESS)
  {
    throw (hackrf_exception(mHackrf.error_name((hackrf_error)iResult)));
  }
}

bool HackRfHandler::check_err(int32_t iResult, const char * const iFncName, uint32_t iLine) const
{
  if (iResult != HACKRF_SUCCESS)
  {
    qCritical("HackRfHandler raised an error: '%s' in function %s:%u", mHackrf.error_name((hackrf_error)iResult), iFncName, iLine);
    return false;
  }
  return true;
}

template<typename T> bool HackRfHandler::load_method(T *& oMethodPtr, const char * iName, uint32_t iLine) const
{
  oMethodPtr = reinterpret_cast<T*>(mpHandle->resolve(iName));

  if (oMethodPtr == nullptr)
  {
    qCritical("Could not find '%s' at %s:%u\n", iName, (strrchr("/" __FILE__, '/') + 1), iLine);
    return false;
  }
  return true;
}
