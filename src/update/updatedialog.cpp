/*
 * This file original taken from AbracaDABra and is adapted by Thomas Neder
 * (https://github.com/tomneda)
 * The original copyright information is preserved below and is acknowledged.
 */

/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2025 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "updatedialog.h"
#include "ui_updatedialog.h"
#include <QDesktopServices>
#include <QPushButton>


UpdateDialog::UpdateDialog(const QString & version, const QString & releaseNotes, Qt::WindowFlags f, QWidget * parent)
  : QDialog(parent, f)
  , ui(new Ui::UpdateDialog)
{
  ui->setupUi(this);
  
  setModal(true);
  setWindowTitle("Application update");

  const QString tableHtml = QString(
    "<style>"
    "  td { padding-right: 10px; }"
    "  .version { color: #FFD666; }"
    "  h2 { color: #00B753; }"
    "</style>"
    "<h2>DABstar update available</h2>"
    "<table>"
    "  <tr><td><b>Current version:</b></td><td class='version'><b>V%1</b></td></tr>"
    "  <tr><td><b>Available version:</b></td><td class='version'><b>V%2</b></td></tr>"
    "</table>"
  ).arg(PRJ_VERS).arg(version.mid(1));

  ui->versionLabel->setTextFormat(Qt::RichText);
  ui->versionLabel->setText(tableHtml);

  ui->releaseNotes->document()->setDefaultStyleSheet("a { color: lightblue; }"); // TODO: this way or other ways do not work changing the link color
  ui->releaseNotes->document()->setMarkdown(releaseNotes, QTextDocument::MarkdownDialectGitHub);
  ui->releaseNotes->setOpenExternalLinks(true);
  ui->releaseNotes->moveCursor(QTextCursor::Start);

  ui->buttonBox->button(QDialogButtonBox::Cancel)->setText("Do not show again");

  const auto goTo = new QPushButton("Go to release page", this);
  ui->buttonBox->addButton(goTo, QDialogButtonBox::ActionRole);

  connect(goTo, &QPushButton::clicked, this, [this, version]()
  {
#ifdef _WIN32
    QDesktopServices::openUrl(QUrl::fromUserInput(QString("https://github.com/old-dab/DABstar/releases/tag/%1").arg(version)));
#else
    QDesktopServices::openUrl(QUrl::fromUserInput(QString("https://github.com/tomneda/DABstar/releases/tag/%1").arg(version)));
#endif
  });

  setFixedSize(size());
}

UpdateDialog::~UpdateDialog()
{
  delete ui;
}
