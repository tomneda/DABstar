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
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSlider>
#include <QTimer>


namespace Settings
{

// here all the definitions of the ini file objects are done via macro filters
#define FILTER_DEFINITIONS
#include "setting-helper.cnf.h"
#undef FILTER_DEFINITIONS

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

Widget::~Widget()
{
  delete mpDeferTimer;
}

void Widget::register_widget_and_update_ui_from_setting(QWidget * const ipWidget, const QVariant & iDefaultValue)
{
  mpWidget = ipWidget;
  mDefaultValue = iDefaultValue;
  _update_ui_state_from_setting();

  if (auto * const pD = dynamic_cast<QCheckBox *>(mpWidget); pD != nullptr)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(pD, &QCheckBox::checkStateChanged, [this](i32 /*iState*/){ _update_ui_state_to_setting(); });
#else
    connect(pD, &QCheckBox::stateChanged, [this](i32 /*iState*/){ _update_ui_state_to_setting(); });
#endif
    return;
  }

  if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
  {
    // connect(pD, &QComboBox::currentTextChanged, [this](const QString &){ _update_ui_state_to_setting(); });
    connect(pD, &QComboBox::currentIndexChanged, [this](i32){ _update_ui_state_to_setting(); });
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    connect(pD, &QSpinBox::valueChanged, [this](i32 /*iValue*/){ _update_ui_state_to_setting(); });
    return;
  }

  if (auto * const pD = dynamic_cast<QDoubleSpinBox *>(mpWidget); pD != nullptr)
  {
    connect(pD, &QDoubleSpinBox::valueChanged, [this](f64 /*iValue*/){ _update_ui_state_to_setting(); });
    return;
  }

  if (auto * const pD = dynamic_cast<QSlider *>(mpWidget); pD != nullptr)
  {
    connect(pD, &QSlider::valueChanged, [this](i32 /*iValue*/){ _update_ui_state_to_setting_deferred(); });
    return;
  }

  qFatal("Pointer to mpWidget not handled (1)");
}

QVariant Widget::read() const
{
  return Storage::instance().value(mKey, mDefaultValue);
}

i32 Widget::get_combobox_index() const
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
    const i32 var = Storage::instance().value(mKey, mDefaultValue).toInt();
    pD->setCheckState(static_cast<Qt::CheckState>(var));
    return;
  }

  if (auto * const pD = dynamic_cast<QComboBox *>(mpWidget); pD != nullptr)
  {
    const QString var = Storage::instance().value(mKey, mDefaultValue).toString();
    const i32 k = pD->findText(var);
    if (k >= 0)
    {
      pD->setCurrentIndex(k);
    }
    return;
  }

  if (auto * const pD = dynamic_cast<QSpinBox *>(mpWidget); pD != nullptr)
  {
    const i32 var = Storage::instance().value(mKey, mDefaultValue).toInt();
    pD->setValue(var);
    return;
  }

  if (auto * const pD = dynamic_cast<QDoubleSpinBox *>(mpWidget); pD != nullptr)
  {
    const f64 var = Storage::instance().value(mKey, mDefaultValue).toDouble();
    pD->setValue(var);
    return;
  }

  if (auto * const pD = dynamic_cast<QSlider *>(mpWidget); pD != nullptr)
  {
    const i32 var = Storage::instance().value(mKey, mDefaultValue).toInt();
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

  if (auto * const pD = dynamic_cast<QDoubleSpinBox *>(mpWidget); pD != nullptr)
  {
    Storage::instance().setValue(mKey, pD->value());
    return;
  }

  if (auto * const pD = dynamic_cast<QSlider *>(mpWidget); pD != nullptr)
  {
    Storage::instance().setValue(mKey, pD->value());
    return;
  }

  qFatal("Pointer to pWidget not handled (3)");
}

/**
 * @brief Triggers a deferred update to synchronize the UI state with the current setting.
 *
 * This method internally manages a timer to delay the UI update operation by a fixed interval.
 * If the timer is not already created, it initializes the timer and sets it up to operate in
 * single-shot mode with an interval of 500 milliseconds. The method ensures that each call
 * starts the timer, scheduling the `_update_ui_state_to_setting` method to execute after the
 * timeout. This prevents frequent immediate updates and consolidates them into a single
 * update after the specified delay.
 */
void Widget::_update_ui_state_to_setting_deferred()
{
  if (mpDeferTimer == nullptr)
  {
    mpDeferTimer = new QTimer(this);
    mpDeferTimer->setSingleShot(true);
    mpDeferTimer->setInterval(500);
    connect(mpDeferTimer, &QTimer::timeout, [this](){ _update_ui_state_to_setting(); });
  }
  mpDeferTimer->start();
}

PosAndSize::PosAndSize(const QString & iCat)
: mKey(iCat + "/posAndSize")
{}

void PosAndSize::read_widget_geometry(QWidget * const iopWidget, const i32 iWidthDef /*= -1*/, const i32 iHeightDef /*= -1*/, const bool iIsFixedSized /*= false*/) const
{
  const i32 x = Storage::instance().value(mKey + "-x", -1).toInt();
  const i32 y = Storage::instance().value(mKey + "-y", -1).toInt();
  const i32 w = Storage::instance().value(mKey + "-w", iWidthDef).toInt();
  const i32 h = Storage::instance().value(mKey + "-h", iHeightDef).toInt();

  if (x >= 0 && y >= 0) // entries valid?
  {
    iopWidget->move(QPoint(x, y));
  }

  if (w > 0 && h > 0) // entries valid?
  {
    iopWidget->resize(QSize(w, h));
  }

  if (iIsFixedSized) // overwrite read settings if fixed-sized in width and height, take only over the position
  {
    iopWidget->setFixedSize(QSize(iWidthDef, iHeightDef));
  }
}

void PosAndSize::write_widget_geometry(const QWidget * const ipWidget) const
{
  Storage::instance().setValue(mKey + "-x", ipWidget->pos().x());
  Storage::instance().setValue(mKey + "-y", ipWidget->pos().y());
  Storage::instance().setValue(mKey + "-w", ipWidget->width());
  Storage::instance().setValue(mKey + "-h", ipWidget->height());
}

} // namespace Settings
