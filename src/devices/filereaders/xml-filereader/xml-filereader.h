/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013 .. 2019
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
#ifndef  XML_FILEREADER_H
#define  XML_FILEREADER_H

#include  <QObject>
#include  <QString>
#include  <QFrame>
#include  <atomic>
#include  "dab-constants.h"
#include  "device-handler.h"
#include  "ringbuffer.h"
#include  "ui_xmlfiles.h"

class QSettings;
class XmlDescriptor;
class XmlReader;

class XmlFileReader final : public QObject, public IDeviceHandler, public Ui_xmlfile_widget
{
Q_OBJECT
public:
  explicit XmlFileReader(const QString &);
  ~XmlFileReader() override;

  i32 getSamples(cf32 *, i32) override;
  i32 Samples() override;
  bool restartReader(i32) override;
  void stopReader() override;
  void setVFOFrequency(i32) override;
  i32 getVFOFrequency() override;
  void hide() override;
  void show() override;
  bool isHidden() override;
  bool isFileInput() override;
  void resetBuffer() override;
  i16 bitDepth() override;
  QString deviceName() override;

private:
  QFrame myFrame;
  std::atomic<bool> running = false;
  QString fileName;
  RingBuffer<cf32> _I_Buffer;
  FILE * theFile = nullptr;
  u32 filePointer = 0;
  XmlDescriptor * theDescriptor = nullptr;
  XmlReader * theReader = nullptr;
  
public slots:
  void slot_set_progress(i32, i32);
  void slot_handle_cb_loop_file(const bool iChecked);
};

#endif

