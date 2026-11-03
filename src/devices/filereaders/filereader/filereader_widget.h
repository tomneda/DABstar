#pragma once

#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

class FileReaderWidget
{
public:
  QLabel * lblTitle = nullptr;
  QLabel * lblFileName = nullptr;
  QLabel * lblSampleRateText = nullptr;
  QLabel * lblSampleRate = nullptr;
  QLabel * lblFormatText = nullptr;
  QLabel * lblFormat = nullptr;

  FileReaderWidget() = default;
  ~FileReaderWidget() = default;

  void setupUi(QWidget * qw, const QString & iWindowTitle)
  {
    qw->setWindowTitle(iWindowTitle);

    lblTitle = new QLabel("Playing pre-recorded file");
    lblFileName = new QLabel();
    lblFileName->setWordWrap(true);
    lblSampleRateText = new QLabel("Sample Rate [Sps]:");
    lblSampleRate = new QLabel("0");
    lblSampleRate->setStyleSheet("font-weight: bold;");
    lblFormatText = new QLabel("Format:");
    lblFormat = new QLabel("-");
    lblFormat->setStyleSheet("font-weight: bold;");

    QHBoxLayout * bottom = new QHBoxLayout();
    bottom->addWidget(lblSampleRateText);
    bottom->addWidget(lblSampleRate);
    bottom->addStretch();
    bottom->addWidget(lblFormatText);
    bottom->addWidget(lblFormat);

    QVBoxLayout * base = new QVBoxLayout();
    base->addWidget(lblTitle);
    base->addWidget(lblFileName);
    base->addLayout(bottom);

    qw->setLayout(base);
  }
};
