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

#include  "openfiledialog.h"

#include  "dab-constants.h"
#include  <QDebug>
#include  <QFileDialog>
#include  <QDateTime>
#include  <QDir>

OpenFileDialog::OpenFileDialog(QSettings * ipSetting) :
  mpSettings(ipSetting)
{
}

FILE * OpenFileDialog::open_content_dump_file_ptr(const QString & iChannelName)
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

FILE * OpenFileDialog::open_frame_dump_file_ptr(const QString & iServiceName)
{
  const QString fileName = _open_file_dialog(iServiceName, msAudioStorageDir, "AAC data", ".aac");

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

SNDFILE * OpenFileDialog::open_audio_dump_sndfile_ptr(const QString & iServiceName)
{
  const QString fileName = _open_file_dialog(iServiceName, msAudioStorageDir, "PCM-WAV", ".wav");

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


SNDFILE * OpenFileDialog::open_raw_dump_sndfile_ptr(const QString & iDeviceName, const QString & iChannelName)
{
  const QString fileName = _open_file_dialog(iDeviceName.trimmed() + "-" + iChannelName.trimmed(), msSampleStorageDir, "RAW-WAV", ".sdr");

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

QString OpenFileDialog::get_skip_file_file_name()
{
  return _open_file_dialog(PRJ_NAME "-skipFile", "contentDir", "XML", ".xml");
}

QString OpenFileDialog::get_dl_text_file_name()
{
  return _open_file_dialog(PRJ_NAME "-dlText", "contentDir", "Text", ".txt");
}

FILE * OpenFileDialog::open_log_file_ptr()
{
  QString fileName = _open_file_dialog(PRJ_NAME "-LOG", "contentDir", "Text", ".txt");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  return fopen(fileName.toUtf8().data(), "w");
}

QString OpenFileDialog::get_maps_file_name()
{
  return _open_file_dialog(PRJ_NAME "-Transmitters", "contentDir", "Text", ".txt");
}

QString OpenFileDialog::get_eti_file_name(const QString & iEnsembleName, const QString & iChannelName)
{
  return _open_file_dialog(iChannelName.trimmed() + "-" + iEnsembleName.trimmed(), "contentDir", "ETI", ".eti");
}

QString OpenFileDialog::_open_file_dialog(const QString & iFileNamePrefix, const QString & iSettingName, const QString & iFileDesc, const QString & iFileExt)
{
  const QDir storedDir = mpSettings->value(iSettingName, QDir::homePath()).toString();
  QString fileName;

  fileName = iFileNamePrefix.trimmed() + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
  _remove_invalid_characters(fileName);
  fileName = storedDir.filePath(fileName + iFileExt);

  fileName = QFileDialog::getSaveFileName(nullptr,
                                          "Save file ...",
                                          fileName,
                                          iFileDesc + " (*" + iFileExt + ")",
                                          nullptr,
                                          QFileDialog::DontUseNativeDialog);

  if (fileName.isEmpty())
  {
    return {};
  }

  if (!fileName.endsWith(iFileExt, Qt::CaseInsensitive))
  {
    fileName.append(iFileExt);
  }

  mpSettings->setValue(iSettingName, QFileInfo(fileName).path());

  return QDir::toNativeSeparators(fileName);
}

QString OpenFileDialog::open_sample_data_file_dialog_for_reading(EType & oType) const
{
  const QString FILETYPE_UFFXML = "UFF-XML (*.uff)";
  const QString FILETYPE_SDRWAV = "SDR-WAV (*.sdr)";
  const QString FILETYPE_RAW    = "RAW (*.raw)";
  const QString FILETYPE_IQ     = "IQ-RAW (*.iq)";

  const QDir storedDir = mpSettings->value(msSampleStorageDir, QDir::homePath()).toString();

  QString fileName;
  QString selectedFilter;

  fileName = QFileDialog::getOpenFileName(nullptr,
                                          "Open file ...",
                                          storedDir.path(),
                                          FILETYPE_UFFXML + ";;" + FILETYPE_SDRWAV + ";;" + FILETYPE_RAW + ";;" + FILETYPE_IQ,
                                          &selectedFilter,
                                          QFileDialog::DontUseNativeDialog);


  oType = EType::UNDEF;

  if (!fileName.isEmpty())
  {
    if      (selectedFilter == FILETYPE_UFFXML) oType = EType::XML;
    else if (selectedFilter == FILETYPE_SDRWAV) oType = EType::SDR;
    else if (selectedFilter == FILETYPE_RAW)    oType = EType::RAW;
    else if (selectedFilter == FILETYPE_IQ)     oType = EType::IQ;

    mpSettings->setValue(msSampleStorageDir, QFileInfo(fileName).path());
  }

  return fileName;
}

void OpenFileDialog::_remove_invalid_characters(QString & ioStr) const
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
