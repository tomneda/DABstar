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

SpyServerHandler::SpyServerHandler(SpyServerClient * parent, const QString & ipAddress, i32 port, RingBuffer<u8> * outB)
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
  u32 sequenceNumberLast = (u32)(-1);
  static std::vector<u8> buffer(64 * 1024);
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
  while (running.load() && (inBuffer.get_ring_buffer_read_available() < (i32)sizeof(struct MessageHeader)))
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (!running.load())
  {
    return false;
  }

  inBuffer.get_data_from_ring_buffer((u8 *)(&header), sizeof(struct MessageHeader));
  return true;
}

bool SpyServerHandler::readBody(u8 * buffer, i32 size)
{
  i32 filler = 0;
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
  const u8 * protocolVersionBytes = (const u8 *)&ProtocolVersion;
  const u8 * softwareVersionBytes = (const u8 *)SoftwareID.c_str();
  std::vector<u8> args = std::vector<u8>(sizeof(ProtocolVersion) + SoftwareID.size());

  std::memcpy(&args[0], protocolVersionBytes, sizeof(ProtocolVersion));
  std::memcpy(&args[0] + sizeof(ProtocolVersion), softwareVersionBytes, SoftwareID.size());
  bool res = send_command(CMD_HELLO, args);

  return res;
}

void SpyServerHandler::cleanRecords()
{
  deviceInfo = {};
  frameNumber = 0;
}

bool SpyServerHandler::send_command(u32 cmd, std::vector<u8> & args)
{
  bool result;
  u32 headerLen = sizeof(CommandHeader);
  u16 argLen = args.size();
  u8 *buffer = make_vla(u8, headerLen + argLen);
  CommandHeader header;

//	if (!is_connected) {
//	   return false;
//	}

//	for (i32 i = 0; i < args. size (); i ++)
//	   fprintf (stderr, "%x ", args [i]);
//	fprintf (stderr, "\n");
  header.CommandType = cmd;
  header.BodySize = argLen;

  for (u32 i = 0; i < sizeof(CommandHeader); i++)
  {
    buffer[i] = ((u8 *)(&header))[i];
  }

  if (argLen > 0)
  {
    for (u16 i = 0; i < argLen; i++)
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

bool SpyServerHandler::process_device_info(u8 * buffer,
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

bool SpyServerHandler::process_client_sync(u8 * buffer, ClientSync & client_sync)
{
  std::memcpy((void *)(&client_sync), buffer, sizeof(ClientSync));
  std::memcpy((void *)(&m_curr_client_sync), buffer, sizeof(ClientSync));
  _gain = (f64)m_curr_client_sync.Gain;
  _center_freq = (f64)m_curr_client_sync.IQCenterFrequency;
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
  theDevice.DecimationStageCount = deviceInfo.DecimationStageCount;
  theDevice.GainStageCount = deviceInfo.GainStageCount;
  theDevice.MaximumSampleRate = deviceInfo.MaximumSampleRate;
  theDevice.MaximumBandwidth = deviceInfo.MaximumBandwidth;
  theDevice.MaximumGainIndex = deviceInfo.MaximumGainIndex;
  theDevice.MinimumFrequency = deviceInfo.MinimumFrequency;
  theDevice.MaximumFrequency = deviceInfo.MaximumFrequency;
  return true;
}

bool SpyServerHandler::set_sample_rate_by_decim_stage(const u32 stage)
{
  std::vector<u32> p(1);
  std::vector<u32> q(1);
  p[0] = stage;
  set_setting(SETTING_IQ_DECIMATION, p);
  q[0] = STREAM_FORMAT_UINT8;
  set_setting(SETTING_IQ_FORMAT, q);
  return true;
}

f64 SpyServerHandler::get_sample_rate()
{
  return 2500000; // TODO: why fix?
}

bool SpyServerHandler::set_iq_center_freq(f64 centerFrequency)
{
  std::vector<u32> param(1);
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

bool SpyServerHandler::set_gain(f64 gain)
{
  std::vector<u32> param(1);
  param[0] = (u32)gain;
  set_setting(SETTING_GAIN, param);
  return true;
}

bool SpyServerHandler::is_streaming()
{
  return streaming.load();
}

void SpyServerHandler::start_running()
{
  std::vector<u32> p(1);
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
  std::vector<u32> p;
  if (streaming.load())
  {
    streaming.store(false);
    p.push_back(0);
    set_setting(SETTING_STREAMING_ENABLED, p);
  }
}

bool SpyServerHandler::set_setting(u32 settingType, std::vector<u32> & params)
{
  std::vector<u8> argBytes;

  if (params.size() > 0)
  {
    argBytes = std::vector<u8>(sizeof(SettingType) + params.size() * sizeof(u32));
    u8 * settingBytes = (u8 *)&settingType;

    for (u32 i = 0; i < sizeof(u32); i++)
    {
      argBytes[i] = settingBytes[i];
    }

    std::memcpy(&argBytes[0] + sizeof(u32), &params[0], sizeof(u32) * params.size());
  }
  else
  {
    argBytes = std::vector<u8>();
  }

  return send_command(CMD_SET_SETTING, argBytes);
}

void SpyServerHandler::process_data(u8 * theBody, i32 length)
{
  outB->put_data_into_ring_buffer(theBody, length);
  emit signal_data_ready();
}

void SpyServerHandler::connection_set()
{
  std::vector<u32> p;
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
