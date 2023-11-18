/*
 *
 *    Copyright (C) 2015
 *    Sebastian Held <sebastian.held@imst.de>
 *
 *    This file is part of Qt-DAB
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
 *    along with Qt-DAB-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <uhd/types/tune_request.hpp>
#include <uhd/utils/thread.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/exception.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <fstream>
#include <csignal>
#include <complex>
#include "uhd-handler.h"
#include "device-exceptions.h"

uhd_streamer::uhd_streamer(UhdHandler * d) :
  m_theStick(d)
{
  uhd::stream_args_t stream_args("fc32", "sc16");
  m_theStick->m_rx_stream = m_theStick->m_usrp->get_rx_stream(stream_args);
  uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
  stream_cmd.num_samps = 0;
  stream_cmd.stream_now = true;
  stream_cmd.time_spec = uhd::time_spec_t();
  m_theStick->m_rx_stream->issue_stream_cmd(stream_cmd);

  start();
}

void uhd_streamer::stop()
{
  m_stop_signal_called = true;
  while (isRunning())
  {
    wait(1);
  }
}

void uhd_streamer::run()
{
  while (!m_stop_signal_called)
  {
    //	get write position, ignore data2 and size2
    int32_t size1;
    int32_t size2;
    void * data1;
    void * data2;
    m_theStick->theBuffer->GetRingBufferWriteRegions(10000, &data1, &size1, &data2, &size2);

    if (size1 == 0)
    {
      // no room in ring buffer, wait for main thread to process the data
      usleep(100); // wait 100 us
      continue;
    }

    uhd::rx_metadata_t md;
    const auto num_rx_samps = (int32_t)m_theStick->m_rx_stream->recv(data1, size1, md, 1.0);
    m_theStick->theBuffer->AdvanceRingBufferWriteIndex(num_rx_samps);

    if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
    {
      std::cout << boost::format("Timeout while streaming") << std::endl;
      continue;
    }

    if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW)
    {
      std::cerr << boost::format("Got an overflow indication") << std::endl;
      continue;
    }

    //	   if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
    //	      std::cerr << boost::format("Receiver error: %s") % md.strerror() << std::endl;
    //	      continue;
    //	   }
  }
}

UhdHandler::UhdHandler(QSettings * s) :
  Ui_uhdWidget(),
  uhdSettings(s)
{
  setupUi(&myFrame);
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();

  // create a usrp device.
  std::string args;
  std::cout << std::endl;
  std::cout << boost::format("Creating the usrp device with: %s...") % args << std::endl;
  try
  {
    m_usrp = uhd::usrp::multi_usrp::make(args);

    std::string ref("internal");
    m_usrp->set_clock_source(ref);

    std::cout << boost::format("Using Device: %s") % m_usrp->get_pp_string() << std::endl;
    //	set sample rate
    m_usrp->set_rx_rate(inputRate);
    inputRate = (int32_t)std::round(m_usrp->get_rx_rate());
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (inputRate / 1e6) << std::endl << std::endl;

    //	allocate the rx buffer
    theBuffer = new RingBuffer<cmplx>(ringbufferSize * 1024);
  }
  catch (...)
  {
    fprintf(stderr, "No luck with uhd\n");
    throw (uhd_exception("No luck with UHD"));
  }
  //	some housekeeping for the local frame
  externalGain->setMaximum(_maxGain());
  uhdSettings->beginGroup("uhdSettings");
  externalGain->setValue(uhdSettings->value("externalGain", 40).toInt());
  f_correction->setValue(uhdSettings->value("f_correction", 0).toInt());
  KhzOffset->setValue(uhdSettings->value("KhzOffset", 0).toInt());
  uhdSettings->endGroup();

  _slot_set_external_gain(externalGain->value());
  _slot_set_khz_offset(KhzOffset->value());
  _slot_set_f_correction(f_correction->value());

  connect(externalGain, qOverload<int>(&QSpinBox::valueChanged), this, &UhdHandler::_slot_set_external_gain);
  connect(KhzOffset, qOverload<int>(&QSpinBox::valueChanged), this, &UhdHandler::_slot_set_khz_offset);
  connect(f_correction, qOverload<int>(&QSpinBox::valueChanged), this, &UhdHandler::_slot_set_f_correction);
}

UhdHandler::~UhdHandler()
{
  if (theBuffer != nullptr)
  {
    stopReader();
    uhdSettings->beginGroup("uhdSettings");
    uhdSettings->setValue("externalGain", externalGain->value());
    uhdSettings->setValue("f_correction", f_correction->value());
    uhdSettings->setValue("KhzOffset", KhzOffset->value());
    uhdSettings->endGroup();
    delete theBuffer;
  }
}

void UhdHandler::setVFOFrequency(int32_t freq)
{
  std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq / 1e6) << std::endl;
  uhd::tune_request_t tune_request(freq);
  m_usrp->set_rx_freq(tune_request);
}

int32_t UhdHandler::getVFOFrequency()
{
  auto freq = (int32_t)std::round(m_usrp->get_rx_freq());
  //std::cout << boost::format("Actual RX Freq: %f MHz...") % (freq / 1e6) << std::endl << std::endl;
  return freq;
}

bool UhdHandler::restartReader(int32_t freq)
{
  setVFOFrequency(freq);

  if (m_workerHandle != nullptr)
  {
    return true;
  }

  theBuffer->FlushRingBuffer();
  m_workerHandle = new uhd_streamer(this);
  return true;
}

void UhdHandler::stopReader()
{
  if (m_workerHandle == nullptr)
  {
    return;
  }

  m_workerHandle->stop();

  delete m_workerHandle;
  m_workerHandle = nullptr;
}

int32_t UhdHandler::getSamples(cmplx * v, int32_t size)
{
  size = std::min(size, theBuffer->GetRingBufferReadAvailable());
  theBuffer->getDataFromBuffer(v, size);
  return size;
}

int32_t UhdHandler::Samples()
{
  return theBuffer->GetRingBufferReadAvailable();
}

void UhdHandler::resetBuffer()
{
  theBuffer->FlushRingBuffer();
}

int16_t UhdHandler::bitDepth()
{
  return 12;
}

void UhdHandler::show()
{
  myFrame.show();
}

void UhdHandler::hide()
{
  myFrame.hide();
}

bool UhdHandler::isHidden()
{
  return myFrame.isHidden();
}

QString UhdHandler::deviceName()
{
  return "UHD";
}

int16_t UhdHandler::_maxGain() const
{
  uhd::gain_range_t range = m_usrp->get_rx_gain_range();
  return (int16_t)std::round(range.stop());
}

void UhdHandler::_slot_set_external_gain(int gain) const
{
  std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
  m_usrp->set_rx_gain(gain);
  //double gain_f = m_usrp->get_rx_gain();
  //std::cout << boost::format("Actual RX Gain: %f dB...") % gain_f << std::endl << std::endl;
}

void UhdHandler::_slot_set_f_correction(int f) const
{
  (void)f;
}

void UhdHandler::_slot_set_khz_offset(int o)
{
  vfoOffset = o;
}

