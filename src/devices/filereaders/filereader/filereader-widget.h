#ifndef  __FILEREADER_WIDGET__
#define  __FILEREADER_WIDGET__

#include  <QLabel>
#include  <QProgressBar>
#include  <QLCDNumber>
#include  <QHBoxLayout>
#include  <QVBoxLayout>
#include  <QCheckBox>
#include  <QSpacerItem>

class FileReaderWidget
{
public:
  QLabel * lblTitle = nullptr;
  QLabel * lblFileName = nullptr;
  QProgressBar * progressFile = nullptr;
  QLCDNumber * lcdCurrTime = nullptr;
  QLabel * lblSeconds = nullptr;
  QLCDNumber * lcdTotalTime = nullptr;
  QSpacerItem * spacerHorizontal1 = nullptr ;
  QLabel * lblSampleRate = nullptr;
  QLCDNumber * lcdSampleRate = nullptr;
  QSpacerItem * spacerHorizontal2 = nullptr ;
  QCheckBox * cbLoopFile = nullptr;

  FileReaderWidget() = default;
  ~FileReaderWidget() = default;

  void setupUi(QWidget * qw)
  {
    lblTitle = new QLabel("Playing pre-recorded file");
    lblFileName = new QLabel();
    progressFile = new QProgressBar();

    lcdCurrTime = new QLCDNumber();
    lcdCurrTime->setFrameShape(QFrame::NoFrame);
    lcdCurrTime->setSegmentStyle(QLCDNumber::Flat);

    lblSeconds = new QLabel("seconds of");
    lblSeconds->setAlignment(Qt::AlignCenter);

    lcdTotalTime = new QLCDNumber();
    lcdTotalTime->setFrameShape(QFrame::NoFrame);
    lcdTotalTime->setSegmentStyle(QLCDNumber::Flat);

    spacerHorizontal1 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);
    lblSampleRate = new QLabel("Source sample rate [Sps]: ");
    lcdSampleRate = new QLCDNumber();
    lcdSampleRate->setFrameShape(QFrame::NoFrame);
    lcdSampleRate->setSegmentStyle(QLCDNumber::Flat);
    lcdSampleRate->setDigitCount(7);
    lcdSampleRate->display(0);
    spacerHorizontal2 = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);
    cbLoopFile = new QCheckBox("Loop");
    cbLoopFile->setChecked(true);

    QHBoxLayout * bottom = new QHBoxLayout();
    bottom->addWidget(lcdCurrTime);
    bottom->addWidget(lblSeconds);
    bottom->addWidget(lcdTotalTime);
    bottom->addItem(spacerHorizontal1);
    bottom->addWidget(lblSampleRate);
    bottom->addWidget(lcdSampleRate);
    bottom->addItem(spacerHorizontal2);
    bottom->addWidget(cbLoopFile);

    QVBoxLayout * base = new QVBoxLayout();
    base->addWidget(lblTitle);
    base->addWidget(lblFileName);
    base->addWidget(progressFile);
    base->addItem(bottom);

    qw->setLayout(base);
  }
};

#endif

