#include "setting_helper.h"

#include <QApplication>
#include <QTemporaryDir>
#include <QWidget>
#include <cstdio>

namespace
{
bool check(const char* const iName, const bool iCondition)
{
  if (!iCondition)
  {
    std::fprintf(stderr, "FAILED: %s\n", iName);
  }
  return iCondition;
}
} // namespace

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);

  const QTemporaryDir tempDir;
  if (!tempDir.isValid())
  {
    std::fprintf(stderr, "pos_and_size_test FAILED: no temporary directory\n");
    return 1;
  }

  QSettings settings(tempDir.filePath("settings.ini"), QSettings::IniFormat);
  Settings::Storage::instance(&settings);

  const Settings::PosAndSize posAndSize("PosAndSizeTest");
  constexpr i32 cMinDefaultWidth = 760;
  bool ok = true;

  // no stored width: the minimum default width is used
  {
    QWidget widget;
    posAndSize.read_widget_geometry(&widget, false, false, cMinDefaultWidth);
    ok &= check("default width used", widget.width() == cMinDefaultWidth);
  }

  // no stored width and no minimum default width: the size hint is used
  {
    QWidget widget;
    widget.setMinimumWidth(100);
    posAndSize.read_widget_geometry(&widget);
    ok &= check("size hint width used", widget.width() < cMinDefaultWidth);
  }

  // stored width wins, also when it is smaller than the minimum default width
  {
    settings.setValue("PosAndSizeTest/posAndSize-w", 500);
    QWidget widget;
    posAndSize.read_widget_geometry(&widget, false, false, cMinDefaultWidth);
    ok &= check("stored width used", widget.width() == 500);
  }

  if (!ok)
  {
    return 1;
  }

  std::printf("pos_and_size_test passed\n");
  return 0;
}
