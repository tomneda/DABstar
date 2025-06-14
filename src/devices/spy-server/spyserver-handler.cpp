/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2016 .. 2023
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
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "spyserver-client.h"
#include "spyserver-handler.h"
#include <chrono>
#include <iostream>
#include <qloggingcategory.h>
#include <thread>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(sLogSpyServerHandler, "SpyServerHandler", QtDebugMsg)

SpyServerHandler::SpyServerHandler(SpyServerClient * parent, const QString & ipAddress, int port, RingBuffer<uint8_t> * outB)
  : inBuffer(64 * 32768)
  , tcpHandler(ipAddress, port, &inBuffer)
{
  if (!tcpHandler.is_connected())
  {
    throw std::runtime_error(std::string(__FUNCTION__) + " " + "Failed to connect!");
  }

  this->parent = parent;
  this->outB = outB;
  streamingMode = STREAM_TYPE_IQ;
  streaming.store(false);
  running.store(false);

  bool success = show_attendance();
  if (!success)
  {
    throw std::runtime_error(std::string(__FUNCTION__) + " " + "Failed to establish connection!");
  }

  is_connected.store(true);
  cleanRecords();
  testTimer = new QTimer();
  connect(testTimer, &QTimer::timeout, this, &SpyServerHandler::_slot_no_device_info);
  connect(this, &SpyServerHandler::signal_data_ready, parent, &SpyServerClient::slot_data_ready);
  start();
  testTimer->start(10000);
}

SpyServerHandler::~SpyServerHandler()
{
  running.store(false);
  while (isRunning())
    usleep(1000);
}

void SpyServerHandler::_slot_no_device_info()
{
  if (!got_device_info)
    running.store(false);
}

void SpyServerHandler::run()
{
  MessageHeader theHeader;
  uint32_t sequenceNumberLast = (uint32_t)(-1);
  static std::vector<uint8_t> buffer(64 * 1024);
  running.store(true);

  while (running.load())
  {
    readHeader(theHeader);

    if (theHeader.SequenceNumber != sequenceNumberLast + 1 && theHeader.SequenceNumber > 0)
    {
      qCWarning(sLogSpyServerHandler) << (theHeader.SequenceNumber - (sequenceNumberLast + 1)) << "packets missing, current:"
                                      << theHeader.SequenceNumber << ", expected:" << sequenceNumberLast + 1;
    }

    sequenceNumberLast = theHeader.SequenceNumber;

    if (theHeader.BodySize > buffer.size())
    {
      buffer.resize(theHeader.BodySize);
    }

    readBody(buffer.data(), theHeader.BodySize);

    switch (theHeader.MessageType)
    {
    case MSG_TYPE_DEVICE_INFO:
      got_device_info = process_device_info(buffer.data(), deviceInfo);
      break;
    case MSG_TYPE_UINT8_IQ:
      process_data(buffer.data(), theHeader.BodySize);
      break;
    case MSG_TYPE_CLIENT_SYNC:
      process_client_sync(buffer.data(), m_curr_client_sync);
      break;
    }
  }
}

bool SpyServerHandler::readHeader(struct MessageHeader & header)
{
  while (running.load() && (inBuffer.get_ring_buffer_read_available() < (int)sizeof(struct MessageHeader)))
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (!running.load())
  {
    return false;
  }

  inBuffer.get_data_from_ring_buffer((uint8_t *)(&header), sizeof(struct MessageHeader));
  return true;
}

bool SpyServerHandler::readBody(uint8_t * buffer, int size)
{
  int filler = 0;
  while (running.load())
  {
    if (inBuffer.get_ring_buffer_read_available() > size / 2)
    {
      filler += inBuffer.get_data_from_ring_buffer(buffer, size - filler);

      if (filler >= size)
      {
        return true;
      }

      if (!running.load())
      {
        return false;
      }
    }
    else
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  return false;
}

bool SpyServerHandler::show_attendance()
{
  const uint8_t * protocolVersionBytes = (const uint8_t *)&ProtocolVersion;
  const uint8_t * softwareVersionBytes = (const uint8_t *)SoftwareID.c_str();
  std::vector<uint8_t> args = std::vector<uint8_t>(sizeof(ProtocolVersion) + SoftwareID.size());

  std::memcpy(&args[0], protocolVersionBytes, sizeof(ProtocolVersion));
  std::memcpy(&args[0] + sizeof(ProtocolVersion), softwareVersionBytes, SoftwareID.size());
  bool res = send_command(CMD_HELLO, args);

  return res;
}

void SpyServerHandler::cleanRecords()
{
  deviceInfo.DeviceType = 0;
  deviceInfo.DeviceSerial = 0;
  deviceInfo.DecimationStageCount = 0;
  deviceInfo.GainStageCount = 0;
  deviceInfo.MaximumSampleRate = 0;
  deviceInfo.MaximumBandwidth = 0;
  deviceInfo.MaximumGainIndex = 0;
  deviceInfo.MinimumFrequency = 0;
  deviceInfo.MaximumFrequency = 0;
  frameNumber = 0;
}

bool SpyServerHandler::send_command(uint32_t cmd, std::vector<uint8_t> & args)
{
  bool result;
  uint32_t headerLen = sizeof(CommandHeader);
  uint16_t argLen = args.size();
  uint8_t buffer[headerLen + argLen];
  CommandHeader header;

//	if (!is_connected) {
//	   return false;
//	}

//	for (int i = 0; i < args. size (); i ++)
//	   fprintf (stderr, "%x ", args [i]);
//	fprintf (stderr, "\n");
  header.CommandType = cmd;
  header.BodySize = argLen;

  for (uint32_t i = 0; i < sizeof(CommandHeader); i++)
  {
    buffer[i] = ((uint8_t *)(&header))[i];
  }

  if (argLen > 0)
  {
    for (uint16_t i = 0; i < argLen; i++)
    {
      buffer[headerLen + i] = args[i];
    }
  }

  try
  {
    tcpHandler.send_data(buffer, headerLen + argLen);
    result = true;
  }
  catch (std::exception & e)
  {
    std::cerr << "caught exception while sending command.\n";
    result = false;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return result;
}

bool SpyServerHandler::process_device_info(uint8_t * buffer,
                                       DeviceInfo & deviceInfo)
{
  std::memcpy(&deviceInfo, buffer, sizeof(DeviceInfo));
  std::cerr << "\n**********\nDevice Info:"
    << "\n   Type:                 " << deviceInfo.DeviceType
    << "\n   Serial:               " << deviceInfo.DeviceSerial
    << "\n   MaximumSampleRate:    " << deviceInfo.MaximumSampleRate
    << "\n   MaximumBandwidth:     " << deviceInfo.MaximumBandwidth
    << "\n   DecimationStageCount: " << deviceInfo.DecimationStageCount
    << "\n   GainStageCount:       " << deviceInfo.GainStageCount
    << "\n   MaximumGainIndex:     " << deviceInfo.MaximumGainIndex
    << "\n   MinimumFrequency:     " << deviceInfo.MinimumFrequency
    << "\n   MaximumFrequency:     " << deviceInfo.MaximumFrequency
    << "\n   Resolution:           " << deviceInfo.Resolution
    << "\n   MinimumIQDecimation:  " << deviceInfo.MinimumIQDecimation
    << "\n   ForcedIQFormat:       " << deviceInfo.ForcedIQFormat
    << std::endl;
  return true;
}

bool SpyServerHandler::process_client_sync(uint8_t * buffer, ClientSync & client_sync)
{
  std::memcpy((void *)(&client_sync), buffer, sizeof(ClientSync));
  std::memcpy((void *)(&m_curr_client_sync), buffer, sizeof(ClientSync));
  _gain = (double)m_curr_client_sync.Gain;
  _center_freq = (double)m_curr_client_sync.IQCenterFrequency;
  std::cerr << "\n**********\nClient sync:"
    << std::dec
    << "\n   Control:     " << (m_curr_client_sync.CanControl == 1 ? "Yes" : "No")
    << "\n   gain:        " << m_curr_client_sync.Gain
    << "\n   dev_ctr:     " << m_curr_client_sync.DeviceCenterFrequency
    << "\n   ch_ctr:      " << m_curr_client_sync.IQCenterFrequency
    << "\n   iq_ctr:      " << m_curr_client_sync.IQCenterFrequency
    << "\n   fft_ctr:     " << m_curr_client_sync.FFTCenterFrequency
    << "\n   min_iq_ctr:  " << m_curr_client_sync.MinimumIQCenterFrequency
    << "\n   max_iq_ctr:  " << m_curr_client_sync.MaximumIQCenterFrequency
    << "\n   min_fft_ctr: " << m_curr_client_sync.MinimumFFTCenterFrequency
    << "\n   max_fft_ctr: " << m_curr_client_sync.MaximumFFTCenterFrequency
    << std::endl;

  emit signal_call_parent();
  // parent->connect_on();
  qCDebug(sLogSpyServerHandler) << "Calling the parent to set up connection";
  got_sync_info = true;
  return true;
}

bool SpyServerHandler::get_deviceInfo(struct DeviceInfo & theDevice)
{
  if (!got_device_info)
    return false;

  theDevice.DeviceType = deviceInfo.DeviceType;
  theDevice.DeviceSerial = deviceInfo.DeviceSerial;
  theDevice.DecimationStageCount =
    deviceInfo.DecimationStageCount;
  theDevice.GainStageCount = deviceInfo.GainStageCount;
  theDevice.MaximumSampleRate = deviceInfo.MaximumSampleRate;
  theDevice.MaximumBandwidth = deviceInfo.MaximumBandwidth;
  theDevice.MaximumGainIndex = deviceInfo.MaximumGainIndex;
  theDevice.MinimumFrequency = deviceInfo.MinimumFrequency;
  theDevice.MaximumFrequency = deviceInfo.MaximumFrequency;
  return true;
}

bool SpyServerHandler::set_sample_rate_by_decim_stage(const uint32_t stage)
{
  std::vector<uint32_t> p(1);
  std::vector<uint32_t> q(1);
  p[0] = stage;
  set_setting(SETTING_IQ_DECIMATION, p);
  q[0] = STREAM_FORMAT_UINT8;
  set_setting(SETTING_IQ_FORMAT, q);
  return true;
}

double SpyServerHandler::get_sample_rate()
{
  return 2500000; // TODO: why fix?
}

bool SpyServerHandler::set_iq_center_freq(double centerFrequency)
{
  std::vector<uint32_t> param(1);
  param[0] = centerFrequency;
  set_setting(SETTING_IQ_FREQUENCY, param);
  param[0] = STREAM_FORMAT_UINT8;
  set_setting(SETTING_IQ_FORMAT, param);
  return true;
}

bool SpyServerHandler::set_gain_mode(bool automatic, size_t chan)
{
  (void)automatic;
  (void)chan;
  return 0;
}

bool SpyServerHandler::set_gain(double gain)
{
  std::vector<uint32_t> param(1);
  param[0] = (uint32_t)gain;
  set_setting(SETTING_GAIN, param);
  return true;
}

bool SpyServerHandler::is_streaming()
{
  return streaming.load();
}

void SpyServerHandler::start_running()
{
  std::vector<uint32_t> p(1);
  if (!streaming.load())
  {
    qCInfo(sLogSpyServerHandler) << "Starting Streaming";
    streaming.store(true);
    p[0] = 1;
    set_setting(SETTING_STREAMING_ENABLED, p);
  }
}

void SpyServerHandler::stop_running()
{
  std::vector<uint32_t> p;
  if (streaming.load())
  {
    streaming.store(false);
    p.push_back(0);
    set_setting(SETTING_STREAMING_ENABLED, p);
  }
}

bool SpyServerHandler::set_setting(uint32_t settingType, std::vector<uint32_t> & params)
{
  std::vector<uint8_t> argBytes;

  if (params.size() > 0)
  {
    argBytes = std::vector<uint8_t>(sizeof(SettingType) + params.size() * sizeof(uint32_t));
    uint8_t * settingBytes = (uint8_t *)&settingType;

    for (uint32_t i = 0; i < sizeof(uint32_t); i++)
    {
      argBytes[i] = settingBytes[i];
    }

    std::memcpy(&argBytes[0] + sizeof(uint32_t), &params[0], sizeof(uint32_t) * params.size());
  }
  else
  {
    argBytes = std::vector<uint8_t>();
  }

  return send_command(CMD_SET_SETTING, argBytes);
}

void SpyServerHandler::process_data(uint8_t * theBody, int length)
{
  outB->put_data_into_ring_buffer(theBody, length);
  emit signal_data_ready();
}

void SpyServerHandler::connection_set()
{
  std::vector<uint32_t> p;
  p.push_back(streamingMode);
  set_setting(SETTING_STREAMING_MODE, p);
  p[0] = 0x0;
  set_setting(SETTING_IQ_DIGITAL_GAIN, p);
  p[0] = STREAM_FORMAT_UINT8;
  set_setting(SETTING_IQ_FORMAT, p);
//	fprintf (stderr, "Connection is gezet, waar blijft de call?\n");
}

bool SpyServerHandler::isFileInput()
{
  return true;
}

QString SpyServerHandler::deviceName()
{
  return "spy-server-8Bits :";
}
