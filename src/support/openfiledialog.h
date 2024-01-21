/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C)  2015, 2016, 2017, 2018, 2019, 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB 
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
#ifndef  FIND_FILENAMES_H
#define  FIND_FILENAMES_H

//	just a convenience class to pack a number of functions
//	dealing with getting a filename
#include  <QSettings>
#include  <QString>
#include  <QDateTime>
#include  <QDir>
#include  <QFileDialog>
#include  <QMessageBox>
#include  <cstdio>
#include  <sndfile.h>

class OpenFileDialog
{
  QSettings * dabSettings;
public:
  explicit OpenFileDialog(QSettings *);
  ~OpenFileDialog() = default;

  FILE * open_content_dump_file_ptr(const QString & iChannelName);
  FILE * open_frame_dump_file_ptr(const QString & iServiceName);
  FILE * open_log_file_ptr();
  SNDFILE * open_audio_dump_sndfile_ptr(const QString & iServiceName);
  SNDFILE * open_raw_dump_sndfile_ptr(const QString & iDeviceName, const QString & iChannelName);
  QString get_skip_file_file_name();
  QString get_dl_text_file_name();
  QString get_maps_file_name();
  QString get_eti_file_name(const QString &, const QString &);

private:
  QString _open_file_dialog(const QString & iFileNamePrefix, const QString & iSettingName, const QString & iFileDesc, const QString & iFileExt);
  void _remove_invalid_characters(QString & ioStr) const;
};

#endif
