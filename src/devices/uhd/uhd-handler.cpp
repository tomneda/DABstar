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
    i32 size;
    void * data;
    m_theStick->theBuffer->get_writable_ring_buffer_segment(10000, &data, &size);

    if (size == 0)
    {
      // no room in ring buffer, wait for main thread to process the data
      usleep(100); // wait 100 us
      continue;
    }

    uhd::rx_metadata_t md;
    const auto num_rx_samps = (i32)m_theStick->m_rx_stream->recv(data, size, md, 1.0);

    m_theStick->theBuffer->advance_ring_buffer_write_index(num_rx_samps);

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

  std::vector<std::string> antList;

  // create a usrp device.
  std::string args;
  std::cout << std::endl;
  std::cout << boost::format("Creating the USRP device with: %s...") % args << std::endl;
  try
  {
    m_usrp = uhd::usrp::multi_usrp::make(args);

    std::string ref("internal");
    m_usrp->set_clock_source(ref);

    // fill antenna connectors combobox
    antList = m_usrp->get_rx_antennas();
    if (antList.empty())
    {
      antList.emplace_back("(empty)");
    }
    for (const auto & al : antList)
    {
      cmbAnt->addItem(al.c_str());
    }

    std::cout << boost::format("Using Device: %s") % m_usrp->get_pp_string() << std::endl;
    //	set sample rate
    m_usrp->set_rx_rate(inputRate);
    inputRate = (i32)std::round(m_usrp->get_rx_rate());
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (inputRate / 1e6) << std::endl << std::endl;

    //	allocate the rx buffer
    theBuffer = new RingBuffer<cf32>(ringbufferSize * 1024);
  }
  catch (...)
  {
    qWarning("No luck with UHD\n");
    throw (std_exception_string("No luck with UHD"));
  }
  //	some housekeeping for the local frame
  externalGain->setMaximum(_maxGain());
  uhdSettings->beginGroup(SETTING_GROUP_NAME);
  externalGain->setValue(uhdSettings->value("externalGain", 40).toInt());
  f_correction->setValue(uhdSettings->value("f_correction", 0).toInt());
  KhzOffset->setValue(uhdSettings->value("KhzOffset", 0).toInt());
  uhdSettings->endGroup();

  _load_save_combobox_settings(cmbAnt, "antSelector", false);

  _slot_set_external_gain(externalGain->value());
  _slot_set_khz_offset(KhzOffset->value());
  _slot_set_f_correction(f_correction->value());
  _slot_handle_ant_selector(cmbAnt->currentText());

  connect(externalGain, qOverload<i32>(&QSpinBox::valueChanged), this, &UhdHandler::_slot_set_external_gain);
  connect(KhzOffset, qOverload<i32>(&QSpinBox::valueChanged), this, &UhdHandler::_slot_set_khz_offset);
  connect(f_correction, qOverload<i32>(&QSpinBox::valueChanged), this, &UhdHandler::_slot_set_f_correction);
  connect(cmbAnt, &QComboBox::textActivated, this, &UhdHandler::_slot_handle_ant_selector);
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

void UhdHandler::setVFOFrequency(i32 freq)
{
  std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq / 1e6) << std::endl;
  uhd::tune_request_t tune_request(freq);
  m_usrp->set_rx_freq(tune_request);
}

i32 UhdHandler::getVFOFrequency()
{
  auto freq = (i32)std::round(m_usrp->get_rx_freq());
  //std::cout << boost::format("Actual RX Freq: %f MHz...") % (freq / 1e6) << std::endl << std::endl;
  return freq;
}

bool UhdHandler::restartReader(i32 freq)
{
  setVFOFrequency(freq);

  if (m_workerHandle != nullptr)
  {
    return true;
  }

  theBuffer->flush_ring_buffer();
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

i32 UhdHandler::getSamples(cf32 * v, i32 size)
{
  size = std::min(size, theBuffer->get_ring_buffer_read_available());
  theBuffer->get_data_from_ring_buffer(v, size);
  return size;
}

i32 UhdHandler::Samples()
{
  return theBuffer->get_ring_buffer_read_available();
}

void UhdHandler::resetBuffer()
{
  theBuffer->flush_ring_buffer();
}

i16 UhdHandler::bitDepth()
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

i16 UhdHandler::_maxGain() const
{
  uhd::gain_range_t range = m_usrp->get_rx_gain_range();
  return (i16)std::round(range.stop());
}

void UhdHandler::_slot_set_external_gain(i32 gain) const
{
  std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
  m_usrp->set_rx_gain(gain);
  //f64 gain_f = m_usrp->get_rx_gain();
  //std::cout << boost::format("Actual RX Gain: %f dB...") % gain_f << std::endl << std::endl;
}

void UhdHandler::_slot_set_f_correction(i32 f) const
{
  (void)f;
}

void UhdHandler::_slot_set_khz_offset(i32 o)
{
  vfoOffset = o;
}

void UhdHandler::_slot_handle_ant_selector(const QString & iAnt)
{
  try
  {
    m_usrp->set_rx_antenna(iAnt.toStdString());
    _load_save_combobox_settings(cmbAnt, "antSelector", true);
  }
  catch (...)
  {
    qWarning("Unknown Antenna name: %s", iAnt.toStdString().c_str());
  }
}

void UhdHandler::_load_save_combobox_settings(QComboBox * ipCmb, const QString & iName, bool iSave)
{
  uhdSettings->beginGroup(SETTING_GROUP_NAME);
  if (iSave)
  {
    uhdSettings->setValue(iName, ipCmb->currentText());
  }
  else
  {
    const QString h = uhdSettings->value(iName, "default").toString();
    const i32 k = ipCmb->findText(h);
    if (k != -1)
    {
      ipCmb->setCurrentIndex(k);
    }
  }
  uhdSettings->endGroup();
}

bool UhdHandler::isFileInput()
{
  return false;
}
