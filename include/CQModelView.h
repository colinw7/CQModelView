#ifndef CQModelView_H
#define CQModelView_H

#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QPointer>
#include <QModelIndex>

class CQModelViewHeader;
class CQModelViewCornerButton;
class CQModelViewSelectionModel;

class QAbstractItemModel;
class QItemSelectionModel;
class QScrollBar;

/*!
 * Viewer for QAbstractItemModel with support for:
 *  . flat/hierarchical models
 *  . multi-line horizontal header
 *  . vertical/horizontal headers
 *  . fixed columns
 *  . fast path when no delegate
 *
 * TODO:
 *  . hierarchical
 *  . change item selection mode
 *  . paint header section
 *  . multiple freeze columns
 *  . sort indicator
 *  . horizontal scroll range/update
 *  . corner button
 *  . optional vertical header text (numbers)
 *  . model flags for cell enabled
 *  . show grid (on/off style)
 *  . scroll on mouse drag
 */
class CQModelView : public QAbstractItemView {
  Q_OBJECT

  Q_PROPERTY(bool freezeFirstColumn   READ isFreezeFirstColumn   WRITE setFreezeFirstColumn  )
  Q_PROPERTY(bool stretchLastColumn   READ isStretchLastColumn   WRITE setStretchLastColumn  )
  Q_PROPERTY(bool sortingEnabled      READ isSortingEnabled      WRITE setSortingEnabled     )
  Q_PROPERTY(bool cornerButtonEnabled READ isCornerButtonEnabled WRITE setCornerButtonEnabled)

 public:
  CQModelView(QWidget *parent=nullptr);

  //---

  QAbstractItemModel *model() const;
  void setModel(QAbstractItemModel *model) override;

  QItemSelectionModel *selectionModel() const;
  void setSelectionModel(QItemSelectionModel *sm);

  QAbstractItemDelegate *itemDelegate() const;
  void setItemDelegate(QAbstractItemDelegate *delegate);

  QHeaderView *horizontalHeader() const;
  QHeaderView *verticalHeader() const;

  //---

  int rowHeight(int row) const;

  int columnWidth(int column) const;
  void setColumnWidth(int column, int width);

  bool columnHeatmap(int column) const;
  void setColumnHeatmap(int column, bool heatmap);

  //---

  bool isStretchLastColumn() const { return stretchLastColumn_; }
  void setStretchLastColumn(bool b) { stretchLastColumn_ = b; }

  bool isFreezeFirstColumn() const { return freezeFirstColumn_; }
  void setFreezeFirstColumn(bool b) { freezeFirstColumn_ = b; }

  bool isSortingEnabled() const { return sortingEnabled_; }
  void setSortingEnabled(bool b);

  bool isCornerButtonEnabled() const;
  void setCornerButtonEnabled(bool enable);

  //---

  void sortByColumn(int column, Qt::SortOrder order);
  void sortByColumn(int column);

  //---

  bool isColumnHidden(int column) const;
  void setColumnHidden(int column, bool hide);

  //---

  void fitColumn(int column);

  //---

  void resizeEvent(QResizeEvent *) override;

  void paintEvent(QPaintEvent *) override;

  void mousePressEvent  (QMouseEvent *e) override;
  void mouseMoveEvent   (QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;

  void mouseDoubleClickEvent(QMouseEvent *e) override;

  void keyPressEvent(QKeyEvent *e) override;

  void leaveEvent(QEvent *) override;

  void contextMenuEvent(QContextMenuEvent *e) override;

  //---

  //QAbstractItemView interface

  QRect visualRect(const QModelIndex &index) const override;

  void scrollTo(const QModelIndex&, QAbstractItemView::ScrollHint=EnsureVisible) override;

  QModelIndex indexAt(const QPoint &point) const override;

  QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;

  int horizontalOffset() const override;
  int verticalOffset  () const override;

  bool isIndexHidden(const QModelIndex &index) const override;

  void setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags command) override;

  QRegion visualRegionForSelection(const QItemSelection &selection) const override;

  //---

  QSize sizeHint() const override { return QSize(100, 100); }

 private:
  friend class CQModelViewHeader;
  friend class CQModelViewSelectionModel;

  struct PositionData {
    QPoint      pos;
    int         hsection  { -1 };
    int         hsectionh { -1 };
    int         vsection  { -1 };
    QModelIndex ind;

    void reset() {
      hsection  = -1;
      hsectionh = -1;
      vsection  = -1;
      ind       = QModelIndex();
    }

    friend bool operator==(const PositionData &lhs, const PositionData &rhs) {
      return ((lhs.hsection  == rhs.hsection ) &&
              (lhs.hsectionh == rhs.hsectionh) &&
              (lhs.vsection  == rhs.vsection ) &&
              (lhs.ind       == rhs.ind      ));
    }

    friend bool operator!=(const PositionData &lhs, const PositionData &rhs) {
      return ! operator==(lhs, rhs);
    }
  };

  enum class ColorRole {
    None,
    Window,
    Text,
    HeaderBg,
    HeaderFg,
    MouseOverBg,
    MouseOverFg,
    HighlightBg,
    HighlightFg,
    AlternateBg,
    AlternateFg,
    SelectionBg,
    SelectionFg
  };

  struct MouseData {
    bool                  pressed      { false };
    Qt::KeyboardModifiers modifiers    { Qt::NoModifier };
    PositionData          pressData;
    PositionData          moveData;
    PositionData          releaseData;
    PositionData          menuData;
    int                   headerWidth  { 0 };

    void reset() {
      pressed   = false;
      modifiers = Qt::NoModifier;

      pressData  .reset();
      moveData   .reset();
      releaseData.reset();
    }
  };

  bool hheaderPositionToIndex(PositionData &posData) const;
  bool vheaderPositionToIndex(PositionData &posData) const;

  void drawCells(QPainter *painter);

  void drawHHeader(QPainter *painter);
  void drawHHeaderSection(QPainter *painter, int c, int x);

  void drawVHeader(QPainter *painter);

  void initVisCellDatas() const;

  void initVisibleColumns(bool clear);

  bool isNumericColumn(int c) const;

  void updateCurrentIndices();
  void updateSelection();

  void handleMousePress  ();
  void handleMouseMove   ();
  void handleMouseRelease();

  void handleMouseDoubleClick();

  void showMenu(const QPoint &pos);

  void redraw();

  void setRolePen  (QPainter *painter, ColorRole role, double alpha=1.0);
  void setRoleBrush(QPainter *painter, ColorRole role, double alpha=1.0);

  const MouseData &mouseData() const { return mouseData_; }
  void setMouseData(const MouseData &data) { mouseData_ = data; }

 private:
  void updateGeometries();
  void updateScrollBars();

  void drawRow (QPainter *painter, int r, int y);
  void drawCell(QPainter *painter, int r, int c, int x, int y);

  bool cellPositionToIndex(PositionData &posData) const;

  QColor roleColor(ColorRole role) const;

  QColor textColor(const QColor &c) const;

  QColor blendColors(const QColor &c1, const QColor &c2, double f) const;

 public slots:
  void hideColumn(int column);
  void showColumn(int column);

 private slots:
  void modelChangedSlot();

  void hscrollSlot(int v);
  void vscrollSlot(int v);

  void alternatingRowColorsSlot(bool b);
  void freezeFirstColumnSlot   (bool b);
  void stretchLastColumnSlot   (bool b);

  void sortingEnabledSlot   (bool b);
  void sortIncreasingSlot();
  void sortDecreasingSlot();

  void hideColumnSlot();
  void showAllColumnsSlot();

  void fitAllColumnsSlot();
  void fitColumnSlot();

  void setHeatmapSlot(bool b);

 private:
  struct GlobalColumnData {
    int headerWidth { 10 };
    int margin      { 4 };
  };

  struct ColumnData {
    int  width   { 100 };
    bool heatmap { false };
  };

  struct GlobalRowData {
    int headerHeight { 10 };
    int height       { 10 };
    int margin       { 6 };
    int headerMargin { 8 };
  };

  struct RowData {
    bool hidden { false };
  };

  struct State {
    bool updateScrollBars { false };
  };

  struct VisColumnData {
    QRect rect;
    QRect hrect;
    bool  first { false };
    bool  last  { false };
  };

  struct VisRowData {
    QRect rect;
  };

  struct VisCellData {
    QRect rect;
  };

  struct ScrollData {
    int hpos { 0 };
    int vpos { 0 };
    int nv   { -1 };
  };

  using RoleColors = std::map<ColorRole,QColor>;

  struct PaintData {
    ColorRole  penRole    = ColorRole::Text;
    double     penAlpha   = 1.0;
    ColorRole  brushRole  = ColorRole::Window;
    double     brushAlpha = 1.0;
    RoleColors roleColors;

    QSize decorationSize;
    bool  showDecorationSelected { false };
    int   w                      { 0 };
    int   margin                 { 0 };

    void reset() {
      resetRole();
    }

    void resetRole() {
      penRole    = ColorRole::Text;
      penAlpha   = 1.0;
      brushRole  = ColorRole::Window;
      brushAlpha = 1.0;
    }
  };

  using ModelP         = QPointer<QAbstractItemModel>;
  using SelModelP      = QPointer<QItemSelectionModel>;
  using DelegateP      = QPointer<QAbstractItemDelegate>;
  using ColumnDatas    = std::vector<ColumnData>;
  using RowDatas       = std::map<int,RowData>;
  using VisColumnDatas = std::map<int,VisColumnData>;
  using VisRowDatas    = std::map<int,VisRowData>;
  using VisCellDatas   = std::map<QModelIndex,VisCellData>;

  ModelP    model_;
  SelModelP sm_;
  DelegateP delegate_;

  bool freezeFirstColumn_ { false };
  bool stretchLastColumn_ { false };
  bool sortingEnabled_    { false };

  CQModelViewCornerButton *cornerWidget_ { nullptr };

  GlobalColumnData       globalColumnData_;
  ColumnDatas            columnDatas_;
  GlobalRowData          globalRowData_;
  RowDatas               rowDatas_;
  State                  state_;
  ScrollData             scrollData_;
  mutable VisColumnDatas visColumnDatas_;
  mutable VisRowDatas    visRowDatas_;
  mutable VisCellDatas   visCellDatas_;
  mutable bool           visCellDataAll_ { false };
  PaintData              paintData_;
  MouseData              mouseData_;

  // widgets
  CQModelViewHeader* vh_ { nullptr };
  CQModelViewHeader* hh_ { nullptr };
  QScrollBar*        hs_ { nullptr };
  QScrollBar*        vs_ { nullptr };

  QItemSelectionModel *vsm_ { nullptr };
  QItemSelectionModel *hsm_ { nullptr };

  // draw data
  int          nr_ { 0 };
  int          nc_ { 0 };
  int          r1_ { 0 };
  int          r2_ { 0 };
  QFontMetrics fm_;
};

//---

class CQModelViewHeader : public QHeaderView {
  Q_OBJECT

 public:
  CQModelViewHeader(Qt::Orientation orient, CQModelView *view);

  //---

  void resizeEvent(QResizeEvent *) override;

  void paintEvent(QPaintEvent *) override;

  void mousePressEvent  (QMouseEvent *e) override;
  void mouseMoveEvent   (QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;

  void mouseDoubleClickEvent(QMouseEvent *e) override;

  void keyPressEvent(QKeyEvent *e) override;

  void leaveEvent(QEvent *) override;

  void contextMenuEvent(QContextMenuEvent *e) override;

  //---

  void redraw();

  QSize sizeHint() const override { return QSize(100, 100); }

 private:
  CQModelView* view_ { nullptr };
};

//---

class CQModelViewSelectionModel : public QItemSelectionModel {
  Q_OBJECT

 public:
  CQModelViewSelectionModel(CQModelView *view);

 ~CQModelViewSelectionModel() override;

  void setCurrentIndex(const QModelIndex &ind, QItemSelectionModel::SelectionFlags flags) override;

  void select(const QModelIndex &ind, QItemSelectionModel::SelectionFlags flags) override;
  void select(const QItemSelection &sel, QItemSelectionModel::SelectionFlags flags) override;

  void clear() override;
  void reset() override;

  void clearCurrentIndex() override;

 private:
  CQModelView* view_ { nullptr };
};

#endif
