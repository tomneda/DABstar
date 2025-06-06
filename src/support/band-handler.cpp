/*
 * This file is adapted by Thomas Neder (https://github.com/tomneda)
 *
 * This project was originally forked from the project Qt-DAB by Jan van Katwijk. See https://github.com/JvanKatwijk/qt-dab.
 * Due to massive changes it got the new name DABstar. See: https://github.com/tomneda/DABstar
 *
 * The original copyright information is preserved below and is acknowledged.
 */

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

#include  "band-handler.h"
#include  "dab-constants.h"
#include  <QHeaderView>
#include  <QDomDocument>
#include  <cstdio>

struct SDabFrequencies
{
  QString key;
  i32 fKHz;
  bool skip;
};

static SDabFrequencies frequencies_1[] =
{
  {"05A", 174928, false},
  {"05B", 176640, false},
  {"05C", 178352, false},
  {"05D", 180064, false},
  {"06A", 181936, false},
  {"06B", 183648, false},
  {"06C", 185360, false},
  {"06D", 187072, false},
  {"07A", 188928, false},
  {"07B", 190640, false},
  {"07C", 192352, false},
  {"07D", 194064, false},
  {"08A", 195936, false},
  {"08B", 197648, false},
  {"08C", 199360, false},
  {"08D", 201072, false},
  {"09A", 202928, false},
  {"09B", 204640, false},
  {"09C", 206352, false},
  {"09D", 208064, false},
  {"10A", 209936, false},
  {"10B", 211648, false},
  {"10C", 213360, false},
  {"10D", 215072, false},
  {"11A", 216928, false},
  {"11B", 218640, false},
  {"11C", 220352, false},
  {"11D", 222064, false},
  {"12A", 223936, false},
  {"12B", 225648, false},
  {"12C", 227360, false},
  {"12D", 229072, false},
  {"13A", 230784, false},
  {"13B", 232496, false},
  {"13C", 234208, false},
  {"13D", 235776, false},
  {"13E", 237488, false},
  {"13F", 239200, false},
  {nullptr, 0, false}
};

static SDabFrequencies frequencies_2[] =
{
  {"LA", 1452960, false},
  {"LB", 1454672, false},
  {"LC", 1456384, false},
  {"LD", 1458096, false},
  {"LE", 1459808, false},
  {"LF", 1461520, false},
  {"LG", 1463232, false},
  {"LH", 1464944, false},
  {"LI", 1466656, false},
  {"LJ", 1468368, false},
  {"LK", 1470080, false},
  {"LL", 1471792, false},
  {"LM", 1473504, false},
  {"LN", 1475216, false},
  {"LO", 1476928, false},
  {"LP", 1478640, false},
  {nullptr, 0, false}
};


static SDabFrequencies alternatives[100];

BandHandler::BandHandler(const QString & a_band, QSettings * s)
  : theTable(nullptr)
{
  selectedBand = nullptr;
  dabSettings = s;
  fileName = "";

  theTable.setColumnCount(2);
  QStringList header;
  header << tr("channel") << tr("scan");
  theTable.setHorizontalHeaderLabels(header);
  theTable.verticalHeader()->hide();
  theTable.setShowGrid(true);
#if !defined(__MINGW32__) && !defined(_WIN32)
  if (a_band == QString(""))
  {
    return;
  }

  FILE * f = fopen(a_band.toUtf8().data(), "r");
  if (f == nullptr)
  {
    return;
  }

  //	OK we have a file with - hopefully - some input
  size_t amount = 128;
  i32 filler = 0;
  char * line = new char[512];
  while ((filler < 100) && (amount > 0))
  {
    amount = getline(&line, &amount, f);
    //	   fprintf (stderr, "%s (%d)\n", line, (i32)amount);
    if ((i32)amount <= 0)
    {  // eof detected
      break;
    }
    if (((i32)amount < 8) || ((i32)amount > 128))
    { // ?????
      continue;
    }
    line[amount] = 0;
    char channelName[128];
    i32 freq;
    i32 res = sscanf(line, "%s %d", channelName, &freq);
    if (res != 2)
    {
      continue;
    }
    fprintf(stderr, "adding %s %d\n", channelName, freq);
    alternatives[filler].key = QString(channelName);
    alternatives[filler].fKHz = freq;
    alternatives[filler].skip = false;
    filler++;
  }

  free(line);
  alternatives[filler].key = "";
  alternatives[filler].fKHz = 0;
  fclose(f);
  selectedBand = alternatives;
#else
  (void)a_band;
#endif
}

//
//	note that saving settings has to be done separately
BandHandler::~BandHandler()
{
  if (!theTable.isHidden())
  {
    theTable.hide();
  }
}

//
//	The main program calls this once, the combobox will be filled
void BandHandler::setupChannels(QComboBox * s, u8 band)
{
  i16 i;
  i16 c = s->count();

  if (selectedBand == nullptr)
  {  // no preset band
    if (band == BAND_III)
    {
      selectedBand = frequencies_1;
    }
    else
    {
      selectedBand = frequencies_2;
    }
  }

  //	clear the fields in the comboBox
  for (i = 0; i < c; i++)
  {
    s->removeItem(c - (i + 1));
  }
  //
  //	The table elements are by default all "+";
  for (i32 i = 0; selectedBand[i].fKHz != 0; i++)
  {
    s->insertItem(i, selectedBand[i].key, QVariant(i));
    theTable.insertRow(i);
    theTable.setItem(i, 0, new QTableWidgetItem(selectedBand[i].key));
    theTable.setItem(i, 1, new QTableWidgetItem(QString("+")));
  }
}

//
//	Note that we only save the channels that are to be skipped
void BandHandler::saveSettings()
{
  if (!theTable.isHidden())
  {
    theTable.hide();
  }

  if (fileName == "")
  {
    dabSettings->beginGroup("skipTable");
    for (i32 i = 0; selectedBand[i].fKHz != 0; i++)
    {
      if (selectedBand[i].skip)
      {
        dabSettings->setValue(selectedBand[i].key, 1);
      }
      else
      {
        dabSettings->remove(selectedBand[i].key);
      }
    }
    dabSettings->endGroup();
  }
  else
  {
    QDomDocument skipList;
    QDomElement root;

    root = skipList.createElement("skipList");
    skipList.appendChild(root);

    for (i32 i = 0; selectedBand[i].fKHz != 0; i++)
    {
      if (!selectedBand[i].skip)
      {
        continue;
      }
      QDomElement skipElement = skipList.createElement("BAND_ELEMENT");
      skipElement.setAttribute("CHANNEL", selectedBand[i].key);
      skipElement.setAttribute("VALUE", "-");
      root.appendChild(skipElement);
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
      return;
    }

    QTextStream stream(&file);
    stream << skipList.toString();
    file.close();
  }
}

//
//	when setup_skipList is called, we start with blacklisting all entries
//
void BandHandler::setup_skipList(const QString & fileName)
{
  disconnect(&theTable, &QTableWidget::cellDoubleClicked, this, &BandHandler::slot_cell_selected);

  for (i32 i = 0; selectedBand[i].fKHz > 0; i++)
  {
    selectedBand[i].skip = false;
    theTable.item(i, 1)->setText("+");
  }

  this->fileName = fileName;
  if (fileName == "")
  {
    default_skipList();
  }
  else
  {
    file_skipList(fileName);
  }

  connect(&theTable, &QTableWidget::cellDoubleClicked, this, &BandHandler::slot_cell_selected);
}

//
//	default setting in the ini file!!
void BandHandler::default_skipList() const
{
  dabSettings->beginGroup("skipTable");
  for (i32 i = 0; selectedBand[i].fKHz != 0; i++)
  {
    bool skipValue = dabSettings->value(selectedBand[i].key, 0).toInt() == 1;
    if (skipValue)
    {
      selectedBand[i].skip = true;
      theTable.item(i, 1)->setText("-");
    }
  }
  dabSettings->endGroup();
}

void BandHandler::file_skipList(const QString & fileName) const
{
  QDomDocument xml_bestand;

  QFile f(fileName);
  if (f.open(QIODevice::ReadOnly))
  {
    xml_bestand.setContent(&f);
    QDomElement root = xml_bestand.documentElement();
    QDomElement component = root.firstChild().toElement();
    while (!component.isNull())
    {
      if (component.tagName() == "BAND_ELEMENT")
      {
        QString channel = component.attribute("CHANNEL", "???");
        QString skipItem = component.attribute("VALUE", "+");
        if ((channel != "???") && (skipItem == "-"))
        {
          updateEntry(channel);
        }
      }
      component = component.nextSibling().toElement();
    }
  }
}

void BandHandler::updateEntry(const QString & channel) const
{
  for (i32 i = 0; selectedBand[i].key != nullptr; i++)
  {
    if (selectedBand[i].key == channel)
    {
      selectedBand[i].skip = true;
      theTable.item(i, 1)->setText("-");
      return;
    }
  }
}

//	find the frequency for a given channel in a given band
i32 BandHandler::get_frequency_Hz(const QString & Channel) const
{
  i32 tunedFrequency = 0;
  i32 i;

  for (i = 0; selectedBand[i].key != nullptr; i++)
  {
    if (selectedBand[i].key == Channel)
    {
      tunedFrequency = kHz(selectedBand[i].fKHz);
      break;
    }
  }

  if (tunedFrequency == 0)
  {  // should not happen
    tunedFrequency = kHz(selectedBand[0].fKHz);
  }

  return tunedFrequency;
}

i32 BandHandler::firstChannel() const
{
  i32 index = 0;
  while (selectedBand[index].skip)
  {
    index++;
  }
  if (selectedBand[index].fKHz == 0)
  {
    return 0;
  }
  return index;
}

i32 BandHandler::nextChannel(i32 index) const
{
  i32 hulp = index;
  do
  {
    hulp++;
    if (selectedBand[hulp].fKHz == 0)
    {
      index = 0;
    }
  }
  while (selectedBand[hulp].skip && (hulp != index));
  return hulp;
}

// i32 BandHandler::lastOf(SDabFrequencies * b) const
// {
//   i32 index;
//   for (index = 0; selectedBand[index].fKHz != 0; index++) {}
//   return index - 1;
// }

// i32 BandHandler::prevChannel(i32 index)
// {
//   i32 hulp = index;
//   do
//   {
//     if (hulp == 0)
//     {
//       hulp = lastOf(selectedBand);
//     }
//     else
//     {
//       hulp--;
//     }
//   }
//   while (selectedBand[hulp].skip && (hulp != index));
//   return hulp;
// }

void BandHandler::slot_cell_selected(i32 row, i32 column) const
{
  QString s1 = theTable.item(row, 0)->text();
  QString s2 = theTable.item(row, 1)->text();
  (void)column;
  if (s2 == "-")
  {
    theTable.item(row, 1)->setText("+");
  }
  else
  {
    theTable.item(row, 1)->setText("-");
  }
  selectedBand[row].skip = s2 != "-";
  //	fprintf (stderr, "we zetten voor %s de zaak op %d\n",
  //	              selectedBand [row]. key. toUtf8 (). data (),
  //	              selectedBand [row]. skip);
}

void BandHandler::show()
{
  theTable.show();
}

void BandHandler::hide()
{
  theTable.hide();
}

bool BandHandler::isHidden() const
{
  return theTable.isHidden();
}
