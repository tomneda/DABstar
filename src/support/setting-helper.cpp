/*
 * Copyright (c) 2024 by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
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

#include "setting-helper.h"
#include "dab-constants.h"
#include <QDir>
#include <QWidget>


SettingWidget::SettingWidget(const QString & key)
  : mKey(key)
{
}

void SettingWidget::register_widget_and_update_ui_from_setting(QWidget * const ipWidget, const QVariant & iDefaultValue)
{
  assert(mpWidget == nullptr); // only one-time registration necessary
  mpWidget = ipWidget;
  mDefaultValue = iDefaultValue;
  _update_ui_state_from_setting();

  if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(pD, &QCheckBox::checkStateChanged, [this](int iState){ _update_ui_state_to_setting(); });
#else
    connect(pD, &QCheckBox::stateChanged, [this](int iState){ update_ui_state_to_setting(); });
#endif
    return;
  }

  if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
  {
    connect(pD, &QComboBox::currentTextChanged, [this](const QString &){ _update_ui_state_to_setting(); });
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    connect(pD, qOverload<int>(&QSpinBox::valueChanged), [this](int iValue){ _update_ui_state_to_setting(); });
    return;
  }

  qFatal("Pointer to mpWidget not handled (1)");
}

QVariant SettingWidget::get_variant() const
{
  return SettingsStorage::instance().value(mKey, mDefaultValue);
}

void SettingWidget::_update_ui_state_from_setting() const
{
  assert(mpWidget != nullptr); // only one-time registration necessary

  if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
  {
    const int32_t var = SettingsStorage::instance().value(mKey, mDefaultValue).toInt();
    pD->setCheckState(static_cast<Qt::CheckState>(var));
    return;
  }

  if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
  {
    const QString var = SettingsStorage::instance().value(mKey, mDefaultValue).toString();
    if (const int32_t k = pD->findText(var);
        k >= 0)
    {
      pD->setCurrentIndex(k);
    }
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    const int32_t var = SettingsStorage::instance().value(mKey, mDefaultValue).toInt();
    pD->setValue(var);
    return;
  }

  qFatal("Pointer to mpWidget not handled (2)");
}

void SettingWidget::_update_ui_state_to_setting() const
{
  assert(mpWidget != nullptr); // only one-time registration necessary

  if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
  {
    SettingsStorage::instance().setValue(mKey, pD->checkState());
    return;
  }

  if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
  {
    SettingsStorage::instance().setValue(mKey, pD->currentText());
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    SettingsStorage::instance().setValue(mKey, pD->value());
    return;
  }

  qFatal("Pointer to pWidget not handled");
}


SettingPosAndSize::SettingPosAndSize(const QString & iCat)
: mCat(iCat)
{}

void SettingPosAndSize::read_widget_geometry(QWidget * const iopWidget, const int32_t iXPosDef, const int32_t iYPosDef, const int32_t iWidthDef, const int32_t iHeightDef, const bool iIsFixedSized) const
{
  const QVariant var = SettingsStorage::instance().value(mCat + "posAndSize", QVariant());

  if(!var.canConvert<QByteArray>())
  {
    qWarning("Cannot retrieve widget geometry from settings. Using default settings.");
    iopWidget->resize(QSize(iWidthDef, iHeightDef));
    iopWidget->move(QPoint(iXPosDef, iYPosDef));
    return;
  }

  if (!iopWidget->restoreGeometry(var.toByteArray()))
  {
    qWarning("restoreGeometry() returns false");
  }

  if (iIsFixedSized)
  {
    iopWidget->setFixedSize(QSize(iWidthDef, iHeightDef));
  }
}

void SettingPosAndSize::write_widget_geometry(const QWidget * const ipWidget) const
{
  const QByteArray var = ipWidget->saveGeometry();
  SettingsStorage::instance().setValue(mCat + "posAndSize", var);
}











SettingHelper::SettingHelper(QSettings * ipSettings) :
  mpSettings(ipSettings)
{
  Q_ASSERT(mpSettings != nullptr);
  _fill_map_with_defaults();
  _fill_map_from_settings();
}

void SettingHelper::_fill_map_with_defaults()
{
  QDir tempPath = QDir::tempPath();
  tempPath.setPath(tempPath.filePath(PRJ_NAME));
  const QString tempPicPath = tempPath.filePath("PIC").append('/');
  const QString tempMotPath = tempPath.filePath("MOT").append('/');
  const QString tempEpgPath = tempPath.filePath("EPG").append('/');

  mMap.insert(soundChannel, { "", "soundChannelQt", "default" });
  mMap.insert(picturesPath, { "", "picturesPath", tempPicPath });
  mMap.insert(filePath, { "", "filePath", tempMotPath });
  mMap.insert(epgPath, { "", "epgPath", tempEpgPath });
  mMap.insert(skipFile, { "", "skipFile", "" });
  mMap.insert(tiiFile, { "", "tiiFile", "" });
  mMap.insert(device, { "", "device", "no device" });
  mMap.insert(deviceFile, { "", "deviceFile", "" });
  mMap.insert(deviceVisible, { "", "deviceVisible", true });
  mMap.insert(spectrumVisible, { "", "spectrumVisible", false });
  mMap.insert(cirVisible, { "", "cirVisible", false });
  mMap.insert(techDataVisible, { "", "techDataVisible", false });
  mMap.insert(showDeviceWidget, { "", "showDeviceWidget", false });
  mMap.insert(presetName, { "", "presetname", "" });
  mMap.insert(channel, { "", "channel", "" });
  mMap.insert(epgWidth, { "", "epgWidth", 70 });
  mMap.insert(browserAddress, { "", "browserAddress", "http://localhost" });
  mMap.insert(mapPort, { "", "mapPort", 8080 });
  mMap.insert(epgFlag, { "", "epgFlag", false });

  mMap.insert(hidden, { "", "hidden", true });
  mMap.insert(latitude, { "", "latitude", 0 });
  mMap.insert(longitude, { "", "longitude", 0 });
  mMap.insert(saveDirAudioDump, { "", "saveDirAudioDump", QDir::homePath() });
  mMap.insert(saveDirSampleDump, { "", "saveDirSampleDump", QDir::homePath() });
  mMap.insert(saveDirContent, { "", "saveDirContent", QDir::homePath() });
  mMap.insert(serviceListSortCol, { "", "serviceListSortCol", false });
  mMap.insert(serviceListSortDesc, { "", "serviceListSortDesc", false });
  mMap.insert(cbShowTiiList, {"", "showTiiList", false });
  // mMap.insert(tii_threshold, { "", "tii_threshold", 6 });
  // mMap.insert(tii_subid, { "", "tii_subid", 0 });

  // special enums for windows position and size storage
  mMap.insert(mainWidget, { "", "mainWidget", QVariant() });
}

void SettingHelper::_fill_map_from_settings()
{
  for (auto & it : mMap)
  {
    mpSettings->beginGroup(it.Group);
    it.KeyVal = mpSettings->value(it.KeyStr, it.KeyVal);
    mpSettings->endGroup();
  }
}

QVariant SettingHelper::read(const EElem iElem) const
{
  return _get_map_elem(iElem).KeyVal;
}

void SettingHelper::write(const EElem iElem, const QVariant & iVal)
{
  SMapElem & me = _get_map_elem(iElem);

  if (me.pWidget != nullptr)
  {
    qFatal("Elem %d (setting name '%s') is registered as UI item, use sync_ui_state() instead", iElem, me.KeyStr.toUtf8().data());
  }

  _write_setting(me, iVal);
}

void SettingHelper::read_widget_geometry(const SettingHelper::EElem iElem, QWidget * const iopWidget) const
{
  QVariant var = read(iElem);

  if(!var.canConvert<QByteArray>())
  {
    qWarning("Cannot retrieve widget geometry from settings.");
    return;
  }

  if (!iopWidget->restoreGeometry(var.toByteArray()))
  {
    qWarning("restoreGeometry() returns false");
  }
}

void SettingHelper::write_widget_geometry(const EElem iElem, const QWidget * const ipWidget)
{
  write(iElem, ipWidget->saveGeometry());
}

void SettingHelper::sync_and_unregister_ui_elements()
{
  sync_all_ui_states(true);
  mMap.clear(); // avoid calling elements again because this method is called shortly before the UI elements are destroyed
  mpSettings->sync();
}

void SettingHelper::sync_ui_state(const EElem iElem, const bool iWriteSetting)
{
  SettingHelper::SMapElem & me = _get_map_elem(iElem);

  if (auto * const pAB = dynamic_cast<QCheckBox *>(me.pWidget);
      pAB != nullptr)
  {
    if (iWriteSetting)
    {
      _write_setting(me, pAB->checkState());
    }
    else
    {
      pAB->setCheckState(static_cast<Qt::CheckState>(read(iElem).toInt()));
    }
    return;
  }

  if (auto * const pSB = dynamic_cast<QSpinBox *>(me.pWidget);
      pSB != nullptr)
  {
    if (iWriteSetting)
    {
      _write_setting(me, pSB->value());
    }
    else
    {
      pSB->setValue(read(iElem).toInt());
    }
    return;
  }

  qFatal("Pointer to pWidget not valid for iElem %d", iElem);
}

void SettingHelper::sync_all_ui_states(const bool iWriteSetting) noexcept
{
  for (auto it = mMap.begin(); it != mMap.end(); ++it)
  {
    if (it.value().pWidget != nullptr) // update only registered UI elements
    {
      sync_ui_state(it.key(), iWriteSetting);
    }
  }
}

const SettingHelper::SMapElem & SettingHelper::_get_map_elem(SettingHelper::EElem iElem) const
{
  auto it = mMap.find(iElem);
  Q_ASSERT(it != mMap.end());
  return it.value();
}

SettingHelper::SMapElem & SettingHelper::_get_map_elem(SettingHelper::EElem iElem)
{
  auto it = mMap.find(iElem);
  Q_ASSERT(it != mMap.end());
  return it.value();
}




