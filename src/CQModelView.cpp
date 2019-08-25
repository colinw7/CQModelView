#include <CQModelView.h>
#include <CQBaseModelTypes.h>
#include <CQItemDelegate.h>
//#include <CQPerfMonitor.h>

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

//#define CQ_MODEL_VIEW_TRACE 1

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

  return columnWidth(column, visColumnData);
}

int
CQModelView::
columnWidth(int column, const VisColumnData &visColumnData) const
{
  if (visColumnData.last && isStretchLastColumn()) {
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
setFreezeFirstColumn(bool b)
{
  freezeFirstColumn_ = b;

  state_.updateAll();

  redraw();
}

//---

void
CQModelView::
setStretchLastColumn(bool b)
{
  stretchLastColumn_ = b;

  state_.updateAll();

  redraw();
}

//---

void
CQModelView::
setMultiHeaderLines(bool b)
{
  multiHeaderLines_ = b;

  state_.updateAll();

  redraw();
}

//---

void
CQModelView::
setShowVerticalHeader(bool b)
{
  showVerticalHeader_ = b;

  state_.updateAll();

  redraw();
}

//---

void
CQModelView::
setShowFilter(bool b)
{
  showFilter_ = b;

  state_.updateAll();

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
setIndentation(int i)
{
  indentation_ = i;

  state_.updateAll();

  redraw();
}

//---

void
CQModelView::
setRootIsDecorated(bool b)
{
  rootIsDecorated_ = b;

  state_.updateAll();

  redraw();
}

//---

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

  redraw();
}

//---

void
CQModelView::
resizeEvent(QResizeEvent *)
{
  visualRect_ = viewport()->rect();

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

#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::updateGeometries");
#endif

  updateVisColumns();

  //---

  int fh = fm_.height();

  globalRowData_.headerHeight = 0;

  using ColumnStrs = std::map<int,QStringList>;

  ColumnStrs columnStrs;
  int        ncr = 0;

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    QVariant data = model_->headerData(c, Qt::Horizontal, Qt::DisplayRole);

    QString str = data.toString();

    if (isMultiHeaderLines()) {
      QStringList strs = str.split(QRegExp("\n|\r\n|\r"));

      int ns = strs.size();

      ncr = std::max(ncr, ns);

      for (int i = 0; i < ns; ++i)
        columnStrs[c].push_back(strs[i]);
    }
    else  {
      columnStrs[c].push_back(str);

      ncr = 1;
    }
  }

  //---

  rowColumnSpans_.clear();

  //---

  using RowStrs = std::map<int,QStringList>;

  if (isMultiHeaderLines()) {
    // pad from front (lines are bottom to top)
    for (auto &pcs : columnStrs) {
      QStringList &strs = pcs.second;

      while (strs.size() < ncr)
        strs.push_front("");
    }

    //---

    RowStrs rowStrs;

    for (int r = 0; r < ncr; ++r) {
      for (int c = 0; c < nc_; ++c) {
        rowStrs[r].push_back(columnStrs[c][r]);
      }
    }

    //---

    for (auto &prs : rowStrs) {
      int          r    = prs.first;
      QStringList &strs = prs.second;

      int nc = strs.size();

      int c1 = 0;

      while (c1 + 1 < nc) {
        int c2 = c1;

        while (c2 + 1 < nc && strs[c2 + 1] == strs[c1])
          ++c2;

        if (c2 > c1)
          rowColumnSpans_[r].push_back(ColumnSpan(c1, c2));

        c1 = c2 + 1;
      }
    }
  }

  //---

  globalRowData_   .headerHeight = ncr*(fh + 1) + 2*globalRowData_.margin;
  globalColumnData_.headerWidth  = fm_.width("X") + 4;
  globalRowData_   .height       = fh + globalRowData_.margin;

  // space for headers
  int filterHeight = (isShowFilter() ? fh + 8 : 0);

  int lm = (isShowVerticalHeader() ? globalColumnData_.headerWidth : 0);

  setViewportMargins(lm, globalRowData_.headerHeight + filterHeight, 0, 0);

  //---

  QRect vrect = viewport()->geometry();

  //---

  cornerWidget_->setVisible(isShowVerticalHeader());

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

#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::updateScrollBars");
#endif

  updateVisColumns();
  updateRowDatas  ();

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
paintEvent(QPaintEvent *e)
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfMonitorInst->resetStartsWith("CQModelView::");

  CQPerfTrace trace("CQModelView::paintEvent");
#endif

  QPainter painter(this->viewport());

//painter.setClipRegion(e->region());

  paintRect_ = e->rect();

  //---

  QColor c = palette().color(QPalette::Window);

  painter.fillRect(viewport()->rect(), c);

  //---

  fm_ = QFontMetrics(font());

  //---

  int nr = nr_;
  int nc = nc_;

  if (model_) {
    QModelIndex parent;

    nr_ = model_->rowCount   (parent);
    nc_ = model_->columnCount(parent);
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
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawCells");
#endif

  updateScrollBars();
  updateVisCells  ();
  updateGeometries();

  //---

  paintData_.reset();

  paintData_.vrect     = viewport()->rect();
  paintData_.vw        = paintData_.vrect.width ();
  paintData_.vh        = paintData_.vrect.height();
  paintData_.margin    = style()->pixelMetric(QStyle::PM_HeaderMargin, 0, hh_);
  paintData_.rowHeight = this->rowHeight(0);

  if (iconSize().isValid()) {
    paintData_.decorationSize = iconSize();
  }
  else {
    int pm = style()->pixelMetric(QStyle::PM_SmallIconSize, 0, this);

    paintData_.decorationSize = QSize(pm, pm);
  }

  paintData_.showDecorationSelected =
    style()->styleHint(QStyle::SH_ItemView_ShowDecorationSelected, 0, this);

  //---

  drawCellsRows(painter);

  drawCellsSelection(painter);
}

void
CQModelView::
drawCellsRows(QPainter *painter) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawCellsRows");
#endif

  // draw rows
  for (const auto &flatRow : visFlatRows_) {
    auto pr = rowDatas_.find(flatRow);
    assert(pr != rowDatas_.end());

    const RowData &rowData = (*pr).second;

    //---

    auto pp = visRowDatas_.find(rowData.parent);
    if (pp == visRowDatas_.end()) continue;

    const RowVisRowDatas &rowVisRowDatas = (*pp).second;

    auto ppr = rowVisRowDatas.find(rowData.row);
    if (ppr == rowVisRowDatas.end()) continue;

    const VisRowData &visRowData = (*ppr).second;

    //---

    drawRow(painter, rowData.row, rowData.parent, visRowData);
  }
}

void
CQModelView::
drawCellsSelection(QPainter *painter)
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawCellsSelection");
#endif

  updateVisCells();

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
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawHHeader");
#endif

  updateScrollBars();
  updateVisColumns();
  updateGeometries();

  //---

  paintData_.reset();

  paintData_.vrect     = hh_->viewport()->rect();
  paintData_.vw        = paintData_.vrect.width ();
  paintData_.vh        = paintData_.vrect.height();
  paintData_.margin    = style()->pixelMetric(QStyle::PM_HeaderMargin, 0, hh_);
  paintData_.rowHeight = this->rowHeight(0);

  //---

  painter->fillRect(paintData_.vrect, roleColor(ColorRole::HeaderBg));

  //---

  painter->save();

  for (const auto &pv : visColumnDatas_) {
    int                  c             = pv.first;
    const VisColumnData &visColumnData = pv.second;

    bool valid = (c != freezeColumn_);

    if (valid) {
      if (freezeColumn_ >= 0) {
        int x  = visColumnData.rect.left () + paintData_.margin;
        int cw = visColumnData.rect.width() + 2*paintData_.margin;

        int x1 = std::max(x, freezeWidth_);
        int x2 = (! visColumnData.last ? std::max(x + cw, freezeWidth_) : paintData_.vw - 1);

        if (x1 < x2) {
          painter->setClipRect(QRect(x1, 0, x2 - x1 + 1, globalRowData_.headerHeight));

          drawHHeaderSection(painter, c, visColumnData);
        }
      }
      else {
        drawHHeaderSection(painter, c, visColumnData);
      }
    }
  }

  //---

  if (freezeColumn_ >= 0) {
    auto p = visColumnDatas_.find(freezeColumn_);
    assert(p != visColumnDatas_.end());

    const VisColumnData &visColumnData = (*p).second;

    painter->setClipRect(QRect(0, 0, freezeWidth_, globalRowData_.headerHeight));

    drawHHeaderSection(painter, freezeColumn_, visColumnData);
  }

  //---

  // draw span text
  if (isMultiHeaderLines()) {
    int fh = fm_.height();

    for (const auto &pr : rowColumnSpans_) {
      int                r           = pr.first;
      const ColumnSpans &columnSpans = pr.second;

      int y1 = r*(fh + 1) + globalRowData_.margin;
      int y2 = y1 + fh;

      for (const auto &span : columnSpans) {
        int c1 = span.first;
        int c2 = span.second;

        const VisColumnData &visColumnData1 = visColumnDatas_[c1];
        const VisColumnData &visColumnData2 = visColumnDatas_[c2];

        QRect rect(visColumnData1.rect.left(), y1,
                   visColumnData2.rect.right() - visColumnData1.rect.left() + 1, y2 - y1 + 1);

        QVariant data = model_->headerData(c1, Qt::Horizontal, Qt::DisplayRole);

        QString str = data.toString();

        if (isMultiHeaderLines()) {
          QStringList strs = str.split(QRegExp("\n|\r\n|\r"));

          setRolePen(painter, ColorRole::HeaderFg);

          painter->drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, strs[r]);
        }
        else
          painter->drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, str);

        //---

        setRolePen(painter, ColorRole::HeaderLineFg);

        painter->drawLine(visColumnData2.rect.topRight  (), visColumnData2.rect.bottomRight());
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());
      }
    }
  }

  //---

  painter->restore();
}

void
CQModelView::
drawHHeaderSection(QPainter *painter, int c, const VisColumnData &visColumnData)
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawHHeaderSection");
#endif

  QModelIndex ind = model_->index(0, c, QModelIndex());

  //---

  QVariant data = model_->headerData(c, Qt::Horizontal, Qt::DisplayRole);

  QString str = data.toString();

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

  auto fillBackground = [&](const QRect &r) {
    initBrush();

    painter->fillRect(r, painter->brush());
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

  struct RectSpan {
    QRect rect;
    bool  inSpan { false };

    RectSpan(const QRect &rect, bool inSpan) :
     rect(rect), inSpan(inSpan) {
    }
  };

  using RowRects = std::vector<RectSpan>;

  RowRects rowRects;

  int y = option.rect.top() + globalRowData_.margin;

  //---

  QStringList strs;

  if (isMultiHeaderLines())
    strs = str.split(QRegExp("\n|\r\n|\r"));
  else
    strs.push_back(str);

  int ns = strs.length();

  //---

  if (ns > 1) {
    int fh = fm_.height();

    for (int r = 0; r < ns; ++r) {
      bool inSpan = false;

      const ColumnSpans &columnSpans = rowColumnSpans_[r];

      for (const auto &span : columnSpans) {
        if (c >= span.first && c <= span.second) {
          inSpan = true;
          break;
        }
      }

      //---

      QRect rect;

      if      (r == 0)
        rect = QRect(option.rect.left(), y - globalRowData_.margin, option.rect.width(), fh);
      else if (r == ns - 1)
        rect = QRect(option.rect.left(), y, option.rect.width(), fh + globalRowData_.margin);
      else
        rect = QRect(option.rect.left(), y, option.rect.width(), fh);

      rowRects.push_back(RectSpan(rect, inSpan));

      y += fh + 1;
    }
  }
  else {
    rowRects.push_back(RectSpan(option.rect, false));
  }

  //---

  for (const auto &rowRect : rowRects) {
    if (! rowRect.inSpan)
      fillBackground(rowRect.rect.adjusted(-paintData_.margin, 0, paintData_.margin, 0));
  }

  //---

  initPen();

  for (int r = 0; r < strs.size(); ++r) {
    const RectSpan &rectSpan = rowRects[r];

    //---

    QStyleOptionHeader subopt = option;

    subopt.text = strs[r];

    if (! rectSpan.inSpan) {
      subopt.rect = style()->subElementRect(QStyle::SE_HeaderLabel, &subopt, hh_);

      QRect rect(subopt.rect.left(), rectSpan.rect.top(),
                 subopt.rect.width(), rectSpan.rect.height());

      painter->drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, strs[r]);
    }
  }

  //---

  if (option.sortIndicator != QStyleOptionHeader::None) {
    QStyleOptionHeader subopt = option;

    subopt.rect = style()->subElementRect(QStyle::SE_HeaderArrow, &option, hh_);

    style()->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &subopt, painter, hh_);
  }

  //---

  QRect fullRect = visColumnData.rect.adjusted(-paintData_.margin, 0, paintData_.margin, 0);

  setRolePen(painter, ColorRole::HeaderLineFg);

  painter->drawLine(fullRect.topLeft   (), fullRect.topRight   ());
  painter->drawLine(fullRect.bottomLeft(), fullRect.bottomRight());

  for (const auto &rowRect : rowRects) {
    if (! rowRect.inSpan) {
      int x  = rowRect.rect.right() + paintData_.margin;
      int y1 = rowRect.rect.top();
      int y2 = rowRect.rect.bottom();

      painter->drawLine(x, y1, x, y2);
    }
  }

  //---

  if (option.state & QStyle::State_HasFocus)
    drawFocus();

  //---

  if (c == mouseData_.pressData.hsectionh || c == mouseData_.moveData.hsectionh) {
    setRolePen(painter, ColorRole::MouseOverFg);

    for (const auto &rowRect : rowRects) {
      if (! rowRect.inSpan) {
        int x  = rowRect.rect.right() + paintData_.margin;
        int y1 = rowRect.rect.top();
        int y2 = rowRect.rect.bottom();

        for (int i = 0; i < 3; ++i)
          painter->drawLine(x - i, y1, x - i, y2);
      }
    }
  }

  //---

  auto drawSelection = [&](const QRect &rect) {
    setRolePen  (painter, ColorRole::SelectionFg, 0.3);
    setRoleBrush(painter, ColorRole::SelectionBg, 0.3);

    painter->fillRect(rect, painter->brush());
  };

  if (option.state & QStyle::State_Selected) {
    for (const auto &rowRect : rowRects) {
      if (! rowRect.inSpan)
        drawSelection(rowRect.rect.adjusted(-paintData_.margin, 0, paintData_.margin, 0));
    }
  }
}

void
CQModelView::
drawVHeader(QPainter *painter)
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawVHeader");
#endif

  updateScrollBars();
  updateVisColumns();
  updateVisRows   ();
  updateGeometries();

  //---

  paintData_.reset();

  paintData_.vrect     = vh_->viewport()->rect();
  paintData_.vw        = paintData_.vrect.width ();
  paintData_.vh        = paintData_.vrect.height();
  paintData_.margin    = style()->pixelMetric(QStyle::PM_HeaderMargin, 0, hh_);
  paintData_.rowHeight = this->rowHeight(0);

  //---

  painter->fillRect(paintData_.vrect, roleColor(ColorRole::HeaderBg));

  //---

  auto p = indRow_.find(vsm_->currentIndex());

  int currentRow = (p != indRow_.end() ? (*p).second : -1);

  for (const auto &flatRow : visFlatRows_) {
    auto pr = rowDatas_.find(flatRow);
    assert(pr != rowDatas_.end());

    const RowData &rowData = (*pr).second;

    //---

    auto pp = visRowDatas_.find(rowData.parent);
    if (pp == visRowDatas_.end()) continue;

    const RowVisRowDatas &rowVisRowDatas = (*pp).second;

    auto ppr = rowVisRowDatas.find(rowData.row);
    if (ppr == rowVisRowDatas.end()) continue;

    const VisRowData &visRowData = (*ppr).second;

    //---

    QModelIndex ind = model_->index(rowData.row, 0, rowData.parent);

    //---

    // init style data
    QStyleOptionHeader option;

    if (visRowData.flatRow == mouseData_.moveData.vsection)
      option.state |= QStyle::State_MouseOver;

    if (vsm_->isSelected(ind))
      option.state |= QStyle::State_Selected;

    if (flatRow == currentRow)
      option.state |= QStyle::State_HasFocus;

    option.rect             = visRowData.rect;
    option.icon             = QIcon();
    option.iconAlignment    = Qt::AlignCenter;
    option.position         = QStyleOptionHeader::Middle; // TODO
    option.section          = visRowData.flatRow;
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

    setRolePen(painter, ColorRole::HeaderLineFg);

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
}

void
CQModelView::
drawRow(QPainter *painter, int r, const QModelIndex &parent, const VisRowData &visRowData) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawRow");
#endif

  //---

  painter->save();

  for (const auto &pv : visColumnDatas_) {
    int                  c             = pv.first;
    const VisColumnData &visColumnData = pv.second;

    bool valid = (c != freezeColumn_);

    if (valid) {
      if (freezeColumn_ >= 0) {
        int x = visColumnData.rect.left() - paintData_.margin;

        int cw = visColumnData.rect.width() + 2*paintData_.margin;

        int x1 = std::max(x, freezeWidth_);
        int x2 = (! visColumnData.last ? std::max(x + cw, freezeWidth_) : paintData_.vw - 1);

        if (x1 < x2) {
          int y = visRowData.rect.top();

          painter->setClipRect(QRect(x1, y, x2 - x1 + 1, paintData_.rowHeight));

          drawCell(painter, r, c, parent, visRowData, visColumnData);
        }
      }
      else {
        drawCell(painter, r, c, parent, visRowData, visColumnData);
      }
    }
  }

  //---

  if (freezeWidth_ > 0) {
    auto pv = visColumnDatas_.find(freezeColumn_);
    assert(pv != visColumnDatas_.end());

    const VisColumnData &visColumnData = (*pv).second;

    int y = visRowData.rect.top();

    painter->setClipRect(QRect(0, y, freezeWidth_, paintData_.rowHeight));

    drawCell(painter, r, freezeColumn_, parent, visRowData, visColumnData);
  }

  painter->restore();
}

void
CQModelView::
drawCell(QPainter *painter, int r, int c, const QModelIndex &parent,
         const VisRowData &visRowData, const VisColumnData &visColumnData) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawCell");
#endif

  QModelIndex ind = model_->index(r, c, parent);

  //--

  int x1 = visColumnData.rect.left() - paintData_.margin;

  if (hierarchical_ && c == 0) {
    int indent = (visRowData.depth + rootIsDecorated())*indentation();

    //---

    auto pc = ivisCellDatas_.find(ind);
    if (pc == ivisCellDatas_.end()) return;

    const VisCellData &ivisCellData = (*pc).second;

    //---

    bool children     = model_->hasChildren(ind);
    bool expanded     = isExpanded(ind);
    bool moreSiblings = false;

    QStyleOptionViewItem opt;

    opt.rect = ivisCellData.rect;

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

  auto pc = visCellDatas_.find(ind);
  if (pc == visCellDatas_.end()) return;

  const VisCellData &visCellData = (*pc).second;

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

    if (idelegate) {
      const ColumnData &columnData = columnDatas_[c];

      idelegate->setHeatmap(columnData.heatmap);
    }

    delegate_->paint(painter, option, ind);
  }
}

//------

void
CQModelView::
updateVisColumns()
{
  if (! state_.updateVisColumns)
    return;

  state_.updateVisColumns = false;

  //---

#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::updateVisColumns");
#endif

  nvc_ = 0;

  int lastC = -1;

  visColumnDatas_.clear();

  int x1 = -horizontalOffset();

  int margin = style()->pixelMetric(QStyle::PM_HeaderMargin, 0, hh_);

  freezeWidth_  = 0;
  freezeColumn_ = -1;

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    //---

    const ColumnData &columnData = columnDatas_[c];

    int cw = (columnData.width > 0 ? columnData.width : 100);
    int x2 = x1 + cw;

    VisColumnData &visColumnData = visColumnDatas_[c];

    //---

    if (isFreezeFirstColumn() && freezeColumn_ < 0) {
      freezeColumn_ = c;
      freezeWidth_  = columnWidth(c, visColumnData);
    }

    //---

    int xl, xr;

    if (c == freezeColumn_) {
      xl = margin;
      xr = cw - margin;
    }
    else {
      xl = x1 + margin;
      xr = x2 - margin;
    }

    visColumnData.rect  = QRect(xl, 0, xr - xl + 1, globalRowData_.headerHeight);
    visColumnData.hrect = QRect(xr, 0, 2*margin   , globalRowData_.headerHeight);

    visColumnData.first = (nvc_ == 0);

    visColumnData.visible = ! (x1 > visualRect_.right() || x2 < visualRect_.left());

    ++nvc_;

    lastC = c;

    x1 = x2;
  }

  int vw = viewport()->rect().width();

  if (lastC > 0) {
    VisColumnData &visColumnData = visColumnDatas_[lastC];

    visColumnData.last = true;

    if (isStretchLastColumn()) {
      int xr = visColumnData.rect.right();

      if (xr + margin < vw) {
        int xl = visColumnData.rect.left();

        xr = vw - margin - 1;

        visColumnData.rect  = QRect(xl, 0, xr - xl + 1, globalRowData_.headerHeight);
        visColumnData.hrect = QRect(xr, 0, 2*margin, globalRowData_.headerHeight);
      }
    }
  }
}

//------

void
CQModelView::
updateRowDatas()
{
  if (! state_.updateRowDatas)
    return;

  state_.updateRowDatas = false;

  //---

#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::updateRowDatas");
#endif

  // update visible rows
  nvr_ = 0;

  QModelIndex parent;

  numVisibleRows(parent, nvr_, 0);
}

void
CQModelView::
numVisibleRows(const QModelIndex &parent, int &nvr, int depth)
{
  int nr = model_->rowCount(parent);

  for (int r = 0; r < nr; ++r) {
    if (isRowHidden(r, parent))
      continue;

    QModelIndex ind1 = model_->index(r, 0, parent);

    bool children = model_->hasChildren(ind1);
    bool expanded = children && isExpanded(ind1);

    rowDatas_[nvr]  = RowData(parent, r, nvr, children, expanded, depth);
    indRow_  [ind1] = nvr;

    ++nvr;

    if (children) {
      if (model_->canFetchMore(parent))
        model_->fetchMore(parent);

      numVisibleRows(ind1, nvr, depth + 1);
    }
  }
}

//------

void
CQModelView::
updateVisRows()
{
  if (! state_.updateVisRows)
    return;

  state_.updateVisRows = false;

  //---

#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::updateVisRows");
#endif

  updateRowDatas();

  //---

  visRowDatas_.clear();
  visFlatRows_.clear();

  //---

  int rowHeight = this->rowHeight(0);

  int y1 = -verticalOffset()*rowHeight;

  for (const auto &pr : rowDatas_) {
    const RowData &rowData = pr.second;

    //---

    QModelIndex ind1 = model_->index(rowData.row, 0, rowData.parent);

    bool children = model_->hasChildren(ind1);
    if (children) hierarchical_ = true;

    //---

    int y2 = y1 + rowHeight;

    VisRowData &visRowData = visRowDatas_[rowData.parent][rowData.row];

    visRowData.rect = QRect(0, y1, globalColumnData_.headerWidth, y2 - y1 + 1);

    visRowData.flatRow   = rowData.flatRow;
    visRowData.alternate = (visRowData.flatRow & 1);
    visRowData.depth     = rowData.depth;
    visRowData.visible   = ! (y1 > visualRect_.bottom() || y2 < visualRect_.top());

    //---

    if (visRowData.visible)
      visFlatRows_.push_back(visRowData.flatRow);

    //---

    y1 = y2;
  }
}

//------

void
CQModelView::
updateVisCells()
{
  if (! state_.updateVisCells)
    return;

  state_.updateVisCells = false;

  //---

#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::updateVisCells");
#endif

  updateVisColumns();
  updateVisRows   ();

  //---

  visCellDatas_ .clear();
  ivisCellDatas_.clear();

  for (const auto &pc : visColumnDatas_) {
    int                  c             = pc.first;
    const VisColumnData &visColumnData = pc.second;

    int x1 = visColumnData.rect.left () - paintData_.margin;
    int x2 = visColumnData.rect.right() + paintData_.margin;

    for (const auto &flatRow : visFlatRows_) {
      auto pr = rowDatas_.find(flatRow);
      assert(pr != rowDatas_.end());

      const RowData &rowData = (*pr).second;

      //---

      const VisRowData &visRowData = visRowDatas_[rowData.parent][rowData.row];

      int y1 = visRowData.rect.top   ();
      int y2 = visRowData.rect.bottom();

      //---

      if (hierarchical_ && c == 0) {
        QModelIndex ind = model_->index(rowData.row, c, rowData.parent);

        VisCellData &ivisCellData = ivisCellDatas_[ind];

        int indent = (visRowData.depth + rootIsDecorated())*indentation();

        ivisCellData.rect = QRect(x1, y1, indent, y2 - y1 + 1);
      }

      //---

      QModelIndex ind = model_->index(rowData.row, c, rowData.parent);

      VisCellData &visCellData = visCellDatas_[ind];

      if (visColumnData.last && isStretchLastColumn() && x2 < paintData_.vw)
        x2 = paintData_.vw - 1;

      visCellData.rect = QRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    }
  }
}

//------

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

  state_.updateVisColumns = true;
  state_.updateVisCells   = true;

  redraw();
}

void
CQModelView::
vscrollSlot(int v)
{
  scrollData_.vpos = v;

  state_.updateVisRows  = true;
  state_.updateVisCells = true;

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
      //QRect rrect = mouseData_.moveData.rect.united(moveData.rect);

      mouseData_.moveData = moveData;

      //redraw(rrect);
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
  addCheckedAction("Multi Header Lines", isMultiHeaderLines(),
                   SLOT(multiHeaderLinesSlot(bool)));
  addCheckedAction("Root Is Decorated", rootIsDecorated(),
                   SLOT(rootIsDecoratedSlot(bool)));

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

  addCheckedAction("Show Vertical Header", isShowVerticalHeader(),
                   SLOT(showVerticalHeaderSlot(bool)));
  addCheckedAction("Show Filter", isShowFilter(),
                   SLOT(showFilterSlot(bool)));

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
  redraw(viewport()->rect());
}

void
CQModelView::
redraw(const QRect &rect)
{
  // draw horizontal header (for widths)
  hh_->redraw();

  // draw vertical header (for alternate)
  vh_->redraw();

  // draw cells
  viewport()->update(rect);
  update(rect);
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
}

void
CQModelView::
stretchLastColumnSlot(bool b)
{
  setStretchLastColumn(b);

  state_.updateVisColumns = true;
  state_.updateVisCells   = true;

  redraw();
}

void
CQModelView::
multiHeaderLinesSlot(bool b)
{
  setMultiHeaderLines(b);
}

void
CQModelView::
rootIsDecoratedSlot(bool b)
{
  setRootIsDecorated(b);
}

void
CQModelView::
sortingEnabledSlot(bool b)
{
  setSortingEnabled(b);
}

void
CQModelView::
sortIncreasingSlot()
{
  sortByColumn(mouseData_.menuData.hsection, Qt::AscendingOrder);
}

void
CQModelView::
sortDecreasingSlot()
{
  sortByColumn(mouseData_.menuData.hsection, Qt::DescendingOrder);
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

  if (model_->canFetchMore(index))
    model_->fetchMore(index);

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
  updateRowDatas();

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
showVerticalHeaderSlot(bool b)
{
  setShowVerticalHeader(b);
}

void
CQModelView::
showFilterSlot(bool b)
{
  setShowFilter(b);
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

  QString str = data.toString();

  int w = 0;

  if (isMultiHeaderLines()) {
    QStringList strs = str.split(QRegExp("\n|\r\n|\r"));

    for (int i = 0; i < strs.length(); ++i)
      w = std::max(w, fm_.width(strs[i]));
  }
  else
    w = std::max(w, fm_.width(str));

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
  if (model_->canFetchMore(parent))
    model_->fetchMore(parent);

  int nr = model_->rowCount(parent);

  for (int r = 0; r < nr; ++r) {
    if (isRowHidden(r, parent))
      continue;

    QModelIndex ind = model_->index(r, column, parent);

    QVariant data = model_->data(ind, Qt::DisplayRole);

    int w = fm_.width(data.toString());

    if (hierarchical_ && column == 0)
      w += (depth + rootIsDecorated())*indentation();

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
  const_cast<CQModelView*>(this)->updateVisCells();

  //---

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

  updateVisRows();

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
    ys = visRowData.flatRow;
  else
    ys = visRowData.flatRow - scrollData_.nv + 1;

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

  //---

  updateVisColumns();
  updateVisRows   ();

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

    int flatRow = (*pr).second.flatRow;

    if (flatRow == 0)
      return model_->index(r, c, parent);

    --flatRow;

    const RowData &rowData = rowDatas_[flatRow];

    return model_->index(rowData.row, c, rowData.parent);
  };

  auto nextRow = [&](int r, int c, const QModelIndex &parent) {
    auto pp = visRowDatas_.find(parent);
    if (pp == visRowDatas_.end()) return QModelIndex();

    const RowVisRowDatas &rowVisRowDatas = (*pp).second;

    auto pr = rowVisRowDatas.find(r);
    if (pr == rowVisRowDatas.end()) return QModelIndex();

    int flatRow = (*pr).second.flatRow;

    if (flatRow == nvr_ - 1)
      return model_->index(r, c, parent);

    ++flatRow;

    const RowData &rowData = rowDatas_[flatRow];

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

  updateVisCells();

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
  const_cast<CQModelView *>(this)->updateVisCells();

  //---

  posData.reset();

  for (const auto &cp : visCellDatas_) {
    const VisCellData &visCellData = cp.second;

    if (visCellData.rect.contains(posData.pos)) {
      posData.ind  = cp.first;
      posData.rect = visCellData.rect;
      return true;
    }
  }

  for (const auto &cp : ivisCellDatas_) {
    const VisCellData &visCellData = cp.second;

    if (visCellData.rect.contains(posData.pos)) {
      posData.iind = cp.first;
      posData.rect = visCellData.rect;
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
      posData.rect     = visColumnData.rect;
      return true;
    }

    if (visColumnData.hrect.contains(posData.pos)) {
      posData.hsectionh = vp.first;
      posData.rect      = visColumnData.hrect;
      return true;
    }
  }

  return false;
}

bool
CQModelView::
vheaderPositionToIndex(PositionData &posData) const
{
  const_cast<CQModelView *>(this)->updateVisRows();

  //---

  posData.reset();

  for (const auto &pp : visRowDatas_) {
    const RowVisRowDatas &rowVisRowDatas = pp.second;

    for (const auto &pr : rowVisRowDatas) {
      const VisRowData &visRowData = pr.second;

      if (visRowData.rect.contains(posData.pos)) {
        posData.vsection = visRowData.flatRow;
        posData.rect     = visRowData.rect;
        return true;
      }
    }
  }

  return false;
}

void
CQModelView::
setRolePen(QPainter *painter, ColorRole role, double alpha) const
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
setRoleBrush(QPainter *painter, ColorRole role, double alpha) const
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
    case ColorRole::Window      : return palette().color(QPalette::Window);
    case ColorRole::Text        : return palette().color(QPalette::Text);
    case ColorRole::HeaderBg    : return QColor("#DDDDEE");
    case ColorRole::HeaderFg    : return textColor(roleColor(ColorRole::HeaderBg));
    case ColorRole::HeaderLineFg: return blendColors(roleColor(ColorRole::HeaderFg),
                                                     palette().color(QPalette::Window), 0.5);
    case ColorRole::MouseOverBg : return blendColors(palette().color(QPalette::Highlight),
                                                     palette().color(QPalette::Window), 0.5);
    case ColorRole::MouseOverFg : return textColor(roleColor(ColorRole::MouseOverBg));
    case ColorRole::HighlightBg : return palette().color(QPalette::Highlight);
    case ColorRole::HighlightFg : return palette().color(QPalette::HighlightedText);
    case ColorRole::AlternateBg : return palette().color(QPalette::AlternateBase);
    case ColorRole::AlternateFg : return palette().color(QPalette::Text);
    case ColorRole::SelectionBg : return QColor("#7F7F7F");
    case ColorRole::SelectionFg : return QColor("#000000");
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
