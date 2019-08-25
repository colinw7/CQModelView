#include <CQModelViewTest.h>
#include <CQModelView.h>
#include <CQItemDelegate.h>
#include <CQCsvModel.h>
#include <CQJsonModel.h>
//#include <CQPerfMonitor.h>
#include <CArgs.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QTextEdit>
#include <QTimer>

//#define CQ_MODEL_VIEW_TRACE 1

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
             "-tree:b                (Show QTreeView)");

  args.parse(&argc, argv);

  bool comment_header      = args.getArg<bool>("-comment_header");
  bool first_line_header   = args.getArg<bool>("-first_line_header");
  bool first_column_header = args.getArg<bool>("-first_column_header");
  bool showStats           = args.getArg<bool>("-stats");
  bool showTree            = args.getArg<bool>("-tree");

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

  test->load(filenames[0]);

  test->show();

  return app.exec();
}

//---

CQModelViewTest::
CQModelViewTest()
{
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(2); layout->setSpacing(2);

  view_ = new CQModelView(this);

  view_->setAlternatingRowColors(true);

  view_->setStretchLastColumn(true);

  delegate_ = new CQItemDelegate(view_);

  view_->setItemDelegate(delegate_);

  layout->addWidget(view_);

  //---

  qview_ = new QTreeView(this);

  qview_->setAlternatingRowColors(true);

//qview_->setStretchLastColumn(true);

  delegate_ = new CQItemDelegate(qview_);

  qview_->setItemDelegate(delegate_);

  layout->addWidget(qview_);

  //---

  stats_ = new CQModelViewStats(view_);

  layout->addWidget(stats_);
}

CQModelViewTest::
~CQModelViewTest()
{
  delete model_;
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

    //model->setReadOnly(false);

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
  qview_->setModel(proxyModel_);
}

void
CQModelViewTest::
updateState()
{
  qview_->setVisible(isShowTree());
  stats_->setVisible(isShowStats());
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

  connect(timer_, SIGNAL(timeout()), this, SLOT(updateSlot()));

  timer_->start(500);
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
