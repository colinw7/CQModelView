#include <CQModelView.h>
#include <CQBaseModelTypes.h>
#include <CQItemDelegate.h>

#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QScrollBar>
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>

#include <set>
#include <iostream>
#include <cassert>

//---

CQModelView::
CQModelView(QWidget *parent) :
 QAbstractItemView(parent), fm_(font())
{
  setObjectName("modelView");

  //---

  // headers
  hh_ = new CQModelViewHeader(Qt::Horizontal, this);
  vh_ = new CQModelViewHeader(Qt::Vertical  , this);

  hsm_ = new QItemSelectionModel;
  vsm_ = new QItemSelectionModel;

  hh_->setVisible(true);
  vh_->setVisible(true);

  //---

  // scrollbars
  hs_ = horizontalScrollBar();
  vs_ = verticalScrollBar  ();

  connect(hs_, SIGNAL(valueChanged(int)), this, SLOT(hscrollSlot(int)));
  connect(vs_, SIGNAL(valueChanged(int)), this, SLOT(vscrollSlot(int)));

  //---

  setMouseTracking(true);

  setTabKeyNavigation(true);

  setContextMenuPolicy(Qt::DefaultContextMenu);

  // space for headers
  setViewportMargins(globalColumnData_.headerWidth, globalRowData_.headerHeight, 0, 0);

  //---

  setSelectionModel(new CQModelViewSelectionModel(this));

  //---

  cornerWidget_ = new CQModelViewCornerButton(this);

  connect(cornerWidget_, SIGNAL(clicked()), this, SLOT(selectAll()));
}

QAbstractItemModel *
CQModelView::
model() const
{
  return model_.data();
}

void
CQModelView::
setModel(QAbstractItemModel *model)
{
  if (model == model_)
    return;

  // disconnect from old model
  if (model_) {
    disconnect(model_, SIGNAL(rowsInserted(QModelIndex,int,int)),
               this, SLOT(modelChangedSlot()));
    disconnect(model_, SIGNAL(columnsInserted(QModelIndex,int,int)),
               this, SLOT(modelChangedSlot()));
    disconnect(model_, SIGNAL(rowsRemoved(QModelIndex,int,int)),
               this, SLOT(modelChangedSlot()));
    disconnect(model_, SIGNAL(columnsRemoved(QModelIndex,int,int)),
               this, SLOT(modelChangedSlot()));
  }

  model_ = model;

  // connect to new model
  if (model_) {
    connect(model_, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(modelChangedSlot()));
    connect(model_, SIGNAL(columnsInserted(QModelIndex,int,int)),
            this, SLOT(modelChangedSlot()));
    connect(model_, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SLOT(modelChangedSlot()));
    connect(model_, SIGNAL(columnsRemoved(QModelIndex,int,int)),
            this, SLOT(modelChangedSlot()));
  }

  //---

  QAbstractItemView::setModel(model_);

  if (sm_ && model_) {
    sm_->setModel(model_);

    QAbstractItemView::setSelectionModel(sm_);
  }

  //---

  hh_->setModel(model_);
  vh_->setModel(model_);

  if (model_) {
    hsm_->setModel(model_);
    vsm_->setModel(model_);

    hh_->setSelectionModel(hsm_);
    hh_->setSelectionModel(hsm_);
  }

  //---

  // connect model
  // update cache
  state_.updateAll();

  redraw();
}

QItemSelectionModel *
CQModelView::
selectionModel() const
{
  return sm_.data();
}

void
CQModelView::
setSelectionModel(QItemSelectionModel *sm)
{
  // ignore auto model
  if (! qobject_cast<CQModelViewSelectionModel *>(sm)) {
    delete sm;
    return;
  }

  sm_ = sm;

  if (model_) {
    sm_->setModel(model_);

    QAbstractItemView::setSelectionModel(sm_);
  }

  // TODO: connect signals

  redraw();
}

QAbstractItemDelegate *
CQModelView::
itemDelegate() const
{
  return delegate_.data();
}

void
CQModelView::
setItemDelegate(QAbstractItemDelegate *delegate)
{
  delegate_ = delegate;

  QAbstractItemView::setItemDelegate(delegate_);

  // TODO: connect signals

  redraw();
}

QHeaderView *
CQModelView::
horizontalHeader() const
{
  return hh_;
}

QHeaderView *
CQModelView::
verticalHeader() const
{
  return vh_;
}

int
CQModelView::
rowHeight(int) const
{
  // all rows the same height
  return globalRowData_.height;
}

int
CQModelView::
columnWidth(int column) const
{
  const_cast<CQModelView *>(this)->updateVisColumns();

  //---

  auto p = visColumnDatas_.find(column);
  if (p == visColumnDatas_.end()) return 0;

  const VisColumnData &visColumnData = (*p).second;

  bool isLast = visColumnData.last;

  if (isLast && isStretchLastColumn()) {
    return visColumnData.rect.width();
  }
  else {
    const ColumnData &columnData = columnDatas_[column];

    int cw = (columnData.width > 0 ? columnData.width : 100);

    return cw;
  }
}

void
CQModelView::
setColumnWidth(int column, int width)
{
  ColumnData &columnData = columnDatas_[column];

  columnData.width = width;

  int cw = (columnData.width > 0 ? columnData.width : 100);

  hh_->resizeSection(column, cw);

  state_.updateAll();

  redraw();
}

bool
CQModelView::
columnHeatmap(int column) const
{
  const ColumnData &columnData = columnDatas_[column];

  return columnData.heatmap;
}

void
CQModelView::
setColumnHeatmap(int column, bool heatmap)
{
  ColumnData &columnData = columnDatas_[column];

  columnData.heatmap = heatmap;

  redraw();
}

//---

void
CQModelView::
setSortingEnabled(bool b)
{
  sortingEnabled_ = b;

  hh_->setSortIndicatorShown(sortingEnabled_);

  if (sortingEnabled_)
    sortByColumn(hh_->sortIndicatorSection(), hh_->sortIndicatorOrder());

  redraw();
}

void
CQModelView::
sortByColumn(int column, Qt::SortOrder order)
{
  hh_->setSortIndicator(column, order);

  sortByColumn(column);
}

void
CQModelView::
sortByColumn(int column)
{
  if (column < 0 || column >= nc_)
    return;

  model()->sort(column, hh_->sortIndicatorOrder());
}

//---

bool
CQModelView::
isCornerButtonEnabled() const
{
  return cornerWidget_->isEnabled();
}

void
CQModelView::
setCornerButtonEnabled(bool enable)
{
  cornerWidget_->setEnabled(enable);

  redraw();
}

//---

void
CQModelView::
setShowFilter(bool b)
{
  showFilter_ = b;

  state_.updateGeometries = true;

  state_.updateAll();

  redraw();
}

//---

void
CQModelView::
resizeEvent(QResizeEvent *)
{
  state_.updateAll();
}

void
CQModelView::
modelChangedSlot()
{
  state_.updateAll();

  redraw();
}

// update geometry
// depends
//   font, margins, header sizes, filter, viewport size, scrollbars, visible columns
void
CQModelView::
updateGeometries()
{
  if (! state_.updateGeometries)
    return;

  state_.updateGeometries = false;

  //---

  int fh = fm_.height();

  globalColumnData_.headerWidth  = fm_.width("X") + 4;
  globalRowData_   .headerHeight = fh + globalRowData_.headerMargin;
  globalRowData_   .height       = fh + globalRowData_.margin;

  // space for headers
  int filterHeight = (isShowFilter() ? fh + 8 : 0);

  setViewportMargins(globalColumnData_.headerWidth,
                     globalRowData_.headerHeight + filterHeight, 0, 0);

  //---

  QRect vrect = viewport()->geometry();

  //---

  cornerWidget_->setGeometry(0, 0, globalColumnData_.headerWidth, globalRowData_.headerHeight);

  //---

  hh_->setGeometry(vrect.left(), vrect.top() - globalRowData_.headerHeight,
                   vrect.width(), globalRowData_.headerHeight);
  vh_->setGeometry(vrect.left() - globalColumnData_.headerWidth, vrect.top(),
                   globalColumnData_.headerWidth, vrect.height());

  //---

  updateScrollBars();
  updateVisColumns();

  //---

  int nfe = filterEdits_.size();

  while (nfe > nvc_) {
    delete filterEdits_.back();

    filterEdits_.pop_back();

    --nfe;
  }

  while (nfe < nvc_) {
    CQModelViewFilterEdit *le = new CQModelViewFilterEdit(this);
    le->setObjectName(QString("filterEdit%1").arg(nfe));

    connect(le, SIGNAL(editingFinished()), this, SLOT(editFilterSlot()));

    filterEdits_.push_back(le);

    ++nfe;
  }

  if (isShowFilter()) {
    int x = vrect.left();

    int ic = 0;

    for (const auto &vp : visColumnDatas_) {
      int c = vp.first;

      CQModelViewFilterEdit *le = filterEdits_[ic];

      le->setColumn(c);

      int cw = columnWidth(c);

      le->setGeometry(x, 0, cw, filterHeight);

      le->setVisible(true);

      x += cw;

      ++ic;
    }
  }
  else {
    for (int c = 0; c < nvc_; ++c) {
      CQModelViewFilterEdit *le = filterEdits_[c];

      le->setVisible(false);
    }
  }
}

// depends:
//  number of rows, row height, column width, row hidden, column hidden, viewport height
void
CQModelView::
updateScrollBars()
{
  if (! state_.updateScrollBars)
    return;

  state_.updateScrollBars = false;

  //---

  nvr_ = 0;

  QModelIndex parent;

  numVisibleRows(parent, nvr_);

  //---

  int rowHeight = this->rowHeight(0);

  int w = 0;
  int h = nvr_*rowHeight;

  nvc_ = 0;

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    ++nvc_;

    const ColumnData &columnData = columnDatas_[c];

    int cw = (columnData.width > 0 ? columnData.width : 100);

    w += cw;
  }

  //---

  int ah = viewport()->height();

  if (h > ah) {
    scrollData_.nv = (ah + rowHeight - 1)/rowHeight;

    vs_->setPageStep(scrollData_.nv);
    vs_->setRange(0, nvr_ - vs_->pageStep());

    vs_->setVisible(true);
  }
  else {
    scrollData_.nv = -1;

    vs_->setVisible(false);
  }

  //---

  int aw = viewport()->width();

  if (w > aw) {
    hs_->setPageStep(aw);
    hs_->setRange(0, w - hs_->pageStep());

    hs_->setVisible(true);
  }
  else {
    hs_->setVisible(false);
  }
}

void
CQModelView::
numVisibleRows(const QModelIndex &parent, int &nvr)
{
  int nr = model_->rowCount(parent);

  for (int r = 0; r < nr; ++r) {
    if (isRowHidden(r, parent))
      continue;

    QModelIndex ind1 = model_->index(r, 0, parent);

    bool children = model_->hasChildren(ind1);
    bool expanded = children && isExpanded(ind1);

    rowDatas_[nvr]  = RowData(parent, r, nvr, children, expanded);
    indRow_  [ind1] = nvr;

    ++nvr;

    if (children) {
      numVisibleRows(ind1, nvr);
    }
  }
}

void
CQModelView::
updateCurrentIndices()
{
  QModelIndex ind = sm_->currentIndex();

  if (ind.isValid()) {
    QModelIndex hind = model_->index(0, ind.column(), QModelIndex());
    QModelIndex vind = model_->index(ind.row   (), 0, ind.parent());

    hsm_->setCurrentIndex(hind, QItemSelectionModel::NoUpdate);
    vsm_->setCurrentIndex(vind, QItemSelectionModel::NoUpdate);
  }
  else {
    hsm_->clearCurrentIndex();
    vsm_->clearCurrentIndex();
  }

  redraw();
}

void
CQModelView::
updateSelection()
{
  const QItemSelection selection = sm_->selection();

  std::set<int>         columns;
  std::set<QModelIndex> rows;

  for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
    const QItemSelectionRange &range = *it;

    for (int c = range.left(); c <= range.right(); ++c)
      columns.insert(c);

    for (int r = range.top(); r <= range.bottom(); ++r) {
      QModelIndex ind = model_->index(r, 0, range.parent());

      rows.insert(ind);
    }
  }

  QItemSelection hselection, vselection;

  for (auto &c : columns) {
    QModelIndex hind = model_->index(0, c, QModelIndex());

    hselection.select(hind, hind);
  }

  for (auto &r : rows)
    vselection.select(r, r);

  hsm_->select(hselection, QItemSelectionModel::ClearAndSelect);
  vsm_->select(vselection, QItemSelectionModel::ClearAndSelect);

  redraw();
}

void
CQModelView::
paintEvent(QPaintEvent *)
{
  QPainter painter(this->viewport());

  QColor c = palette().color(QPalette::Window);

  painter.fillRect(viewport()->rect(), c);

  //---

  fm_ = QFontMetrics(font());

  //---

  int nr = nr_;
  int nc = nc_;

  if (model_) {
    nr_ = model_->rowCount   ();
    nc_ = model_->columnCount();
  }
  else {
    nr_ = 0;
    nc_ = 0;
  }

  if (nc_ != int(columnDatas_.size())) {
    columnDatas_.resize(nc_);
  }

  //---

  // TODO: needed, model signals should handle this
  if (! state_.updateScrollBars)
    state_.updateScrollBars = (nc_ != nc || nr_ !=  nr);

  //---

  hierarchical_ = false;

  drawCells(&painter);
}

void
CQModelView::
drawCells(QPainter *painter)
{
  updateScrollBars();
  updateVisColumns();
  updateGeometries();

  //---

  visCellDatas_ .clear();
  ivisCellDatas_.clear();

  //---

  paintData_.rowHeight = this->rowHeight(0);

  //---

  // draw rows
  QModelIndex parent;

  paintData_.y = -verticalOffset()*paintData_.rowHeight;

  paintData_.depth    = 0;
  paintData_.maxDepth = 0;

  for (int r = 0; r < nr_; ++r) {
    if (isRowHidden(r, parent))
      continue;

    drawRow(painter, r, parent);

    paintData_.y += paintData_.rowHeight;
  }

  //---

  // draw selection
  auto drawSelection = [&](const QRect &r) {
    setRolePen  (painter, ColorRole::SelectionFg, 0.7);
    setRoleBrush(painter, ColorRole::SelectionBg, 0.3);

    painter->fillRect(r, painter->brush());

    setRoleBrush(painter, ColorRole::None);

    painter->drawRect(r.adjusted(0, 0, -1, -1));
  };

  const QItemSelection selection = sm_->selection();

  for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
    const QItemSelectionRange &range = *it;

    const VisColumnData &visColumnData1 = visColumnDatas_[range.left ()];
    const VisColumnData &visColumnData2 = visColumnDatas_[range.right()];

    const VisRowData &visRowData1 = visRowDatas_[range.parent()][range.top   ()];
    const VisRowData &visRowData2 = visRowDatas_[range.parent()][range.bottom()];

    int x1 = visColumnData1.rect.left  ();
    int x2 = visColumnData2.rect.right ();
    int y1 = visRowData1   .rect.top   ();
    int y2 = visRowData2   .rect.bottom();

    QRect r(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

    drawSelection(r);
  }
}

void
CQModelView::
drawHHeader(QPainter *painter)
{
  updateScrollBars();
  updateVisColumns();
  updateGeometries();

  //---

  paintData_.reset();

  paintData_.w         = viewport()->width();
  paintData_.margin    = style()->pixelMetric(QStyle::PM_HeaderMargin, 0, hh_);
  paintData_.rowHeight = this->rowHeight(0);

  //---

  painter->fillRect(viewport()->rect(), roleColor(ColorRole::HeaderBg));

  //---

  int freezeWidth = 0;

  if (nc_ > 1 && isFreezeFirstColumn()) {
    if (! isColumnHidden(0))
      freezeWidth = columnWidth(0);
  }

  painter->save();

  int x = -horizontalOffset();

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    ColumnData &columnData = columnDatas_[c];

    bool valid = (! isFreezeFirstColumn() || c != 0);

    int cw = (columnData.width > 0 ? columnData.width : 100);

    if (valid) {
      if (isFreezeFirstColumn()) {
        const VisColumnData &visColumnData = visColumnDatas_[c];

        bool isLast = visColumnData.last;

        int x1 = std::max(x, freezeWidth);
        int x2 = (! isLast ? std::max(x + cw, freezeWidth) : paintData_.w - 1);

        if (x1 < x2)
          painter->setClipRect(QRect(x1, 0, x2 - x1 + 1, globalRowData_.headerHeight));
        else
          valid = false;
      }
    }

    if (valid)
      drawHHeaderSection(painter, c, x);

    x += cw;
  }

  //---

  if (freezeWidth > 0) {
    if (! isColumnHidden(0)) {
      painter->setClipRect(QRect(0, 0, freezeWidth, globalRowData_.headerHeight));

      drawHHeaderSection(painter, 0, 0);
    }
  }

  painter->restore();
}

void
CQModelView::
drawHHeaderSection(QPainter *painter, int c, int x)
{
  QModelIndex ind = model_->index(0, c, QModelIndex());

  //---

  ColumnData &columnData = columnDatas_[c];

  VisColumnData &visColumnData = visColumnDatas_[c];

  bool isLast = visColumnData.last;

  QVariant data = model_->headerData(c, Qt::Horizontal, Qt::DisplayRole);

  QString str = data.toString();

  int cw = (columnData.width > 0 ? columnData.width : 100);

  int xl = x + paintData_.margin;
  int xr = xl + cw - 2*paintData_.margin - 1;

  if (isLast && isStretchLastColumn() && x + cw < paintData_.w)
    xr = paintData_.w - 2*paintData_.margin - 1;

  visColumnData.rect  = QRect(xl, 0, xr - xl + 1, globalRowData_.headerHeight);
  visColumnData.hrect = QRect(xr, 0, 2*paintData_.margin, globalRowData_.headerHeight);

  //---

  bool isNumeric = isNumericColumn(c);

  //---

  // init style data
  QStyleOptionHeader option;

  option.state = QStyle::State_Horizontal;

  if (c == mouseData_.moveData.hsection)
    option.state |= QStyle::State_MouseOver;

  if (c == mouseData_.pressData.hsection)
    option.state |= QStyle::State_Sunken;

  if (hsm_->isSelected(ind))
    option.state |= QStyle::State_Selected;

  if (c == hsm_->currentIndex().column())
    option.state |= QStyle::State_HasFocus;

  if (hh_->isSortIndicatorShown() && hh_->sortIndicatorSection() == c)
    option.sortIndicator = (hh_->sortIndicatorOrder() == Qt::AscendingOrder ?
      QStyleOptionHeader::SortDown : QStyleOptionHeader::SortUp);

  option.rect          = visColumnData.rect;
  option.iconAlignment = Qt::AlignCenter;
  option.section       = c;
  option.text          = str;
  option.textAlignment = (isNumeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
  option.orientation   = Qt::Horizontal;

  //---

  // get decoration icon
  QVariant ivar = model_->headerData(c, Qt::Horizontal, Qt::DecorationRole);

  option.icon = qvariant_cast<QIcon>(ivar);

  if (option.icon.isNull())
    option.icon = qvariant_cast<QPixmap>(ivar);

  //---

  option.position         = QStyleOptionHeader::Middle; // TODO
  option.selectedPosition = QStyleOptionHeader::NotAdjacent; // TODO

  //---

  auto initBrush = [&]() {
    if (option.state & QStyle::State_MouseOver)
      setRoleBrush(painter, ColorRole::MouseOverBg);
    else
      setRoleBrush(painter, ColorRole::HeaderBg);
  };

  auto initPen = [&]() {
    if (option.state & QStyle::State_MouseOver)
      setRolePen(painter, ColorRole::MouseOverFg);
    else
      setRolePen(painter, ColorRole::HeaderFg);
  };

  auto fillBackground = [&]() {
    initBrush();

    painter->fillRect(option.rect, painter->brush());
  };

  auto drawFocus = [&]() {
    setRolePen  (painter, ColorRole::HighlightBg);
    setRoleBrush(painter, ColorRole::None);

    int x1 = option.rect.left  ();
    int x2 = option.rect.right ();
    int y  = option.rect.bottom();

    for (int i = 0; i < 3; ++i)
      painter->drawLine(x1, y - i, x2, y - i);
  };

  //---

  fillBackground();

  //---

  QStyleOptionHeader subopt = option;

  subopt.rect = style()->subElementRect(QStyle::SE_HeaderLabel, &option, hh_);

  initPen();

  painter->drawText(subopt.rect, option.textAlignment, option.text);

  //---

  if (option.sortIndicator != QStyleOptionHeader::None) {
    QStyleOptionHeader subopt = option;

    subopt.rect = style()->subElementRect(QStyle::SE_HeaderArrow, &option, hh_);

    style()->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &subopt, painter, hh_);
  }

  //---

  QRect fullRect = visColumnData.rect.adjusted(-paintData_.margin, 0, paintData_.margin, 0);

  setRolePen(painter, ColorRole::HeaderFg);

  painter->drawLine(fullRect.topLeft   (), fullRect.topRight   ());
  painter->drawLine(fullRect.bottomLeft(), fullRect.bottomRight());

  //---

  if (option.state & QStyle::State_HasFocus)
    drawFocus();

  //---

  if (c == mouseData_.pressData.hsectionh || c == mouseData_.moveData.hsectionh) {
    setRolePen(painter, ColorRole::MouseOverFg);

    int x  = fullRect.right();
    int y1 = fullRect.top();
    int y2 = fullRect.bottom();

    for (int i = 0; i < 3; ++i)
      painter->drawLine(x - i, y1, x - i, y2);
  }

  //---

  auto drawSelection = [&]() {
    setRolePen  (painter, ColorRole::SelectionFg, 0.3);
    setRoleBrush(painter, ColorRole::SelectionBg, 0.3);

    painter->fillRect(fullRect, painter->brush());
  };

  if (option.state & QStyle::State_Selected)
    drawSelection();
}

void
CQModelView::
drawVHeader(QPainter *painter)
{
  updateScrollBars();
  updateVisColumns();
  updateGeometries();

  //---

  paintData_.reset();

  paintData_.rowHeight = this->rowHeight(0);
  paintData_.vh        = viewport()->height();

  //---

  visRowDatas_.clear();

  //---

  int ir = 0;

  paintData_.y = -verticalOffset()*paintData_.rowHeight;

  auto p = indRow_.find(vsm_->currentIndex());

  int currentRow = (p != indRow_.end() ? (*p).second : -1);

  for (const auto &pr : rowDatas_) {
    const RowData &rowData = pr.second;

    VisRowData &visRowData = visRowDatas_[rowData.parent][rowData.row];

    visRowData.rect = QRect(0, paintData_.y, globalColumnData_.headerWidth, paintData_.rowHeight);

    visRowData.ir        = ir;
    visRowData.alternate = (ir & 1);

    //---

    bool visible = (visRowData.rect.bottom() > 0 && visRowData.rect.top() <= paintData_.vh);

    //---

    if (visible) {
      QModelIndex ind = model_->index(rowData.row, 0, rowData.parent);

      //---

      // init style data
      QStyleOptionHeader option;

      if (rowData.flatRow == mouseData_.moveData.vsection)
        option.state |= QStyle::State_MouseOver;

      if (vsm_->isSelected(ind))
        option.state |= QStyle::State_Selected;

      if (ir == currentRow)
        option.state |= QStyle::State_HasFocus;

      option.rect             = visRowData.rect;
      option.icon             = QIcon();
      option.iconAlignment    = Qt::AlignCenter;
      option.position         = QStyleOptionHeader::Middle; // TODO
      option.section          = rowData.flatRow;
      option.selectedPosition = QStyleOptionHeader::NotAdjacent; // TODO
      option.text             = "";
      option.textAlignment    = Qt::AlignLeft | Qt::AlignVCenter;

      //---

      auto fillBackground = [&]() {
        if (option.state & QStyle::State_MouseOver) {
          setRolePen  (painter, ColorRole::MouseOverFg);
          setRoleBrush(painter, ColorRole::MouseOverBg);
        }
        else {
          setRolePen  (painter, ColorRole::HeaderFg);
          setRoleBrush(painter, ColorRole::HeaderBg);
        }

        painter->fillRect(option.rect, painter->brush());

        setRoleBrush(painter, ColorRole::Window);
      };

      auto drawFocus = [&]() {
        setRolePen  (painter, ColorRole::HighlightBg);
        setRoleBrush(painter, ColorRole::None);

        int x  = option.rect.right ();
        int y1 = option.rect.top   ();
        int y2 = option.rect.bottom();

        for (int i = 0; i < 3; ++i)
          painter->drawLine(x - i, y1, x - i, y2);
      };

      //---

      fillBackground();

      //---

      setRolePen(painter, ColorRole::HeaderFg);

      painter->drawLine(option.rect.topRight(), option.rect.bottomRight());

      //---

      if (option.state & QStyle::State_HasFocus)
        drawFocus();

      //---

      auto drawSelection = [&]() {
        setRolePen  (painter, ColorRole::SelectionFg, 0.3);
        setRoleBrush(painter, ColorRole::SelectionBg, 0.3);

        painter->fillRect(option.rect, painter->brush());
      };

      if (option.state & QStyle::State_Selected)
        drawSelection();
    }

    //---

    paintData_.y += paintData_.rowHeight;

    ++ir;
  }
}

void
CQModelView::
drawRow(QPainter *painter, int r, const QModelIndex &parent)
{
  paintData_.reset();

  if (iconSize().isValid()) {
    paintData_.decorationSize = iconSize();
  }
  else {
    int pm = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);

    paintData_.decorationSize = QSize(pm, pm);
  }

  paintData_.showDecorationSelected =
    style()->styleHint(QStyle::SH_ItemView_ShowDecorationSelected, 0, this);

  paintData_.w = viewport()->width();

  //---

  QModelIndex ind1 = model_->index(r, 0, parent);

  bool children = model_->hasChildren(ind1);
  if (children) hierarchical_ = true;

  //---

  const VisRowData &visRowData = visRowDatas_[parent][r];

  //---

  int freezeWidth = 0;

  if (nc_ > 1 && isFreezeFirstColumn()) {
    if (! isColumnHidden(0))
      freezeWidth = columnWidth(0);
  }

  painter->save();

  int x = -horizontalOffset();

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    ColumnData &columnData = columnDatas_[c];

    bool valid = (! isFreezeFirstColumn() || c != 0);

    int cw = (columnData.width > 0 ? columnData.width : 100);

    if (valid) {
      if (isFreezeFirstColumn()) {
        const VisColumnData &visColumnData = visColumnDatas_[c];

        bool isLast = visColumnData.last;

        int x1 = std::max(x, freezeWidth);
        int x2 = (! isLast ? std::max(x + cw, freezeWidth) : paintData_.w - 1);

        if (x1 < x2)
          painter->setClipRect(QRect(x1, paintData_.y, x2 - x1 + 1, paintData_.rowHeight));
        else
          valid = false;
      }
    }

    if (valid)
      drawCell(painter, r, c, parent, x, visRowData);

    x += cw;
  }

  //---

  if (freezeWidth > 0) {
    if (! isColumnHidden(0)) {
      painter->setClipRect(QRect(0, paintData_.y, freezeWidth, paintData_.rowHeight));

      drawCell(painter, r, 0, parent, 0, visRowData);
    }
  }

  painter->restore();

  //---

  if (children && isExpanded(ind1)) {
    ++paintData_.depth;

    paintData_.maxDepth = std::max(paintData_.maxDepth, paintData_.depth);

    int nr1 = model_->rowCount(ind1);

    for (int r1 = 0; r1 < nr1; ++r1) {
      paintData_.y += paintData_.rowHeight;

      drawRow(painter, r1, ind1);
    }

    --paintData_.depth;
  }
}

void
CQModelView::
drawCell(QPainter *painter, int r, int c, const QModelIndex &parent, int x,
         const VisRowData &visRowData)
{
  QModelIndex ind = model_->index(r, c, parent);

  ColumnData &columnData = columnDatas_[c];

  const VisColumnData &visColumnData = visColumnDatas_[c];

  bool isLast = visColumnData.last;

  //--

  int cw = (columnData.width > 0 ? columnData.width : 100);

  int x1 = x;
  int x2 = x + cw - 1;

  if (hierarchical_ && c == 0) {
    int indent = (paintData_.depth + 1)*indentation();

    //---

    VisCellData &visCellData = ivisCellDatas_[ind];

    visCellData.rect = QRect(x, paintData_.y, indent, paintData_.rowHeight);

    //---

    bool children     = model_->hasChildren(ind);
    bool expanded     = isExpanded(ind);
    bool moreSiblings = false;

    QStyleOptionViewItem opt;

    opt.rect = visCellData.rect;

    opt.state = (moreSiblings ? QStyle::State_Sibling  : QStyle::State_None) |
                (children     ? QStyle::State_Children : QStyle::State_None) |
                (expanded     ? QStyle::State_Open     : QStyle::State_None);

    if (ind == mouseData_.moveData.iind)
      opt.state |= QStyle::State_MouseOver;

    if (alternatingRowColors() && visRowData.alternate)
      opt.features |= QStyleOptionViewItem::Alternate;
    else
      opt.features &= ~QStyleOptionViewItem::Alternate;

    //---

    auto fillIndicatorBackground = [&]() {
      if (opt.state & QStyle::State_MouseOver) {
        setRolePen  (painter, ColorRole::MouseOverFg);
        setRoleBrush(painter, ColorRole::MouseOverBg);
      }
      else {
        setRolePen(painter, ColorRole::Text);

        if (opt.features & QStyleOptionViewItem::Alternate)
          setRoleBrush(painter, ColorRole::AlternateBg);
        else
          setRoleBrush(painter, ColorRole::Window);
      }

      painter->fillRect(opt.rect, painter->brush());

      if (opt.features & QStyleOptionViewItem::Alternate)
        setRoleBrush(painter, ColorRole::AlternateBg);
      else
        setRoleBrush(painter, ColorRole::Window);
    };

    //---

    fillIndicatorBackground();

    style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, painter, this);

    //---

    x1 += indent;
  }

  //--

  VisCellData &visCellData = visCellDatas_[ind];

  if (isLast && isStretchLastColumn() && x2 < paintData_.w)
    x2 = paintData_.w - 1;

  visCellData.rect = QRect(x1, paintData_.y, x2 - x1 + 1, paintData_.rowHeight);

  //---

  bool visible = (visCellData.rect.bottom() > 0 && visCellData.rect.top() <= paintData_.vh);

  if (! visible)
    return;

  //---

  bool isNumeric = isNumericColumn(c);

  //--

  // init style data
  QStyleOptionViewItem option;

  option.init(this);

  option.state &= ~QStyle::State_MouseOver;

  if (ind == mouseData_.moveData.ind)
    option.state |= QStyle::State_MouseOver;

  if (ind == currentIndex())
    option.state |= QStyle::State_HasFocus;
  else
    option.state &= ~QStyle::State_HasFocus;

  if (alternatingRowColors() && visRowData.alternate)
    option.features |= QStyleOptionViewItem::Alternate;
  else
    option.features &= ~QStyleOptionViewItem::Alternate;

  option.widget                 = this;
  option.rect                   = visCellData.rect;
  option.font                   = font();
  option.decorationSize         = paintData_.decorationSize;
  option.decorationPosition     = QStyleOptionViewItem::Left;
  option.decorationAlignment    = Qt::AlignCenter;
  option.displayAlignment       = (isNumeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
  option.textElideMode          = textElideMode();
  option.showDecorationSelected = paintData_.showDecorationSelected;

  //---

  auto fillBackground = [&]() {
    if (option.state & QStyle::State_MouseOver) {
      setRolePen  (painter, ColorRole::MouseOverFg);
      setRoleBrush(painter, ColorRole::MouseOverBg);
    }
    else {
      setRolePen(painter, ColorRole::Text);

      if (option.features & QStyleOptionViewItem::Alternate)
        setRoleBrush(painter, ColorRole::AlternateBg);
      else
        setRoleBrush(painter, ColorRole::Window);
    }

    painter->fillRect(option.rect, painter->brush());

    if (option.features & QStyleOptionViewItem::Alternate)
      setRoleBrush(painter, ColorRole::AlternateBg);
    else
      setRoleBrush(painter, ColorRole::Window);
  };

  //---

  if (! delegate_) {
    fillBackground();

    // draw cell value
    setRolePen(painter, ColorRole::HeaderFg);

    QVariant data = model_->data(ind, Qt::DisplayRole);

    QString str = data.toString();

    painter->drawText(option.rect.adjusted(4, 0, -4, 0), option.displayAlignment, str);
  }
  else {
    //style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &option, painter, this);

    fillBackground();

    CQItemDelegate *idelegate = qobject_cast<CQItemDelegate *>(delegate_);

    if (idelegate)
      idelegate->setHeatmap(columnData.heatmap);

    delegate_->paint(painter, option, ind);
  }
}

void
CQModelView::
updateVisColumns()
{
  if (! state_.updateVisColumns)
    return;

  state_.updateVisColumns = false;

  //---

  nvc_ = 0;

  int lastC = -1;

  visColumnDatas_.clear();

  int x = -horizontalOffset();

  int margin = style()->pixelMetric(QStyle::PM_HeaderMargin, 0, hh_);

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    const ColumnData &columnData = columnDatas_[c];

    int cw = (columnData.width > 0 ? columnData.width : 100);

    VisColumnData &visColumnData = visColumnDatas_[c];

    int xl = x + margin;
    int xr = xl + cw - margin - 1;

    visColumnData.rect  = QRect(xl, 0, xr - xl + 1, globalRowData_.headerHeight);
    visColumnData.hrect = QRect(xr, 0, 2*margin, globalRowData_.headerHeight);

    visColumnData.first = (nvc_ == 0);

    ++nvc_;

    lastC = c;
  }

  if (lastC > 0) {
    VisColumnData &visColumnData = visColumnDatas_[lastC];

    visColumnData.last = true;

    if (isStretchLastColumn()) {
      int xr = visColumnData.rect.right();

      if (xr + margin < paintData_.w) {
        int xl = visColumnData.rect.left();

        xr = paintData_.w - paintData_.margin - 1;

        visColumnData.rect  = QRect(xl, 0, xr - xl + 1, globalRowData_.headerHeight);
        visColumnData.hrect = QRect(xr, 0, 2*margin, globalRowData_.headerHeight);
      }
    }
  }
}

bool
CQModelView::
isNumericColumn(int c) const
{
  CQBaseModelType type = CQBaseModelType::STRING;

  QVariant tvar = model_->headerData(c, Qt::Horizontal, (int) CQBaseModelRole::Type);
  if (! tvar.isValid()) return false;

  bool ok;
  type = (CQBaseModelType) tvar.toInt(&ok);
  if (! ok) return false;

  //---

  return (type == CQBaseModelType::REAL || type == CQBaseModelType::INTEGER);
}

void
CQModelView::
editFilterSlot()
{
  CQModelViewFilterEdit *le = qobject_cast<CQModelViewFilterEdit *>(sender());
  assert(le);

  int column = le->column();

  QModelIndex parent;

  QString filterStr = le->text().simplified();

  if (filterStr.length()) {
    for (int r = 0; r < nr_; ++r) {
      QModelIndex ind = model_->index(r, column, parent);

      QVariant data = model_->data(ind, Qt::DisplayRole);

      bool visible = (data.toString() == filterStr);

      setRowHidden(r, parent, ! visible);
    }
  }
  else {
    for (int r = 0; r < nr_; ++r) {
      setRowHidden(r, parent, false);
    }
  }

  state_.updateScrollBars = true;

  redraw();
}

void
CQModelView::
hscrollSlot(int v)
{
  scrollData_.hpos = v;

  redraw();
}

void
CQModelView::
vscrollSlot(int v)
{
  scrollData_.vpos = v;

  redraw();
}

void
CQModelView::
mousePressEvent(QMouseEvent *e)
{
  mouseData_.pressed   = true;
  mouseData_.modifiers = e->modifiers();

  if (e->button() != Qt::LeftButton)
    return;

  mouseData_.pressData.pos = e->pos();

  cellPositionToIndex(mouseData_.pressData);

  handleMousePress();
}

void
CQModelView::
handleMousePress()
{
  if      (mouseData_.pressData.hsection >= 0) {
    QItemSelection selection;

    QModelIndex ind = model_->index(0, mouseData_.pressData.hsection, QModelIndex());

    selection.select(ind, ind);

    if (! (mouseData_.modifiers & Qt::ControlModifier))
      sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Columns);
    else
      sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Columns);

    //--

    sm_->setCurrentIndex(ind, QItemSelectionModel::NoUpdate);

    //--

    redraw();
  }
  else if (mouseData_.pressData.hsectionh >= 0) {
    const ColumnData &columnData = columnDatas_[mouseData_.pressData.hsectionh];

    int cw = (columnData.width > 0 ? columnData.width : 100);

    mouseData_.headerWidth = cw;
  }
  else if (mouseData_.pressData.vsection >= 0) {
    QItemSelection selection;

    QModelIndex ind = model_->index(mouseData_.pressData.vsection, 0, QModelIndex());

    selection.select(ind, ind);

    if (! (mouseData_.modifiers & Qt::ControlModifier))
      sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    else
      sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    //--

    sm_->setCurrentIndex(ind, QItemSelectionModel::NoUpdate);

    redraw();
  }
  else if (mouseData_.pressData.ind.isValid()) {
    QItemSelection selection;

    selection.select(mouseData_.pressData.ind, mouseData_.pressData.ind);

    if (! (mouseData_.modifiers & Qt::ControlModifier))
      sm_->select(selection, QItemSelectionModel::ClearAndSelect);
    else
      sm_->select(selection, QItemSelectionModel::Select);

    //--

    sm_->setCurrentIndex(mouseData_.pressData.ind, QItemSelectionModel::NoUpdate);

    redraw();
  }
  else if (mouseData_.pressData.iind.isValid()) {
    setExpanded(mouseData_.pressData.iind, ! isExpanded(mouseData_.pressData.iind));
  }
}

void
CQModelView::
mouseMoveEvent(QMouseEvent *e)
{
  PositionData moveData;

  moveData.pos = e->pos();

  cellPositionToIndex(moveData);

  if (! mouseData_.pressed) {
    if (moveData != mouseData_.moveData) {
      mouseData_.moveData = moveData;

      redraw();
    }
  }
  else {
    mouseData_.moveData = moveData;

    handleMouseMove();
  }
}

void
CQModelView::
handleMouseMove()
{
  if      (mouseData_.pressData.hsection >= 0) {
    if (mouseData_.moveData.hsection < 0)
      return;

    int c1 = std::min(mouseData_.pressData.hsection, mouseData_.moveData.hsection);
    int c2 = std::max(mouseData_.pressData.hsection, mouseData_.moveData.hsection);

    QItemSelection selection;

    QModelIndex parent;

    QModelIndex ind1 = model_->index(0, c1, parent);
    QModelIndex ind2 = model_->index(0, c2, parent);

    selection.select(ind1, ind2);

    if (! (mouseData_.modifiers & Qt::ControlModifier))
      sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Columns);
    else
      sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Columns);

    redraw();
  }
  else if (mouseData_.pressData.hsectionh >= 0) {
    int dx = mouseData_.moveData.pos.x() - mouseData_.pressData.pos.x();

    ColumnData &columnData = columnDatas_[mouseData_.pressData.hsectionh];

    columnData.width = std::min(std::max(mouseData_.headerWidth + dx, 4), 9999);

    hh_->resizeSection(mouseData_.pressData.hsectionh, columnData.width);

    state_.updateAll();

    redraw();
  }
  else if (mouseData_.pressData.vsection >= 0) {
    if (mouseData_.moveData.vsection < 0)
      return;

    int r1 = std::min(mouseData_.pressData.vsection, mouseData_.moveData.vsection);
    int r2 = std::max(mouseData_.pressData.vsection, mouseData_.moveData.vsection);

    QItemSelection selection;

    QModelIndex parent;

    QModelIndex ind1 = model_->index(r1, 0, parent);
    QModelIndex ind2 = model_->index(r2, 0, parent);

    selection.select(ind1, ind2);

    if (! (mouseData_.modifiers & Qt::ControlModifier))
      sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    else
      sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    redraw();
  }
  else if (mouseData_.pressData.ind.isValid() && mouseData_.moveData .ind.isValid()) {
    // TODO: hierarchical will need more work
    int c1 = std::min(mouseData_.pressData.ind.column(), mouseData_.moveData.ind.column());
    int c2 = std::max(mouseData_.pressData.ind.column(), mouseData_.moveData.ind.column());

    int r1 = std::min(mouseData_.pressData.ind.row(), mouseData_.moveData.ind.row());
    int r2 = std::max(mouseData_.pressData.ind.row(), mouseData_.moveData.ind.row());

    QModelIndex parent;

    QModelIndex ind1 = model_->index(r1, c1, parent);
    QModelIndex ind2 = model_->index(r2, c2, parent);

    QItemSelection selection;

    selection.select(ind1, ind2);

    if (! (mouseData_.modifiers & Qt::ControlModifier))
      sm_->select(selection, QItemSelectionModel::ClearAndSelect);
    else
      sm_->select(selection, QItemSelectionModel::Select);

    //--

    sm_->setCurrentIndex(mouseData_.moveData.ind, QItemSelectionModel::NoUpdate);

    scrollTo(mouseData_.moveData.ind);

    redraw();
  }
}

void
CQModelView::
mouseReleaseEvent(QMouseEvent *e)
{
  if (e->button() != Qt::LeftButton)
    return;

  mouseData_.releaseData.pos = e->pos();

  mouseData_.pressed = false;

  mouseData_.pressData.reset();
  mouseData_.moveData .reset();

  cellPositionToIndex(mouseData_.releaseData);

  handleMouseRelease();
}

void
CQModelView::
handleMouseRelease()
{
}

void
CQModelView::
mouseDoubleClickEvent(QMouseEvent *e)
{
  if (e->button() != Qt::LeftButton)
    return;

  mouseData_.pressData.pos = e->pos();

  cellPositionToIndex(mouseData_.pressData);

  handleMouseRelease();

  if      (mouseData_.pressData.ind.isValid()) {
    QPersistentModelIndex persistent = mouseData_.pressData.ind;

    if (edit(persistent, DoubleClicked, e))
      return;

    QStyleOptionViewItem option;

    if (style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, &option, this))
      emit activated(mouseData_.pressData.ind);
  }
  else if (mouseData_.pressData.iind.isValid()) {
    setExpanded(mouseData_.pressData.iind, ! isExpanded(mouseData_.pressData.iind));
  }
}

void
CQModelView::
handleMouseDoubleClick()
{
  if      (mouseData_.pressData.hsection >= 0) {
  }
  else if (mouseData_.pressData.hsectionh >= 0) {
    fitColumn(mouseData_.pressData.hsectionh);

    redraw();
  }
  else if (mouseData_.pressData.vsection >= 0) {
  }
  else if (mouseData_.pressData.ind.isValid()) {
  }
}

void
CQModelView::
keyPressEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Slash)
    setShowFilter(! isShowFilter());

  QAbstractItemView::keyPressEvent(e);
}

void
CQModelView::
leaveEvent(QEvent *)
{
  mouseData_.reset();

  redraw();
}

void
CQModelView::
contextMenuEvent(QContextMenuEvent *e)
{
  mouseData_.menuData.pos = e->pos();

  cellPositionToIndex(mouseData_.menuData);

  showMenu(e->globalPos());
}

void
CQModelView::
showMenu(const QPoint &pos)
{
  updateVisColumns();

  //---

  QMenu *menu = new QMenu;

  //---

  auto addAction = [&](const QString &name, const char *slotName) {
    QAction *action = menu->addAction(name);

    connect(action, SIGNAL(triggered()), this, slotName);
  };

  auto addCheckedAction = [&](const QString &name, bool checked, const char *slotName) {
    QAction *action = menu->addAction(name);

    action->setCheckable(true);
    action->setChecked  (checked);

    connect(action, SIGNAL(triggered(bool)), this, slotName);
  };

  addCheckedAction("Alternating Row Colors", alternatingRowColors(),
                   SLOT(alternatingRowColorsSlot(bool)));
  addCheckedAction("Freeze First Column", isFreezeFirstColumn(),
                   SLOT(freezeFirstColumnSlot(bool)));
  addCheckedAction("Stretch Last Column", isStretchLastColumn(),
                   SLOT(stretchLastColumnSlot(bool)));

  addCheckedAction("Sorting Enabled", isSortingEnabled(),
                   SLOT(sortingEnabledSlot(bool)));

  if (isSortingEnabled() && mouseData_.menuData.hsection >= 0) {
    if (mouseData_.menuData.hsection == hh_->sortIndicatorSection()) {
      if (hh_->sortIndicatorOrder() != Qt::AscendingOrder)
        addAction("Sort Increasing", SLOT(sortIncreasingSlot()));

      if (hh_->sortIndicatorOrder() != Qt::DescendingOrder)
        addAction("Sort Decreasing", SLOT(sortDecreasingSlot()));
    }
    else {
      addAction("Sort Increasing", SLOT(sortIncreasingSlot()));
      addAction("Sort Decreasing", SLOT(sortDecreasingSlot()));
    }
  }

  addAction("Fit All Columns", SLOT(fitAllColumnsSlot()));

  if (mouseData_.menuData.hsection >= 0)
    addAction("Fit Column", SLOT(fitColumnSlot()));

  addAction("Hide Column", SLOT(hideColumnSlot()));

  if (int(visColumnDatas_.size()) != nc_)
    addAction("Show All Columns", SLOT(showAllColumnsSlot()));

  if (mouseData_.menuData.iind.isValid() || mouseData_.menuData.ind.isValid()) {
    bool expanded = false;

    if (mouseData_.menuData.iind.isValid())
      expanded = isExpanded(mouseData_.menuData.iind);
    else
      expanded = isExpanded(mouseData_.menuData.ind);

    if (expanded)
      addAction("Collapse", SLOT(collapseSlot()));
    else
      addAction("Expand", SLOT(expandSlot()));

    addAction("Expand All"  , SLOT(expandAll()));
    addAction("Collapse All", SLOT(collapseAll()));
  }

  if (mouseData_.menuData.hsection >= 0) {
    const ColumnData &columnData = columnDatas_[mouseData_.menuData.hsection];

    addCheckedAction("Show Heatmap", columnData.heatmap,
                     SLOT(setHeatmapSlot(bool)));
  }

  //---

  (void) menu->exec(pos);

  delete menu;
}

void
CQModelView::
redraw()
{
  // draw horizontal header (for widths)
  hh_->redraw();

  // draw vertical header (for alternate)
  vh_->redraw();

  // draw cells
  viewport()->update(viewport()->rect());
  update(rect());
}

void
CQModelView::
alternatingRowColorsSlot(bool b)
{
  setAlternatingRowColors(b);

  redraw();
}

void
CQModelView::
freezeFirstColumnSlot(bool b)
{
  setFreezeFirstColumn(b);

  redraw();
}

void
CQModelView::
stretchLastColumnSlot(bool b)
{
  setStretchLastColumn(b);

  state_.updateVisColumns = true;

  redraw();
}

void
CQModelView::
sortingEnabledSlot(bool b)
{
  setSortingEnabled(b);

  redraw();
}

void
CQModelView::
sortIncreasingSlot()
{
  sortByColumn(mouseData_.menuData.hsection, Qt::AscendingOrder);

  redraw();
}

void
CQModelView::
sortDecreasingSlot()
{
  sortByColumn(mouseData_.menuData.hsection, Qt::DescendingOrder);

  redraw();
}

//------

void
CQModelView::
hideColumnSlot()
{
  hideColumn(mouseData_.menuData.hsection);
}

void
CQModelView::
showAllColumnsSlot()
{
  for (int c = 0; c < nc_; ++c)
    showColumn(c);

  redraw();
}

void
CQModelView::
hideColumn(int column)
{
  hh_->hideSection(column);

  state_.updateAll();

  redraw();
}

void
CQModelView::
showColumn(int column)
{
  hh_->showSection(column);

  state_.updateAll();

  redraw();
}

bool
CQModelView::
isColumnHidden(int column) const
{
  return hh_->isSectionHidden(column);
}

void
CQModelView::
setColumnHidden(int column, bool hide)
{
  if (column < 0 || column >= nc_)
    return;

  hh_->setSectionHidden(column, hide);

  state_.updateAll();

  redraw();
}

//------

void
CQModelView::
hideRow(int row)
{
  vh_->hideSection(row);

  state_.updateAll();

  redraw();
}

void
CQModelView::
showRow(int row)
{
  vh_->showSection(row);

  state_.updateAll();

  redraw();
}

bool
CQModelView::
isRowHidden(int row, const QModelIndex &) const
{
  return vh_->isSectionHidden(row);
}

void
CQModelView::
setRowHidden(int row, const QModelIndex &, bool hide)
{
  if (row < 0 || row >= nr_)
    return;

  vh_->setSectionHidden(row, hide);

  state_.updateAll();

  redraw();
}

//------

void
CQModelView::
expand(const QModelIndex &index)
{
  if (isIndexExpanded(index))
    return;

  auto p = collapsed_.find(index);

  if (p != collapsed_.end())
    collapsed_.erase(p);

  redraw();

  emit expanded(index);
}

void
CQModelView::
collapse(const QModelIndex &index)
{
  if (! isIndexExpanded(index))
    return;

  collapsed_.insert(index);

  redraw();

  emit collapsed(index);
}

bool
CQModelView::
isExpanded(const QModelIndex &index) const
{
  return isIndexExpanded(index);
}

void
CQModelView::
setExpanded(const QModelIndex &index, bool expanded)
{
  if (expanded)
    expand(index);
  else
    collapse(index);
}

void
CQModelView::
expandAll()
{
  collapsed_.clear();

  redraw();
}

void
CQModelView::
collapseAll()
{
  for (const auto &pr : rowDatas_) {
    const RowData &rowData = pr.second;

    if (rowData.children && rowData.expanded) {
      QModelIndex index = model_->index(rowData.row, 0, rowData.parent);

      collapsed_.insert(index);
    }
  }

  redraw();
}

void
CQModelView::
expandToDepth(int /*depth*/)
{
  // TODO:
}

bool
CQModelView::
isIndexExpanded(const QModelIndex &index) const
{
  auto p = collapsed_.find(index);

  return (p == collapsed_.end());
}

void
CQModelView::
expandSlot()
{
  if      (mouseData_.menuData.iind.isValid())
    expand(mouseData_.menuData.iind);
  else if (mouseData_.menuData.ind.isValid())
    expand(mouseData_.menuData.ind);
}

void
CQModelView::
collapseSlot()
{
  if      (mouseData_.menuData.iind.isValid())
    collapse(mouseData_.menuData.iind);
  else if (mouseData_.menuData.ind.isValid())
    collapse(mouseData_.menuData.ind);
}

//------

void
CQModelView::
fitAllColumnsSlot()
{
  for (int c = 0; c < nc_; ++c)
    fitColumn(c);

  redraw();
}

void
CQModelView::
fitColumnSlot()
{
  fitColumn(mouseData_.menuData.hsection);

  redraw();
}

void
CQModelView::
fitColumn(int column)
{
  int maxWidth = 0;

  //---

  QVariant data = model_->headerData(column, Qt::Horizontal, Qt::DisplayRole);

  int w = fm_.width(data.toString());

  maxWidth = std::max(maxWidth, w);

  //---

  int nvr = 0;

  QModelIndex parent;

  maxColumnWidth(column, parent, 0, nvr, maxWidth);

  //---

  ColumnData &columnData = columnDatas_[column];

  columnData.width = std::max(maxWidth + 2*globalColumnData_.margin, 16);

  hh_->resizeSection(column, columnData.width);

  //---

  state_.updateAll();

  redraw();
}

bool
CQModelView::
maxColumnWidth(int column, const QModelIndex &parent, int depth, int &nvr, int &maxWidth)
{
  int nr = model_->rowCount(parent);

  for (int r = 0; r < nr; ++r) {
    if (isRowHidden(r, parent))
      continue;

    QModelIndex ind = model_->index(r, column, parent);

    QVariant data = model_->data(ind, Qt::DisplayRole);

    int w = fm_.width(data.toString());

    if (hierarchical_ && column == 0)
      w += (depth + 1)*indentation();

    maxWidth = std::max(maxWidth, w);

    ++nvr;

    if (nvr > 1000)
      return false;

    //---

    QModelIndex ind1 = model_->index(r, 0, parent);

    if (model_->hasChildren(ind1) && isExpanded(ind1)) {
      if (! maxColumnWidth(column, ind1, depth + 1, nvr, maxWidth))
        return false;
    }
  }

  return true;
}

//------

void
CQModelView::
setHeatmapSlot(bool b)
{
  ColumnData &columnData = columnDatas_[mouseData_.menuData.hsection];

  columnData.heatmap = b;

  redraw();
}

//------

QRect
CQModelView::
visualRect(const QModelIndex &ind) const
{
  auto p = visCellDatas_.find(ind);

  if (p != visCellDatas_.end())
    return (*p).second.rect;

  return QRect();
}

void
CQModelView::
scrollTo(const QModelIndex &index, ScrollHint)
{
  if (! index.isValid())
    return;

  auto pp = visRowDatas_.find(index.parent());
  if (pp == visRowDatas_.end()) return;

  const RowVisRowDatas &rowVisRowDatas = (*pp).second;

  auto pr = rowVisRowDatas.find(index.row());
  if (pr == rowVisRowDatas.end()) return;

  const VisRowData &visRowData = (*pr).second;

  int vh = viewport()->geometry().height();

  bool visible = (visRowData.rect.bottom() > 0 && visRowData.rect.top() <= vh);

  if (visible)
    return;

  int ys;

  if (visRowData.rect.bottom() <= 0)
    ys = visRowData.ir;
  else
    ys = visRowData.ir - scrollData_.nv + 1;

  vs_->setValue(std::min(std::max(ys, vs_->minimum()), vs_->maximum()));

  redraw();
}

QModelIndex
CQModelView::
indexAt(const QPoint &p) const
{
  PositionData posData;

  posData.pos = p;

  cellPositionToIndex(posData);

  return posData.ind;
}

QModelIndex
CQModelView::
moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers)
{
  Q_UNUSED(modifiers);

  QModelIndex current = currentIndex();

  if (! current.isValid())
    return QModelIndex();

  updateVisColumns();

  //---

  int         r      = current.row   ();
  int         c      = current.column();
  QModelIndex parent = current.parent();

  //---

  auto prevRow = [&](int r, int c, const QModelIndex &parent) {
    auto pp = visRowDatas_.find(parent);
    if (pp == visRowDatas_.end()) return QModelIndex();

    const RowVisRowDatas &rowVisRowDatas = (*pp).second;

    auto pr = rowVisRowDatas.find(r);
    if (pr == rowVisRowDatas.end()) return QModelIndex();

    int ir = (*pr).second.ir;

    if (ir == 0)
      return model_->index(r, c, parent);

    --ir;

    const RowData &rowData = rowDatas_[ir];

    return model_->index(rowData.row, c, rowData.parent);
  };

  auto nextRow = [&](int r, int c, const QModelIndex &parent) {
    auto pp = visRowDatas_.find(parent);
    if (pp == visRowDatas_.end()) return QModelIndex();

    const RowVisRowDatas &rowVisRowDatas = (*pp).second;

    auto pr = rowVisRowDatas.find(r);
    if (pr == rowVisRowDatas.end()) return QModelIndex();

    int ir = (*pr).second.ir;

    if (ir == nvr_ - 1)
      return model_->index(r, c, parent);

    ++ir;

    const RowData &rowData = rowDatas_[ir];

    return model_->index(rowData.row, c, rowData.parent);
  };

  //---

  auto prevCol = [&](int c) {
    auto p = visColumnDatas_.find(c);

    if (p != visColumnDatas_.end())
      --p;

    if (p == visColumnDatas_.end())
      return c;

    return (*p).first;
  };

  auto nextCol = [&](int c) {
    auto p = visColumnDatas_.find(c);

    if (p != visColumnDatas_.end())
      ++p;

    if (p == visColumnDatas_.end())
      return c;

    return (*p).first;
  };

  switch (action) {
    case MoveUp:
      return prevRow(r, c, parent);
    case MoveDown:
      return nextRow(r, c, parent);
    case MovePrevious:
    case MoveLeft:
      return model_->index(r, prevCol(c), parent);
    case MoveNext:
    case MoveRight:
      return model_->index(r, nextCol(c), parent);
    case MoveHome:
      return model_->index(0, 0, parent);
    case MoveEnd:
      // TODO: ensure valid column
      return model_->index(nr_ - 1, nc_ - 1, parent);
    case MovePageUp:
      return model_->index(std::max(r - scrollData_.nv, 0), c, parent);
    case MovePageDown:
      return model_->index(std::min(r + scrollData_.nv, nr_ - 1), c, parent);
  }

  return QModelIndex();
}

int
CQModelView::
horizontalOffset() const
{
  return scrollData_.hpos;
}

int
CQModelView::
verticalOffset() const
{
  return scrollData_.vpos;
}

bool
CQModelView::
isIndexHidden(const QModelIndex &index) const
{
  int r = index.row();
  int c = index.column();

  if (isColumnHidden(c))
    return true;

  if (isRowHidden(r, index.parent()))
    return true;

  return false;
}

void
CQModelView::
setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags flags)
{
  if (! sm_)
    return;

  QItemSelection selection;

  for (const auto &cp : visCellDatas_) {
    const VisCellData &visCellData = cp.second;

    if (visCellData.rect.intersects(rect))
      selection.select(cp.first, cp.first);
  }

  sm_->select(selection, flags);
}

QRegion
CQModelView::
visualRegionForSelection(const QItemSelection &) const
{
  // TODO: used as update region for selection change
  QRegion r;

  return r;
}

//------

bool
CQModelView::
cellPositionToIndex(PositionData &posData) const
{
  posData.reset();

  for (const auto &cp : visCellDatas_) {
    const VisCellData &visCellData = cp.second;

    if (visCellData.rect.contains(posData.pos)) {
      posData.ind = cp.first;
      return true;
    }
  }

  for (const auto &cp : ivisCellDatas_) {
    const VisCellData &visCellData = cp.second;

    if (visCellData.rect.contains(posData.pos)) {
      posData.iind = cp.first;
      return true;
    }
  }

  return false;
}

bool
CQModelView::
hheaderPositionToIndex(PositionData &posData) const
{
  const_cast<CQModelView *>(this)->updateVisColumns();

  //---

  posData.reset();

  for (const auto &vp : visColumnDatas_) {
    const VisColumnData &visColumnData = vp.second;

    if (visColumnData.rect.contains(posData.pos)) {
      posData.hsection = vp.first;
      return true;
    }

    if (visColumnData.hrect.contains(posData.pos)) {
      posData.hsectionh = vp.first;
      return true;
    }
  }

  return false;
}

bool
CQModelView::
vheaderPositionToIndex(PositionData &posData) const
{
  posData.reset();

  for (const auto &pp : visRowDatas_) {
    const RowVisRowDatas &rowVisRowDatas = pp.second;

    for (const auto &pr : rowVisRowDatas) {
      const VisRowData &visRowData = pr.second;

      if (visRowData.rect.contains(posData.pos)) {
        posData.vsection = visRowData.ir;
        return true;
      }
    }
  }

  return false;
}

void
CQModelView::
setRolePen(QPainter *painter, ColorRole role, double alpha)
{
  if (role != paintData_.penRole || alpha != paintData_.penAlpha) {
    paintData_.penRole  = role;
    paintData_.penAlpha = alpha;

    if (paintData_.penRole != ColorRole::None) {
      auto p = paintData_.roleColors.find(paintData_.penRole);

      if (p == paintData_.roleColors.end()) {
        QColor c = roleColor(role);

        c.setAlphaF(alpha);

        p = paintData_.roleColors.insert(p, RoleColors::value_type(paintData_.penRole, c));
      }

      painter->setPen((*p).second);
    }
    else
      painter->setPen(Qt::NoPen);
  }
}

void
CQModelView::
setRoleBrush(QPainter *painter, ColorRole role, double alpha)
{
  if (role != paintData_.brushRole || alpha != paintData_.brushAlpha) {
    paintData_.brushRole  = role;
    paintData_.brushAlpha = alpha;

    if (paintData_.brushRole != ColorRole::None) {
      auto p = paintData_.roleColors.find(paintData_.brushRole);

      if (p == paintData_.roleColors.end()) {
        QColor c = roleColor(role);

        c.setAlphaF(alpha);

        p = paintData_.roleColors.insert(p, RoleColors::value_type(paintData_.brushRole, c));
      }

      painter->setBrush((*p).second);
    }
    else
      painter->setBrush(Qt::NoBrush);
  }
}

QColor
CQModelView::
roleColor(ColorRole role) const
{
  switch (role) {
    case ColorRole::Window     : return palette().color(QPalette::Window);
    case ColorRole::Text       : return palette().color(QPalette::Text);
    case ColorRole::HeaderBg   : return QColor("#DDDDEE");
    case ColorRole::HeaderFg   : return textColor(roleColor(ColorRole::HeaderBg));
    case ColorRole::MouseOverBg: return blendColors(palette().color(QPalette::Highlight),
                                                    palette().color(QPalette::Window), 0.5);
    case ColorRole::MouseOverFg: return textColor(roleColor(ColorRole::MouseOverBg));
    case ColorRole::HighlightBg: return palette().color(QPalette::Highlight);
    case ColorRole::HighlightFg: return palette().color(QPalette::HighlightedText);
    case ColorRole::AlternateBg: return palette().color(QPalette::AlternateBase);
    case ColorRole::AlternateFg: return palette().color(QPalette::Text);
    case ColorRole::SelectionBg: return QColor("#7F7F7F");
    case ColorRole::SelectionFg: return QColor("#000000");
    default: assert(false); return QColor();
  }
}

QColor
CQModelView::
textColor(const QColor &c) const
{
  int g = qGray(c.red(), c.green(), c.blue());

  return (g < 127 ? Qt::white : Qt::black);
}

QColor
CQModelView::
blendColors(const QColor &c1, const QColor &c2, double f) const
{
  auto clamp = [](int i, int low, int high) { return std::min(std::max(i, low), high); };

  double f1 = 1.0 - f;

  double r = c1.redF  ()*f + c2.redF  ()*f1;
  double g = c1.greenF()*f + c2.greenF()*f1;
  double b = c1.blueF ()*f + c2.blueF ()*f1;

  return QColor(clamp(int(255*r), 0, 255),
                clamp(int(255*g), 0, 255),
                clamp(int(255*b), 0, 255));
}

//------

CQModelViewHeader::
CQModelViewHeader(Qt::Orientation orientation, CQModelView *view) :
 QHeaderView(orientation, view), view_(view)
{
  setContextMenuPolicy(Qt::DefaultContextMenu);

  setSectionsClickable(true);
  setHighlightSections(true);

  setMouseTracking(true);
}

void
CQModelViewHeader::
resizeEvent(QResizeEvent *)
{
}

void
CQModelViewHeader::
paintEvent(QPaintEvent *)
{
  QPainter painter(this->viewport());

  QColor c = palette().color(QPalette::Window);

  painter.fillRect(rect(), c);

  if (orientation() == Qt::Horizontal)
    view_->drawHHeader(&painter);
  else
    view_->drawVHeader(&painter);
}

void
CQModelViewHeader::
mousePressEvent(QMouseEvent *e)
{
  if (e->button() != Qt::LeftButton)
    return;

  auto mouseData = view_->mouseData();

  mouseData.pressData.pos = e->pos();

  mouseData.pressed = true;

  if (orientation() == Qt::Horizontal)
    view_->hheaderPositionToIndex(mouseData.pressData);
  else
    view_->vheaderPositionToIndex(mouseData.pressData);

  view_->setMouseData(mouseData);

  view_->handleMousePress();
}

void
CQModelViewHeader::
mouseMoveEvent(QMouseEvent *e)
{
  auto mouseData = view_->mouseData();

  CQModelView::PositionData moveData;

  moveData.pos = e->pos();

  if (orientation() == Qt::Horizontal)
    view_->hheaderPositionToIndex(moveData);
  else
    view_->vheaderPositionToIndex(moveData);

  if (! mouseData.pressed) {
    if (moveData != mouseData.moveData) {
      mouseData.moveData = moveData;

      if (orientation() == Qt::Horizontal) {
        if (mouseData.moveData.hsectionh != -1)
          setCursor(Qt::SplitHCursor);
        else
          unsetCursor();
      }

      view_->setMouseData(mouseData);

      view_->redraw();
    }
  }
  else {
    mouseData.moveData = moveData;

    view_->setMouseData(mouseData);

    view_->handleMouseMove();
  }
}

void
CQModelViewHeader::
mouseReleaseEvent(QMouseEvent *e)
{
  if (e->button() != Qt::LeftButton)
    return;

  auto mouseData = view_->mouseData();

  mouseData.releaseData.pos = e->pos();

  mouseData.pressed = false;

  if (orientation() == Qt::Horizontal)
    view_->hheaderPositionToIndex(mouseData.releaseData);
  else
    view_->vheaderPositionToIndex(mouseData.releaseData);

  view_->setMouseData(mouseData);

  view_->handleMouseRelease();
}

void
CQModelViewHeader::
mouseDoubleClickEvent(QMouseEvent *e)
{
  if (e->button() != Qt::LeftButton)
    return;

  auto mouseData = view_->mouseData();

  mouseData.pressData.pos = e->pos();

  if (orientation() == Qt::Horizontal)
    view_->hheaderPositionToIndex(mouseData.pressData);
  else
    view_->vheaderPositionToIndex(mouseData.pressData);

  view_->setMouseData(mouseData);

  view_->handleMouseDoubleClick();
}

void
CQModelViewHeader::
keyPressEvent(QKeyEvent *)
{
}

void
CQModelViewHeader::
leaveEvent(QEvent *)
{
  auto mouseData = view_->mouseData();

  mouseData.reset();

  view_->setMouseData(mouseData);

  view_->redraw();
}

void
CQModelViewHeader::
contextMenuEvent(QContextMenuEvent *e)
{
  auto mouseData = view_->mouseData();

  mouseData.menuData.pos = e->pos();

  if (orientation() == Qt::Horizontal)
    view_->hheaderPositionToIndex(mouseData.menuData);
  else
    view_->vheaderPositionToIndex(mouseData.menuData);

  view_->setMouseData(mouseData);

  view_->showMenu(e->globalPos());
}

void
CQModelViewHeader::
redraw()
{
  viewport()->update(viewport()->rect());
  update(rect());
}

//------

CQModelViewSelectionModel::
CQModelViewSelectionModel(CQModelView *view) :
 view_(view)
{
}

CQModelViewSelectionModel::
~CQModelViewSelectionModel()
{
}

void
CQModelViewSelectionModel::
setCurrentIndex(const QModelIndex &index, QItemSelectionModel::SelectionFlags command)
{
  QItemSelectionModel::setCurrentIndex(index, command);

  view_->updateCurrentIndices();
}

void
CQModelViewSelectionModel::
select(const QModelIndex &index, QItemSelectionModel::SelectionFlags command)
{
  QItemSelectionModel::select(index, command);

  view_->updateSelection();
}

void
CQModelViewSelectionModel::
select(const QItemSelection &selection, QItemSelectionModel::SelectionFlags command)
{
  QItemSelectionModel::select(selection, command);

  view_->updateSelection();
}

void
CQModelViewSelectionModel::
clear()
{
  QItemSelectionModel::clear();

  view_->updateSelection();
}

void
CQModelViewSelectionModel::
reset()
{
  QItemSelectionModel::reset();

  view_->updateSelection();
  view_->updateCurrentIndices();
}

void
CQModelViewSelectionModel::
clearCurrentIndex()
{
  QItemSelectionModel::clearCurrentIndex();

  view_->updateCurrentIndices();
}

//------

CQModelViewCornerButton::
CQModelViewCornerButton(CQModelView *view) :
 QAbstractButton(view), view_(view)
{
  setObjectName("corner");

  setFocusPolicy(Qt::NoFocus);
}

void
CQModelViewCornerButton::
paintEvent(QPaintEvent*)
{
  QStyleOptionHeader opt;

  opt.init(this);

  QStyle::State state = QStyle::State_None;

  if (isEnabled())
    state |= QStyle::State_Enabled;

  if (isActiveWindow())
    state |= QStyle::State_Active;

  if (isDown())
    state |= QStyle::State_Sunken;

  opt.state    = state;
  opt.rect     = rect();
  opt.position = QStyleOptionHeader::OnlyOneSection;

  QPainter painter(this);

  style()->drawControl(QStyle::CE_Header, &opt, &painter, this);
}

//------

CQModelViewFilterEdit::
CQModelViewFilterEdit(CQModelView *view, int column) :
 QLineEdit(view), view_(view), column_(column)
{
}
