#ifndef CQModelViewTest_H
#define CQModelViewTest_H

#include <QFrame>

class CQModelView;
class CQCsvModel;
class CQItemDelegate;
class QSortFilterProxyModel;

class CQModelViewTest : public QFrame {
  Q_OBJECT

 public:
  CQModelViewTest();
 ~CQModelViewTest();

  bool isCommentHeader() const { return commentHeader_; }
  void setCommentHeader(bool b) { commentHeader_ = b; }

  bool isFirstLineHeader() const { return firstLineHeader_; }
  void setFirstLineHeader(bool b) { firstLineHeader_ = b; }

  bool isFirstColumnHeader() const { return firstColumnHeader_; }
  void setFirstColumnHeader(bool b) { firstColumnHeader_ = b; }

  void load(const QString &filename);

  QSize sizeHint() const override { return QSize(800, 600); }

 private:
  bool                   commentHeader_     { false };
  bool                   firstLineHeader_   { false };
  bool                   firstColumnHeader_ { false };
  CQModelView*           view_              { nullptr };
  CQCsvModel*            model_             { nullptr };
  QSortFilterProxyModel* proxyModel_        { nullptr };
  CQItemDelegate*        delegate_          { nullptr };
};

#endif
