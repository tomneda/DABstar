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
#include <QWidget>
#include <QObject>
#include <QVariant>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>

// #define USE_RESTORE_GEOMETRY  // with this switch the ini file is not so well readable

namespace Settings
{

QString Config::category = "Configuration";
PosAndSize Config::posAndSize{category};
Variant Config::varPicturesPath{category, "varPicturesPath"};
Variant Config::varMotPath{category, "varFilePath"};
Variant Config::varEpgPath{category, "varEpgPath"};
Variant Config::varSkipFile{category, "varSkipFile"};
Variant Config::varTiiFile{category, "varTiiFile"};
Variant Config::varBrowserAddress{category, "varBrowserAddress", "http://localhost"};
Variant Config::varMapPort{category, "varMapPort", 8080};
Variant Config::varLatitude{category, "varLatitude", 0};
Variant Config::varLongitude{category, "varLongitude", 0};
Variant Config::varSdrDevice{category, "varSdrDevice", "no device"};
Variant Config::varDeviceFile{category, "varDeviceFile", ""};
Variant Config::varDeviceUiVisible{category, "varDeviceUiVisible", true};
Variant Config::varPresetName{category, "varPresetName", ""};
Variant Config::varChannel{category, "varChannel", ""};
Widget Config::cbCloseDirect{category, "cbCloseDirect"};
Widget Config::cbUseStrongestPeak{category, "cbUseStrongestPeak"};
Widget Config::cbUseNativeFileDialog{category, "cbUseNativeFileDialog"};
Widget Config::cbUseUtcTime{category, "cbUseUtcTime"};
Widget Config::cbGenXmlFromEpg{category, "cbGenXmlFromEpg"};
Widget Config::cbAlwaysOnTop{category, "cbAlwaysOnTop"};
Widget Config::cbManualBrowserStart{category, "cbManualBrowserStart"};
Widget Config::cbSaveSlides{category, "cbSaveSlides"};
Widget Config::cbSaveTransToCsv{category, "cbSaveTransToCsv"};
Widget Config::cbUseDcAvoidance{category, "cbUseDcAvoidance"};
Widget Config::cbUseDcRemoval{category, "cbUseDcRemoval"};
Widget Config::cbShowNonAudioInServiceList{category, "cbShowNonAudioInService"};
Widget Config::sbTiiThreshold{category, "sbTiiThreshold"};
Widget Config::cbTiiCollisions{category, "cbTiiCollisions"};
Widget Config::sbTiiSubId{category, "sbTiiSubId"};
Widget Config::cbUrlClickable{category, "cbUrlClickable"};
Widget Config::cbAutoIterTiiEntries{category, "cbAutoIterTiiEntries"};
Widget Config::cmbSoundOutput{category, "cmbSoundOutput"};


QString Main::category = "Main";
PosAndSize Main::posAndSize{category};
Variant Main::varVolume{category, "varVolume", 100};


QString SpectrumViewer::category = "SpectrumViewer";
PosAndSize SpectrumViewer::posAndSize{category};
Variant SpectrumViewer::varUiVisible{category, "varUiVisible", false};


QString CirViewer::category = "CirViewer";
PosAndSize CirViewer::posAndSize{category};
Variant CirViewer::varUiVisible{category, "varUiVisible", false};


QString TiiViewer::category = "TiiViewer";
PosAndSize TiiViewer::posAndSize{category};
Variant TiiViewer::varUiVisible{category, "varUiVisible", false};


QString ServiceList::category = "ServiceList";
Variant ServiceList::varSortCol{category, "varSortCol", 0};
Variant ServiceList::varSortDesc{category, "varSSortDesc", false};


QString TechDataViewer::category = "TechDataViewer";
PosAndSize TechDataViewer::posAndSize{category};
Variant TechDataViewer::varUiVisible{category, "varUiVisible", false};


Variant::Variant(const QString & cat, const QString & key)
  : mKey(cat + "/" + key)
{}

Variant::Variant(const QString & cat, const QString & key, const QVariant & iDefaultValue)
  : mKey(cat + "/" + key)
  , mDefaultValue(iDefaultValue)
{}

void Variant::define_default_value(const QVariant & iDefaultValue)
{
  assert(!mDefaultValue.isValid()); // is it really not initialized yet?
  mDefaultValue = iDefaultValue;
}

QVariant Variant::read() const
{
  assert(mDefaultValue.isValid()); // is it really initialized yet?
  return Storage::instance().value(mKey, mDefaultValue);
}

void Variant::write(const QVariant & iValue) const
{
  return Storage::instance().setValue(mKey, iValue);
}


Widget::Widget(const QString & cat, const QString & key)
  : mKey(cat + "/" + key)
{
}

void Widget::register_widget_and_update_ui_from_setting(QWidget * const ipWidget, const QVariant & iDefaultValue)
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
    // connect(pD, &QComboBox::currentTextChanged, [this](const QString &){ _update_ui_state_to_setting(); });
    connect(pD, &QComboBox::currentIndexChanged, [this](int){ _update_ui_state_to_setting(); });
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    connect(pD, qOverload<int>(&QSpinBox::valueChanged), [this](int iValue){ _update_ui_state_to_setting(); });
    return;
  }

  qFatal("Pointer to mpWidget not handled (1)");
}

QVariant Widget::read() const
{
  return Storage::instance().value(mKey, mDefaultValue);
}

int32_t Widget::get_combobox_index() const
{
  auto * const pD = dynamic_cast<QComboBox *>(mpWidget);
  assert(pD != nullptr); // only for comboboxes
  const QString var = read().toString();
  return pD->findText(var);
}

void Widget::_update_ui_state_from_setting()
{
  assert(mpWidget != nullptr); // only one-time registration necessary

  if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
  {
    const int32_t var = Storage::instance().value(mKey, mDefaultValue).toInt();
    pD->setCheckState(static_cast<Qt::CheckState>(var));
    return;
  }

  if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
  {
    const QString var = Storage::instance().value(mKey, mDefaultValue).toString();
    const int32_t k = pD->findText(var);
    if (k >= 0)
    {
      pD->setCurrentIndex(k);
    }
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    const int32_t var = Storage::instance().value(mKey, mDefaultValue).toInt();
    pD->setValue(var);
    return;
  }

  qFatal("Pointer to mpWidget not handled (2)");
}

void Widget::_update_ui_state_to_setting() const
{
  assert(mpWidget != nullptr); // only one-time registration necessary

  if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
  {
    Storage::instance().setValue(mKey, pD->checkState());
    return;
  }

  if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
  {
    Storage::instance().setValue(mKey, pD->currentText());
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    Storage::instance().setValue(mKey, pD->value());
    return;
  }

  qFatal("Pointer to pWidget not handled (3)");
}


PosAndSize::PosAndSize(const QString & iCat)
: mKey(iCat + "/posAndSize")
{}

void PosAndSize::read_widget_geometry(QWidget * const iopWidget, const int32_t iWidthDef, const int32_t iHeightDef, const bool iIsFixedSized) const
{
#ifdef USE_RESTORE_GEOMETRY
  const QVariant var = Storage::instance().value(mKey, QVariant());

  if (!var.canConvert<QByteArray>())
  {
    if (iIsFixedSized)
    {
      iopWidget->setFixedSize(QSize(iWidthDef, iHeightDef));
    }
    else
    {
      iopWidget->resize(QSize(iWidthDef, iHeightDef));
    }
    return;
  }

  if (!iopWidget->restoreGeometry(var.toByteArray()))
  {
    qWarning("restoreGeometry() returns false");
  }
#else
  const int32_t x = Storage::instance().value(mKey + "-x", -1).toInt();
  const int32_t y = Storage::instance().value(mKey + "-y", -1).toInt();
  const int32_t w = Storage::instance().value(mKey + "-w", iWidthDef).toInt();
  const int32_t h = Storage::instance().value(mKey + "-h", iHeightDef).toInt();

  if (x >= 0 && y >= 0 && h > 0 && w > 0) // entries valid?
  {
    iopWidget->move(QPoint(x, y));
    iopWidget->resize(QSize(w, h));
  }
  else // only set width and height proposals
  {
    iopWidget->resize(QSize(iWidthDef, iHeightDef));
  }

#endif
  if (iIsFixedSized) // overwrite read settings if fixed-sized in width and height, take only over the position
  {
    iopWidget->setFixedSize(QSize(iWidthDef, iHeightDef));
  }
}

void PosAndSize::write_widget_geometry(const QWidget * const ipWidget) const
{
#ifdef USE_RESTORE_GEOMETRY
  const QByteArray var = ipWidget->saveGeometry();
  Storage::instance().setValue(mKey, var);
#else
  Storage::instance().setValue(mKey + "-x", ipWidget->pos().x());
  Storage::instance().setValue(mKey + "-y", ipWidget->pos().y());
  Storage::instance().setValue(mKey + "-w", ipWidget->width());
  Storage::instance().setValue(mKey + "-h", ipWidget->height());
#endif
}

} // namespace Settings
