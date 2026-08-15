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

#pragma once

#include "glob_data_types.h"
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QPixmap>
#include <QString>
#include <QSize>

// TrafficLight is a UI widget displaying a horizontal traffic light icon
// with an optional text label on the left side.
//
// Light stages (0..5):
//   0: Off (no lights illuminated)
//   1: Red
//   2: Red-Yellow
//   3: Yellow
//   4: Yellow-Green
//   5: Green
//
// The widget can be enabled or disabled via standard QWidget::setEnabled().
// When disabled, all lights are turned off (stage 0 / off graphic).

class TrafficLight : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(QString text READ get_text WRITE set_text)
  Q_PROPERTY(int stage READ get_stage WRITE set_stage NOTIFY signal_stage_changed)
  Q_PROPERTY(QSize iconSize READ get_icon_size WRITE set_icon_size)
  Q_PROPERTY(Qt::Alignment alignment READ get_alignment WRITE set_alignment)

public:
  enum class EStage : u8
  {
    Off = 0,         // No lights illuminated
    Red = 1,         // Red
    RedYellow = 2,   // Red + Yellow
    Yellow = 3,      // Yellow
    YellowGreen = 4, // Yellow + Green
    Green = 5        // Green
  };

  static constexpr i32 cStageCount = 6;
  static constexpr i32 cDefaultIconWidth = 48;
  static constexpr i32 cDefaultIconHeight = 24;

  explicit TrafficLight(QWidget * parent = nullptr);
  explicit TrafficLight(const QString & iText, QWidget * parent = nullptr);
  TrafficLight(const QString & iText, i32 iStage, QWidget * parent = nullptr);
  explicit TrafficLight(i32 iStage, QWidget * parent = nullptr);
  ~TrafficLight() override = default;

  // Stage control (0..5)
  void set_stage(i32 iStage);
  void set_stage(EStage iStage);
  void set_value(i32 iValue) { set_stage(iValue); }
  void set_level(i32 iLevel) { set_stage(iLevel); }
  [[nodiscard]] i32 get_stage() const { return mStage; }
  [[nodiscard]] i32 get_value() const { return mStage; }
  [[nodiscard]] EStage get_stage_enum() const { return static_cast<EStage>(mStage); }

  // Label text on the left (works seamlessly with or without label)
  void set_text(const QString & iText);
  void setText(const QString & iText) { set_text(iText); }
  void set_label_text(const QString & iText) { set_text(iText); }
  [[nodiscard]] QString get_text() const { return mText; }
  [[nodiscard]] QString text() const { return mText; }
  [[nodiscard]] bool has_label() const { return !mText.isEmpty(); }
  // Alignment
  void set_alignment(Qt::Alignment iAlignment);
  [[nodiscard]] Qt::Alignment get_alignment() const;
  void set_label_alignment(Qt::Alignment iAlignment);
  [[nodiscard]] Qt::Alignment get_label_alignment() const;

  // Icon sizing
  void set_icon_size(const QSize & iSize);
  void set_icon_size(i32 iWidth, i32 iHeight) { set_icon_size(QSize(iWidth, iHeight)); }
  [[nodiscard]] QSize get_icon_size() const { return mIconSize; }

  // Direct access to internal child widgets
  [[nodiscard]] QLabel * get_label() const { return mpLabel; }
  [[nodiscard]] QLabel * get_icon_label() const { return mpIconLabel; }
  [[nodiscard]] QHBoxLayout * get_layout() const { return mpLayout; }

  // Qt Widget overrides
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] QSize minimumSizeHint() const override;

public slots:
  void slot_set_stage(i32 iStage) { set_stage(iStage); }
  void slot_set_text(const QString & iText) { set_text(iText); }
  void slot_set_alignment(Qt::Alignment iAlignment) { set_alignment(iAlignment); }

signals:
  void signal_stage_changed(i32 iStage);

protected:
  void changeEvent(QEvent * event) override;

private:
  i32 mStage = static_cast<i32>(EStage::Off);
  QString mText;
  QSize mIconSize{cDefaultIconWidth, cDefaultIconHeight};

  QHBoxLayout * mpLayout = nullptr;
  QLabel * mpLabel = nullptr;
  QLabel * mpIconLabel = nullptr;

  void _init_ui(const QString & iText);
  void _update_display();

  static const QPixmap & _get_cached_pixmap(i32 iStage);
};

using TrafficLightWidget = TrafficLight;
