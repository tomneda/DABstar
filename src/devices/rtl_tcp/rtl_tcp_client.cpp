/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 ..2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
 *
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    A simple client for rtl_tcp
 */

#include "rtl_tcp_client.h"
#include "rtl-sdr.h"
#include "qt_compat.h"
#include "xml_filewriter.h"
#include "device_exceptions.h"
#include "openfiledialog.h"
#include "setting_helper.h"
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSettings>

#if !defined(_WIN32)
#include <netinet/in.h>  // for macro htonl
#endif


static constexpr i32 cDefaultPort = 1234;

struct SDongleInfo
{ /* structure size must be multiple of 2 bytes */
  char magic[4];
  u32 tuner_type;
  u32 tuner_gain_count;
};

RtlTcpClient::RtlTcpClient(QSettings * s, const QString & iRecorderVersion)
  : mFrame(nullptr)
  , mpSettings(s)
  , mRecorderVersion(iRecorderVersion)
{
  for (i32 i = 0; i < 256; i++)
  {
    mMapTable[i] = ((f32)i - 127.38f) / 128.0f;
  }

  setupUi(&mFrame);

  mFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  mFrame.show();

  lblTunerType->setStyleSheet("color: #FFBB00;");

  _show_connection_state(false); // set the final texts before the (fixed) window size is calculated below

  Settings::RtlTcp::posAndSize.read_widget_geometry(&mFrame, false, true);

  Settings::RtlTcp::sbGain.register_widget_and_update_ui_from_setting(sbGain, 20);
  Settings::RtlTcp::sbPpm.register_widget_and_update_ui_from_setting(sbPpm, 0);
  Settings::RtlTcp::sbBandwidth.register_widget_and_update_ui_from_setting(sbBandwidth, 1750);
  Settings::RtlTcp::cbBiasT.register_widget_and_update_ui_from_setting(cbBiasT, 0);

  editIpAddress->setText(Settings::RtlTcp::varIpAddress.read().toString());

  // setting the defaults and constants
  mBitRate = INPUT_RATE;
  mGain = (i16)sbGain->value();
  mPpm = sbPpm->value();
  mBandwidthKhz = (i16)sbBandwidth->value();
  mBiasT = cbBiasT->isChecked() ? 1 : 0;
  mVfoFrequency = 220'000'000;
  mpBuffer = std::make_unique<RingBuffer<cf32>>(32 * 32768);

  connect(&mTcpSocket, &QTcpSocket::disconnected, this, &RtlTcpClient::_slot_socket_disconnected);
  connect(&mTcpSocket, &QAbstractSocket::errorOccurred, this, &RtlTcpClient::_slot_socket_error);
  connect(btnConnect, &QPushButton::clicked, this, &RtlTcpClient::_slot_handle_connect_button);
  connect(sbGain, qOverload<i32>(&QSpinBox::valueChanged), this, &RtlTcpClient::_slot_handle_gain);
  connect(sbPpm, qOverload<f64>(&QDoubleSpinBox::valueChanged), this, &RtlTcpClient::_slot_handle_ppm);
  connect(sbBandwidth, qOverload<i32>(&QSpinBox::valueChanged), this, &RtlTcpClient::_slot_handle_bandwidth);
  connect(rbAgcHw, &QRadioButton::clicked, this, [this]() { _set_agc_mode(EAgcMode::HW); });
  connect(rbAgcSw, &QRadioButton::clicked, this, [this]() { _set_agc_mode(EAgcMode::SW); });
  connect(rbAgcOff, &QRadioButton::clicked, this, [this]() { _set_agc_mode(EAgcMode::OFF); });
  connect(cbBiasT, &QCheckBox::stateChangedSubst, this, &RtlTcpClient::_slot_handle_biast);

  // the AGC mode also enables/disables the gain elements, so apply it after the UI is wired up
  _set_agc_mode((EAgcMode)Settings::RtlTcp::varAgcMode.read().toInt());

  mpXmlDumper = nullptr;
  mIsXmlDumping.store(false);
}

RtlTcpClient::~RtlTcpClient()
{
  if (mIsConnected) // close previous connection
  {
    stopReader();
    mIsConnected = false;
  }

  Settings::RtlTcp::posAndSize.write_widget_geometry(&mFrame);
  mFrame.hide();
  usleep(1000);
  mTcpSocket.close();
}

void RtlTcpClient::_slot_handle_connect_button()
{
  if (mIsConnected)
  {
    stopReader();
    mIsConnected = false;
    mTcpSocket.close();
    mTunerText.clear();
    lblTunerType->setText("---");
    _show_connection_state(false);
    return;
  }

  if (!_check_and_cleanup_ip_address())
  {
    return;
  }

  if (_setup_connection())
  {
    _show_connection_state(true);
    emit signal_device_connected(); // queued, so this method is fully left before the channel is restarted
  }
}

bool RtlTcpClient::_setup_connection()
{
  qDebug().noquote().nospace() << "Connect to " << mServerAddress << ":" << mPort;
  mTcpSocket.connectToHost(mServerAddress, (quint16)mPort);

  if (!mTcpSocket.waitForConnected(2000))
  {
    mTcpSocket.abort();
    _show_error_state("Connection failed");
    return false;
  }

  mIsConnected = true; // from here on sendCommand() is allowed to write to the server

  _send_rate(mBitRate);
  _set_agc_mode(mAgcMode);
  _slot_handle_ppm(mPpm);
  _slot_handle_bandwidth(mBandwidthKhz);
  _slot_handle_biast(mBiasT);

  mTcpSocket.waitForBytesWritten();

  mDongleInfoReceived = false;
  mTotalSampleCnt = 0;
  mDroppedSampleCnt = 0;
  mDroppedSampleCntLastBurst = 0;
  mOverflowActive = false;

  return true;
}

bool RtlTcpClient::_check_and_cleanup_ip_address()
{
  QString addr = editIpAddress->text().trimmed();

  // remove "sdr://" or similar from the front
  if (const i32 pos = addr.indexOf("://"); pos >= 0)
  {
    addr = addr.mid(pos + 3);
  }

  const QStringList parts = addr.split(":");
  mServerAddress = parts[0].trimmed();
  mPort = parts.size() > 1 ? parts[1].toInt() : cDefaultPort;

  if (mServerAddress.isEmpty() || mPort <= 0 || mPort > 65535)
  {
    _show_error_state("Invalid URL");
    return false;
  }

  const QString addrStr = mServerAddress + ":" + QString::number(mPort);
  editIpAddress->setText(addrStr);

  Settings::RtlTcp::varIpAddress.write(addrStr);

  return true;
}

void RtlTcpClient::_show_connection_state(const bool iIsConnected)
{
  if (iIsConnected)
  {
    btnConnect->setStyleSheet("background-color: #6688FF88;");
    btnConnect->setText("Disconnect");
    lblState->setStyleSheet("color: #88FF88;");
    lblState->setText("Connected");
  }
  else
  {
    btnConnect->setStyleSheet("background-color: #668888FF;");
    btnConnect->setText("Connect");
    lblState->setStyleSheet("color: #8888FF;");
    lblState->setText("Disconnected");
  }

  editIpAddress->setEnabled(!iIsConnected);
}

void RtlTcpClient::_show_error_state(const QString & iText)
{
  lblState->setStyleSheet("color: #FF8888;");
  lblState->setText(iText);
}

void RtlTcpClient::setVFOFrequency(const i32 iNewFrequency)
{
  if (!mIsConnected)
  {
    return;
  }

  _send_vfo(iNewFrequency);
}

i32 RtlTcpClient::getVFOFrequency()
{
  return mVfoFrequency;
}

bool RtlTcpClient::restartReader(i32 freq)
{
  if (!mIsConnected)
  {
    return true;
  }

  _send_vfo(freq);

  connect(&mTcpSocket, &QTcpSocket::readyRead, this, &RtlTcpClient::_slot_read_data);
  return true;
}

void RtlTcpClient::stopReader()
{
  if (!mIsConnected)
  {
    return;
  }

  stopDumping();
  disconnect(&mTcpSocket, &QTcpSocket::readyRead, this, &RtlTcpClient::_slot_read_data);
  resetBuffer();
}

i32 RtlTcpClient::getSamples(cf32 * V, i32 size)
{
  return mpBuffer->get_data_from_ring_buffer(V, size);
}

i32 RtlTcpClient::Samples()
{
  return mpBuffer->get_ring_buffer_read_available();
}

void RtlTcpClient::_slot_read_data()
{
  if (!mDongleInfoReceived)
  {
    SDongleInfo dongleInfo;

    if (mTcpSocket.bytesAvailable() >= (qint64)sizeof(dongleInfo))
    {
      mTcpSocket.read((char*)&dongleInfo, sizeof(dongleInfo));

      mDongleInfoReceived = true;

      if (memcmp(dongleInfo.magic, "RTL0", 4) == 0)
      {
        switch (htonl(dongleInfo.tuner_type))
        {
        case RTLSDR_TUNER_E4000:  mTunerText = "E4000";  break;
        case RTLSDR_TUNER_FC0012: mTunerText = "FC0012"; break;
        case RTLSDR_TUNER_FC0013: mTunerText = "FC0013"; break;
        case RTLSDR_TUNER_FC2580: mTunerText = "FC2580"; break;
        case RTLSDR_TUNER_R820T:  mTunerText = "R820T";  break;
        case RTLSDR_TUNER_R828D:  mTunerText = "R828D";  break;
        default:                  mTunerText = "unknown";
        }
        lblTunerType->setText(mTunerText);
      }
    }
  }

  if (mDongleInfoReceived)
  {
    std::array<cf32, 4096> complexBuffer;
    std::array<u8, 2*complexBuffer.size()> byteBuffer;

    while (mTcpSocket.bytesAvailable() >= (qint64)byteBuffer.size())
    {
      const qint64 bytesRead = mTcpSocket.read((char*)byteBuffer.data(), byteBuffer.size());

      if (bytesRead <= 0)
      {
        qWarning() << "RtlTcp: read from socket returned" << bytesRead << "although data were announced";
        break;
      }

      if ((bytesRead & 1) != 0)
      {
        // An odd byte count would swap I and Q for all following samples, which silently ruins the
        // whole reception. Cannot happen with a sane server, but do not fail quietly if it does.
        qCritical() << "RtlTcp: got odd byte count" << bytesRead << "-> I/Q alignment is lost";
      }

      const i32 sampleCnt = (i32)(bytesRead / 2);

      for (i32 i = 0; i < sampleCnt; i++)
      {
        complexBuffer[i] = cf32(mMapTable[byteBuffer[2 * i]], mMapTable[byteBuffer[2 * i + 1]]);
      }

      const i32 storedCnt = mpBuffer->put_data_into_ring_buffer(complexBuffer.data(), sampleCnt);
      mTotalSampleCnt += (u64)sampleCnt;

      if (storedCnt < sampleCnt)
      {
        // The input ring buffer is full because the DAB processor could not fetch fast enough or the
        // GUI thread (which feeds this buffer) was blocked too long. The resulting gap in the sample
        // stream desynchronizes the OFDM decoding and produces corrupted FIBs.
        mDroppedSampleCnt += (u64)(sampleCnt - storedCnt);
        mDroppedSampleCntLastBurst += (u64)(sampleCnt - storedCnt);

        if (!mOverflowActive)
        {
          mOverflowActive = true;
          qWarning() << "RtlTcp: input ring buffer overflow, dropping samples -> expect corrupted FIC data"
                     << "(socket backlog" << mTcpSocket.bytesAvailable() << "bytes)";
        }
      }
      else if (mOverflowActive)
      {
        mOverflowActive = false;
        qWarning() << "RtlTcp: input ring buffer recovered after dropping" << mDroppedSampleCntLastBurst
                   << "samples (" << mDroppedSampleCnt << "in total of" << mTotalSampleCnt << ")";
        mDroppedSampleCntLastBurst = 0;
      }

      if (mIsXmlDumping.load())
      {
        mpXmlWriter->add((std::complex<uint8_t>*)byteBuffer.data(), sampleCnt);
      }
    }
  }
}

void RtlTcpClient::_slot_socket_error(const QAbstractSocket::SocketError iSocketError)
{
  if (!mIsConnected) // an error after a deliberate disconnect is of no interest
  {
    return;
  }

  qCritical() << "RtlTcp: socket error" << iSocketError << mTcpSocket.errorString();
  _slot_socket_disconnected();
}

void RtlTcpClient::_slot_socket_disconnected()
{
  if (!mIsConnected)
  {
    return;
  }

  // Without this the reader would stay "connected" forever while no sample arrives anymore and the
  // DAB processor would wait for samples endlessly without any hint what went wrong.
  qCritical() << "RtlTcp: connection to server lost after" << mTotalSampleCnt << "samples (" << mDroppedSampleCnt << "dropped)";

  stopReader();

  mIsConnected = false;
  mDongleInfoReceived = false;

  mTcpSocket.abort();

  mTunerText.clear();
  lblTunerType->setText("---");
  _show_error_state("Connection lost");
  btnConnect->setStyleSheet("background-color: #668888FF;");
  btnConnect->setText("Connect");
  editIpAddress->setEnabled(true);
}


void RtlTcpClient::_send_command(u8 cmd, i32 param)
{
  // Commands are packed in 5 bytes, one "command byte" and an integer parameter
  if (mIsConnected)
  {
    QByteArray datagram;
    datagram.resize(5);
    datagram[0] = cmd; // command
    datagram[4] = (param >>  0) & 0xFF;  // lsb last
    datagram[3] = (param >>  8) & 0xFF;
    datagram[2] = (param >> 16) & 0xFF;
    datagram[1] = (param >> 24) & 0xFF;
    mTcpSocket.write(datagram.data(), datagram.size());
  }
}

void RtlTcpClient::_send_vfo(const i32 iFrequency)
{
  mVfoFrequency = iFrequency;
  _send_command(0x01, iFrequency);
}

void RtlTcpClient::_send_rate(const i32 iBitRate)
{
  mBitRate = iBitRate;
  _send_command(0x02, iBitRate);
}

void RtlTcpClient::_slot_handle_gain(const i32 iGain)
{
  mGain = (i16)iGain;
  _send_command(0x04, 10 * iGain);
}

//  correction is in ppm
void RtlTcpClient::_slot_handle_ppm(const f64 iPpm)
{
  mPpm = iPpm;
  const i32 corr = (i32)(iPpm * 1000);
  _send_command(0x83, corr);
}

void RtlTcpClient::_set_agc_mode(const EAgcMode iAgcMode)
{
  mAgcMode = iAgcMode;
  Settings::RtlTcp::varAgcMode.write((i32)iAgcMode);

  switch (iAgcMode)
  {
  case EAgcMode::HW:  rbAgcHw->setChecked(true);  break;
  case EAgcMode::OFF: rbAgcOff->setChecked(true); break;
  case EAgcMode::SW:  rbAgcSw->setChecked(true);  break;
  }

  _send_command(0x08, iAgcMode == EAgcMode::HW);
  _send_command(0x03, (i32)iAgcMode);

  if (iAgcMode == EAgcMode::OFF)
  {
    _slot_handle_gain(mGain);
  }

  sbGain->setEnabled(iAgcMode == EAgcMode::OFF);
  lblGain->setEnabled(iAgcMode == EAgcMode::OFF);
}

void RtlTcpClient::_slot_handle_biast(const i32 iBiasT)
{
  mBiasT = iBiasT ? 1 : 0;
  _send_command(0x0e, mBiasT);
}

void RtlTcpClient::_slot_handle_bandwidth(const i32 iBwKhz)
{
  mBandwidthKhz = (i16)iBwKhz;
  _send_command(0x40, iBwKhz * 1000);
}

void RtlTcpClient::show()
{
  mFrame.show();
}

void RtlTcpClient::hide()
{
  mFrame.hide();
}

bool RtlTcpClient::isHidden()
{
  return mFrame.isHidden();
}

void RtlTcpClient::resetBuffer()
{
  mpBuffer->flush_ring_buffer();
}

QString RtlTcpClient::deviceName()
{
  return "RtlTcp";
}

bool RtlTcpClient::startDumping()
{
  bool result = false;

  if (!mIsConnected)
  {
    return result;
  }

  if (!mIsXmlDumping.load())
  {
    result = _setup_xml_dump();
  }
  else
  {
    stopDumping();
  }

  return result;
}

bool RtlTcpClient::_setup_xml_dump()
{
  OpenFileDialog filenameFinder(mpSettings);
  mpXmlDumper = filenameFinder.open_raw_dump_xmlfile_ptr();

  if (mpXmlDumper == nullptr)
  {
    return false;
  }

  mpXmlWriter = std::make_unique<XmlFileWriter>(mpXmlDumper, 8, "uint8", INPUT_RATE, getVFOFrequency(), deviceName(), mTunerText, mRecorderVersion);
  mIsXmlDumping.store(true);
  return true;
}

void RtlTcpClient::stopDumping()
{
  if (mpXmlDumper == nullptr)
  {
    return;
  }

  mIsXmlDumping.store(false);
  usleep(1000);
  mpXmlWriter->computeHeader();
  mpXmlWriter.reset();
  fclose(mpXmlDumper);
  mpXmlDumper = nullptr;
}

bool RtlTcpClient::hasDump()
{
  return true;
}
