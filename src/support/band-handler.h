/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2015 .. 2020
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

#ifndef  BANDHANDLER_H
#define  BANDHANDLER_H

#include  <cstdint>
#include  <QComboBox>
#include  <QObject>
#include  <QString>
#include  <QSettings>
#include  <QTableWidget>
#include  <QFile>
#include  <QtXml>
#include  <cstdio>

struct SDabFrequencies;

class BandHandler : public QObject
{
Q_OBJECT
public:
  BandHandler(const QString &, QSettings *);
  ~BandHandler() override;

  void saveSettings();
  void setupChannels(QComboBox * s, uint8_t band);
  void setup_skipList(const QString &);
  int32_t get_frequency_Hz(const QString & Channel) const;
  int firstChannel() const;
  int nextChannel(int) const;
  // int prevChannel(int);
  void show();
  void hide();
  bool isHidden() const;

public slots:
  void slot_cell_selected(int, int) const;

private:
  QSettings * dabSettings;
  QString fileName;
  SDabFrequencies * selectedBand;
  QTableWidget theTable;

  // int lastOf(SDabFrequencies *) const;
  void default_skipList() const;
  void file_skipList(const QString &) const;
  void updateEntry(const QString &) const;
};

#endif

