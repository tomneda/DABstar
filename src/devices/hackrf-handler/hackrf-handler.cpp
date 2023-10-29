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

#include  <QThread>
#include  <QSettings>
#include  <QTime>
#include  <QDate>
#include  <QLabel>
#include  <QDebug>
#include  <QFileDialog>
#include  "hackrf-handler.h"
#include  "xml-filewriter.h"
#include  "device-exceptions.h"

#define CHECK_ERR_RETURN(x_)       if (!check_err(x_, __FUNCTION__, __LINE__)) return
#define CHECK_ERR_RETURN_FALSE(x_) if (!check_err(x_, __FUNCTION__, __LINE__)) return false

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
  const char * libraryString = "libhackrf.so.0";
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

  mVfoFrequency = kHz(220000);

  //	See if there are settings from previous incarnations
  mpHackrfSettings->beginGroup("hackrfSettings");
  lnaGainSlider->setValue(mpHackrfSettings->value("hack_lnaGain", DEFAULT_LNA_GAIN).toInt());
  vgaGainSlider->setValue(mpHackrfSettings->value("hack_vgaGain", DEFAULT_VGA_GAIN).toInt());
  bool isChecked = mpHackrfSettings->value("hack_AntEnable", false).toBool();
  AntEnableButton->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
  isChecked = mpHackrfSettings->value("hack_AmpEnable", false).toBool();
  AmpEnableButton->setCheckState(isChecked ? Qt::Checked : Qt::Unchecked);
  ppm_correction->setValue(mpHackrfSettings->value("hack_ppmCorrection", 0).toInt());
  save_gain_settings = mpHackrfSettings->value("save_gainSettings", 1).toInt() != 0;
  mpHackrfSettings->endGroup();

  check_err_throw(mHackrf.init());
  check_err_throw(mHackrf.open(&theDevice));
  check_err_throw(mHackrf.set_sample_rate(theDevice, 2048000.0));
  check_err_throw(mHackrf.set_baseband_filter_bandwidth(theDevice, 1750000));
  check_err_throw(mHackrf.set_freq(theDevice, 220000000));
  check_err_throw(mHackrf.set_antenna_enable(theDevice, 1));
  check_err_throw(mHackrf.set_amp_enable(theDevice, 1));

  uint8_t revNo = 0;
  check_err_throw(mHackrf.board_rev_read(theDevice, &revNo));
  mRevNo = (hackrf_board_rev)revNo;
  lblBoardRev->setText(mHackrf.board_rev_name(mRevNo));

  //check_err_throw(mHackrf.si5351c_read(theDevice, 162, &mRegA2));
  //check_err_throw(mHackrf.si5351c_write(theDevice, 162, mRegA2));

  slot_set_lna_gain(lnaGainSlider->value());
  slot_set_vga_gain(vgaGainSlider->value());
  slot_enable_antenna(1);    // value is a dummy really
  slot_enable_amp(1);    // value is a dummy, really
  slot_set_ppm_correction(ppm_correction->value());

  if (hackrf_device_list_t const * deviceList = mHackrf.device_list();
      deviceList != nullptr)
  {  // well, it should be
    char const * pSerial = deviceList->serial_numbers[0];
    while (*pSerial == '0') ++pSerial; // remove leading '0'
    lblSerialNum->setText(pSerial);
    enum hackrf_usb_board_id board_id = deviceList->usb_board_ids[0];
    usb_board_id_display->setText(mHackrf.usb_board_id_name(board_id));
  }


  //	and be prepared for future changes in the settings
  connect(lnaGainSlider, &QSlider::valueChanged, this, &HackRfHandler::slot_set_lna_gain);
  connect(vgaGainSlider, &QSlider::valueChanged, this, &HackRfHandler::slot_set_vga_gain);
  connect(AntEnableButton, &QCheckBox::stateChanged, this, &HackRfHandler::slot_enable_antenna);
  connect(AmpEnableButton, &QCheckBox::stateChanged, this, &HackRfHandler::slot_enable_amp);
  connect(ppm_correction, qOverload<int>(&QSpinBox::valueChanged), this, &HackRfHandler::slot_set_ppm_correction);
  connect(dumpButton, &QPushButton::clicked, this, &HackRfHandler::slot_xml_dump);

  connect(this, &HackRfHandler::signal_new_ant_enable, AntEnableButton, &QCheckBox::setChecked);
  connect(this, &HackRfHandler::signal_new_amp_enable, AmpEnableButton, &QCheckBox::setChecked);
  connect(this, &HackRfHandler::signal_new_vga_value, vgaGainSlider, &QSlider::setValue);
  connect(this, &HackRfHandler::signal_new_vga_value, vgagainDisplay, qOverload<int>(&QLCDNumber::display));
  connect(this, &HackRfHandler::signal_new_lna_value, lnaGainSlider, &QSlider::setValue);
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

  mpHackrfSettings->setValue("hack_lnaGain", lnaGainSlider->value());
  mpHackrfSettings->setValue("hack_vgaGain", vgaGainSlider->value());
  mpHackrfSettings->setValue("hack_AntEnable", AntEnableButton->checkState() == Qt::Checked);
  mpHackrfSettings->setValue("hack_AmpEnable", AmpEnableButton->checkState() == Qt::Checked);
  mpHackrfSettings->setValue("hack_ppmCorrection", ppm_correction->value());
  mpHackrfSettings->endGroup();

  mHackrf.close(theDevice);
  mHackrf.exit();
}

void HackRfHandler::setVFOFrequency(int32_t newFrequency)
{
  CHECK_ERR_RETURN(mHackrf.set_freq(theDevice, newFrequency));

  //	It seems that after changing the frequency, the preamp is switched off
  //	(tomneda: do not see this, I guess the bug is already fixed, but leave the workaround)
  if (AmpEnableButton->checkState() == Qt::Checked)
  {
    slot_enable_amp(1);
  }

  mVfoFrequency = newFrequency;
}

int32_t HackRfHandler::getVFOFrequency()
{
  return mVfoFrequency;
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
  if ((newGain <= 62) && (newGain >= 0))
  {
    CHECK_ERR_RETURN(mHackrf.set_vga_gain(theDevice, newGain));
    vgagainDisplay->display(newGain);
  }
}

void HackRfHandler::slot_enable_antenna(int d)
{
  (void)d;
  const bool b = AntEnableButton->checkState() == Qt::Checked;
  CHECK_ERR_RETURN(mHackrf.set_antenna_enable(theDevice, b));
}

void HackRfHandler::slot_enable_amp(int a)
{
  (void)a;
  const bool b = AmpEnableButton->checkState() == Qt::Checked;
  CHECK_ERR_RETURN(mHackrf.set_amp_enable(theDevice, b));
}

// correction is in Hz
// This function has to be modified to implement ppm correction
// writing in the si5351 register does not seem to work yet
// To be completed

void HackRfHandler::slot_set_ppm_correction(int32_t ppm)
{
//  uint16_t value;
//  CHECK_ERR_RETURN(mHackrf.si5351c_write(theDevice, 162, static_cast<uint16_t>(ppm)));
//  CHECK_ERR_RETURN(mHackrf.si5351c_read(theDevice, 162, &value));
}

//	we use a static large buffer, rather than trying to allocate a buffer on the stack
static std::array<std::complex<int8_t>, 32 * 32768> buffer;

static int callback(hackrf_transfer * transfer)
{
  auto * ctx = static_cast<HackRfHandler *>(transfer->rx_ctx);
  const uint8_t * const p = transfer->buffer;
  HackRfHandler::TRingBuffer * q = &(ctx->mRingBuffer);
  int bufferIndex = 0;

  for (int i = 0; i < transfer->valid_length / 2; i += 1)
  {
    const int8_t re = ((const int8_t *)p)[2 * i + 0];
    const int8_t im = ((const int8_t *)p)[2 * i + 1];
    buffer[bufferIndex] = std::complex<int8_t>(re, im);
    ++bufferIndex;
  }
  q->putDataIntoBuffer(buffer.data(), bufferIndex);
  return 0;
}

bool HackRfHandler::restartReader(int32_t freq)
{
  if (mRunning.load())
  {
    return true;
  }

  mVfoFrequency = freq;

  if (save_gain_settings)
  {
    update_gain_settings(freq / MHz (1));
  }

  CHECK_ERR_RETURN_FALSE(mHackrf.set_lna_gain(theDevice, lnaGainSlider->value()));
  CHECK_ERR_RETURN_FALSE(mHackrf.set_vga_gain(theDevice, vgaGainSlider->value()));
  CHECK_ERR_RETURN_FALSE(mHackrf.set_amp_enable(theDevice, AmpEnableButton->isChecked() ? 1 : 0));
  CHECK_ERR_RETURN_FALSE(mHackrf.set_antenna_enable(theDevice, AntEnableButton->isChecked() ? 1 : 0));

  CHECK_ERR_RETURN_FALSE(mHackrf.set_freq(theDevice, freq));
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
    record_gain_settings(mVfoFrequency / MHz (1));
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
  // link the required procedures
  mHackrf.init = (pfn_hackrf_init)mpHandle->resolve("hackrf_init");
  if (mHackrf.init == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_init\n");
    return false;
  }

  mHackrf.open = (pfn_hackrf_open)mpHandle->resolve("hackrf_open");
  if (mHackrf.open == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_open\n");
    return false;
  }

  mHackrf.close = (pfn_hackrf_close)mpHandle->resolve("hackrf_close");
  if (mHackrf.close == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_close\n");
    return false;
  }

  mHackrf.exit = (pfn_hackrf_exit)mpHandle->resolve("hackrf_exit");
  if (mHackrf.exit == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_exit\n");
    return false;
  }

  mHackrf.start_rx = (pfn_hackrf_start_rx)mpHandle->resolve("hackrf_start_rx");
  if (mHackrf.start_rx == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_start_rx\n");
    return false;
  }

  mHackrf.stop_rx = (pfn_hackrf_stop_rx)mpHandle->resolve("hackrf_stop_rx");
  if (mHackrf.stop_rx == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_stop_rx\n");
    return false;
  }

  mHackrf.device_list = (pfn_hackrf_device_list)mpHandle->resolve("hackrf_device_list");
  if (mHackrf.device_list == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_device_list\n");
    return false;
  }

  mHackrf.set_baseband_filter_bandwidth = (pfn_hackrf_set_baseband_filter_bandwidth)mpHandle->resolve(
    "hackrf_set_baseband_filter_bandwidth");
  if (mHackrf.set_baseband_filter_bandwidth == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_set_baseband_filter_bandwidth\n");
    return false;
  }

  mHackrf.set_lna_gain = (pfn_hackrf_set_lna_gain)mpHandle->resolve("hackrf_set_lna_gain");
  if (mHackrf.set_lna_gain == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_set_lna_gain\n");
    return false;
  }

  mHackrf.set_vga_gain = (pfn_hackrf_set_vga_gain)mpHandle->resolve("hackrf_set_vga_gain");
  if (mHackrf.set_vga_gain == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_set_vga_gain\n");
    return false;
  }

  mHackrf.set_freq = (pfn_hackrf_set_freq)mpHandle->resolve("hackrf_set_freq");
  if (mHackrf.set_freq == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_set_freq\n");
    return false;
  }

  mHackrf.set_sample_rate = (pfn_hackrf_set_sample_rate)mpHandle->resolve("hackrf_set_sample_rate");
  if (mHackrf.set_sample_rate == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_set_sample_rate\n");
    return false;
  }

  mHackrf.is_streaming = (pfn_hackrf_is_streaming)mpHandle->resolve("hackrf_is_streaming");
  if (mHackrf.is_streaming == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_is_streaming\n");
    return false;
  }

  mHackrf.error_name = (pfn_hackrf_error_name)mpHandle->resolve("hackrf_error_name");
  if (mHackrf.error_name == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_error_name\n");
    return false;
  }

  mHackrf.usb_board_id_name = (pfn_hackrf_usb_board_id_name)mpHandle->resolve("hackrf_usb_board_id_name");
  if (mHackrf.usb_board_id_name == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_usb_board_id_name\n");
    return false;
  }
  // Aggiunta Fabio
  mHackrf.set_antenna_enable = (pfn_hackrf_set_antenna_enable)mpHandle->resolve("hackrf_set_antenna_enable");
  if (mHackrf.set_antenna_enable == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_set_antenna_enable\n");
    return false;
  }

  mHackrf.set_amp_enable = (pfn_hackrf_set_amp_enable)mpHandle->resolve("hackrf_set_amp_enable");
  if (mHackrf.set_amp_enable == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_set_amp_enable\n");
    return false;
  }

  mHackrf.si5351c_read = (pfn_hackrf_si5351c_read)mpHandle->resolve("hackrf_si5351c_read");
  if (mHackrf.si5351c_read == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_si5351c_read\n");
    return false;
  }

  mHackrf.si5351c_write = (pfn_hackrf_si5351c_write)mpHandle->resolve("hackrf_si5351c_write");
  if (mHackrf.si5351c_write == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_si5351c_write\n");
    return false;
  }

  mHackrf.board_rev_read = (pfn_hackrf_board_rev_read)mpHandle->resolve("hackrf_board_rev_read");
  if (mHackrf.board_rev_read == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_board_rev_read\n");
    return false;
  }

  mHackrf.board_rev_name = (pfn_hackrf_board_rev_name)mpHandle->resolve("hackrf_board_rev_name");
  if (mHackrf.board_rev_name == nullptr)
  {
    fprintf(stderr, "Could not find hackrf_board_rev_name\n");
    return false;
  }

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

  vgaValue = vgaGainSlider->value();
  lnaValue = lnaGainSlider->value();
  ampEnable = AmpEnableButton->isChecked() ? 1 : 0;
  theValue = QString::number(vgaValue) + ":";
  theValue += QString::number(lnaValue) + ":";
  theValue += QString::number(ampEnable);

  mpHackrfSettings->beginGroup("hackrfSettings");
  mpHackrfSettings->setValue(QString::number(freq), theValue);
  mpHackrfSettings->endGroup();
}

void HackRfHandler::update_gain_settings(int freq)
{
  int vgaValue;
  int lnaValue;
  int ampEnable;
  QString theValue = "";

  mpHackrfSettings->beginGroup("hackrfSettings");
  theValue = mpHackrfSettings->value(QString::number(freq), "").toString();
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

  vgaGainSlider->blockSignals(true);
  signal_new_vga_value(vgaValue);
  while (vgaGainSlider->value() != vgaValue)
  {
    usleep(1000);
  }
  vgagainDisplay->display(vgaValue);
  vgaGainSlider->blockSignals(false);

  lnaGainSlider->blockSignals(true);
  signal_new_lna_value(lnaValue);
  while (lnaGainSlider->value() != lnaValue)
  {
    usleep(1000);
  }
  lnagainDisplay->display(lnaValue);
  lnaGainSlider->blockSignals(false);

  AmpEnableButton->blockSignals(true);
  signal_new_amp_enable(ampEnable == 1);
  while (AmpEnableButton->isChecked() != (ampEnable == 1))
  {
    usleep(1000);
  }
  AmpEnableButton->blockSignals(false);
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
