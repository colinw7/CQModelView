#ifndef CQModelViewHeader_H
#define CQModelViewHeader_H

#include <QHeaderView>

class CQModelView;

class CQModelViewHeader : public QHeaderView {
  Q_OBJECT

 public:
  CQModelViewHeader(Qt::Orientation orient, CQModelView *view);

  //---

  void setModel(QAbstractItemModel *model) override;

  void setVisible(bool v) override;

  void doItemsLayout() override;

  void reset() override;

  //---

//bool event(QEvent *e) override;

  void resizeEvent(QResizeEvent *) override;

  void paintEvent(QPaintEvent *) override;

  void mousePressEvent  (QMouseEvent *e) override;
  void mouseMoveEvent   (QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;

  void mouseDoubleClickEvent(QMouseEvent *e) override;

  void keyPressEvent(QKeyEvent *e) override;

  void leaveEvent(QEvent *) override;

  void contextMenuEvent(QContextMenuEvent *e) override;

  bool viewportEvent(QEvent *e) override;

  bool event(QEvent *e) override;

  //---

  int horizontalOffset() const override;
  int verticalOffset() const override;
  void updateGeometries() override;
  void scrollContentsBy(int dx, int dy) override;

  void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                   const QVector<int> &roles=QVector<int>()) override;
  void rowsInserted(const QModelIndex &parent, int start, int end) override;

  QRect visualRect(const QModelIndex &index) const override;
  void scrollTo(const QModelIndex &index, ScrollHint hint) override;

  QModelIndex indexAt(const QPoint &p) const override;
  bool isIndexHidden(const QModelIndex &index) const override;

  QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override;
  void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags flags) override;
  QRegion visualRegionForSelection(const QItemSelection &selection) const override;

  //---

  void emitSectionClicked(int section);

  //---

  void redraw();

  QSize sizeHint() const override { return QSize(100, 100); }

 protected:
  void currentChanged(const QModelIndex &current, const QModelIndex &old) override;

 private:
  CQModelView* view_ { nullptr };
};

#endif
