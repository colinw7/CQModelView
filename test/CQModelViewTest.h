#ifndef CQModelViewTest_H
#define CQModelViewTest_H

#include <QFrame>

class CQModelView;
class CQModelViewStats;

class QAbstractItemModel;
class CQItemDelegate;
class QTreeView;
class QTableView;
class QSortFilterProxyModel;
class QLabel;

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

  bool isShowStats() const { return showStats_; }
  void setShowStats(bool b) { showStats_ = b;  updateVisibleState(); }

  bool isShowTree() const { return showTree_; }
  void setShowTree(bool b) { showTree_ = b; updateVisibleState(); }

  bool isShowTable() const { return showTable_; }
  void setShowTable(bool b) { showTable_ = b; updateVisibleState(); }

  void load(const QString &filename);

  QSize sizeHint() const override;

  void updateVisibleState();

 private slots:
  void gridSlot(int state);

  void updateState();

 private:
  bool                   commentHeader_     { false };
  bool                   firstLineHeader_   { false };
  bool                   firstColumnHeader_ { false };
  bool                   showTree_          { false };
  bool                   showTable_         { false };
  bool                   showStats_         { false };
  CQModelView*           view_              { nullptr };
  CQModelViewStats*      stats_             { nullptr };
  QTreeView*             tree_              { nullptr };
  QTableView*            table_             { nullptr };
  QLabel*                statusLabel_       { nullptr };
  QAbstractItemModel*    model_             { nullptr };
  QSortFilterProxyModel* proxyModel_        { nullptr };
  CQItemDelegate*        delegate_          { nullptr };
};

//------

class QTextEdit;
class QTimer;

class CQModelViewStats : public QFrame {
  Q_OBJECT

 public:
  CQModelViewStats(CQModelView *view);

 private:
  CQModelView* view_ { nullptr };

  QSize sizeHint() const;

 private slots:
  void updateSlot();

 private:
  QTextEdit* textEdit_ { nullptr };
  QTimer*    timer_    { nullptr };
};

#endif
