#include <CQModelViewTest.h>
#include <CQModelView.h>
#include <CQItemDelegate.h>
#include <CQCsvModel.h>
#include <CQJsonModel.h>
#ifdef CQ_MODEL_VIEW_TRACE
#include <CQPerfMonitor.h>
#endif
#include <CArgs.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QTreeView>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QTextEdit>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>

int
main(int argc, char **argv)
{
  QApplication app(argc, argv);

  QFont font("FreeSans", 18);

  qApp->setFont(font);

  CArgs args("-comment_header:b      (Comment Header) "
             "-first_line_header:b   (First Line Header) "
             "-first_column_header:b (First Column Header) "
             "-stats:b               (Show Stats) "
             "-tree:b                (Show QTreeView) "
             "-table:b               (Show QableView)");

  args.parse(&argc, argv);

  bool comment_header      = args.getArg<bool>("-comment_header");
  bool first_line_header   = args.getArg<bool>("-first_line_header");
  bool first_column_header = args.getArg<bool>("-first_column_header");
  bool showStats           = args.getArg<bool>("-stats");
  bool showTree            = args.getArg<bool>("-tree");
  bool showTable           = args.getArg<bool>("-table");

  std::vector<QString> filenames;

  for (int i = 1; i < argc; ++i)
    filenames.push_back(argv[i]);

  if (filenames.empty())
    exit(1);

  //---

  CQModelViewTest *test = new CQModelViewTest;

  test->setCommentHeader    (comment_header);
  test->setFirstLineHeader  (first_line_header);
  test->setFirstColumnHeader(first_column_header);

  test->setShowStats(showStats);
  test->setShowTree (showTree);
  test->setShowTable(showTable);

  test->load(filenames[0]);

  test->show();

  return app.exec();
}

//---

CQModelViewTest::
CQModelViewTest()
{
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(2); layout->setSpacing(2);

  //---

  QFrame *viewFrame = new QFrame;
  viewFrame->setObjectName("viewFrame");

  QHBoxLayout *viewLayout = new QHBoxLayout(viewFrame);
  viewLayout->setMargin(2); viewLayout->setSpacing(2);

  layout->addWidget(viewFrame);

  //---

  // model view
  view_ = new CQModelView(this);
  view_->setObjectName("modelView");

  view_->setAlternatingRowColors(true);
  view_->setStretchLastColumn(true);
  view_->setShowGrid(false);

  delegate_ = new CQItemDelegate(view_);

  view_->setItemDelegate(delegate_);

  viewLayout->addWidget(view_);

  connect(view_, SIGNAL(stateChanged()), this, SLOT(updateState()));

  //---

  // tree view
  tree_ = new QTreeView(this);
  tree_->setObjectName("treeView");

  tree_->setSelectionBehavior(QAbstractItemView::SelectItems);
  tree_->setAlternatingRowColors(true);
//tree_->setShowGrid(state);
//tree_->setStretchLastColumn(true);

  delegate_ = new CQItemDelegate(tree_);

  tree_->setItemDelegate(delegate_);

  viewLayout->addWidget(tree_);

  //---

  // tree view
  table_ = new QTableView(this);
  table_->setObjectName("tableView");

  table_->setSelectionBehavior(QAbstractItemView::SelectItems);
  table_->setAlternatingRowColors(true);
  table_->setShowGrid(false);
//table_->setStretchLastColumn(true);

  delegate_ = new CQItemDelegate(table_);

  table_->setItemDelegate(delegate_);

  viewLayout->addWidget(table_);

  //---

  // stats
  stats_ = new CQModelViewStats(view_);
  stats_->setObjectName("stats");

  viewLayout->addWidget(stats_);

  //---

  QFrame *statusFrame = new QFrame;
  statusFrame->setObjectName("statusFrame");

  QHBoxLayout *statusLayout = new QHBoxLayout(statusFrame);
  statusLayout->setMargin(2); statusLayout->setSpacing(2);

  statusFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  //---

  statusLabel_ = new QLabel;
  statusLabel_->setText(" ");

  statusLayout->addWidget(statusLabel_);

  QCheckBox *gridCheck = new QCheckBox("Grid");

  statusLayout->addWidget(gridCheck);

  connect(gridCheck, SIGNAL(stateChanged(int)), this, SLOT(gridSlot(int)));

  //---

  layout->addWidget(statusFrame);
}

CQModelViewTest::
~CQModelViewTest()
{
  delete model_;
}

void
CQModelViewTest::
gridSlot(int state)
{
  view_ ->setShowGrid(state);
  table_->setShowGrid(state);
//tree_ ->setShowGrid(state);
}

void
CQModelViewTest::
load(const QString &filename)
{
  auto p = filename.lastIndexOf('.');

  QString suffix = (p >= 0 ? filename.mid(p + 1) : "");

  if      (suffix == "json") {
    CQJsonModel *model = new CQJsonModel;

    model_ = model;

    model->setReadOnly(false);

    model->load(filename);
  }
  else if (suffix == "csv") {
    CQCsvModel *model = new CQCsvModel;

    model_ = model;

    if (isCommentHeader())
      model->setCommentHeader(true);

    if (isFirstLineHeader())
      model->setFirstLineHeader(true);

    if (isFirstColumnHeader())
      model->setFirstColumnHeader(true);

    model->setReadOnly(false);

    model->load(filename);
  }
  else {
    std::cerr << "Invalid filename " << filename.toStdString() << "\n";
    return;
  }

  proxyModel_ = new QSortFilterProxyModel;

  proxyModel_->setSourceModel(model_);
  proxyModel_->setSortRole(Qt::EditRole);

  view_ ->setModel(proxyModel_);
  tree_ ->setModel(proxyModel_);
  table_->setModel(proxyModel_);
}

void
CQModelViewTest::
updateVisibleState()
{
  tree_ ->setVisible(isShowTree());
  table_->setVisible(isShowTable());
  stats_->setVisible(isShowStats());
}

void
CQModelViewTest::
updateState()
{
  int nr = view_->numModelRows();
  int nv = view_->numVisibleRows();

  QString str = QString("%1 Rows, %2 Visible Rows").arg(nr).arg(nv);

  statusLabel_->setText(str);
}

QSize
CQModelViewTest::
sizeHint() const
{
  int width = 800;

  if (isShowStats())
    width += 400;

  if (isShowTree())
    width += 400;

  if (isShowTable())
    width += 400;

  return QSize(width, 800);
}

//------

CQModelViewStats::
CQModelViewStats(CQModelView *view) :
 view_(view)
{
  QVBoxLayout *layout = new QVBoxLayout(this);

  textEdit_ = new QTextEdit;

  layout->addWidget(textEdit_);

  timer_ = new QTimer(this);

#ifdef CQ_MODEL_VIEW_TRACE
  connect(timer_, SIGNAL(timeout()), this, SLOT(updateSlot()));

  timer_->start(500);
#endif
}

void
CQModelViewStats::
updateSlot()
{
#ifdef CQ_MODEL_VIEW_TRACE
  QString str;

  auto addTrace = [&](const CQPerfTraceData *trace) {
    double t = trace->elapsed().getMSecs();
    int    n = trace->numCalls();

    if (str.length())
      str += "\n";

    str += QString("%1 %2 (#%3)").arg(trace->name()).arg(t).arg(n);
  };

  str += QString("\nNum Redraws %1").arg(view_->numRedraws());

  CQPerfMonitor::TraceList traces;

  CQPerfMonitorInst->getTracesStartingWith("CQModelView::", traces);

  for (const auto &trace : traces)
    addTrace(trace);

  textEdit_->setText(str);
#endif
}

QSize
CQModelViewStats::
sizeHint() const
{
  return QSize(256, 256);
}
