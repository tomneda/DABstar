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

// TrafficLight is a UI widget displaying a vertical traffic light icon
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
//
// An optional timeout can be configured via set_timeout(). If configured (timeout > 0),
// a single-shot timer is restarted on every set_stage() call. If no new stage is set
// before the timeout expires, the traffic light automatically switches to the defined
// timeout stage (default: Off).

class QTimer;
class QEnterEvent;

class TrafficLight : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(QString text READ get_text WRITE set_text)
  Q_PROPERTY(EStage stage READ get_stage WRITE set_stage NOTIFY signal_stage_changed)
  Q_PROPERTY(QSize iconSize READ get_icon_size WRITE set_icon_size)
  Q_PROPERTY(Qt::Alignment alignment READ get_alignment WRITE set_alignment)
  Q_PROPERTY(i32 timeoutMs READ get_timeout_ms WRITE set_timeout_ms)
  Q_PROPERTY(EStage timeoutStage READ get_timeout_stage WRITE set_timeout_stage)
  Q_PROPERTY(QPixmap pixmap READ pixmap WRITE setPixmap)
  Q_PROPERTY(bool scaledContents READ has_scaled_contents WRITE set_scaled_contents)

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
  Q_ENUM(EStage)

  static constexpr i32 cStageCount = 6;
  static constexpr i32 cDefaultIconWidth = 18;
  static constexpr i32 cDefaultIconHeight = 36;

  explicit TrafficLight(QWidget * parent = nullptr);
  explicit TrafficLight(const QString & iText, QWidget * parent = nullptr);
  TrafficLight(const QString & iText, EStage iStage, QWidget * parent = nullptr);
  explicit TrafficLight(EStage iStage, QWidget * parent = nullptr);
  TrafficLight(const QString & iText, EStage iStage, i32 iTimeoutMs, EStage iTimeoutStage, QWidget * parent = nullptr);
  TrafficLight(EStage iStage, i32 iTimeoutMs, EStage iTimeoutStage, QWidget * parent = nullptr);
  ~TrafficLight() override = default;

  // Stage / state control (0..5)
  void set_stage(EStage iStage);
  void set_state(EStage iStage) { set_stage(iStage); }
  [[nodiscard]] EStage get_stage() const { return mStage; }
  [[nodiscard]] EStage get_state() const { return get_stage(); }
  [[nodiscard]] EStage stage() const { return mStage; }

  // Timeout control
  void set_timeout(i32 iTimeoutMs, EStage iTimeoutStage = EStage::Off);
  void set_timeout(std::chrono::milliseconds iTimeout, EStage iTimeoutStage = EStage::Off) { set_timeout(static_cast<i32>(iTimeout.count()), iTimeoutStage); }
  void set_timeout_ms(i32 iTimeoutMs) { set_timeout(iTimeoutMs, mTimeoutStage); }
  void set_timeout_ms(std::chrono::milliseconds iTimeout) { set_timeout_ms(static_cast<i32>(iTimeout.count())); }
  void set_timeout_stage(EStage iTimeoutStage) { set_timeout(mTimeoutMs, iTimeoutStage); }
  [[nodiscard]] i32 get_timeout_ms() const { return mTimeoutMs; }
  [[nodiscard]] i32 get_timeout() const { return mTimeoutMs; }
  [[nodiscard]] EStage get_timeout_stage() const { return mTimeoutStage; }
  [[nodiscard]] bool has_timeout() const { return mTimeoutMs > 0; }
  [[nodiscard]] bool is_timer_active() const;
  void restart_timeout_timer();
  void stop_timeout_timer();
  void disable_timeout() { set_timeout(0, mTimeoutStage); }

  // Label text on the left (works seamlessly with or without label)
  void set_text(const QString & iText);
  void setText(const QString & iText) { set_text(iText); }
  void set_label_text(const QString & iText) { set_text(iText); }
  [[nodiscard]] QString get_text() const { return mText; }
  [[nodiscard]] QString text() const { return mText; }
  [[nodiscard]] bool has_label() const { return !mText.isEmpty(); }
  // Alignment
  void set_alignment(Qt::Alignment iAlignment);
  void setAlignment(Qt::Alignment iAlignment) { set_alignment(iAlignment); }
  [[nodiscard]] Qt::Alignment get_alignment() const;
  [[nodiscard]] Qt::Alignment alignment() const { return get_alignment(); }
  void set_label_alignment(Qt::Alignment iAlignment);
  [[nodiscard]] Qt::Alignment get_label_alignment() const;

  // Icon sizing
  void set_icon_size(const QSize & iSize);
  void set_icon_size(i32 iWidth, i32 iHeight) { set_icon_size(QSize(iWidth, iHeight)); }
  [[nodiscard]] QSize get_icon_size() const { return mIconSize; }

  // Pixmap support (e.g. for Qt Designer preview)
  void setPixmap(const QPixmap & iPixmap);
  void set_pixmap(const QPixmap & iPixmap) { setPixmap(iPixmap); }
  [[nodiscard]] QPixmap pixmap() const;

  // Scaled contents support (e.g. for Qt Designer preview)
  void setScaledContents(bool iScaled);
  void set_scaled_contents(bool iScaled) { setScaledContents(iScaled); }
  [[nodiscard]] bool hasScaledContents() const;
  [[nodiscard]] bool has_scaled_contents() const { return hasScaledContents(); }

  // Direct access to internal child widgets
  [[nodiscard]] QLabel * get_label() const { return mpLabel; }
  [[nodiscard]] QLabel * get_icon_label() const { return mpIconLabel; }
  [[nodiscard]] QHBoxLayout * get_layout() const { return mpLayout; }

  // Qt Widget overrides
  [[nodiscard]] QSize sizeHint() const override;
  [[nodiscard]] QSize minimumSizeHint() const override;

public slots:
  void slot_set_stage(EStage iStage) { set_stage(iStage); }
  void slot_set_state(EStage iStage) { set_stage(iStage); }
  void slot_set_text(const QString & iText) { set_text(iText); }
  void slot_set_alignment(Qt::Alignment iAlignment) { set_alignment(iAlignment); }
  void slot_set_timeout(i32 iTimeoutMs) { set_timeout_ms(iTimeoutMs); }
  void slot_restart_timeout_timer() { restart_timeout_timer(); }
  void slot_stop_timeout_timer() { stop_timeout_timer(); }
  void slot_disable_timeout() { disable_timeout(); }

signals:
  void signal_stage_changed(EStage iStage);
  void signal_timeout();

protected:
  void changeEvent(QEvent * event) override;
  void enterEvent(QEnterEvent * event) override;

private slots:
  void _slot_timeout();

private:
  EStage mStage = EStage::Off;
  QString mText;
  QSize mIconSize{cDefaultIconWidth, cDefaultIconHeight};
  i32 mTimeoutMs = 0;
  EStage mTimeoutStage = EStage::Off;

  QHBoxLayout * mpLayout = nullptr;
  QLabel * mpLabel = nullptr;
  QLabel * mpIconLabel = nullptr;
  QTimer * mpTimer = nullptr;

  void _init_ui(const QString & iText);
  void _update_display();
  void _restart_timer_if_needed();
  QTimer * _get_or_create_timer();

  static const QPixmap & _get_cached_pixmap(EStage iStage);
};

using TrafficLightWidget = TrafficLight;
