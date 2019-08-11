#include <CQModelViewTest.h>
#include <CQModelView.h>
#include <CQItemDelegate.h>
#include <CQCsvModel.h>
#include <CArgs.h>
#include <QApplication>
#include <QHBoxLayout>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>

int
main(int argc, char **argv)
{
  QApplication app(argc, argv);

  QFont font("FreeSans", 18);

  qApp->setFont(font);

  CArgs args("-comment_header:b      (Comment Header) "
             "-first_line_header:b   (First Line Header) "
             "-first_column_header:b (First Column Header)");

  args.parse(&argc, argv);

  bool comment_header      = args.getArg<bool>("-comment_header");
  bool first_line_header   = args.getArg<bool>("-first_line_header");
  bool first_column_header = args.getArg<bool>("-first_column_header");

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

  view_->setFreezeFirstColumn(true);
  view_->setStretchLastColumn(true);

  delegate_ = new CQItemDelegate(view_);

  view_->setItemDelegate(delegate_);

  layout->addWidget(view_);
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
  model_ = new CQCsvModel;

  if (isCommentHeader())
    model_->setCommentHeader(true);

  if (isFirstLineHeader())
    model_->setFirstLineHeader(true);

  if (isFirstColumnHeader())
    model_->setFirstColumnHeader(true);

  model_->load(filename);

  proxyModel_ = new QSortFilterProxyModel;

  proxyModel_->setSourceModel(model_);

  view_->setModel(proxyModel_);
}
