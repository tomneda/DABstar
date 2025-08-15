#ifndef  __FILEREADER_WIDGET__
#define  __FILEREADER_WIDGET__

#include  <QLabel>
#include  <QLCDNumber>
#include  <QHBoxLayout>
#include  <QVBoxLayout>
#include  <QCheckBox>
#include  <QSlider>
#include  <QSpacerItem>

class FileReaderWidget
{
public:
  QLabel * lblTitle = nullptr;
  QLabel * lblFileName = nullptr;
  QSlider * sliderFilePos = nullptr;
  QLCDNumber * lcdCurrTime = nullptr;
  QLabel * lblSeconds = nullptr;
  QLCDNumber * lcdTotalTime = nullptr;
  QSpacerItem * spacerHorizontal1 = nullptr ;
  QLabel * lblSampleRate = nullptr;
  QLCDNumber * lcdSampleRate = nullptr;
  QSpacerItem * spacerHorizontal2 = nullptr ;
  QCheckBox * cbLoopFile = nullptr;
  QLabel * lblFormatText = nullptr;
  QLabel * lblFormat = nullptr;

  FileReaderWidget() = default;
  ~FileReaderWidget() = default;

  void setupUi(QWidget * qw)
  {
    lblTitle = new QLabel("Playing pre-recorded file");
    lblFileName = new QLabel();
    sliderFilePos = new QSlider();
    sliderFilePos->setOrientation(Qt::Orientation::Horizontal);
    sliderFilePos->setRange(0, 1000);
    sliderFilePos->setFixedHeight(31);
    sliderFilePos->setToolTip("Grab the handle and slide to move through the file.");
    sliderFilePos->setStyleSheet("QSlider::handle:horizontal { width:  16px; margin-top: -9px;  margin-bottom: -9px; border-radius: 5px; }");

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
    lblFormatText = new QLabel("Format: ");
    lblFormat = new QLabel();

    QHBoxLayout * timebar = new QHBoxLayout();
    timebar->addWidget(lcdCurrTime);
    timebar->addWidget(lblSeconds);
    timebar->addWidget(lcdTotalTime);
    timebar->addItem(spacerHorizontal1);
    timebar->addWidget(cbLoopFile);

    QHBoxLayout * bottom = new QHBoxLayout();
    bottom->addWidget(lblSampleRate);
    bottom->addWidget(lcdSampleRate);
    bottom->addItem(spacerHorizontal2);
    bottom->addWidget(lblFormatText);
    bottom->addWidget(lblFormat);

    QVBoxLayout * base = new QVBoxLayout();
    base->addWidget(lblTitle);
    base->addWidget(lblFileName);
    base->addWidget(sliderFilePos);
    base->addItem(timebar);
    base->addItem(bottom);

    qw->setLayout(base);
  }
};

#endif

