#include "ensemble_list_filter_proxy.h"

#include <QCoreApplication>
#include <QStandardItemModel>
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

void add_row(QStandardItemModel& ioModel, const QString& iCh, const QString& iSL, const QString& iName, const QString& iEId, const QString& iNote)
{
  ioModel.appendRow({ new QStandardItem(iCh), new QStandardItem(iSL), new QStandardItem(iName), new QStandardItem(iEId), new QStandardItem(iNote) });
}

constexpr i32 cNoteCol = 4;
} // namespace

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);

  QStandardItemModel model;
  add_row(model, "5C", "2", "DR Deutschland", "10BC", "Good reception with roof antenna only");
  add_row(model, "11D", "3", "Bayern Süd", "1D01", "");
  add_row(model, "12A", "0", "", "", "short note");
  model.setHorizontalHeaderLabels({ "Ch", "SL", "Ensemble\nName", "EId", "Note" });

  EnsembleListFilterProxy proxy;
  proxy.setSourceModel(&model);
  proxy.set_ignored_columns({ 1 }); // the scan level column
  proxy.set_note_column(cNoteCol);
  bool ok = true;

  ok &= check("empty search shows all rows", proxy.rowCount() == 3);

  proxy.set_search_text("deutsch");
  ok &= check("case-insensitive match in name column", proxy.rowCount() == 1);
  ok &= check("matched row is 5C", proxy.data(proxy.index(0, 0)).toString() == "5C");

  proxy.set_search_text("1d");
  ok &= check("match in channel and EId columns", proxy.rowCount() == 1 && proxy.data(proxy.index(0, 0)).toString() == "11D");

  proxy.set_search_text("1");
  ok &= check("substring matches multiple columns", proxy.rowCount() == 3);

  proxy.set_search_text("0");
  ok &= check("ignored column is not searched", proxy.rowCount() == 2); // row 12A has "0" only in the ignored column

  proxy.set_search_text("  süd ");
  ok &= check("search text is trimmed", proxy.rowCount() == 1);

  proxy.set_search_text("xyz");
  ok &= check("no match shows no rows", proxy.rowCount() == 0);

  proxy.set_search_text("");
  ok &= check("cleared search shows all rows", proxy.rowCount() == 3);

  proxy.set_search_text("0");
  proxy.set_ignored_columns({});
  ok &= check("changing ignored column refilters", proxy.rowCount() == 3);

  proxy.set_ignored_columns({ 1, 3 });
  ok &= check("multiple ignored columns", proxy.rowCount() == 0); // "0" is only in the SL and EId columns

  proxy.set_search_text("");
  proxy.sort(0); // 11D, 12A, 5C
  ok &= check("long note is shortened", proxy.data(proxy.index(2, cNoteCol)).toString() == "Good reception with ...");
  ok &= check("tooltip shows full note", proxy.data(proxy.index(2, cNoteCol), Qt::ToolTipRole).toString() == "Good reception with roof antenna only");
  ok &= check("short note is not shortened", proxy.data(proxy.index(1, cNoteCol)).toString() == "short note");
  ok &= check("empty note has no tooltip", !proxy.data(proxy.index(0, cNoteCol), Qt::ToolTipRole).isValid());
  ok &= check("other columns unchanged", proxy.data(proxy.index(2, 2)).toString() == "DR Deutschland");

  proxy.set_search_text("antenna");
  ok &= check("search finds text beyond the shortened note", proxy.rowCount() == 1 && proxy.data(proxy.index(0, 0)).toString() == "5C");
  proxy.set_search_text("");

  // the sort direction is shown as arrow in the header label of the sorted column
  i32 headerChangedCnt = 0;
  QObject::connect(&proxy, &QAbstractItemModel::headerDataChanged, [&headerChangedCnt]() { ++headerChangedCnt; });
  auto header = [&proxy](const i32 iCol) { return proxy.headerData(iCol, Qt::Horizontal).toString(); };

  proxy.sort(2, Qt::AscendingOrder);
  ok &= check("ascending arrow at sorted column", header(2) == "Ensemble\nName↓");
  ok &= check("no arrow at other columns", header(0) == "Ch" && header(4) == "Note");
  ok &= check("header change signaled on sort", headerChangedCnt == 1);

  proxy.sort(2, Qt::DescendingOrder);
  ok &= check("descending arrow at sorted column", header(2) == "Ensemble\nName↑");

  proxy.sort(0, Qt::AscendingOrder);
  ok &= check("arrow moved to new sorted column", header(0) == "Ch↓" && header(2) == "Ensemble\nName");
  ok &= check("header change signaled on each sort", headerChangedCnt == 3);
  const i32 srcRow = proxy.mapToSource(proxy.index(0, 0)).row();
  ok &= check("vertical header unchanged", proxy.headerData(0, Qt::Vertical) == model.headerData(srcRow, Qt::Vertical));

  // usage in the service list: columns Fav, Service, Ch, SId; the sorting is done by the database, the proxy only filters
  QStandardItemModel serviceModel;
  serviceModel.appendRow({ new QStandardItem("1"), new QStandardItem("Bayern 3"), new QStandardItem("11D"), new QStandardItem("4011") });
  serviceModel.appendRow({ new QStandardItem("0"), new QStandardItem("Ö1"), new QStandardItem("5C"), new QStandardItem("6201") });
  serviceModel.appendRow({ new QStandardItem("0"), new QStandardItem("Antenne Bayern"), new QStandardItem("12A"), new QStandardItem("D210") });
  serviceModel.setHorizontalHeaderLabels({ "Fav↓", "Service", "Ch", "SId" });

  EnsembleListFilterProxy serviceProxy;
  serviceProxy.setSourceModel(&serviceModel);
  serviceProxy.set_ignored_columns({ 0, 2, 3 }); // the favorite, channel and SId columns
  auto service = [&serviceProxy](const i32 iRow) { return serviceProxy.data(serviceProxy.index(iRow, 1)).toString(); };

  serviceProxy.set_search_text("bayern");
  ok &= check("service: source order is kept", serviceProxy.rowCount() == 2 && service(0) == "Bayern 3" && service(1) == "Antenne Bayern");

  serviceProxy.set_search_text("ö");
  ok &= check("service: case-insensitive umlaut match", serviceProxy.rowCount() == 1 && service(0) == "Ö1");

  serviceProxy.set_search_text("5c");
  ok &= check("service: channel column is not searched", serviceProxy.rowCount() == 0);

  serviceProxy.set_search_text("0");
  ok &= check("service: favorite, channel and SId columns are not searched", serviceProxy.rowCount() == 0);

  serviceProxy.set_search_text("");
  ok &= check("service: header without sort arrow change", serviceProxy.headerData(0, Qt::Horizontal).toString() == "Fav↓");

  if (!ok)
  {
    return 1;
  }

  std::printf("ensemble_list_filter_proxy_test passed\n");
  return 0;
}
