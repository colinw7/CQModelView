#ifndef CQModelView_H
#define CQModelView_H

#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QAbstractButton>
#include <QHeaderView>
#include <QLineEdit>
#include <QPointer>
#include <QModelIndex>
#include <set>

class CQModelViewHeader;
class CQModelViewCornerButton;
class CQModelViewSelectionModel;
class CQModelViewFilterEdit;

class QAbstractItemModel;
class QItemSelectionModel;
class QScrollBar;

/*!
 * Viewer for QAbstractItemModel with support for:
 *  . flat/hierarchical models
 *  . multi-line horizontal header
 *  . vertical/horizontal headers
 *  . frozen columns
 *  . fixed row heights for speed
 *  . fast path when no delegate
 *
 * TODO:
 *  . change item selection mode
 *  . paint header section
 *  . multiple freeze columns
 *  . draw sort indicator/column menu
 *  . optional vertical header text (numbers)
 *  . model flags for cell enabled
 *  . show grid (on/off style)
 *  . regexp for filter
 *  . root index
 *  . auto expand delay
 *  . root is decorated
 *  . items expandable
 *  . expand on double click
 *  . sync header width for expand last for tooltip
 *  . column header widgets
 *  . persistent model indices
 *  . select all for hier
 */
class CQModelView : public QAbstractItemView {
  Q_OBJECT

  // new
  Q_PROPERTY(bool freezeFirstColumn    READ isFreezeFirstColumn   WRITE setFreezeFirstColumn  )
  Q_PROPERTY(bool stretchLastColumn    READ isStretchLastColumn   WRITE setStretchLastColumn  )
  Q_PROPERTY(bool multiHeaderLines     READ isMultiHeaderLines    WRITE setMultiHeaderLines   )
  Q_PROPERTY(bool showVerticalHeader   READ isShowVerticalHeader  WRITE setShowVerticalHeader )
  Q_PROPERTY(bool showFilter           READ isShowFilter          WRITE setShowFilter         )

  // qtableview+qtreeview
  Q_PROPERTY(bool sortingEnabled       READ isSortingEnabled      WRITE setSortingEnabled     )
//Q_PROPERTY(bool wordWrap             READ wordWrap              WRITE setWordWrap           )

  // qtableview
//Q_PROPERTY(bool showGrid             READ showGrid              WRITE setShowGrid           )
  Q_PROPERTY(bool cornerButtonEnabled  READ isCornerButtonEnabled WRITE setCornerButtonEnabled)

//Q_PROPERTY(Qt::PenStyle gridStyle READ gridStyle WRITE setGridStyle)

  // qtreeview
//Q_PROPERTY(int  autoExpandDelay      READ autoExpandDelay      WRITE setAutoExpandDelay     )
  Q_PROPERTY(int  indentation          READ indentation          WRITE setIndentation         )
  Q_PROPERTY(bool rootIsDecorated      READ rootIsDecorated      WRITE setRootIsDecorated     )
//Q_PROPERTY(bool uniformRowHeights    READ uniformRowHeights    WRITE setUniformRowHeights   )
//Q_PROPERTY(bool itemsExpandable      READ itemsExpandable      WRITE setItemsExpandable     )
//Q_PROPERTY(bool animated             READ isAnimated           WRITE setAnimated            )
//Q_PROPERTY(bool allColumnsShowFocus  READ allColumnsShowFocus  WRITE setAllColumnsShowFocus )
//Q_PROPERTY(bool headerHidden         READ isHeaderHidden       WRITE setHeaderHidden        )
//Q_PROPERTY(bool expandsOnDoubleClick READ expandsOnDoubleClick WRITE setExpandsOnDoubleClick)


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

  bool isFreezeFirstColumn() const { return freezeFirstColumn_; }
  void setFreezeFirstColumn(bool b);

  bool isStretchLastColumn() const { return stretchLastColumn_; }
  void setStretchLastColumn(bool b);

  bool isMultiHeaderLines() const { return multiHeaderLines_; }
  void setMultiHeaderLines(bool b);

  bool isShowVerticalHeader() const { return showVerticalHeader_; }
  void setShowVerticalHeader(bool b);

  bool isShowFilter() const { return showFilter_; }
  void setShowFilter(bool b);

  //---

  bool isSortingEnabled() const { return sortingEnabled_; }
  void setSortingEnabled(bool b);

  bool isCornerButtonEnabled() const;
  void setCornerButtonEnabled(bool enable);

  //---

  int indentation() const { return indentation_; }
  void setIndentation(int i);

  bool rootIsDecorated() const { return rootIsDecorated_; }
  void setRootIsDecorated(bool b);

  //---

  void sortByColumn(int column, Qt::SortOrder order);
  void sortByColumn(int column);

  //---

  bool isColumnHidden(int column) const;
  void setColumnHidden(int column, bool hide);

  //---

  bool isRowHidden(int row, const QModelIndex &parent) const;
  void setRowHidden(int row, const QModelIndex &parent, bool hide);

  //---

  bool isExpanded(const QModelIndex &index) const;
  void setExpanded(const QModelIndex &index, bool expand);

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

 signals:
  void expanded (const QModelIndex &index);
  void collapsed(const QModelIndex &index);

 private:
  friend class CQModelViewHeader;
  friend class CQModelViewSelectionModel;

  struct PositionData {
    QPoint      pos;
    int         hsection  { -1 };
    int         hsectionh { -1 };
    int         vsection  { -1 };
    QModelIndex ind;
    QModelIndex iind;
    QRect       rect;

    void reset() {
      hsection  = -1;
      hsectionh = -1;
      vsection  = -1;
      ind       = QModelIndex();
      iind      = QModelIndex();
    }

    friend bool operator==(const PositionData &lhs, const PositionData &rhs) {
      return ((lhs.hsection  == rhs.hsection ) &&
              (lhs.hsectionh == rhs.hsectionh) &&
              (lhs.vsection  == rhs.vsection ) &&
              (lhs.ind       == rhs.ind      ) &&
              (lhs.iind      == rhs.iind     ));
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
    HeaderLineFg,
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

  struct VisRowData;
  struct VisColumnData;

 private:
  int columnWidth(int column, const VisColumnData &visColumnData) const;

  bool hheaderPositionToIndex(PositionData &posData) const;
  bool vheaderPositionToIndex(PositionData &posData) const;

  void drawCells(QPainter *painter);

  void drawCellsRows(QPainter *painter) const;
  void drawCellsSelection(QPainter *painter);

  void drawHHeader(QPainter *painter);
  void drawHHeaderSection(QPainter *painter, int c, const VisColumnData &visColumnData);

  void drawVHeader(QPainter *painter);

  void updateRowDatas(); // nvr_, rowDatas_, indRow_

  void updateVisRows();    // visRowDatas_
  void updateVisColumns(); // nvc_, visColumnDatas_
  void updateVisCells();   // visCellDatas_

  bool isNumericColumn(int c) const;

  void updateCurrentIndices();
  void updateSelection();

  void handleMousePress  ();
  void handleMouseMove   ();
  void handleMouseRelease();

  void handleMouseDoubleClick();

  void showMenu(const QPoint &pos);

  void redraw();
  void redraw(const QRect &rect);

  void setRolePen  (QPainter *painter, ColorRole role, double alpha=1.0) const;
  void setRoleBrush(QPainter *painter, ColorRole role, double alpha=1.0) const;

  const MouseData &mouseData() const { return mouseData_; }
  void setMouseData(const MouseData &data) { mouseData_ = data; }

 private:
  void updateGeometries();
  void updateScrollBars();

  void numVisibleRows(const QModelIndex &parent, int &nvr, int depth);

  void drawRow(QPainter *painter, int r, const QModelIndex &parent,
               const VisRowData &visRowData) const;

  void drawCell(QPainter *painter, int r, int c, const QModelIndex &parent,
                const VisRowData &visRowData, const VisColumnData &visColumnData) const;

  bool isIndexExpanded(const QModelIndex &index) const;

  bool cellPositionToIndex(PositionData &posData) const;

  bool maxColumnWidth(int column, const QModelIndex &parent, int depth, int &nvr, int &maxWidth);

  QColor roleColor(ColorRole role) const;

  QColor textColor(const QColor &c) const;

  QColor blendColors(const QColor &c1, const QColor &c2, double f) const;

 public slots:
  void hideColumn(int column);
  void showColumn(int column);

  void expand  (const QModelIndex &index);
  void collapse(const QModelIndex &index);

  void hideRow(int row);
  void showRow(int row);

  void expandAll();
  void collapseAll();
  void expandToDepth(int depth);

 private slots:
  void modelChangedSlot();

  void hscrollSlot(int v);
  void vscrollSlot(int v);

  void alternatingRowColorsSlot(bool b);
  void freezeFirstColumnSlot   (bool b);
  void stretchLastColumnSlot   (bool b);
  void multiHeaderLinesSlot    (bool b);
  void rootIsDecoratedSlot     (bool b);

  void sortingEnabledSlot(bool b);
  void sortIncreasingSlot();
  void sortDecreasingSlot();

  void showVerticalHeaderSlot(bool b);
  void showFilterSlot(bool b);

  void hideColumnSlot();
  void showAllColumnsSlot();

  void fitAllColumnsSlot();
  void fitColumnSlot();

  void expandSlot();
  void collapseSlot();

  void setHeatmapSlot(bool b);

  void editFilterSlot();

 private:
  struct GlobalColumnData {
    int headerWidth { 10 };
    int margin      { 4 };
  };

  struct ColumnData {
    int  width   { -1 };
    bool heatmap { false };
  };

  struct GlobalRowData {
    int headerHeight { 10 };
    int height       { 10 };
    int margin       { 6 };
    int headerMargin { 8 };
  };

  struct RowData {
    QModelIndex parent;
    int         row      { 0 };
    int         flatRow  { 0 };
    bool        children { false };
    bool        expanded { true };
    bool        hidden   { false };
    int         depth    { 0 };

    RowData(const QModelIndex &parent=QModelIndex(), int row=0, int flatRow=0,
            bool children=false, bool expanded=true, int depth=0) :
     parent(parent), row(row), flatRow(flatRow), children(children),
     expanded(expanded), depth(depth) {
    }
  };

  struct State {
    bool updateScrollBars { false }; // update scrollbars (new rows, columns)
    bool updateRowDatas   { false }; // update flat vertical rows (visibility)
    bool updateVisRows    { false }; // update visible rows (resize, visibility)
    bool updateVisColumns { false }; // update visible columns (resize, visibility)
    bool updateVisCells   { false }; // update visible cells (resize, visibility)
    bool updateGeometries { false }; // update widgets (resize)

    void updateAll() {
      updateScrollBars = true;
      updateRowDatas   = true;
      updateVisRows    = true;
      updateVisColumns = true;
      updateVisCells   = true;
      updateGeometries = true;
    }
  };

  struct VisColumnData {
    QRect rect;
    QRect hrect;
    bool  first   { false };
    bool  last    { false };
    bool  visible { true };
  };

  struct VisRowData {
    QRect rect;
    int   flatRow   { -1 };
    bool  alternate { false };
    int   depth     { 0 };
    bool  visible   { true };
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
    QRect vrect;
    int   vw                     { 0 };
    int   vh                     { 0 };
    int   margin                 { 0 };
    int   rowHeight              { 0 };
    int   depth                  { 0 };

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
  using IndRow         = std::map<QModelIndex,int>;
  using VisColumnDatas = std::map<int,VisColumnData>;
  using RowVisRowDatas = std::map<int,VisRowData>;
  using VisRowDatas    = std::map<QModelIndex,RowVisRowDatas>;
  using VisFlatRows    = std::vector<int>;
  using VisCellDatas   = std::map<QModelIndex,VisCellData>;
  using FilterEdits    = std::vector<CQModelViewFilterEdit*>;
  using IndexSet       = std::set<QModelIndex>;
  using ColumnSpan     = std::pair<int,int>;
  using ColumnSpans    = std::vector<ColumnSpan>;
  using RowColumnSpans = std::map<int,ColumnSpans>;

  ModelP    model_;
  SelModelP sm_;
  DelegateP delegate_;

  bool freezeFirstColumn_  { false };
  bool stretchLastColumn_  { false };
  bool multiHeaderLines_   { false };
  bool showVerticalHeader_ { true };
  bool showFilter_         { false };

  bool sortingEnabled_     { false };

  int  indentation_        { 20 };
  bool rootIsDecorated_    { true };

  CQModelViewCornerButton *cornerWidget_ { nullptr };

  GlobalColumnData  globalColumnData_; // global column data
  ColumnDatas       columnDatas_;      // per column data

  GlobalRowData     globalRowData_;    // global row data
  RowDatas          rowDatas_;         // per row data
  IndRow            indRow_;           // index to visual row
  RowColumnSpans    rowColumnSpans_;   // per header row column spans

  State             state_;            // state

  ScrollData        scrollData_;       // scroll data (updateScrollBars)
  VisColumnDatas    visColumnDatas_;   // vis column data (updateVisibleColumns)
  VisRowDatas       visRowDatas_;      // vis row data (updateVisRows)
  VisFlatRows       visFlatRows_;      // visible rows (flat index)
  VisCellDatas      visCellDatas_;     // vis cell data (updateVisCells)
  VisCellDatas      ivisCellDatas_;    // vis tree cell data (updateVisCells)
  mutable PaintData paintData_;
  MouseData         mouseData_;
  FilterEdits       filterEdits_;
  bool              hierarchical_ { false };
  int               freezeColumn_ { -1 };
  int               freezeWidth_  { 0 };
  IndexSet          collapsed_;

  // widgets
  CQModelViewHeader* vh_ { nullptr };
  CQModelViewHeader* hh_ { nullptr };
  QScrollBar*        hs_ { nullptr };
  QScrollBar*        vs_ { nullptr };

  QItemSelectionModel *vsm_ { nullptr };
  QItemSelectionModel *hsm_ { nullptr };

  // draw data
  int          nr_  { 0 };
  int          nc_  { 0 };
  int          nvr_ { 0 };
  int          nvc_ { 0 };
  QRect        visualRect_;
  QRect        paintRect_;
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

//---

class CQModelViewCornerButton : public QAbstractButton {
 public:
  CQModelViewCornerButton(CQModelView *view);

  void paintEvent(QPaintEvent*) override;

 private:
  CQModelView* view_ { nullptr };
};

//---

class CQModelViewFilterEdit : public QLineEdit {
  Q_OBJECT

 public:
  CQModelViewFilterEdit(CQModelView *view, int column = 1);

  int column() const { return column_; }
  void setColumn(int c) { column_ = c; }

 private:
  CQModelView* view_   { nullptr };
  int          column_ { -1 };
};

#endif
