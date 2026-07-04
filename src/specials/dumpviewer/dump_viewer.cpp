/*
 *    Copyright (C) 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the dumpViewer
 *
 *    dumpViewer is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    dumpViewer is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with dumpViewer; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <QCoreApplication>
#include <QPen>
#include <QChart>
#include "dump_viewer.h"

dumpViewer::dumpViewer(FILE * f, QWidget * parent)
  : QDialog(parent)
  , theFile(f)
{
  setupUi(this);

  viewerWindow->get_x_axis()->setGridLineVisible(true);
  viewerWindow->get_x_axis()->setMinorGridLineVisible(true);
  viewerWindow->get_y_axis()->setGridLineVisible(true);
  viewerWindow->get_y_axis()->setMinorGridLineVisible(false);

  mpCurve = new QLineSeries();
  mpCurve->setPen(QPen(Qt::red, 2.0));
  mpCurve->setUseOpenGL(false);
  viewerWindow->chart()->addSeries(mpCurve);
  mpCurve->attachAxis(viewerWindow->get_x_axis());
  mpCurve->attachAxis(viewerWindow->get_y_axis());

  connect(viewSlider,      &QSlider::valueChanged, this, &dumpViewer::handle_viewSlider);
  connect(amplitudeSlider, &QSlider::valueChanged, this, &dumpViewer::handle_amplitudeSlider);
  connect(compressor,      &QSlider::valueChanged, this, &dumpViewer::handle_compressor);

  seconds_per_frame  = 12.0f / 125.0f;
  seconds_per_sample = 2.0f * seconds_per_frame;

  fseek(theFile, 0, SEEK_END);
  fileLength = ftell(theFile);
  fseek(theFile, 0, SEEK_SET);

  fprintf(stderr, "samples per minute: %d\n",  (i32)(60.0f / seconds_per_sample));
  fprintf(stderr, "duration of record: %d seconds\n", (i32)(fileLength / sizeof(f32) * seconds_per_sample));

  show_segment(0, 1);
}

void dumpViewer::handle_viewSlider(i32 pos)
{
  show_segment(pos, compressor->value());
}

void dumpViewer::handle_amplitudeSlider(i32)
{
  show_segment(viewSlider->value(), compressor->value());
}

void dumpViewer::handle_compressor(i32 h)
{
  show_segment(viewSlider->value(), h);
}

// pos is the slider position (0..100)
void dumpViewer::show_segment(i32 pos, i32 compression)
{
  constexpr i32 cPlotSize = 512;
  auto * const temp = make_vla(f32, cPlotSize * compression);
  const i32 lengthF = fileLength / (i32)sizeof(f32);
  const i32 p = pos * lengthF / 100;

  const f64 xStart = (f64)(p * compression) * seconds_per_sample;
  const f64 xEnd   = (f64)((p + cPlotSize) * compression) * seconds_per_sample;
  const f64 yMax   = (f64)amplitudeSlider->value();

  viewerWindow->set_x_range((f32)xStart, (f32)xEnd);
  viewerWindow->set_y_range(0, (f32)yMax);

  fseek(theFile, p * (i32)sizeof(f32), SEEK_SET);
  const i32 length = (i32)fread(temp, sizeof(f32), cPlotSize * compression, theFile);

  QList<QPointF> pts;
  pts.reserve(cPlotSize);
  for (i32 i = 0; i < cPlotSize; i++)
  {
    const f64 xVal = (f64)((p + i) * compression) * seconds_per_sample;
    const f64 yVal = (i * compression < length) ? (f64)temp[i * compression] : 0.0;
    pts.append(QPointF(xVal, yVal));
  }
  mpCurve->replace(pts);
}
