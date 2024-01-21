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

#include  "findfilenames.h"
#include  "dab-constants.h"
#include  <QDebug>
#include  <QFileDialog>

FindFileNames::FindFileNames(QSettings * ipSetting)
{
  dabSettings = ipSetting;
}

FILE * FindFileNames::open_content_dump_file_ptr(const QString & iChannelName)
{
  const QString fileName = _open_file_dialog(PRJ_NAME "-" + iChannelName, "contentDir", "CSV", ".csv");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  FILE * fileP = fopen(fileName.toUtf8().data(), "w");

  if (fileP == nullptr)
  {
    qDebug() << "Could not open file " << fileName;
  }

  return fileP;
}

FILE * FindFileNames::open_frame_dump_file_ptr(const QString & iServiceName)
{
  const QString fileName = _open_file_dialog(iServiceName, "saveDir_frameDump", "AAC data", ".aac");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  FILE * theFile = fopen(fileName.toUtf8().data(), "w+b");

  if (theFile == nullptr)
  {
    qDebug() << "Cannot open ACC file " << fileName;
  }

  return theFile;
}

SNDFILE * FindFileNames::open_audio_dump_sndfile_ptr(const QString & iServiceName)
{
  const QString fileName = _open_file_dialog(iServiceName, "saveDir_audioDump", "PCM wave file", ".wav");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  SF_INFO sf_info;
  sf_info.samplerate = 48000;
  sf_info.channels = 2;
  sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE * theFile = sf_open(fileName.toUtf8().data(), SFM_WRITE, &sf_info);

  if (theFile == nullptr)
  {
    qDebug() << "Cannot open WAV file " << fileName;
  }

  return theFile;
}


SNDFILE * FindFileNames::open_raw_dump_sndfile_ptr(const QString & iDeviceName, const QString & iChannelName)
{
  const QString fileName = _open_file_dialog(iDeviceName.trimmed() + "-" + iChannelName.trimmed(), "saveDir_rawDump", "Raw Data", ".sdr");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  SF_INFO sf_info;
  sf_info.samplerate = INPUT_RATE;
  sf_info.channels = 2;
  sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE * theFile = sf_open(fileName.toUtf8().data(), SFM_WRITE, &sf_info);

  if (theFile == nullptr)
  {
    qDebug() << "Cannot open SDR file " << fileName;
  }

  return theFile;
}

QString FindFileNames::get_skip_file_file_name()
{
  return _open_file_dialog(PRJ_NAME "-skipFile", "contentDir", "XML", ".xml");
}

QString FindFileNames::get_dl_text_file_name()
{
  return _open_file_dialog(PRJ_NAME "-dlText", "contentDir", "Text", ".txt");
}

FILE * FindFileNames::open_log_file_ptr()
{
  QString fileName = _open_file_dialog(PRJ_NAME "-LOG", "contentDir", "Text", ".txt");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  return fopen(fileName.toUtf8().data(), "w");
}

QString FindFileNames::get_maps_file_name()
{
  return _open_file_dialog(PRJ_NAME "-Transmitters", "contentDir", "Text", ".txt");
}

QString FindFileNames::get_eti_file_name(const QString & iEnsembleName, const QString & iChannelName)
{
  return _open_file_dialog(iChannelName.trimmed() + "-" + iEnsembleName.trimmed(), "contentDir", "ETI", ".eti");
}

QString FindFileNames::_open_file_dialog(const QString & iFileNamePrefix, const QString & iSettingName,
                                         const QString & iFileDesc, const QString & iFileExt)
{
  const QDir saveDir = dabSettings->value(iSettingName, QDir::homePath()).toString();

  QString fileName = iFileNamePrefix.trimmed() + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
  _remove_invalid_characters(fileName);
  fileName = saveDir.filePath(fileName + iFileExt);

  fileName = QFileDialog::getSaveFileName(nullptr,
                                          "Save file ...",
                                          fileName,
                                          iFileDesc + " (*" + iFileExt + ")",
                                          Q_NULLPTR,
                                          QFileDialog::DontUseNativeDialog);

  if (fileName.isEmpty())
  {
    return {};
  }

  if (!fileName.endsWith(iFileExt, Qt::CaseInsensitive))
  {
    fileName.append(iFileExt);
  }

  dabSettings->setValue(iSettingName, QFileInfo(fileName).path());

  return QDir::toNativeSeparators(fileName);
}

void FindFileNames::_remove_invalid_characters(QString & ioStr) const
{
  auto isValid = [](QChar c)
  {
    return c.isLetter() || c.isDigit() || (c == '-');
  };

  for (int i = 0; i < ioStr.length(); i++)
  {
    if (!isValid(ioStr.at(i)))
    {
      ioStr.replace(i, 1, '-');
    }
  }
}
