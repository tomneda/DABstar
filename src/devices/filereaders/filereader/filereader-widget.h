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
  QLabel * titleLabel{};
  QLabel * nameofFile{};
  QProgressBar * fileProgress{};
  QLCDNumber * currentTime{};
  QLabel * seconds{};
  QLCDNumber * totalTime{};
  QCheckBox *cbLoopFile;
  QSpacerItem *horizontalSpacer;

  FileReaderWidget() = default;
  ~FileReaderWidget() = default;

  void setupUi(QWidget * qw)
  {
    titleLabel = new QLabel("Playing pre-recorded file");
    nameofFile = new QLabel();
    fileProgress = new QProgressBar();

    currentTime = new QLCDNumber();
    currentTime->setFrameShape(QFrame::NoFrame);
    currentTime->setSegmentStyle(QLCDNumber::Flat);

    seconds = new QLabel("seconds of");
    seconds->setAlignment(Qt::AlignCenter);

    totalTime = new QLCDNumber();
    totalTime->setFrameShape(QFrame::NoFrame);
    totalTime->setSegmentStyle(QLCDNumber::Flat);

    horizontalSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);
    cbLoopFile = new QCheckBox("Loop");
    cbLoopFile->setChecked(true);

    QHBoxLayout * bottom = new QHBoxLayout();
    bottom->addWidget(currentTime);
    bottom->addWidget(seconds);
    bottom->addWidget(totalTime);
    bottom->addItem(horizontalSpacer);
    bottom->addWidget(cbLoopFile);

    QVBoxLayout * base = new QVBoxLayout();
    base->addWidget(titleLabel);
    base->addWidget(nameofFile);
    base->addWidget(fileProgress);
    base->addItem(bottom);

    qw->setLayout(base);
  }
};

#endif

