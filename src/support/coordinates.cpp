/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 *    Copyright (C) 2013, 2014, 2015, 2016, 2017
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of the Qt-DAB (formerly SDR-J, JSDR).
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

#include "coordinates.h"
#include "setting-helper.h"
#include <QDoubleValidator>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QSettings>
#include <QRegularExpression>


class MyDoubleValidator : public QDoubleValidator
{
  using QDoubleValidator::QDoubleValidator; // takeover the base class constructors

  QValidator::State validate(QString & input, int &) const override
  {
    // number of decimal digits are ignored
    const QRegularExpression regExpAllowed("^-?\\d*(\\.\\d*)?$");

    if (const QRegularExpressionMatch match = regExpAllowed.match(input);
        !match.hasMatch())
    {
      return Invalid;
    }

    // We expect a '.' also in languages where ',' is a decimal point. So let toDouble() perform only in en_US localization.
    QLocale englishLocale(QLocale::English, QLocale::UnitedStates);

    // Set the application's locale to the English (United States) locale.
    QLocale previousLocale = QLocale::system();
    QLocale::setDefault(englishLocale);

    // Convert the QString to a f64 using the current (English, United States) locale.
    const f64 val = input.toDouble();

    // Reset the locale to its previous value.
    QLocale::setDefault(previousLocale);

    if (val < bottom() || val > top())
    {
      return Invalid;
    }

    return Acceptable;
  }
};

Coordinates::Coordinates()
{
  mpLabelUrlHint = new QLabel(this);

  const QString url1 = "https://www.google.com/maps";
  const QString url2 = "https://www.openstreetmap.org";
  const QString url3 = "https://mapy.cz";
  const QString style = "style='color: lightblue;'";

  const QString htmlLink = "Get coordinates on e.g.:<br>"
                           "<a " + style + " href='" + url1 + "'>" + url1 + "</a><br>"
                           "<a " + style + " href='" + url2 + "'>" + url2 + "</a><br>"
                           "<a " + style + " href='" + url3 + "'>" + url3 + "</a><br>";

  mpLabelUrlHint->setText(htmlLink);
  mpLabelUrlHint->setOpenExternalLinks(true);

  mpLabelLatitude = new QLabel(this);
  mpLabelLatitude->setText("Latitude (decimal)");

  mpEditBoxLatitude = new QLineEdit(this);
  mpEditBoxLatitude->setValidator(new MyDoubleValidator(-90.0, 90.0, 5, mpEditBoxLatitude));

  mpLabelLongitude = new QLabel(this);
  mpLabelLongitude->setText("Longitude (decimal)");

  mpEditBoxLongitude = new QLineEdit(this);
  mpEditBoxLongitude->setValidator(new MyDoubleValidator(-180.0, 180.0, 5, mpEditBoxLongitude));

  auto * layout = new QFormLayout;
  layout->addWidget(mpLabelUrlHint);
  layout->addWidget(mpLabelLatitude);
  layout->addWidget(mpEditBoxLatitude);
  layout->addWidget(mpLabelLongitude);
  layout->addWidget(mpEditBoxLongitude);

  setWindowTitle("Select Coordinates");

  mpBtnAccept = new QPushButton("Accept", this);

  auto * total = new QVBoxLayout(this);
  total->addItem(layout);
  total->addWidget(mpBtnAccept);
  setLayout(total);

  mpEditBoxLatitude->setText(Settings::Config::varLatitude.read().toString());
  mpEditBoxLongitude->setText(Settings::Config::varLongitude.read().toString());

  connect(mpBtnAccept, &QPushButton::clicked, this, &Coordinates::slot_accept_button);

  show();
}

Coordinates::~Coordinates()
{
  hide();
}

void Coordinates::slot_accept_button()
{
  Settings::Config::varLatitude.write(mpEditBoxLatitude->text());
  Settings::Config::varLongitude.write(mpEditBoxLongitude->text());

  QDialog::done(0);
}

