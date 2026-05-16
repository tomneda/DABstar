/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "epg_mot_handler.h"
#include "configuration.h"
#include "dab-processor.h"
#include "epgdec.h"
#include "epg-decoder.h"
#include "mot-content-types.h"
#include "setting-helper.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLoggingCategory>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(sLogEpgMot, "EpgMot", QtInfoMsg)

EpgMotHandler::EpgMotHandler(const SResourceConfig & iCfg, QObject * ipParent)
  : QObject(ipParent)
  , mpConfig(iCfg.pConfig)
  , mpPictureLabel(iCfg.pPictureLabel)
  , mpBtnOpenPicFolder(iCfg.pBtnOpenPicFolder)
  , mpOpenFileDialog(iCfg.pOpenFileDialog)
{
  assert(mpConfig != nullptr);
  assert(mpPictureLabel != nullptr);
  assert(mpBtnOpenPicFolder != nullptr);
  assert(mpOpenFileDialog != nullptr);

  mpEpgHandler.reset(new CEPGDecoder);
  mpEpgProcessor.reset(new EpgDecoder);

  connect(mpEpgProcessor.get(), &EpgDecoder::signal_set_epg_data, this, &EpgMotHandler::slot_set_epg_data);

  mTimerMotReceived.setInterval(100);
  mTimerMotReceived.setSingleShot(true);
  connect(&mTimerMotReceived, &QTimer::timeout, this, &EpgMotHandler::_slot_mot_timer_timeout);

  emit signal_epg_indicator(false);
}

EpgMotHandler::~EpgMotHandler()
{
  close_dl_text_file();
}

void EpgMotHandler::slot_init_paths()
{
  const QString basePath = Settings::Config::varDataBasePath5.read().toString();
  mPicPath = basePath + "PIC/";
  mMotPath = basePath + "MOT/";
  mEpgPath = basePath + "EPG/";
}

void EpgMotHandler::set_channel_info(const QString & iFIdOrChDescr, const QString & iEnsembleName)
{
  mCurFIdOrChDescr = iFIdOrChDescr;
  mCurEnsembleName = iEnsembleName;
}

void EpgMotHandler::on_stop_services()
{
  mMotPicPathLast.clear();
}

void EpgMotHandler::show_pause_slide() const
{
  const bool showSecondSlide = (QDateTime::currentDateTime().time().second() / 10 & 0x1) == 0;
  const char * const picFile = (showSecondSlide ? ":res/logo/dabinvlogo.png" : ":res/logo/dabstar320x240.png");

  QPixmap p;
  if (p.load(picFile, "png"))
  {
    _write_picture(p);
  }
}

void EpgMotHandler::close_dl_text_file()
{
  if (mpDlTextFile != nullptr)
  {
    fclose(mpDlTextFile);
    mpDlTextFile = nullptr;
  }
}

QString EpgMotHandler::get_pic_folder() const
{
  return (mMotPicPathLast.isEmpty() ? mPicPath : QFileInfo(mMotPicPathLast).absolutePath());
}

void EpgMotHandler::handle_mot_object(const QByteArray & iResult, const QString & iObjectName, const i32 iContentType, bool /*iDirElement*/)
{
  qCDebug(sLogEpgMot) << "ObjectName" << iObjectName << "ContentType" << iContentType;

  switch (getContentBaseType((MOTContentType)iContentType))
  {
  case MOTBaseTypeGeneralData: break;
  case MOTBaseTypeText:        _save_MOT_text(iResult, iContentType, iObjectName); break;
  case MOTBaseTypeImage:       _show_MOT_image(iResult, iContentType, iObjectName, 0); break;
  case MOTBaseTypeAudio: break;
  case MOTBaseTypeVideo: break;
  case MOTBaseTypeTransport:   _save_MOT_object(iResult, iObjectName); break;
  case MOTBaseTypeSystem: break;
  case MOTBaseTypeApplication: _save_MOT_EPG_data(iResult, iObjectName, iContentType); break;
  case MOTBaseTypeProprietary: break;
  }
}

void EpgMotHandler::slot_handle_mot_saving_selector(const i32 iIdx)
{
  mpBtnOpenPicFolder->setEnabled(iIdx > 0);
}

void EpgMotHandler::trigger_mot_indicator()
{
  if (!mTimerMotReceived.isActive())
  {
    emit signal_mot_indicator(true);
  }
  mTimerMotReceived.start();
}

void EpgMotHandler::slot_set_epg_data(i32 /*iSId*/, i32 /*iTheTime*/, const QString & /*iTheText*/, const QString & /*iTheDescr*/)
{
  // TODO: check EPG
}

void EpgMotHandler::slot_handle_dl_text_button()
{
  if (mpDlTextFile != nullptr)
  {
    close_dl_text_file();
    mpConfig->dlTextButton->setText("dlText");
    return;
  }

  const QString fileName = mpOpenFileDialog->get_dl_text_file_name();
  mpDlTextFile = fopen(fileName.toUtf8().data(), "w+");

  if (mpDlTextFile == nullptr)
  {
    return;
  }

  mpConfig->dlTextButton->setText("writing");
}

void EpgMotHandler::_slot_mot_timer_timeout()
{
  emit signal_mot_indicator(false);
}

bool EpgMotHandler::_save_MOT_EPG_data(const QByteArray & iResult, const QString & iObjectName, const i32 iContentType) const
{
  if (mEpgPath.isEmpty() || mpDabProcessor == nullptr)
  {
    return false;
  }

  std::vector<u8> epgData(iResult.begin(), iResult.end());
  const u32 ensembleId = mpDabProcessor->get_fib_decoder()->get_EId();
  const u32 currentSId = _extract_epg(iObjectName, ensembleId);
  const u32 julianDate = mpDabProcessor->get_fib_decoder()->get_mod_julian_date();
  const i32 subType = getContentSubType((MOTContentType)iContentType);
  mpEpgProcessor->process_epg(epgData.data(), (i32)epgData.size(), currentSId, subType, julianDate);

  if (mpConfig->cmbEpgObjectSaving5->currentIndex() > 0)
  {
    QString temp = iObjectName;
    if (temp.isEmpty())
    {
      temp = "epg_file";
    }

    const i32 dirLevel = mpConfig->cmbEpgObjectSaving5->currentIndex() - 1;
    const QString path = _generate_file_path(mEpgPath, temp, dirLevel);
    mpEpgHandler->decode(epgData, QDir::toNativeSeparators(path));
  }

  return true;
}

void EpgMotHandler::_save_MOT_text(const QByteArray & iResult, const i32 /*iContentType*/, const QString & /*iName*/) const
{
  if (mMotPath.isEmpty())
  {
    return;
  }

  // TODO: we do not know the real content type for sure, so we omit the file extension here (seems to be almost png images)
  const i32 dirLevel = mpConfig->cmbMotObjectSaving5->currentIndex() - 1;
  QString path = _generate_unique_file_path_from_hash(mMotPath, "", iResult, dirLevel);
  path = QDir::toNativeSeparators(path);

  if (!QFile::exists(path))
  {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
      qCCritical(sLogEpgMot, "_save_MOT_text(): cannot write file %s", path.toUtf8().data());
    }
    else
    {
      qCDebug(sLogEpgMot, "_save_MOT_text(): going to write MOT file %s", path.toUtf8().data());
      file.write(iResult);
    }
  }
  else
  {
    qCDebug(sLogEpgMot, "_save_MOT_text(): file %s already exists", path.toUtf8().data());
  }
}

void EpgMotHandler::_save_MOT_object(const QByteArray & iResult, const QString & iName)
{
  if (mMotPath.isEmpty())
  {
    return;
  }

  // _create_directory(mMotPath, false) is called in _save_MOT_text()
  const QString effectiveName = (iName.isEmpty() ? "motObject_" + QString::number(mMotObjectCnt++) : iName);
  _save_MOT_text(iResult, 5, effectiveName);
}

void EpgMotHandler::_show_MOT_image(const QByteArray & iData, const i32 iContentType, const QString & iPictureName, const i32 iDirs)
{
  if (!mIsChannelRunning || iPictureName.isEmpty())
  {
    return;
  }

  const char * type;

  switch (static_cast<MOTContentType>(iContentType))
  {
  case MOTCTImageGIF:  type = "gif"; break;
  case MOTCTImageJFIF: type = "jpg"; break;
  case MOTCTImageBMP:  type = "bmp"; break;
  case MOTCTImagePNG:  type = "png"; break;
  default: qWarning() << "Unknown content type 0x" << QString::number(iContentType, 16) << "for picture"; return;
  }

  qCDebug(sLogEpgMot, "show_MOTlabel %s, contentType 0x%x, dirs %d, type %s", iPictureName.toLocal8Bit().constData(), iContentType, iDirs, type);

  const bool saveMotObject = mpConfig->cmbMotObjectSaving5->currentIndex() > 0;

  if (saveMotObject && !mPicPath.isEmpty() && !mCurServiceLabel.isEmpty())
  {
    const i32 dirLevel = mpConfig->cmbMotObjectSaving5->currentIndex() - 1;
    QString pict = _generate_unique_file_path_from_hash(mPicPath, type, iData, dirLevel);
    pict = QDir::toNativeSeparators(pict);

    if (!QFile::exists(pict))
    {
      QFile file(pict);
      if (!file.open(QIODevice::WriteOnly))
      {
        qCCritical(sLogEpgMot) << "Cannot write file" << pict;
      }
      else
      {
        qCInfo(sLogEpgMot) << "Write picture to" << pict;
        file.write(iData);
      }
    }
    else if (pict != mMotPicPathLast) // show this message only once because some services transfers only one picture all day long
    {
      qCInfo(sLogEpgMot) << "File" << pict << "already exists";
    }
    mMotPicPathLast = pict;
  }

  {
    QPixmap p;
    p.loadFromData(iData, type);
    _write_picture(p);
  }
}

void EpgMotHandler::_write_picture(const QPixmap & iPixMap) const
{
  constexpr i32 w = 320;
  constexpr i32 h = 240;

  // typical the MOT size is 320 : 240 , so only scale for other sizes
  if (iPixMap.width() != w || iPixMap.height() != h)
  {
    mpPictureLabel->setPixmap(iPixMap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
  else
  {
    mpPictureLabel->setPixmap(iPixMap);
  }

  mpPictureLabel->setAlignment(Qt::AlignCenter);
  mpPictureLabel->show();
}

QString EpgMotHandler::_check_and_create_dir(const QString & iPath) const
{
  if (iPath.isEmpty())
  {
    return iPath;
  }

  QString dir = iPath;

  if (!dir.endsWith(QChar('/')))
  {
    dir += QChar('/');
  }

  _create_directory(dir, false);

  return dir;
}

void EpgMotHandler::_create_directory(const QString & iDirOrPath, const bool iContainsFileName) const
{
  const QString temp = iContainsFileName ? iDirOrPath.left(iDirOrPath.lastIndexOf(QChar('/'))) : iDirOrPath;

  if (!QDir(temp).exists())
  {
    QDir().mkpath(temp);
  }
}

QString EpgMotHandler::_generate_unique_file_path_from_hash(const QString & iBasePath, const QString & iFileExt, const QByteArray & iData, const i32 iDirLevel) const
{
  QString filename = QCryptographicHash::hash(iData, QCryptographicHash::Sha1).toHex().left(8);
  if (!iFileExt.isEmpty()) filename += "." + iFileExt;
  return _generate_file_path(iBasePath, filename, iDirLevel);
}

QString EpgMotHandler::_generate_file_path(const QString & iBasePath, const QString & iFileName, const i32 iDirLevel) const
{
  static const QRegularExpression regex(R"([<>:\"/\\|?*\s])"); // remove invalid file path characters

  const QString pathEnsemble = mCurEnsembleName.trimmed().replace(regex, "-");
  const QString pathService = mCurServiceLabel.trimmed().replace(regex, "-");
  const QString filename = pathEnsemble + "_" + pathService + "_" + iFileName;

  QString path = iBasePath;
  if (iDirLevel >= 1) path += pathEnsemble + "/";
  if (iDirLevel >= 2) path += pathService + "/";
  path += filename;

  _create_directory(path, true);

  return path;
}

u32 EpgMotHandler::_extract_epg(const QString & iName, const u32 /*iEnsembleId*/) const
{
  for (const auto & serv : *mpServiceList)
  {
    if (iName.contains(QString::number(serv.SId, 16), Qt::CaseInsensitive))
    {
      return serv.SId;
    }
  }
  return 0;
}
