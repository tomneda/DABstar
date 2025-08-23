#
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

#include    <QSettings>
#include    <QLabel>
#include    <QMessageBox>
#include    <QHostAddress>
#include    <QTcpSocket>
#include    <QDir>
#include    "rtl_tcp_client.h"
#include    "rtl-sdr.h"
#include    "xml-filewriter.h"
#include    "device-exceptions.h"
#include    "openfiledialog.h"

#if !defined(__MINGW32__) && !defined(_WIN32)
  #include <netinet/in.h>  // for macro htonl
#endif


#define DEFAULT_FREQUENCY (kHz(220000))

typedef struct { /* structure size must be multiple of 2 bytes */
    char magic[4];
    u32 tuner_type;
    u32 tuner_gain_count;
} dongle_info_t;

RtlTcpClient::RtlTcpClient(QSettings *s, const QString &recorderVersion):myFrame(nullptr)
{
  for(i32 i = 0; i < 256; i++)
    mapTable[i] = ((f32)i - 127.38) / 128.0;
  remoteSettings = s;
  this->recorderVersion = recorderVersion;
  remoteSettings->beginGroup("RtlTcpClient");
  i32 x = remoteSettings->value("position-x", 100).toInt();
  i32 y = remoteSettings->value("position-y", 100).toInt();
  remoteSettings->endGroup();
  setupUi(&myFrame);
  myFrame.move(QPoint(x, y));
  myFrame.setWindowFlag(Qt::Tool, true); // does not generate a task bar icon
  myFrame.show();

  // setting the defaults and constants
  Bitrate = INPUT_RATE;
  remoteSettings->beginGroup("RtlTcpClient");
  Gain = remoteSettings->value("RtlTcpClient-gain", 20).toInt();
  Ppm = remoteSettings->value("RtlTcpClient-ppm", 0).toDouble();
  AgcMode = remoteSettings->value("RtlTcpClient-agc", 2).toInt();
  BiasT = remoteSettings->value("RtlTcpClient-biast", 0).toInt();
  basePort = remoteSettings->value("rtl_tcp_port", 1234).toInt();
  Bandwidth = remoteSettings->value("RtlTcpClient-bw", 1750).toInt();
  ipAddress = remoteSettings->value("remote-server", "127.0.0.1").toString();
  remoteSettings->endGroup();
  tcp_gain->setValue(Gain);
  tcp_ppm->setValue(Ppm);
  setAgcMode(AgcMode);
  tcp_bandwidth->setValue(Bandwidth);
  tcp_port->setValue(basePort);
  tcp_address->setText(ipAddress);
  vfoFrequency = DEFAULT_FREQUENCY;
  _I_Buffer = new RingBuffer<cf32>(32 * 32768);
  connected = false;

  connect(tcp_connect, SIGNAL(clicked(void)), this, SLOT(wantConnect(void)));
  connect(tcp_disconnect, SIGNAL(clicked(void)), this, SLOT(setDisconnect(void)));
  connect(tcp_gain, SIGNAL(valueChanged(int)), this, SLOT(sendGain(int)));
  connect(tcp_ppm, SIGNAL(valueChanged(double)), this, SLOT(set_fCorrection(double)));
  connect(hw_agc, SIGNAL(clicked()), this, SLOT(handle_hw_agc()));
  connect(sw_agc, SIGNAL(clicked()), this, SLOT(handle_sw_agc()));
  connect(manual, SIGNAL(clicked()), this, SLOT(handle_manual()));
  connect(tcp_biast, SIGNAL(stateChanged(int)), this, SLOT(setBiasT(int)));
  connect(tcp_bandwidth, SIGNAL(valueChanged(int)), this, SLOT(setBandwidth(int)));
  connect(tcp_port, SIGNAL(valueChanged(int)), this, SLOT(setPort(int)));
  connect(tcp_address, SIGNAL(returnPressed()), this, SLOT(setAddress()));
  connect(xml_dumpButton, SIGNAL(clicked()), this, SLOT(set_xmlDump()));
  theState->setText("waiting to start");

  xmlDumper = nullptr;
  xml_dumping.store(false);
}

RtlTcpClient::~RtlTcpClient()
{
  remoteSettings->beginGroup("RtlTcpClient");
  if (connected) // close previous connection
  {
    stopReader();
    remoteSettings->setValue("remote-server", toServer.peerAddress().toString());
    remoteSettings->setValue("RtlTcpClient_port", basePort);
  }
  remoteSettings->setValue("position-x", myFrame.pos().x());
  remoteSettings->setValue("position-y", myFrame.pos().y());
  remoteSettings->setValue("RtlTcpClient-gain", Gain);
  remoteSettings->setValue("RtlTcpClient-ppm", Ppm);
  remoteSettings->setValue("RtlTcpClient-agc", AgcMode);
  remoteSettings->setValue("RtlTcpClient-biast", BiasT);
  remoteSettings->setValue("RtlTcpClient-bw", Bandwidth);
  remoteSettings->endGroup();
  myFrame.hide();
  toServer.close();
  delete _I_Buffer;
}

void RtlTcpClient::wantConnect()
{

  if (connected)
    return;

  QString s = ipAddress;
  qDebug().noquote().nospace() << "Connect to " << s << ":" << basePort;
  toServer.connectToHost(QHostAddress(s), basePort);
  if (!toServer.waitForConnected(2000))
  {
    QMessageBox::warning(&myFrame, tr("sdr"), tr("connection failed\n"));
    return;
  }
  connected = true;
  theState->setText("connected");
  sendRate(Bitrate);
  setAgcMode(AgcMode);
  set_fCorrection(Ppm);
  setBandwidth(Bandwidth);
  setBiasT(BiasT);
  toServer.waitForBytesWritten();
  dongle_info_received = false;
}

i32 RtlTcpClient::defaultFrequency()
{
  return DEFAULT_FREQUENCY; // choose any legal frequency here
}

void RtlTcpClient::setVFOFrequency(i32 newFrequency)
{
  if (!connected)
    return;
  vfoFrequency = newFrequency;
  // here the command to set the frequency
  sendVFO(newFrequency);
}

i32 RtlTcpClient::getVFOFrequency()
{
  return vfoFrequency;
}

bool RtlTcpClient::restartReader(i32 freq)
{
  if (!connected)
    return true;
  vfoFrequency = freq;
  // here the command to set the frequency
  sendVFO(freq);
  connect(&toServer, SIGNAL(readyRead(void)), this, SLOT(readData(void)));
  return true;
}

void RtlTcpClient::stopReader()
{
  if (!connected)
    return;
  disconnect(&toServer, SIGNAL(readyRead(void)), this, SLOT(readData(void)));
  close_xmlDump();
}

//
//  The brave old getSamples.For the dab stick, we get
//  size: still in I/Q pairs, but we have to convert the data from
//  u8 to DSPCOMPLEX *
i32 RtlTcpClient::getSamples(cf32 *V, i32 size)
{
  i32 amount = 0;
  amount = _I_Buffer->get_data_from_ring_buffer(V, size);
  return amount;
}

i32 RtlTcpClient::Samples()
{
  return _I_Buffer->get_ring_buffer_read_available();
}

i16 RtlTcpClient::bitDepth()
{
  return 8;
}

//  These functions are typical for network use
void RtlTcpClient::readData()
{
  if(!dongle_info_received)
  {
    dongle_info_t dongle_info;
    if (toServer.bytesAvailable() >= (qint64)sizeof(dongle_info))
    {
      toServer.read((char *)&dongle_info, sizeof(dongle_info));
      dongle_info_received = true;
      if(memcmp(dongle_info.magic, "RTL0", 4) == 0)
      {
        switch(htonl(dongle_info.tuner_type))
        {
          case RTLSDR_TUNER_E4000:
            tuner_text = "E4000";
            break;
          case RTLSDR_TUNER_FC0012:
            tuner_text = "FC0012";
            break;
          case RTLSDR_TUNER_FC0013:
            tuner_text = "FC0013";
            break;
          case RTLSDR_TUNER_FC2580:
            tuner_text = "FC2580";
            break;
          case RTLSDR_TUNER_R820T:
            tuner_text = "R820T";
            break;
          case RTLSDR_TUNER_R828D:
            tuner_text = "R828D";
            break;
          default:
            tuner_text = "unknown";
        }
        tuner_type->setText(tuner_text);
      }
    }
  }
  if(dongle_info_received)
  {
    u8 buffer[8192];
    cf32 localBuffer[4096];

    while (toServer.bytesAvailable() > 8192)
    {
      toServer.read((char *)buffer, 8192);
      for (i32 i = 0; i < 4096; i ++)
        localBuffer[i] = cf32(mapTable[buffer[2 * i]], mapTable[buffer[2 * i + 1]]);
      _I_Buffer->put_data_into_ring_buffer(localBuffer, 4096);
      if (xml_dumping.load())
        xmlWriter->add((std::complex<uint8_t> *)buffer, 4096);
    }
  }
}

//  commands are packed in 5 bytes, one "command byte"
//  and an integer parameter
struct command {
  u8 cmd;
  u32 param;
}__attribute__((packed));

#define ONE_BYTE    8

void RtlTcpClient::sendCommand(u8 cmd, i32 param)
{
  if(connected)
  {
    QByteArray datagram;

    datagram.resize(5);
    datagram[0] = cmd;      // command to set rate
    datagram[4] = param & 0xFF;  //lsb last
    datagram[3] = (param >> ONE_BYTE) & 0xFF;
    datagram[2] = (param >> (2 * ONE_BYTE)) & 0xFF;
    datagram[1] = (param >> (3 * ONE_BYTE)) & 0xFF;
    toServer.write(datagram.data(), datagram.size());
  }
}

void RtlTcpClient::sendVFO(i32 frequency)
{
  sendCommand(0x01, frequency);
}

void RtlTcpClient::sendRate(i32 rate)
{
  Bitrate = rate;
  sendCommand(0x02, rate);
}

void RtlTcpClient::sendGain(i32 gain)
{
  Gain = gain;
  sendCommand(0x04, 10 * gain);
}

//  correction is in ppm
void RtlTcpClient::set_fCorrection(f64 ppm)
{
  Ppm = ppm;
  i32 corr = ppm * 1000;
  sendCommand(0x83, corr);
}

void RtlTcpClient::setAgcMode(i32 agc)
{
  AgcMode = agc;
  if(agc == 0)
    hw_agc->setChecked(true);
  else if(agc == 1)
    manual->setChecked(true);
  else
    sw_agc->setChecked(true);
  sendCommand(0x08, agc == 0);
  sendCommand(0x03, agc);
  if(agc == 1)
    sendGain(Gain);
  tcp_gain->setEnabled(agc == 1);
  gainLabel->setEnabled(agc == 1);
}

void RtlTcpClient::setBiasT(i32 biast)
{
  BiasT = biast ? 1 : 0;
  sendCommand(0x0e, biast ? 1 : 0);
}

void RtlTcpClient::setBandwidth(i32 bw)
{
  Bandwidth = bw;
  sendCommand(0x40, bw * 1000);
}

void RtlTcpClient::setPort(i32 port)
{
  basePort = port;
}

void RtlTcpClient::setAddress()
{
  tcp_address->setInputMask("000.000.000.000");
  ipAddress = tcp_address->text();
}

void RtlTcpClient::setDisconnect()
{
  if (connected) // close previous connection
  {
    stopReader();
    remoteSettings->beginGroup("RtlTcpClient");
    remoteSettings->setValue("remote-server", toServer.peerAddress().toString());
    remoteSettings->setValue("RtlTcpClient_port", basePort);
    remoteSettings->setValue("RtlTcpClient-gain", Gain);
    remoteSettings->setValue("RtlTcpClient-ppm", Ppm);
    remoteSettings->setValue("RtlTcpClient-agc", AgcMode);
    remoteSettings->setValue("RtlTcpClient-biast", BiasT);
    remoteSettings->setValue("RtlTcpClient-bw", Bandwidth);
    remoteSettings->endGroup();
    toServer.close();
  }
  connected = false;
  theState->setText("disconnected");
}

void RtlTcpClient::show()
{
  myFrame.show();
}

void RtlTcpClient::hide()
{
  //myFrame.hide();
}

bool RtlTcpClient::isHidden()
{
  return myFrame.isHidden();
}

bool RtlTcpClient::isFileInput()
{
  return false;
}

void RtlTcpClient::resetBuffer()
{
}

QString RtlTcpClient::deviceName()
{
  return "RtlTcp";
}

void RtlTcpClient::handle_hw_agc()
{
  setAgcMode(0);
}

void RtlTcpClient::handle_sw_agc()
{
  setAgcMode(2);
}

void RtlTcpClient::handle_manual()
{
  setAgcMode(1);
}

void RtlTcpClient::set_xmlDump()
{
  if (!xml_dumping.load())
  {
    if (setup_xmlDump())
    {
      xml_dumpButton->setText("writing xml file");
    }
  }
  else
  {
    close_xmlDump();
    xml_dumpButton->setText("Dump to xml");
  }
}

bool RtlTcpClient::setup_xmlDump()
{
  OpenFileDialog filenameFinder(remoteSettings);
  xmlDumper = filenameFinder.open_raw_dump_xmlfile_ptr(tuner_text);
  if (xmlDumper == nullptr)
    return false;

  xmlWriter = new XmlFileWriter(xmlDumper, 8, "uint8", INPUT_RATE,
                                getVFOFrequency(), deviceName(),
                                tuner_text, recorderVersion);
  xml_dumping.store(true);
  return true;
}

void RtlTcpClient::close_xmlDump()
{
  if (xmlDumper == nullptr)
    return;
  xml_dumping.store(false);
  usleep(1000);
  xmlWriter->computeHeader();
  delete xmlWriter;
  fclose(xmlDumper);
  xmlDumper = nullptr;
}

