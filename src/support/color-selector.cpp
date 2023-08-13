/*
 *    Copyright (C) 2020
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB program
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

#include  <QVBoxLayout>
#include  "color-selector.h"

// /*static*/ const char ColorSelector::DISPCOLOR[]  = "Display Color";
/*static*/ const char ColorSelector::GRIDCOLOR[]  = "Grid Color";
/*static*/ const char ColorSelector::CURVECOLOR[] = "Curve Color";

/*static*/ QString ColorSelector::show_dialog(const QString & iTopText)
{
  ColorSelector * selector = new ColorSelector(iTopText);
  int index = selector->exec();
  QString colStr = selector->getColor(index);
  delete selector;
  return (index != 0 ? colStr : "");
}


ColorSelector::ColorSelector(const QString & topText)
{
  mpToptext = new QLabel(this);
  mpToptext->setText(topText);
  mpSelectorDisplay = new QListView(this);
  QVBoxLayout * layOut = new QVBoxLayout;
  layOut->addWidget(mpToptext);
  layOut->addWidget(mpSelectorDisplay);
  setWindowTitle(tr("color select"));
  setLayout(layOut);

  mColors = QStringList();
  mColors << "--Quit--" << "white" << "black" << "red" << "darkRed" << "green" << "darkGreen" << "blue" << "darkBlue"
          << "cyan" << "darkCyan" << "magenta" << "darkMagenta" << "yellow" << "darkYellow" << "gray" << "darkGray";
  mColorList.setStringList(mColors);
  mpSelectorDisplay->setModel(&mColorList);
  connect(mpSelectorDisplay, SIGNAL(clicked(QModelIndex)), this, SLOT(select_color(QModelIndex)));
  mSelectedItem = -1;
}

void ColorSelector::select_color(QModelIndex s)
{
  QDialog::done(s.row());
}

QString ColorSelector::getColor(int index)
{
  return mColors.at(index);
}

