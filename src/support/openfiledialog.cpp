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

#include "openfiledialog.h"
#include "setting-helper.h"
#include "dab-constants.h"
#include <QDebug>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>

OpenFileDialog::OpenFileDialog(QSettings * ipSetting) :
  mpSettings(ipSetting)
{
}

FILE * OpenFileDialog::open_file(const QString & iFileName, const QString & iFileMode)
{
  FILE * pF = fopen(iFileName.toUtf8().data(), iFileMode.toUtf8().data());
#ifdef _WIN32
  if (pF == nullptr)
  {
    // "umlauts" in Windows need that
    pF = _wfopen(reinterpret_cast<const wchar_t *>(iFileName.utf16()), reinterpret_cast<const wchar_t *>(iFileMode.utf16()));
  }
#endif
  return pF;
}

SNDFILE * OpenFileDialog::open_snd_file(const QString & iFileName, int iMode, SF_INFO * ipSfInfo)
{
  SNDFILE * pSf = sf_open(iFileName.toUtf8().data(), iMode, ipSfInfo);
  if (pSf == nullptr)
  {
    pSf = sf_open(iFileName.toLatin1().data(), iMode, ipSfInfo); // "umlauts" in Windows need that
  }
  return pSf;
}

FILE * OpenFileDialog::open_content_dump_file_ptr(const QString & iChannelName)
{
  const QString fileName = _open_file_dialog(PRJ_NAME "-" + iChannelName, sSettingContentStorageDir, "CSV", ".csv");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  FILE * fileP = open_file(fileName, "w");

  if (fileP == nullptr)
  {
    qDebug() << "Could not open file " << fileName;
  }

  return fileP;
}

FILE * OpenFileDialog::open_frame_dump_file_ptr(const QString & iServiceName)
{
  const QString fileName = _open_file_dialog(iServiceName, sSettingAudioStorageDir, "AAC data", ".aac");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  FILE * theFile = open_file(fileName, "w+b");

  if (theFile == nullptr)
  {
    qDebug() << "Cannot open ACC file " << fileName;
  }

  return theFile;
}

SNDFILE * OpenFileDialog::open_audio_dump_sndfile_ptr(const QString & iServiceName)
{
  const QString fileName = _open_file_dialog(iServiceName, sSettingAudioStorageDir, "PCM-WAV", ".wav");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  SF_INFO sf_info;
  sf_info.samplerate = 48000;
  sf_info.channels = 2;
  sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE * theFile = open_snd_file(fileName.toUtf8().data(), SFM_WRITE, &sf_info);

  if (theFile == nullptr)
  {
    qDebug() << "Cannot open WAV file " << fileName;
  }

  return theFile;
}


SNDFILE * OpenFileDialog::open_raw_dump_sndfile_ptr(const QString & iDeviceName, const QString & iChannelName)
{
  const QString fileName = _open_file_dialog(iDeviceName.trimmed() + "-" + iChannelName.trimmed(), sSettingSampleStorageDir, "RAW-WAV", ".sdr");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  SF_INFO sf_info;
  sf_info.samplerate = INPUT_RATE;
  sf_info.channels = 2;
  sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  SNDFILE * theFile = open_snd_file(fileName.toUtf8().data(), SFM_WRITE, &sf_info);

  if (theFile == nullptr)
  {
    qDebug() << "Cannot open SDR file " << fileName;
  }

  return theFile;
}

QString OpenFileDialog::get_audio_dump_file_name(const QString & iServiceName)
{
  return _open_file_dialog(iServiceName, sSettingAudioStorageDir, "PCM-WAV", ".wav");
}

QString OpenFileDialog::get_skip_file_file_name()
{
  return _open_file_dialog(PRJ_NAME "-skipFile", sSettingContentStorageDir, "XML", ".xml");
}

QString OpenFileDialog::get_dl_text_file_name()
{
  return _open_file_dialog(PRJ_NAME "-dlText", sSettingContentStorageDir, "Text", ".txt");
}

FILE * OpenFileDialog::open_log_file_ptr()
{
  QString fileName = _open_file_dialog(PRJ_NAME "-LOG", sSettingContentStorageDir, "Text", ".txt");

  if (fileName.isEmpty())
  {
    return nullptr;
  }

  return open_file(fileName, "w");
}

QString OpenFileDialog::get_maps_file_name()
{
  return _open_file_dialog(PRJ_NAME "-Transmitters", sSettingContentStorageDir, "Text", ".txt");
}

QString OpenFileDialog::get_eti_file_name(const QString & iEnsembleName, const QString & iChannelName)
{
  return _open_file_dialog(iChannelName.trimmed() + "-" + iEnsembleName.trimmed(), sSettingContentStorageDir, "ETI", ".eti");
}

QString OpenFileDialog::_open_file_dialog(const QString & iFileNamePrefix, const QString & iSettingName, const QString & iFileDesc, const QString & iFileExt)
{
  const bool useNativeFileDialog = SettingHelper::get_instance().read(SettingHelper::cbUseNativeFileDialog).toBool();

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
                                          useNativeFileDialog ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog);

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
  const bool useNativeFileDialog = SettingHelper::get_instance().read(SettingHelper::cbUseNativeFileDialog).toBool();
  const QDir storedDir = mpSettings->value(sSettingSampleStorageDir, QDir::homePath()).toString();

  QString fileName;
  QString selectedFilter;

  fileName = QFileDialog::getOpenFileName(nullptr,
                                          "Open file ...",
                                          storedDir.path(),
                                          "Sample data (*.uff *.sdr *.raw *.iq)",
                                          &selectedFilter,
                                          useNativeFileDialog ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog);

  if (!fileName.isEmpty())
  {
    oType = get_file_type(fileName);
    mpSettings->setValue(sSettingSampleStorageDir, QFileInfo(fileName).path());
  }

  return fileName;
}

OpenFileDialog::EType OpenFileDialog::get_file_type(const QString & fileName) const
{
  EType fileType = EType::UNDEF;

  if      (fileName.endsWith(".uff", Qt::CaseInsensitive)) fileType = EType::XML;
  else if (fileName.endsWith(".sdr", Qt::CaseInsensitive)) fileType = EType::SDR;
  else if (fileName.endsWith(".raw", Qt::CaseInsensitive)) fileType = EType::RAW;
  else if (fileName.endsWith(".iq",  Qt::CaseInsensitive)) fileType = EType::IQ;
  else qDebug() << "Unknown file type in: " << fileName;

  return fileType;
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

