/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original file originated from Qt-DAB but carried no copyright notice.
 */

#pragma once

#include  "dab-constants.h"
#include  <QObject>
#include  <QLineSeries>

class QSettings;
class PlotWidget;

class SpectrumScope : public QObject
{
Q_OBJECT
public:
  SpectrumScope(PlotWidget *, i32, QSettings *);
  ~SpectrumScope() override = default;

  void show_spectrum(const f32 *, const f32 *, const SpecViewLimits<f32> & iSpecViewLimits);

private:
  PlotWidget * mpPlot = nullptr;
  QLineSeries * mpCurve = nullptr;
  QList<QPointF> mPoints;
  const i32 mDisplaySize;
  f32 mScale = 0.0f;

  std::vector<f32> mYValVec;

public slots:
  void slot_scaling_changed(i32);
};
