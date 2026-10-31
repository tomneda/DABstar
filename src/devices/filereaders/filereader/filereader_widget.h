#pragma once

#include <QLabel>
#include <QLCDNumber>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>

class FileReaderWidget
{
public:
  QLabel * lblTitle = nullptr;
  QLabel * lblFileName = nullptr;
  QLabel * lblSampleRate = nullptr;
  QLCDNumber * lcdSampleRate = nullptr;
  QSpacerItem * spacerHorizontal2 = nullptr ;
  QLabel * lblFormatText = nullptr;
  QLabel * lblFormat = nullptr;

  FileReaderWidget() = default;
  ~FileReaderWidget() = default;

  void setupUi(QWidget * qw)
  {
    lblTitle = new QLabel("Playing pre-recorded file");
    lblFileName = new QLabel();
    lblFileName->setWordWrap(true);
    lblSampleRate = new QLabel("Source sample rate [Sps]: ");
    lcdSampleRate = new QLCDNumber();
    lcdSampleRate->setFrameShape(QFrame::NoFrame);
    lcdSampleRate->setSegmentStyle(QLCDNumber::Flat);
    lcdSampleRate->setDigitCount(7);
    lcdSampleRate->display(0);
    spacerHorizontal2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);
    lblFormatText = new QLabel("Format: ");
    lblFormat = new QLabel();

    QHBoxLayout * bottom = new QHBoxLayout();
    bottom->addWidget(lblSampleRate);
    bottom->addWidget(lcdSampleRate);
    bottom->addItem(spacerHorizontal2);
    bottom->addWidget(lblFormatText);
    bottom->addWidget(lblFormat);

    QVBoxLayout * base = new QVBoxLayout();
    base->addWidget(lblTitle);
    base->addWidget(lblFileName);
    base->addItem(bottom);

    qw->setLayout(base);
  }
};


