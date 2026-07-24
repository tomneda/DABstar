/*
 * This file was original taken from AbracaDABra and is adapted by Thomas Neder
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
#include <QRegularExpression>
#include <QTextDocument>

namespace
{
  // The permissive autolink parser behind QTextDocument::setMarkdown() cuts bare URLs short at
  // certain character sequences, e.g. it stops at the "..." of a GitHub compare link and leaves
  // the remainder as plain text. Explicit "[url](url)" links are parsed correctly, so rewrite the
  // bare URLs before importing. URLs that already are part of a link ("](url)", "<url>",
  // "[url](...)" or a "[ref]: url" definition) are left alone.
  QString wrap_bare_urls_in_markdown_links(const QString & iMarkdown)
  {
    static const QRegularExpression sReBareUrl(R"((?<![(<\[])(?<!\]: )\bhttps?://[^\s<>()\[\]]+)");
    static const QString sTrailingPunctuation(".,;:!?");

    QString result;
    qsizetype copiedUpTo = 0;

    QRegularExpressionMatchIterator it = sReBareUrl.globalMatch(iMarkdown);

    while (it.hasNext())
    {
      const QRegularExpressionMatch match = it.next();
      QString url = match.captured();

      // Punctuation at the very end belongs to the sentence, not to the URL.
      while (!url.isEmpty() && sTrailingPunctuation.contains(url.back()))
      {
        url.chop(1);
      }

      if (url.isEmpty())
      {
        continue;
      }

      // The link label still shows the plain URL, but its "://" has to be escaped, otherwise the
      // autolink parser kicks in a second time on the label and nests a truncated link inside.
      QString label = url;
      label.replace("://", "\\://");

      result += iMarkdown.mid(copiedUpTo, match.capturedStart() - copiedUpTo);
      result += '[' + label + "](" + url + ')';
      copiedUpTo = match.capturedStart() + url.size();
    }

    result += iMarkdown.mid(copiedUpTo);
    return result;
  }
}


UpdateDialog::UpdateDialog(const QString & version, const QString & releaseNotes, Qt::WindowFlags f, int32_t updateIntervalDays, QWidget * parent)
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

  // The link color comes from the application palette which is set up in main().
  ui->releaseNotes->document()->setMarkdown(wrap_bare_urls_in_markdown_links(releaseNotes), QTextDocument::MarkdownDialectGitHub);
  ui->releaseNotes->setOpenExternalLinks(true);
  ui->releaseNotes->moveCursor(QTextCursor::Start);

  ui->btnDeferToNextUpdate->setText("Check again in " + QString::number(updateIntervalDays) + (updateIntervalDays == 1 ? " day" : " days"));
  ui->btnOpenReleaseSite->setText("Go to release page for download");

  connect(ui->btnOpenReleaseSite, &QPushButton::clicked, this, &QDialog::accept);
  connect(ui->btnDeferToNextUpdate, &QPushButton::clicked, this, &QDialog::reject);
  connect(ui->btnOpenReleaseSite, &QPushButton::clicked, this, [this, version]()
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
