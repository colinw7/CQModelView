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
class QTextLayout;

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
 *  . multiple freeze columns (order ?)
 *  . column menu
 *  . optional vertical header text (numbers, debug info)
 *  . model flags for cell enabled
 *  . auto expand delay (timer)
 *  . items expandable
 *  . expand on double click
 *  . sync header width for expand last for tooltip
 *  . column header widgets
 *  . persistent model indices
 *  . move columns (drag, reorder)
 *  . span columns ?
 *  . optimized draw/damage rect
 *  . word wrap
 *  . keyboard search multiple characters
 *  . style hints
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
  Q_PROPERTY(bool showGrid             READ showGrid              WRITE setShowGrid           )
  Q_PROPERTY(bool showHHeaderLines     READ showHHeaderLines      WRITE setShowHHeaderLines   )
  Q_PROPERTY(bool showVHeaderLines     READ showVHeaderLines      WRITE setShowVHeaderLines   )
  Q_PROPERTY(bool cornerButtonEnabled  READ isCornerButtonEnabled WRITE setCornerButtonEnabled)

  Q_PROPERTY(Qt::PenStyle gridStyle READ gridStyle WRITE setGridStyle)

  // qtreeview
//Q_PROPERTY(int  autoExpandDelay      READ autoExpandDelay      WRITE setAutoExpandDelay     )
  Q_PROPERTY(int  indentation          READ indentation          WRITE setIndentation         )
  Q_PROPERTY(bool rootIsDecorated      READ rootIsDecorated      WRITE setRootIsDecorated     )
//Q_PROPERTY(bool uniformRowHeights    READ uniformRowHeights    WRITE setUniformRowHeights   )
//Q_PROPERTY(bool itemsExpandable      READ itemsExpandable      WRITE setItemsExpandable     )
//Q_PROPERTY(bool animated             READ isAnimated           WRITE setAnimated            )
//Q_PROPERTY(bool allColumnsShowFocus  READ allColumnsShowFocus  WRITE setAllColumnsShowFocus )
  Q_PROPERTY(bool expandsOnDoubleClick READ expandsOnDoubleClick WRITE setExpandsOnDoubleClick)

 public:
  CQModelView(QWidget *parent=nullptr);
 ~CQModelView();

  //---

  QSize viewportSizeHint() const override;

  //---

  QAbstractItemModel *model() const;
  void setModel(QAbstractItemModel *model) override;

  QItemSelectionModel *selectionModel() const;
  void setSelectionModel(QItemSelectionModel *sm) override;

  void setRootIndex(const QModelIndex &index) override;

  void doItemsLayout() override;

  QAbstractItemDelegate *itemDelegate() const;
  void setItemDelegate(QAbstractItemDelegate *delegate);

  QHeaderView *horizontalHeader() const;
  QHeaderView *verticalHeader() const;

  //---

  void reset() override;

  void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                   const QVector<int> &roles = QVector<int>()) override;

  //---

  void selectColumn     (int section, Qt::KeyboardModifiers modifiers);
  void selectColumnRange(int section1, int section2, Qt::KeyboardModifiers modifiers);
  void selectRow        (int section, Qt::KeyboardModifiers modifiers);
  void selectRowRange   (int section1, int section2, Qt::KeyboardModifiers modifiers);

  void selectCell(const QModelIndex &ind, Qt::KeyboardModifiers modifiers);
  void selectCellRange(const QModelIndex &ind1, const QModelIndex &ind2,
                       Qt::KeyboardModifiers modifiers);

  void selectAll() override;

  //---

  void scrollContentsBy(int dx, int dy) override;

  void rowsInserted(const QModelIndex &parent, int start, int end) override;
  void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;

  QModelIndexList selectedIndexes() const override;

  //void timerEvent(QTimerEvent *event) override;

#ifndef QT_NO_DRAGANDDROP
  //void dragMoveEvent(QDragMoveEvent *event) override;
#endif
  //bool viewportEvent(QEvent *event) override;

  void updateGeometries() override;

  //int sizeHintForColumn(int column) const override;
  //int indexRowSizeHint(const QModelIndex &index) const;

  //void horizontalScrollbarAction(int action) override;

  void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

  //void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

  //---

  void keyboardSearch(const QString &search) override;

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

  bool showGrid() const { return showGrid_; }
  void setShowGrid(bool b);

  bool showHHeaderLines() const { return showHHeaderLines_; }
  void setShowHHeaderLines(bool b);

  bool showVHeaderLines() const { return showVHeaderLines_; }
  void setShowVHeaderLines(bool b);

  const Qt::PenStyle &gridStyle() const { return gridStyle_; }
  void setGridStyle(const Qt::PenStyle &s) { gridStyle_ = s; }

  bool isCornerButtonEnabled() const;
  void setCornerButtonEnabled(bool enable);

  //---

  int indentation() const { return indentation_; }
  void setIndentation(int i);

  bool rootIsDecorated() const { return rootIsDecorated_; }
  void setRootIsDecorated(bool b);

  bool expandsOnDoubleClick() const { return expandsOnDoubleClick_; }
  void setExpandsOnDoubleClick(bool b) { expandsOnDoubleClick_ = b; }

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

  void expandedIndices(QModelIndexList &inds) const;

  //---

  bool isHierarchical() const { return hierarchical_; }

  int numRedraws() const { return numRedraws_; }

  int numModelRows() const { return nmr_; }
  int numVisibleRows() const { return nvr_; }

  //---

  void resizeColumnToContents(int column);

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

  virtual void addMenuActions(QMenu *menu);

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

  void stateChanged();

 private:
  friend class CQModelViewHeader;
  friend class CQModelViewSelectionModel;

  struct PositionData {
    QPoint      pos;
    int         hsection  { -1 };
    int         hsectionh { -1 };
    int         vsection  { -1 };
    int         vsectionh { -1 };
    QModelIndex ind;
    QModelIndex iind;
    QModelIndex currentInd;
    int         flatRow   { -1 };
    QRect       rect;

    void reset() {
      hsection   = -1;
      hsectionh  = -1;
      vsection   = -1;
      vsectionh  = -1;
      ind        = QModelIndex();
      iind       = QModelIndex();
      currentInd = QModelIndex();
    }

    int row() const {
      if      (vsection >= 0)  return vsection;
      else if (iind.isValid()) return iind.row();
      else if (ind.isValid())  return ind.row();
      else                     return -1;
    }

    int column() const {
      if      (hsection >= 0)  return hsection;
      else if (iind.isValid()) return iind.column();
      else if (ind .isValid()) return ind.column();
      else                     return -1;
    }

    QModelIndex calcInd() const {
      if      (iind.isValid()) return iind;
      else if (ind .isValid()) return ind;
      else                     return QModelIndex();
    }

    friend bool operator==(const PositionData &lhs, const PositionData &rhs) {
      return ((lhs.hsection  == rhs.hsection ) &&
              (lhs.hsectionh == rhs.hsectionh) &&
              (lhs.vsection  == rhs.vsection ) &&
              (lhs.vsectionh == rhs.vsectionh) &&
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
    Base,
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
    SelectionFg,
    GridFg
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

  void initDrawGrid() const;

  void drawCells(QPainter *painter) const;

  void drawCellsRows(QPainter *painter) const;
  void drawCellsSelection(QPainter *painter) const;

  void drawAlternateEmpty(QPainter *painter);

  void drawGrid(QPainter *painter);

  void drawHHeader(QPainter *painter) const;
  void drawHHeaderSection(QPainter *painter, int c, const VisColumnData &visColumnData) const;

  void drawVHeader(QPainter *painter) const;

  void updateRowDatas(); // nvr_, rowDatas_, indRow_

  void updateVisRows();    // visRowDatas_
  void updateVisColumns(); // nvc_, visColumnDatas_
  void updateVisCells();   // visCellDatas_

  void updateHierSelection(const QItemSelection &selection);

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
  void updateWidgetGeometries();
  void updateScrollBars();

  void initRowDatas(const QModelIndex &parent, int &nvr, int depth, int parentFlatRow);

  void drawRow(QPainter *painter, int r, const QModelIndex &parent,
               const VisRowData &visRowData) const;

  void drawCell(QPainter *painter, int r, int c, const QModelIndex &parent,
                const VisRowData &visRowData, const VisColumnData &visColumnData) const;

  bool isIndexExpanded(const QModelIndex &index) const;

  bool cellPositionToIndex(PositionData &posData) const;

  bool maxColumnWidth(int column, const QModelIndex &parent, int depth,
                      int &nvr, int &maxWidth, int maxRows);

  void filterColumn(int column);

  QColor roleColor(ColorRole role) const;

  QColor textColor(const QColor &c) const;

  QColor blendColors(const QColor &c1, const QColor &c2, double f) const;

  QSizeF viewItemTextLayout(QTextLayout &textLayout, int lineWidth) const;

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

  void setRootSlot();
  void resetRootSlot();

  void showVerticalHeaderSlot(bool b);

  void showFilterSlot(bool b);
  void filterByValueSlot();

  void hideColumnSlot();
  void showAllColumnsSlot();

  void fitAllColumnsSlot();
  void fitColumnSlot();
  void fitNoScrollSlot();

  void expandSlot();
  void collapseSlot();

  void setHeatmapSlot(bool b);

  void editFilterSlot();

 private:
  struct GlobalColumnData {
    int headerWidth { 10 };
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
    int         depth         { 0 };
    int         parentFlatRow { -1 };
    int         flatRow       { 0 };
    int         row           { 0 };
    int         numRows       { 0 };
    bool        children      { false };
    bool        expanded      { true };
    bool        hidden        { false };

    RowData(const QModelIndex &parent=QModelIndex(), int row=0, int numRows=0, int flatRow=0,
            bool children=false, bool expanded=true, int depth=0, int parentFlatRow=-1) :
     parent(parent), depth(depth), parentFlatRow(parentFlatRow), flatRow(flatRow), row(row),
     numRows(numRows), children(children), expanded(expanded) {
    }
  };

  struct State {
    bool updateScrollBars { false }; // update scrollbars (new rows, columns)
    bool updateRowDatas   { false }; // update flat vertical rows (visibility)
    bool updateVisRows    { false }; // update visible rows (resize, visibility)
    bool updateVisColumns { false }; // update visible columns (resize, visibility)
    bool updateVisCells   { false }; // update visible cells (resize, visibility)
    bool updateGeometries { false }; // update widgets (resize)
    bool updateSelection  { false }; // update selection

    void updateAll() {
      updateScrollBars = true;
      updateRowDatas   = true;
      updateVisRows    = true;
      updateVisColumns = true;
      updateVisCells   = true;
      updateGeometries = true;
      updateSelection  = true;
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
    int   depth         { 0 };
    int   parentFlatRow { -1 };
    int   flatRow       { -1 };
    int   r             { -1 };
    int   nr            { -1 };
    QRect rect;                    // vertical header rect
    bool  alternate     { false };
    bool  children      { false };
    bool  expanded      { true };
    bool  visible       { true };
    bool  evisible      { true };
  };

  struct VisCellData {
    int   depth         { -1 };
    int   parentFlatRow { -1 };
    int   flatRow       { -1 };
    int   r             { -1 };
    int   nr            { 0 };
    int   c             { -1 };
    QRect rect;
    bool  selected      { false };
  };

  struct ScrollData {
    int hpos { 0 };
    int vpos { 0 };
    int nv   { -1 };
  };

  using RoleColors = std::map<ColorRole, QColor>;

  struct PaintData {
    PaintData(CQModelView *view) :
     fm(view->font()) {
    }

    ColorRole    penRole    = ColorRole::Text;
    double       penAlpha   = 1.0;
    ColorRole    brushRole  = ColorRole::Window;
    double       brushAlpha = 1.0;
    RoleColors   roleColors;
    QFontMetrics fm;
    QPen         gridPen;

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

  using ModelP             = QPointer<QAbstractItemModel>;
  using SelModelP          = QPointer<QItemSelectionModel>;
  using DelegateP          = QPointer<QAbstractItemDelegate>;
  using ColumnDatas        = std::vector<ColumnData>;
  using RowDatas           = std::map<int, RowData>;
  using IndRow             = std::map<QModelIndex, int>;
  using VisColumnDatas     = std::map<int, VisColumnData>;          // column data
  using RowVisRowDatas     = std::map<int, VisRowData>;             // row data
  using VisRowDatas        = std::map<QModelIndex, RowVisRowDatas>; // parent rows
  using VisFlatRows        = std::vector<int>;
  using VisCellDatas       = std::map<QModelIndex, VisCellData>;
  using FilterEdits        = std::vector<CQModelViewFilterEdit*>;
  using IndexSet           = std::set<QModelIndex>;
  using ColumnSpan         = std::pair<int, int>;
  using ColumnSpans        = std::vector<ColumnSpan>;
  using RowColumnSpans     = std::map<int, ColumnSpans>;
  using CellAreas          = std::vector<VisCellData *>;
  using HierCellAreas      = std::map<int, CellAreas>;
  using DepthHierCellAreas = std::map<int, HierCellAreas>;

  ModelP    model_;
  SelModelP sm_;
  DelegateP delegate_;

  bool freezeFirstColumn_  { false };
  bool stretchLastColumn_  { false };
  bool multiHeaderLines_   { false };
  bool showVerticalHeader_ { true };
  bool showFilter_         { false };

  bool sortingEnabled_     { false };

  bool         showGrid_         { false };
  bool         showHHeaderLines_ { true };
  bool         showVHeaderLines_ { true };
  Qt::PenStyle gridStyle_        { Qt::SolidLine };

  int  indentation_          { 20 };
  bool rootIsDecorated_      { true };
  bool expandsOnDoubleClick_ { true };

  CQModelViewCornerButton *cornerWidget_ { nullptr };

  GlobalColumnData  globalColumnData_; // global column data
  ColumnDatas       columnDatas_;      // per column data

  GlobalRowData     globalRowData_;    // global row data
  RowDatas          rowDatas_;         // per flat row data
  IndRow            indRow_;           // index to visual row
  RowColumnSpans    rowColumnSpans_;   // per header row column spans

  State             state_;            // state

  ScrollData        scrollData_;       // scroll data (updateScrollBars)
  VisColumnDatas    visColumnDatas_;   // vis column data (updateVisibleColumns)
  VisRowDatas       visRowDatas_;      // vis row data (updateVisRows)
  VisFlatRows       visFlatRows_;      // visible rows (flat index)
  VisCellDatas      visCellDatas_;     // vis cell data (updateVisCells)
  VisCellDatas      ivisCellDatas_;    // vis tree cell data (updateVisCells)
  int               currentFlatRow_ { -1 };

  DepthHierCellAreas selDepthHierCellAreas_;

  mutable PaintData paintData_;
  MouseData         mouseData_;
  FilterEdits       filterEdits_;
  bool              hierarchical_ { false };
  int               freezeColumn_ { -1 };
  int               freezeWidth_  { 0 };

  IndexSet expanded_;
  bool     ignoreExpanded_ { false };

  int numRedraws_ { 0 };

  // widgets
  CQModelViewHeader* vh_ { nullptr };
  CQModelViewHeader* hh_ { nullptr };
  QScrollBar*        hs_ { nullptr };
  QScrollBar*        vs_ { nullptr };

  QItemSelectionModel *vsm_ { nullptr };
  QItemSelectionModel *hsm_ { nullptr };

  bool autoFitOnShow_ { true };
  bool autoFitted_    { false };

  // draw data
  int   nr_  { 0 };
  int   nc_  { 0 };
  int   nmr_ { 0 };
  int   nvr_ { 0 };
  int   nvc_ { 0 };
  QRect visualRect_;
  int   visualBorderRows_ { 3 };
  QRect paintRect_;
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
