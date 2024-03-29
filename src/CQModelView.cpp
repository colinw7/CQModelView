#define QT_KEYPAD_NAVIGATION 1

#include <CQModelView.h>
#include <CQModelViewHeader.h>
#include <CQBaseModelTypes.h>
#include <CQItemDelegate.h>
#ifdef CQ_MODEL_VIEW_TRACE
#include <CQPerfMonitor.h>
#endif

#include <CLargestRect.h>

#include <svg/filter_svg.h>
#include <svg/fit_all_columns_svg.h>
#include <svg/fit_column_svg.h>
#include <svg/sort_az_svg.h>
#include <svg/sort_za_svg.h>
#include <svg/select_all_svg.h>

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QItemSelectionModel>
#include <QScrollBar>
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTextLayout>

#include <set>
#include <iostream>
#include <cmath>
#include <cassert>

CQModelView::
CQModelView(QWidget *parent) :
 QAbstractItemView(parent), paintData_(this)
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

  auto *selectionModel = new CQModelViewSelectionModel(this);

  CQModelView::setSelectionModel(selectionModel);

  //---

  cornerWidget_ = new CQModelViewCornerButton(this);
}

CQModelView::
~CQModelView()
{
  delete hsm_;
  delete vsm_;

  delete sm_;
}

QSize
CQModelView::
viewportSizeHint() const
{
  QSize result((vh_->isHidden() ? 0 : vh_->width ()) + hh_->length(),
               (hh_->isHidden() ? 0 : hh_->height()) + vh_->length());

  result += QSize(verticalScrollBar  ()->isVisible() ? verticalScrollBar  ()->width () : 0,
                  horizontalScrollBar()->isVisible() ? horizontalScrollBar()->height() : 0);

  return result;
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
    disconnect(model_, SIGNAL(rowsInserted(QModelIndex, int, int)),
               this, SLOT(modelChangedSlot()));
    disconnect(model_, SIGNAL(columnsInserted(QModelIndex, int, int)),
               this, SLOT(modelChangedSlot()));
    disconnect(model_, SIGNAL(rowsRemoved(QModelIndex, int, int)),
               this, SLOT(modelChangedSlot()));
    disconnect(model_, SIGNAL(columnsRemoved(QModelIndex, int, int)),
               this, SLOT(modelChangedSlot()));
  }

  if (sm_ && model_)
    disconnect(sm_, SIGNAL(currentRowChanged(QModelIndex, QModelIndex)), model_, SLOT(submit()));

  model_ = model;

  // connect to new model
  if (model_) {
    connect(model_, SIGNAL(rowsInserted(QModelIndex, int, int)),
            this, SLOT(modelChangedSlot()));
    connect(model_, SIGNAL(columnsInserted(QModelIndex, int, int)),
            this, SLOT(modelChangedSlot()));
    connect(model_, SIGNAL(rowsRemoved(QModelIndex, int, int)),
            this, SLOT(modelChangedSlot()));
    connect(model_, SIGNAL(columnsRemoved(QModelIndex, int, int)),
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
    vh_->setSelectionModel(vsm_);
  }

  //---

  // connect model
  // update cache
  state_.updateAll();

  autoFitted_ = false;

  redraw();

  emit stateChanged();
}

void
CQModelView::
setRootIndex(const QModelIndex &index)
{
  if (index == this->rootIndex()) {
    viewport()->update();
    return;
  }

  vh_->setRootIndex(index);
  hh_->setRootIndex(index);

  QAbstractItemView::setRootIndex(index);
}

void
CQModelView::
doItemsLayout()
{
  state_.updateAll();

  autoFitted_ = false;

  redraw();

  emit stateChanged();
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

  if (sm_ && model_)
    disconnect(sm_, SIGNAL(currentRowChanged(QModelIndex, QModelIndex)), model_, SLOT(submit()));

  if (sm_)
    delete sm_;

  sm_ = sm;

  if (model_) {
    sm_->setModel(model_);

    QAbstractItemView::setSelectionModel(sm_);
  }

  if (sm_ && model_)
    connect(sm_, SIGNAL(currentRowChanged(QModelIndex, QModelIndex)), model_, SLOT(submit()));

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

void
CQModelView::
reset()
{
  //std::cerr << "CQModelView::reset\n";

  state_.updateAll();

  autoFitted_ = false;

  QAbstractItemView::reset();
}

void
CQModelView::
dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
  //std::cerr << "CQModelView::dataChanged\n";

  QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
}

void
CQModelView::
selectColumn(int section, Qt::KeyboardModifiers modifiers)
{
  auto parent = rootIndex();

  auto ind = model_->index(0, section, parent);

  QItemSelection selection;

  selection.select(ind, ind);

  if (! (modifiers & Qt::ControlModifier))
    sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Columns);
  else
    sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Columns);

  sm_->setCurrentIndex(ind, QItemSelectionModel::NoUpdate);
}

void
CQModelView::
selectColumnRange(int section1, int section2, Qt::KeyboardModifiers modifiers)
{
  auto parent = rootIndex();

  auto ind1 = model_->index(0, section1, parent);
  auto ind2 = model_->index(0, section2, parent);

  QItemSelection selection;

  if (section1 > section2)
    selection.select(ind1, ind2);
  else
    selection.select(ind2, ind1);

  if (! (modifiers & Qt::ControlModifier))
    sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Columns);
  else
    sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Columns);

  sm_->setCurrentIndex(ind2, QItemSelectionModel::NoUpdate);
}

void
CQModelView::
selectRow(int section, Qt::KeyboardModifiers modifiers)
{
  auto parent = rootIndex();

  auto ind = model_->index(section, 0, parent);

  QItemSelection selection;

  selection.select(ind, ind);

  if (! (modifiers & Qt::ControlModifier))
    sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  else
    sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);

  sm_->setCurrentIndex(ind, QItemSelectionModel::NoUpdate);
}

void
CQModelView::
selectRowRange(int section1, int section2, Qt::KeyboardModifiers modifiers)
{
  auto parent = rootIndex();

  auto ind1 = model_->index(section1, 0, parent);
  auto ind2 = model_->index(section2, 0, parent);

  QItemSelection selection;

  if (section1 < section2)
    selection.select(ind1, ind2);
  else
    selection.select(ind2, ind1);

  if (! (modifiers & Qt::ControlModifier))
    sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  else
    sm_->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);

  sm_->setCurrentIndex(ind2, QItemSelectionModel::NoUpdate);
}

void
CQModelView::
selectCell(const QModelIndex &ind, Qt::KeyboardModifiers modifiers)
{
  QItemSelection selection;

  selection.select(ind, ind);

  if (! (modifiers & Qt::ControlModifier))
    sm_->select(selection, QItemSelectionModel::ClearAndSelect);
  else
    sm_->select(selection, QItemSelectionModel::Select);

  sm_->setCurrentIndex(ind, QItemSelectionModel::NoUpdate);
}

void
CQModelView::
selectCellRange(const QModelIndex &ind1, const QModelIndex &ind2, Qt::KeyboardModifiers modifiers)
{
  int row1 = ind1.row();
  int row2 = ind2.row();

  int column1 = ind1.column();
  int column2 = ind2.column();

  int r1 = std::min(row1, row2);
  int r2 = std::max(row1, row2);

  int c1 = std::min(column1, column2);
  int c2 = std::max(column1, column2);

  auto parent = rootIndex();

  auto oind1 = model_->index(r1, c1, parent);
  auto oind2 = model_->index(r2, c2, parent);

  QItemSelection selection;

  selection.select(oind1, oind2);

  if (! (modifiers & Qt::ControlModifier))
    sm_->select(selection, QItemSelectionModel::ClearAndSelect);
  else
    sm_->select(selection, QItemSelectionModel::Select);

  sm_->setCurrentIndex(ind2, QItemSelectionModel::NoUpdate);
}

void
CQModelView::
selectAllSlot()
{
  selectAll();
}

void
CQModelView::
selectAll()
{
  if (isHierarchical()) {
    QItemSelection selection;

    int  row1   = -1;
    int  row2   = -1;
    auto parent = rootIndex();

    for (const auto &pr : rowDatas_) {
      const auto &rowData = pr.second;

      if (row1 >= 0 && rowData.row == row2 + 1 && rowData.parent == parent) {
        row2 = rowData.row;
        continue;
      }

      if (row1 >= 0) {
        auto index1 = model_->index(row1, 0, parent);
        auto index2 = (row2 != row1 ? model_->index(row2, 0, parent) : index1);

        selection.select(index1, index2);
      }

      row1   = rowData.row;
      row2   = rowData.row;
      parent = rowData.parent;
    }

    if (row1 >= 0) {
      auto index1 = model_->index(row1, 0, parent);
      auto index2 = (row2 != row1 ? model_->index(row2, 0, parent) : index1);

      selection.select(index1, index2);
    }

    sm_->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
  }
  else {
    QAbstractItemView::selectAll();
  }
}

void
CQModelView::
scrollContentsBy(int dx, int dy)
{
  QAbstractItemView::scrollContentsBy(dx, dy);

  int dy1 = dy*paintData_.rowHeight;

  viewport()->scroll(dx, dy1);
}

void
CQModelView::
rowsInserted(const QModelIndex &parent, int start, int end)
{
  //std::cerr << "CQModelView::rowsInserted\n";

  QAbstractItemView::rowsInserted(parent, start, end);
}

void
CQModelView::
rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
  //std::cerr << "CQModelView::rowsAboutToBeRemoved\n";

  QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
}

QModelIndexList
CQModelView::
selectedIndexes() const
{
  //std::cerr << "CQModelView::selectedIndexes\n";

  return QAbstractItemView::selectedIndexes();
}

void
CQModelView::
updateGeometries()
{
  //std::cerr << "CQModelView::updateGeometries\n";

  QAbstractItemView::updateGeometries();
}

void
CQModelView::
selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
  state_.updateSelection = true;

  QAbstractItemView::selectionChanged(selected, deselected);
}

void
CQModelView::
keyboardSearch(const QString &search)
{
  if (! isHierarchical())
    return QAbstractItemView::keyboardSearch(search);

  auto start = sm_->currentIndex();

  bool again = true;

  auto pr = rowDatas_.begin();

  while (pr != rowDatas_.end()) {
    const auto &rowData = (*pr).second;

    auto index = model_->index(rowData.row, 0, rowData.parent);

    if (index == start)
      break;

    ++pr;
  }

  ++pr;

  if (pr == rowDatas_.end()) {
    pr = rowDatas_.begin();

    again = false;
  }

  while (pr != rowDatas_.end()) {
    const auto &rowData = (*pr).second;

    auto index = model_->index(rowData.row, 0, rowData.parent);

    auto str = model_->data(index, Qt::DisplayRole).toString();

    if (str.startsWith(search)) {
      sm_->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
      return;
    }

    ++pr;
  }

  if (again) {
    pr = rowDatas_.begin();

    while (pr != rowDatas_.end()) {
      const auto &rowData = (*pr).second;

      auto index = model_->index(rowData.row, 0, rowData.parent);

      auto str = model_->data(index, Qt::DisplayRole).toString();

      if (str.startsWith(search)) {
        sm_->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        return;
      }

      if (index == start)
        break;

      ++pr;
    }
  }
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

  const auto &visColumnData = (*p).second;

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
    assert(column >= 0);

    const ColumnData &columnData = columnDatas_[uint(column)];

    int cw = (columnData.width > 0 ? columnData.width : 100);

    return cw;
  }
}

void
CQModelView::
setColumnWidth(int column, int width)
{
  assert(column >= 0);

  ColumnData &columnData = columnDatas_[uint(column)];

  if (columnData.width != width) {
    columnData.width = width;

    int cw = (columnData.width > 0 ? columnData.width : 100);

    hh_->resizeSection(column, cw);

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
}

bool
CQModelView::
columnHeatmap(int column) const
{
  assert(column >= 0);

  const ColumnData &columnData = columnDatas_[uint(column)];

  return columnData.heatmap;
}

void
CQModelView::
setColumnHeatmap(int column, bool heatmap)
{
  assert(column >= 0);

  ColumnData &columnData = columnDatas_[uint(column)];

  if (columnData.heatmap != heatmap) {
    columnData.heatmap = heatmap;

    redraw();
  }
}

//---

void
CQModelView::
setFreezeFirstColumn(bool b)
{
  if (freezeFirstColumn_ != b) {
    freezeFirstColumn_ = b;

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
}

//---

void
CQModelView::
setStretchLastColumn(bool b)
{
  if (stretchLastColumn_ != b) {
    stretchLastColumn_ = b;

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
}

//---

void
CQModelView::
setMultiHeaderLines(bool b)
{
  if (multiHeaderLines_ != b) {
    multiHeaderLines_ = b;

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
}

//---

void
CQModelView::
setShowVerticalHeader(bool b)
{
  if (showVerticalHeader_ != b) {
    showVerticalHeader_ = b;

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
}

void
CQModelView::
setVerticalType(VerticalType type)
{
  if (verticalType_ != type) {
    verticalType_ = type;

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
}

//---

void
CQModelView::
setHeaderOnBottom(bool b)
{
  headerOnBottom_ = b;

  state_.updateAll();

  redraw();
}

void
CQModelView::
setHeaderOnRight(bool b)
{
  headerOnRight_ = b;

  state_.updateAll();

  redraw();
}

//---

void
CQModelView::
setShowFilter(bool b)
{
  if (showFilter_ != b) {
    showFilter_ = b;

    state_.updateAll();

    redraw();

    if (showFilter_) {
      int nfe = int(filterEdits_.size());

      if (nfe > 0)
        filterEdits_[0]->setFocus();
    }

    emit stateChanged();
  }
}

//---

void
CQModelView::
setSortingEnabled(bool b)
{
  if (sortingEnabled_ != b) {
    sortingEnabled_ = b;

    hh_->setSortIndicatorShown(sortingEnabled_);

    auto *proxyModel = qobject_cast<QSortFilterProxyModel *>(model());

    if (sortingEnabled_) {
      if (proxyModel) {
        if (sortRole_ >= 0)
          proxyModel->setSortRole(sortRole_);
      }

      sortByColumn(hh_->sortIndicatorSection(), hh_->sortIndicatorOrder());
    }
    else {
      if (proxyModel) {
        sortRole_ = proxyModel->sortRole();

        proxyModel->setSortRole(Qt::InitialSortOrderRole);

        proxyModel->invalidate();
      }
    }

    redraw();
  }
}

void
CQModelView::
setShowGrid(bool b)
{
  showGrid_ = b;

  redraw();
}

void
CQModelView::
setShowHHeaderLines(bool b)
{
  showHHeaderLines_ = b;

  redraw();
}

void
CQModelView::
setShowVHeaderLines(bool b)
{
  showVHeaderLines_ = b;

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
setCornerButtonEnabled(bool enabled)
{
  if (enabled != cornerWidget_->isEnabled()) {
    cornerWidget_->setEnabled(enabled);

    redraw();
  }
}

//---

void
CQModelView::
setIndentation(int i)
{
  if (indentation_ != i) {
    indentation_ = i;

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
}

//---

void
CQModelView::
setRootIsDecorated(bool b)
{
  if (rootIsDecorated_ != b) {
    rootIsDecorated_ = b;

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
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

  emit stateChanged();
}

void
CQModelView::
modelChangedSlot()
{
  state_.updateAll();

  autoFitted_ = false;

  redraw();

  emit stateChanged();
}

// update geometry
// depends
//   font, margins, header sizes, filter, viewport size, scrollbars, visible columns
void
CQModelView::
updateWidgetGeometries()
{
  if (! state_.updateGeometries)
    return;

  state_.updateGeometries = false;

  //---

#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::updateWidgetGeometries");
#endif

  updateVisColumns();

  //---

  int fh = paintData_.fm.height();

  globalRowData_.headerHeight = 0;

  using ColumnStrs = std::map<int, QStringList>;

  ColumnStrs columnStrs;
  int        ncr = 0;

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    auto data = model_->headerData(c, Qt::Horizontal, Qt::DisplayRole);

    auto str = data.toString();

    if (isMultiHeaderLines()) {
      auto strs = str.split(QRegExp("\n|\r\n|\r"));

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

  using RowStrs = std::map<int, QStringList>;

  if (isMultiHeaderLines()) {
    // pad from front (lines are bottom to top)
    for (auto &pcs : columnStrs) {
      auto &strs = pcs.second;

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
      int   r    = prs.first;
      auto &strs = prs.second;

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
  globalColumnData_.headerWidth  = paintData_.fm.horizontalAdvance("X") + 2*globalRowData_.margin;
  globalRowData_   .height       = fh + globalRowData_.margin;

  // space for headers
  int filterHeight = (isShowFilter() ? fh + 8 : 0);

  int lm = 0;

  if (isShowVerticalHeader())
    lm = std::max(globalColumnData_.headerWidth, globalRowData_.vheaderWidth);

  int mx1 = lm                                        , mx2 = 0;
  int my1 = globalRowData_.headerHeight + filterHeight, my2 = 0;

  if (isHeaderOnRight())
    std::swap(mx1, mx2);

  if (isHeaderOnBottom())
    std::swap(my1, my2);

  setViewportMargins(mx1, my1, mx2, my2);

  //---

  // inner viewport
  auto vrect = viewport()->geometry();

  //---

  cornerWidget_->setVisible(isShowVerticalHeader());

  int cx = 0;
  int cy = 0;

  if (isHeaderOnRight())
    cx = vrect.right();

  if (isHeaderOnBottom())
    cy = vrect.bottom();

  cornerWidget_->setGeometry(cx, cy, lm, globalRowData_.headerHeight);

  //---

  int hhx = vrect.left();
  int hhy = vrect.top() - globalRowData_.headerHeight;

  if (isHeaderOnBottom())
    hhy = vrect.bottom();

  hh_->setGeometry(hhx, hhy, vrect.width(), globalRowData_.headerHeight);

  //---

  int vhx = vrect.left() - lm;
  int vhy = vrect.top();

  if (isHeaderOnRight())
    vhx = vrect.right();

  vh_->setGeometry(vhx, vhy, lm, vrect.height());

  //---

  const_cast<CQModelView *>(this)->updateScrollBars();
  const_cast<CQModelView *>(this)->updateVisColumns();

  //---

  int nfe = int(filterEdits_.size());

  while (nfe > nvc_) {
    delete filterEdits_.back();

    filterEdits_.pop_back();

    --nfe;
  }

  while (nfe < nvc_) {
    auto *le = new CQModelViewFilterEdit(this);
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

      auto *le = filterEdits_[uint(ic)];

      le->setColumn(c);

      int cw = columnWidth(c);

      le->setGeometry(x, 0, cw, filterHeight);

      le->setVisible(true);

      auto data = model_->headerData(ic, Qt::Horizontal, Qt::DisplayRole);

      le->setPlaceholderText(data.toString());

      x += cw;

      ++ic;
    }
  }
  else {
    for (int c = 0; c < nvc_; ++c) {
      auto *le = filterEdits_[uint(c)];

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

    const ColumnData &columnData = columnDatas_[uint(c)];

    int cw = (columnData.width > 0 ? columnData.width : 100);

    w += cw;
  }

  //---

  bool vchanged = false;
  bool hchanged = false;

  int ah = viewport()->height();

  if (h > ah) {
    scrollData_.nv = (ah + rowHeight - 1)/rowHeight;

    int maxv = nvr_ - scrollData_.nv;

    if (! vs_->isVisible() || scrollData_.nv != vs_->pageStep() || maxv != vs_->maximum()) {
      vs_->setPageStep(scrollData_.nv);
      vs_->setRange(0, nvr_ - scrollData_.nv);

      vs_->setVisible(true);

      vchanged = true;
    }
  }
  else {
    scrollData_.nv = -1;

    if (vs_->isVisible()) {
      vs_->setVisible(false);

      vchanged = true;
    }
  }

  //---

  int aw = viewport()->width();

  if (w > aw) {
    int maxh = w - aw;

    if (! hs_->isVisible() || aw != hs_->pageStep() || maxh != hs_->maximum()) {
      hs_->setPageStep(aw);
      hs_->setRange(0, w - aw);

      hs_->setVisible(true);

      hchanged = true;
    }
  }
  else {
    if (hs_->isVisible()) {
      hs_->setVisible(false);

      hchanged = true;
    }
  }

  if (hchanged || vchanged)
    scrollTo(currentIndex());
}

void
CQModelView::
updateCurrentIndices()
{
  auto ind = sm_->currentIndex();

  if (ind.isValid()) {
    auto hind = model_->index(0, ind.column(), QModelIndex());
    auto vind = model_->index(ind.row   (), 0, ind.parent());

    hsm_->setCurrentIndex(hind, QItemSelectionModel::NoUpdate);
    vsm_->setCurrentIndex(vind, QItemSelectionModel::NoUpdate);

    auto p = (vsm_->currentIndex().isValid() ? indRow_.find(vsm_->currentIndex()) : indRow_.end());

    currentFlatRow_ = (p != indRow_.end() ? (*p).second : -1);
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
  if (! model_)
    return;

  //---

  // get selected columns and rows
  const auto selection = sm_->selection();

  std::set<int>         columns;
  std::set<QModelIndex> rows;

  for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
    const auto &range = *it;

    for (int c = range.left(); c <= range.right(); ++c)
      columns.insert(c);

    for (int r = range.top(); r <= range.bottom(); ++r) {
      auto ind = model_->index(r, 0, range.parent());

      rows.insert(ind);
    }
  }

  //---

  // select horizontal header columns
  QItemSelection hselection;

  for (auto &c : columns) {
    auto hind = model_->index(0, c, QModelIndex());

    hselection.select(hind, hind);
  }

  if (hsm_->model())
    hsm_->select(hselection, QItemSelectionModel::ClearAndSelect);

  //---

  // select vertical header rows
  QItemSelection vselection;

  for (auto &r : rows)
    vselection.select(r, r);

  if (vsm_->model())
    vsm_->select(vselection, QItemSelectionModel::ClearAndSelect);

  //---

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

  //---

  QPainter painter(this->viewport());

//painter.setClipRegion(e->region());

  paintRect_ = e->rect();

  //---

  auto c = palette().color(QPalette::Base);

  painter.fillRect(viewport()->rect(), c);

  auto isDarkColor = [](const QColor &c) {
    int gray = qGray(c.red(), c.green(), c.blue());
    return (gray <= 127);
  };

  isDark_ = isDarkColor(c);

  //---

  paintData_.fm = QFontMetrics(font());

  //---

#if 0
  int nr = nr_;
  int nc = nc_;
#endif

  if (model_) {
    auto parent = rootIndex();

    nr_ = model_->rowCount   (parent);
    nc_ = model_->columnCount(parent);
  }
  else {
    nr_ = 0;
    nc_ = 0;
  }

  if (nc_ != int(columnDatas_.size())) {
    columnDatas_.resize(uint(nc_));
  }

  //---

#if 0
  // TODO: needed, model signals should handle this
  if (! state_.updateScrollBars)
    state_.updateScrollBars = (nc_ != nc || nr_ !=  nr);
#endif

  //---

  if (autoFitOnShow_) {
    if (! autoFitted_) {
      autoFitted_ = true;

      fitAllColumnsSlot();
    }
  }

  //---

  if (isShowGrid())
    initDrawGrid();

  //---

  drawCells(&painter);

  //---

  drawAlternateEmpty(&painter);

  //---

  if (isShowGrid())
    drawGrid(&painter);
}

void
CQModelView::
drawAlternateEmpty(QPainter *painter)
{
  QStyleOptionViewItem opt;

  bool alternateEmpty =
    style()->styleHint(QStyle::SH_ItemView_PaintAlternatingRowColorsForEmptyArea, &opt, this);

  if (alternateEmpty) {
    int alternate = 0;

    if (nvr_ > 0) {
      int flatRow = visFlatRows_.back();

      auto pr = rowDatas_.find(flatRow);
      assert(pr != rowDatas_.end());

      const auto &rowData = (*pr).second;

      //---

      auto pp = visRowDatas_.find(rowData.parent);
      assert(pp != visRowDatas_.end());

      const auto &rowVisRowDatas = (*pp).second;

      auto ppr = rowVisRowDatas.find(rowData.row);
      assert(ppr != rowVisRowDatas.end());

      const auto &visRowData = (*ppr).second;

      alternate = (visRowData.flatRow & 1);
    }

    int y = nvr_*paintData_.rowHeight;

    while (y < paintData_.vh) {
      alternate = 1 - alternate;

      if (alternate)
        setRoleBrush(painter, ColorRole::AlternateBg);
      else
        setRoleBrush(painter, ColorRole::Base);

      opt.rect = QRect(0, y, paintData_.vw, paintData_.rowHeight);

      painter->fillRect(opt.rect, painter->brush());

      y += paintData_.rowHeight;
    }
  }
}

void
CQModelView::
initDrawGrid() const
{
  QStyleOptionViewItem option;

  int gridHint = style()->styleHint(QStyle::SH_Table_GridLineColor, &option, this);

  auto gridColor = QColor(static_cast<QRgb>(gridHint));

  paintData_.gridPen = QPen(gridColor, 0, gridStyle());
}

void
CQModelView::
drawGrid(QPainter *painter)
{
  setRolePen(painter, ColorRole::GridFg);

  for (const auto &pp : visRowDatas_) {
    const auto &rowVisRowDatas = pp.second;

    for (const auto &pr : rowVisRowDatas) {
      const auto &visRowData = pr.second;

      int x1 = visRowData.rect.left();
//    int y1 = visRowData.rect.top();
      int y2 = visRowData.rect.bottom() - 1;

      painter->drawLine(x1, y2, paintData_.vw - x1, y2);
    }
  }

  int y1 = 0;

  for (const auto &pv : visColumnDatas_) {
    const auto &visColumnData = pv.second;

    if (! visColumnData.visible)
      continue;

//  int x1 = visColumnData.rect.left ();
    int x2 = visColumnData.rect.right();

    painter->drawLine(x2, y1, x2, paintData_.vh - y1);
  }
}

void
CQModelView::
drawCells(QPainter *painter) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawCells");
#endif

  const_cast<CQModelView *>(this)->updateScrollBars      ();
  const_cast<CQModelView *>(this)->updateVisCells        ();
  const_cast<CQModelView *>(this)->updateWidgetGeometries();

  //---

  paintData_.reset();

  paintData_.vrect     = viewport()->rect();
  paintData_.vw        = paintData_.vrect.width ();
  paintData_.vh        = paintData_.vrect.height();
  paintData_.margin    = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, hh_);
  paintData_.rowHeight = this->rowHeight(0);

  if (iconSize().isValid()) {
    paintData_.decorationSize = iconSize();
  }
  else {
    int pm = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);

    paintData_.decorationSize = QSize(pm, pm);
  }

  paintData_.showDecorationSelected =
    style()->styleHint(QStyle::SH_ItemView_ShowDecorationSelected, nullptr, this);

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

    const auto &rowData = (*pr).second;

    //---

    auto pp = visRowDatas_.find(rowData.parent);
    if (pp == visRowDatas_.end()) continue;

    const auto &rowVisRowDatas = (*pp).second;

    auto ppr = rowVisRowDatas.find(rowData.row);
    if (ppr == rowVisRowDatas.end()) continue;

    const auto &visRowData = (*ppr).second;

    //---

    drawRow(painter, rowData.row, rowData.parent, visRowData);
  }
}

void
CQModelView::
drawCellsSelection(QPainter *painter) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawCellsSelection");
#endif

  const_cast<CQModelView *>(this)->updateVisCells();

  // draw selection
  auto drawSelection = [&](const QRect &r) {
    setRolePen  (painter, ColorRole::SelectionFg, 0.7);
    setRoleBrush(painter, ColorRole::SelectionBg, 0.3);

    painter->fillRect(r, painter->brush());

    setRoleBrush(painter, ColorRole::None);

    painter->drawRect(r.adjusted(0, 0, -1, -1));
  };

  const auto selection = sm_->selection();

  if (selection.empty())
    return;

  if (isHierarchical()) {
    const_cast<CQModelView *>(this)->updateHierSelection(selection);

    struct CLargestRectData {
      CLargestRectData(const CellAreas &cellAreas, int nc) :
       cellAreas(cellAreas), nc(nc) {
      }

      int getValue(int x, int y) const {
        const auto *vc = cellAreas[uint(y*nc + x)];

        return (vc && vc->selected ? 1 : 0);
      }

      const CellAreas& cellAreas;
      int              nc { 0 };
    };

    using LargestRect = CLargestRect<CLargestRectData, int>;

    for (auto &p : selDepthHierCellAreas_) {
      const auto &hierCellAreas = p.second;

      for (auto &p1 : hierCellAreas) {
        // selection in flat hierarchical area
        const auto &cellAreas = p1.second;

        // vis grid
        auto na = cellAreas.size();
        if (! na) continue;

        //---

        // reset selected cells
        for (auto &vc : cellAreas) {
          if (vc)
            vc->selected = true;
        }

        //---

        // draw largest rectangle and remove from set until all cells processed
        int nr = int(na/size_t(nc_));

        CLargestRectData largestRectData(cellAreas, nc_);

        LargestRect largestRect(largestRectData, nc_, nr);

        while (true) {
          LargestRect::Rect lrect = largestRect.largestRect(1);

          if (lrect.width <= 0 || lrect.height <= 0)
            break;

          //---

          int r1 = lrect.top;
          int r2 = lrect.top + lrect.height - 1;
          int c1 = lrect.left;
          int c2 = lrect.left + lrect.width - 1;

          const auto *vc1 = cellAreas[uint(r1*nc_ + c1)];
          const auto *vc2 = cellAreas[uint(r2*nc_ + c2)];
          assert(vc1 && vc2);

          int x1 = vc1->rect.left  ();
          int y1 = vc1->rect.top   ();
          int x2 = vc2->rect.right ();
          int y2 = vc2->rect.bottom();

          QRect rect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

          drawSelection(rect);

          //---

          for (int r = 0; r < lrect.height; ++r) {
            int r1 = lrect.top + r;

            for (int c = 0; c < lrect.width; ++c) {
              int c1 = lrect.left + c;

              auto *vc = cellAreas[uint(r1*nc_ + c1)];

              if (vc)
                vc->selected = false;
            }
          }
        }
      }
    }

    for (auto &cp : visCellDatas_) {
      const auto &visCellData = cp.second;

      if (visCellData.selected)
        drawSelection(visCellData.rect);
    }
  }
  else {
    for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
      const auto &range = *it;

      auto pc1 = visColumnDatas_.find(range.left ());
      auto pc2 = visColumnDatas_.find(range.right());

      int x1 = paintData_.vrect.left ();
      int x2 = paintData_.vrect.right();

      if (pc1 != visColumnDatas_.end()) {
        const auto &visColumnData1 = (*pc1).second;

        x1 = visColumnData1.rect.left ();
      }

      if (pc2 != visColumnDatas_.end()) {
        const auto &visColumnData2 = (*pc2).second;

        x2 = visColumnData2.rect.right();
      }

      //---

      auto p = visRowDatas_.find(range.parent());
      if (p == visRowDatas_.end()) return;

      const auto &rowVisRowDatas = (*p).second;

      auto pt = rowVisRowDatas.find(range.top());
      auto pb = rowVisRowDatas.find(range.bottom());

      int y1 = paintData_.vrect.top();
      int y2 = paintData_.vrect.bottom();

      if (pt != rowVisRowDatas.end()) {
        const auto &visRowData1 = (*pt).second;

        y1 = visRowData1.rect.top();
      }

      if (pb != rowVisRowDatas.end()) {
        const auto &visRowData2 = (*pb).second;

        y2 = visRowData2.rect.bottom();
      }

      QRect r(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

      drawSelection(r);
    }
  }
}

void
CQModelView::
updateHierSelection(const QItemSelection &selection)
{
  // update grid of cells selected per depth and parent flat row
  if (state_.updateSelection) {
    state_.updateSelection = false;

    selDepthHierCellAreas_.clear();

    for (auto &cp : visCellDatas_) {
      auto &visCellData = cp.second;

      visCellData.selected = false;
    }

    for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
      const auto &range = *it;

      for (int c = range.left(); c <= range.right(); ++c) {
        for (int r = range.top(); r <= range.bottom(); ++r) {
          auto ind = model_->index(r, c, range.parent());

          auto pc = visCellDatas_.find(ind);
          if (pc == visCellDatas_.end()) continue;

          auto &visCellData = (*pc).second;

          visCellData.selected = true;
        }
      }
    }

    for (auto &cp : visCellDatas_) {
      auto &vc = cp.second;

      if (vc.selected) {
        CellAreas &cellAreas = selDepthHierCellAreas_[-vc.depth][vc.parentFlatRow];

        if (cellAreas.empty())
          cellAreas.resize(uint(vc.nr*nc_));

        cellAreas[uint(vc.r*nc_ + vc.c)] = &vc;
      }
    }
  }
}

void
CQModelView::
drawHHeader(QPainter *painter) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawHHeader");
#endif

  const_cast<CQModelView *>(this)->updateScrollBars      ();
  const_cast<CQModelView *>(this)->updateVisColumns      ();
  const_cast<CQModelView *>(this)->updateWidgetGeometries();

  //---

  paintData_.reset();

  paintData_.vrect     = hh_->viewport()->rect();
  paintData_.vw        = paintData_.vrect.width ();
  paintData_.vh        = paintData_.vrect.height();
  paintData_.margin    = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, hh_);
  paintData_.rowHeight = this->rowHeight(0);

  if (showHHeaderLines())
    initDrawGrid();

  //---

  painter->fillRect(paintData_.vrect, roleColor(ColorRole::HeaderBg));

  //---

  painter->save();

  for (const auto &pv : visColumnDatas_) {
    int         c             = pv.first;
    const auto &visColumnData = pv.second;

    if (! visColumnData.visible)
      continue;

    bool valid = (c != freezeColumn_);

    if (valid) {
      if (freezeColumn_ >= 0) {
        int x  = visColumnData.rect.left () +   paintData_.margin;
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

    const auto &visColumnData = (*p).second;

    painter->setClipRect(QRect(0, 0, freezeWidth_, globalRowData_.headerHeight));

    drawHHeaderSection(painter, freezeColumn_, visColumnData);
  }

  //---

  // draw span text
  if (isMultiHeaderLines()) {
    int fh = paintData_.fm.height();

    for (const auto &pr : rowColumnSpans_) {
      int         r           = pr.first;
      const auto &columnSpans = pr.second;

      int y1 = r*(fh + 1) + globalRowData_.margin;
      int y2 = y1 + fh;

      for (const auto &span : columnSpans) {
        int c1 = span.first;
        int c2 = span.second;

        auto ps1 = visColumnDatas_.find(c1);
        auto ps2 = visColumnDatas_.find(c2);
        if (ps1 == visColumnDatas_.end() || ps2 == visColumnDatas_.end()) continue;

        const auto &visColumnData1 = (*ps1).second;
        const auto &visColumnData2 = (*ps2).second;

        QRect rect(visColumnData1.rect.left(), y1,
                   visColumnData2.rect.right() - visColumnData1.rect.left() + 1, y2 - y1 + 1);

        auto data = model_->headerData(c1, Qt::Horizontal, Qt::DisplayRole);

        auto str = data.toString();

        auto strs = str.split(QRegExp("\n|\r\n|\r"));

        setRolePen(painter, ColorRole::HeaderFg);

        painter->drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, strs[r]);

        //---

        setRolePen(painter, ColorRole::HeaderLineFg);

        painter->drawLine(visColumnData2.rect.topRight  (), visColumnData2.rect.bottomRight());
        painter->drawLine(               rect.bottomLeft(),                rect.bottomRight());
      }
    }
  }

  //---

  painter->restore();
}

void
CQModelView::
drawHHeaderSection(QPainter *painter, int c, const VisColumnData &visColumnData) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawHHeaderSection");
#endif

  auto ind = model_->index(0, c, QModelIndex());

  //---

  auto data = model_->headerData(c, Qt::Horizontal, Qt::DisplayRole);

  auto str = data.toString();

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

  if (c == hsm_->currentIndex().column() && hsm_->currentIndex().isValid())
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
  auto ivar = model_->headerData(c, Qt::Horizontal, Qt::DecorationRole);

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
    int fh = paintData_.fm.height();

    for (int r = 0; r < ns; ++r) {
      bool inSpan = false;

      auto ps = rowColumnSpans_.find(r);

      if (ps != rowColumnSpans_.end()) {
        const auto &columnSpans = (*ps).second;

        for (const auto &span : columnSpans) {
          if (c >= span.first && c <= span.second) {
            inSpan = true;
            break;
          }
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

  for (int r = 0; r < ns; ++r) {
    const RectSpan &rectSpan = rowRects[uint(r)];

    //---

    auto subopt = option;

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
    auto subopt = option;

    subopt.rect = style()->subElementRect(QStyle::SE_HeaderArrow, &option, hh_);

    style()->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &subopt, painter, hh_);
  }

  //---

  auto fullRect = visColumnData.rect.adjusted(-paintData_.margin, 0, paintData_.margin, 0);

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

  if (showHHeaderLines()) {
    setRolePen(painter, ColorRole::GridFg);

    painter->drawLine(option.rect.topRight(), option.rect.bottomRight());
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
drawVHeader(QPainter *painter) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawVHeader");
#endif

  const_cast<CQModelView *>(this)->updateScrollBars      ();
  const_cast<CQModelView *>(this)->updateVisColumns      ();
  const_cast<CQModelView *>(this)->updateVisRows         ();
  const_cast<CQModelView *>(this)->updateWidgetGeometries();

  //---

  paintData_.reset();

  paintData_.vrect     = vh_->viewport()->rect();
  paintData_.vw        = paintData_.vrect.width ();
  paintData_.vh        = paintData_.vrect.height();
  paintData_.margin    = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, hh_);
  paintData_.rowHeight = this->rowHeight(0);

  if (showVHeaderLines())
    initDrawGrid();

  //---

  painter->fillRect(paintData_.vrect, roleColor(ColorRole::HeaderBg));

  //---

  auto p = (vsm_->currentIndex().isValid() ? indRow_.find(vsm_->currentIndex()) : indRow_.end());

  int currentRow = (p != indRow_.end() ? (*p).second : -1);

  // draw header area for each visible flat row
  for (const auto &flatRow : visFlatRows_) {
    auto pr = rowDatas_.find(flatRow);
    assert(pr != rowDatas_.end());

    const auto &rowData = (*pr).second;

    //---

    auto pp = visRowDatas_.find(rowData.parent);
    if (pp == visRowDatas_.end()) continue;

    const auto &rowVisRowDatas = (*pp).second;

    auto ppr = rowVisRowDatas.find(rowData.row);
    if (ppr == rowVisRowDatas.end()) continue;

    const auto &visRowData = (*ppr).second;

    //---

    auto ind = model_->index(rowData.row, 0, rowData.parent);

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

      setRoleBrush(painter, ColorRole::Base);
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

    //---

    if (showVHeaderLines()) {
      setRolePen(painter, ColorRole::GridFg);

      int x1 = option.rect.left  ();
      int x2 = option.rect.right ();
      int y2 = option.rect.bottom();

      painter->drawLine(x1, y2 - 1, x2, y2 - 1);
    }

    //---

    if (! isHierarchical() && globalRowData_.vheaderWidth > 0) {
      int m = globalRowData_.margin;

      int dy = (option.rect.height() - paintData_.fm.height() + 1)/2;

      dy += paintData_.fm.ascent();

      if      (verticalType() == VerticalType::TEXT) {
        auto data = model_->headerData(rowData.row, Qt::Vertical, Qt::DisplayRole);

        if (data.isValid()) {
          auto text = data.toString();

          if (text.length()) {
            setRolePen(painter, ColorRole::HeaderFg);

            painter->drawText(option.rect.topLeft() + QPoint(m, dy), text);
          }
        }
      }
      else if (verticalType() == VerticalType::NUMBER) {
        setRolePen(painter, ColorRole::HeaderFg);

        auto text = QString::number(rowData.row + 1);

        int dx = globalRowData_.vheaderWidth - 2*m - paintData_.fm.horizontalAdvance(text);

        painter->drawText(option.rect.topLeft() + QPoint(m + dx, dy), text);
      }
    }
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
    int         c             = pv.first;
    const auto &visColumnData = pv.second;

    if (! visColumnData.visible)
      continue;

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

    const auto &visColumnData = (*pv).second;

    int y = visRowData.rect.top();

    painter->setClipRect(QRect(0, y, freezeWidth_, paintData_.rowHeight));

    drawCell(painter, r, freezeColumn_, parent, visRowData, visColumnData);
  }

  painter->restore();
}

void
CQModelView::
drawCell(QPainter *painter, int r, int c, const QModelIndex &parent,
         const VisRowData &visRowData, const VisColumnData &) const
{
#ifdef CQ_MODEL_VIEW_TRACE
  CQPerfTrace trace("CQModelView::drawCell");
#endif

  auto ind = model_->index(r, c, parent);

  //--

  if (isHierarchical() && c == 0) {
    auto pc = ivisCellDatas_.find(ind);
    if (pc == ivisCellDatas_.end()) return;

    const auto &ivisCellData = (*pc).second;

    //---

    bool children     = visRowData.children;
    bool expanded     = visRowData.expanded;
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
          setRoleBrush(painter, ColorRole::Base);
      }

      painter->fillRect(opt.rect, painter->brush());

      if (opt.features & QStyleOptionViewItem::Alternate)
        setRoleBrush(painter, ColorRole::AlternateBg);
      else
        setRoleBrush(painter, ColorRole::Base);
    };

    //---

    fillIndicatorBackground();

    style()->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, painter, this);
  }

  //--

  auto pc = visCellDatas_.find(ind);
  if (pc == visCellDatas_.end()) return;

  const auto &visCellData = (*pc).second;

  //---

  bool isNumeric = isNumericColumn(c);

  //--

  // init style data
  QStyleOptionViewItem option;

  option.init(this);

  option.state &= ~QStyle::State_MouseOver;

  if      (selectionBehavior() == SelectRows) {
    if (ind.row() == mouseData_.moveData.ind.row())
      option.state |= QStyle::State_MouseOver;
  }
  else if (selectionBehavior() == SelectColumns) {
    if (ind.column() == mouseData_.moveData.ind.column())
      option.state |= QStyle::State_MouseOver;
  }
  else {
    if (ind == mouseData_.moveData.ind)
      option.state |= QStyle::State_MouseOver;
  }

  if (ind == currentIndex() && currentIndex().isValid())
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
  option.fontMetrics            = QFontMetrics(option.font);
  option.decorationSize         = paintData_.decorationSize;
  option.decorationPosition     = QStyleOptionViewItem::Left;
  option.decorationAlignment    = Qt::AlignCenter;
  option.displayAlignment       = (isNumeric ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
  option.textElideMode          = textElideMode();
  option.showDecorationSelected = paintData_.showDecorationSelected;

  //---

#if 0
  auto fillBackground = [&]() {
    auto bgVar = model_->data(ind, Qt::BackgroundRole);

    if (bgVar.canConvert<QBrush>())
      painter->setBrush(qvariant_cast<QBrush>(bgVar).color());

    //---

    if (option.state & QStyle::State_MouseOver) {
      setRolePen  (painter, ColorRole::MouseOverFg);
      setRoleBrush(painter, ColorRole::MouseOverBg);
    }
    else {
      setRolePen(painter, ColorRole::Text);

      if (option.features & QStyleOptionViewItem::Alternate)
        setRoleBrush(painter, ColorRole::AlternateBg);
      else
        setRoleBrush(painter, ColorRole::Base);
    }

    painter->fillRect(option.rect, painter->brush());

    if (option.features & QStyleOptionViewItem::Alternate)
      setRoleBrush(painter, ColorRole::AlternateBg);
    else
      setRoleBrush(painter, ColorRole::Base);
  };
#endif

  //---

  // set font
  auto fontVar = ind.data(Qt::FontRole);

  if (fontVar.isValid()){
    option.font        = qvariant_cast<QFont>(fontVar).resolve(option.font);
    option.fontMetrics = QFontMetrics(option.font);
  }

  // set text alignment
  auto alignVar = ind.data(Qt::TextAlignmentRole);

  if (alignVar.isValid())
    option.displayAlignment = Qt::Alignment(alignVar.toInt());

  // set foreground brush
  auto fgVar = ind.data(Qt::ForegroundRole);

  if (fgVar.canConvert<QBrush>())
    option.palette.setBrush(QPalette::Text, qvariant_cast<QBrush>(fgVar));

  // disable style animations for checkboxes etc. within itemview
  option.styleObject = nullptr;

  //---

  auto *delegate = itemDelegate();

  if (! delegate) {
    drawCellBackground(painter, option, ind);

    // draw cell value
    setRolePen(painter, ColorRole::HeaderFg);

    auto data = model_->data(ind, Qt::DisplayRole);

    auto str = data.toString();

    auto textRect = option.rect.adjusted(4, 0, -4, 0);

    painter->save();

    painter->setPen(option.palette.brush(QPalette::Text).color());

    painter->setClipRect(textRect);

#if 1
    QTextOption textOption;

    textOption.setAlignment(option.displayAlignment);

    QTextLayout textLayout(str, option.font);

    textLayout.setTextOption(textOption);

    (void) viewItemTextLayout(textLayout, textRect.width());

    int nl = textLayout.lineCount();

    int dy = (option.rect.height() - paintData_.fm.height() + 1)/2;

    dy += paintData_.fm.ascent();

    for (int i = 0; i < nl; ++i) {
      auto line = textLayout.lineAt(i);

      int start  = line.textStart();
      int length = line.textLength();

      auto text = textLayout.text().mid(start, length);

      painter->drawText(textRect.topLeft() + QPoint(0, dy), text);

      //line.draw(painter, textRect.topLeft());

      break;
    }
#else
    painter->drawText(textRect, option.displayAlignment, str);
#endif

    painter->restore();
  }
  else {
    //style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &option, painter, this);

    //drawCellBackground(painter, option, ind);

    auto *delegate  = itemDelegate();
    auto *idelegate = qobject_cast<CQItemDelegate *>(delegate);

    if (idelegate) {
      const ColumnData &columnData = columnDatas_[uint(c)];

      idelegate->setHeatmap(columnData.heatmap);
    }

    if (option.state & QStyle::State_MouseOver) {
      if (selectionBehavior() != SelectRows && selectionBehavior() != SelectColumns)
        option.state |= QStyle::State_HasEditFocus;
    }

    delegate->paint(painter, option, ind);
  }
}

void
CQModelView::
drawCellBackground(QPainter *painter, const QStyleOptionViewItem &option,
                   const QModelIndex &ind) const
{
  auto bgVar = model_->data(ind, Qt::BackgroundRole);

 if (bgVar.canConvert<QBrush>())
   painter->setBrush(qvariant_cast<QBrush>(bgVar).color());

 //---

 if (option.state & QStyle::State_MouseOver) {
   setRolePen  (painter, ColorRole::MouseOverFg);
   setRoleBrush(painter, ColorRole::MouseOverBg);
 }
 else {
   setRolePen(painter, ColorRole::Text);

   if (option.features & QStyleOptionViewItem::Alternate)
     setRoleBrush(painter, ColorRole::AlternateBg);
   else
     setRoleBrush(painter, ColorRole::Base);
 }

 painter->fillRect(option.rect, painter->brush());

 if (option.features & QStyleOptionViewItem::Alternate)
   setRoleBrush(painter, ColorRole::AlternateBg);
 else
   setRoleBrush(painter, ColorRole::Base);
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

  int margin = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, hh_);

  freezeWidth_  = 0;
  freezeColumn_ = -1;

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    //---

    const ColumnData &columnData = columnDatas_[uint(c)];

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
  nmr_          = 0;
  nvr_          = 0;
  hierarchical_ = false;

  rowDatas_.clear();

  auto parent = rootIndex();

  initRowDatas(parent, nvr_, 0, -1);
}

void
CQModelView::
initRowDatas(const QModelIndex &parent, int &nvr, int depth, int parentFlatRow)
{
  int nr = (model_ ? model_->rowCount(parent) : 0);

  for (int r = 0; r < nr; ++r) {
    ++nmr_;

    if (isRowHidden(r, parent))
      continue;

    auto ind1 = model_->index(r, 0, parent);

    bool children = model_->hasChildren(ind1);
    bool expanded = (! ignoreExpanded_ ? isExpanded(ind1) : true);

    if (children)
      hierarchical_ = true;

    int parentFlatRow1 = nvr;

    rowDatas_[nvr]  = RowData(parent, r, nr, nvr, children, expanded, depth, parentFlatRow);
    indRow_  [ind1] = nvr;

    ++nvr;

    if (children && expanded) {
      if (model_ && model_->canFetchMore(parent))
        model_->fetchMore(parent);

      initRowDatas(ind1, nvr, depth + 1, parentFlatRow1);
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

  int borderHeight = visualBorderRows_*rowHeight;

  int vy1 = visualRect_.top   () - borderHeight;
  int vy2 = visualRect_.bottom() + borderHeight;

  //---

  int y1 = 0;

  if (vs_->isVisible() && vs_->value() == vs_->maximum())
    y1 = -(nvr_*rowHeight - paintData_.vh);
  else
    y1 = -verticalOffset()*rowHeight;

  int hw = std::max(globalColumnData_.headerWidth, globalRowData_.vheaderWidth);

  for (const auto &pr : rowDatas_) {
    const auto &rowData = pr.second;

    //---

    int y2 = y1 + rowHeight;

    VisRowData &visRowData = visRowDatas_[rowData.parent][rowData.row];

    visRowData.rect = QRect(0, y1, hw, y2 - y1 + 1);

    visRowData.depth         = rowData.depth;
    visRowData.parentFlatRow = rowData.parentFlatRow;
    visRowData.flatRow       = rowData.flatRow;
    visRowData.r             = rowData.row;
    visRowData.nr            = rowData.numRows;
    visRowData.alternate     = (visRowData.flatRow & 1);
    visRowData.children      = rowData.children;
    visRowData.expanded      = rowData.expanded;
    visRowData.visible       = ! (y1 > visualRect_.bottom() || y2 < visualRect_.top());
    visRowData.evisible      = ! (y1 > vy2 || y2 < vy1);
    visRowData.depth         = rowData.depth;

    //---

    if (visRowData.evisible)
      visFlatRows_.push_back(visRowData.flatRow);

    //---

    y1 = y2;
  }

  //---

  globalRowData_.vheaderWidth = -1;

  if      (verticalType() == VerticalType::TEXT) {
    for (int r = 0; r < nr_; ++r) {
      auto data = (model_ ? model_->headerData(r, Qt::Vertical, Qt::DisplayRole) : QVariant());
      if (! data.isValid()) continue;

      auto str = data.toString();
      if (! str.length()) continue;

      globalRowData_.vheaderWidth =
        std::max(globalRowData_.vheaderWidth, paintData_.fm.horizontalAdvance(str));
    }

    globalRowData_.vheaderWidth += 2*globalRowData_.margin;
  }
  else if (verticalType() == VerticalType::NUMBER) {
    int n = (nr_ > 0 ? int(std::log10(nr_) + 1) : 1);

    globalRowData_.vheaderWidth = n*paintData_.fm.horizontalAdvance("8") + 2*globalRowData_.margin;
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
    int         c             = pc.first;
    const auto &visColumnData = pc.second;

    int x1 = visColumnData.rect.left () - paintData_.margin;
    int x2 = visColumnData.rect.right() + paintData_.margin;

    for (const auto &flatRow : visFlatRows_) {
      auto pr = rowDatas_.find(flatRow);
      assert(pr != rowDatas_.end());

      const auto &rowData = (*pr).second;

      //---

      const VisRowData &visRowData = visRowDatas_[rowData.parent][rowData.row];

      int y1 = visRowData.rect.top   ();
      int y2 = visRowData.rect.bottom();

      //---

      int indent = 0;

      if (isHierarchical() && c == 0) {
        auto ind = model_->index(rowData.row, c, rowData.parent);

        VisCellData &ivisCellData = ivisCellDatas_[ind];

        indent = (visRowData.depth + rootIsDecorated())*indentation();

        ivisCellData.depth         = rowData.depth;
        ivisCellData.parentFlatRow = rowData.parentFlatRow;
        ivisCellData.flatRow       = flatRow;
        ivisCellData.r             = rowData.row;
        ivisCellData.nr            = rowData.numRows;
        ivisCellData.c             = 0;
        ivisCellData.rect          = QRect(x1, y1, indent, y2 - y1 + 1);
        ivisCellData.selected      = false;
      }

      //---

      auto ind = model_->index(rowData.row, c, rowData.parent);

      VisCellData &visCellData = visCellDatas_[ind];

      if (visColumnData.last && isStretchLastColumn() && x2 < paintData_.vw)
        x2 = paintData_.vw - 1;

      int xi1 = x1 + indent;

      visCellData.depth         = rowData.depth;
      visCellData.parentFlatRow = rowData.parentFlatRow;
      visCellData.flatRow       = flatRow;
      visCellData.r             = rowData.row;
      visCellData.nr            = rowData.numRows;
      visCellData.c             = c;
      visCellData.rect          = QRect(xi1, y1, x2 - xi1 + 1, y2 - y1 + 1);
      visCellData.selected      = false;
    }
  }
}

//------

bool
CQModelView::
isNumericColumn(int c) const
{
  auto type = CQBaseModelType::STRING;

  auto tvar = model_->headerData(c, Qt::Horizontal, static_cast<int>(CQBaseModelRole::Type));
  if (! tvar.isValid()) return false;

  bool ok;
  type = static_cast<CQBaseModelType>(tvar.toInt(&ok));
  if (! ok) return false;

  //---

  return (type == CQBaseModelType::REAL || type == CQBaseModelType::INTEGER);
}

void
CQModelView::
editFilterSlot()
{
  auto *le = qobject_cast<CQModelViewFilterEdit *>(sender());
  assert(le);

  filterColumn(le->column());
}

void
CQModelView::
filterColumn(int column)
{
  assert(column >= 0);

  auto *le = filterEdits_[uint(column)];

  auto parent = rootIndex();

  auto filterStr = le->text().trimmed();

  if (filterStr.length()) {
    QRegExp regexp(filterStr, Qt::CaseSensitive, QRegExp::Wildcard);

    for (int r = 0; r < nr_; ++r) {
      auto ind = model_->index(r, column, parent);

      auto str = model_->data(ind, Qt::DisplayRole).toString();

      bool visible = (str == filterStr || regexp.exactMatch(str));

      setRowHidden(r, parent, ! visible);
    }
  }
  else {
    for (int r = 0; r < nr_; ++r) {
      setRowHidden(r, parent, false);
    }
  }

  state_.updateAll();

  redraw();

  emit stateChanged();
}

void
CQModelView::
hscrollSlot(int v)
{
  if (scrollData_.hpos != v) {
    scrollData_.hpos = v;

    state_.updateVisColumns = true;
    state_.updateVisCells   = true;

    redraw();
  }
}

void
CQModelView::
vscrollSlot(int v)
{
  if (scrollData_.vpos != v) {
    scrollData_.vpos = v;

    state_.updateVisRows   = true;
    state_.updateVisCells  = true;
    state_.updateSelection = true;

    redraw();
  }
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
  // mouse type for expand/collapse of tree
  if (isHierarchical()) {
    if (style()->styleHint(QStyle::SH_ListViewExpand_SelectMouseType, nullptr, this) ==
          QEvent::MouseButtonPress) {
      if (mouseData_.pressData.iind.isValid()) {
        setExpanded(mouseData_.pressData.iind, ! isExpanded(mouseData_.pressData.iind));
        return;
      }
    }
  }

  //---

  mouseData_.pressData.currentInd = sm_->currentIndex();

  // horizontal header section pressed
  if      (mouseData_.pressData.hsection >= 0) {
    if (! (mouseData_.modifiers & Qt::ShiftModifier)) {
      selectColumn(mouseData_.pressData.hsection, mouseData_.modifiers);

      redraw();
    }
  }
  // horizontal header section handle pressed
  else if (mouseData_.pressData.hsectionh >= 0) {
    const ColumnData &columnData = columnDatas_[uint(mouseData_.pressData.hsectionh)];

    int cw = (columnData.width > 0 ? columnData.width : 100);

    mouseData_.headerWidth = cw;
  }
  // vertical header section pressed
  else if (mouseData_.pressData.vsection >= 0) {
    if (! (mouseData_.modifiers & Qt::ShiftModifier)) {
      selectRow(mouseData_.pressData.vsection, mouseData_.modifiers);

      redraw();
    }
  }
  // table row/column selected
  else if (mouseData_.pressData.ind.isValid()) {
    if      (selectionBehavior() == SelectRows)
      selectRow(mouseData_.pressData.ind.row(), mouseData_.modifiers);
    else if (selectionBehavior() == SelectColumns)
      selectColumn(mouseData_.pressData.ind.column(), mouseData_.modifiers);
    else
      selectCell(mouseData_.pressData.ind, mouseData_.modifiers);

    redraw();
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
      //auto rrect = mouseData_.moveData.rect.united(moveData.rect);

      mouseData_.moveData = moveData;

      //redraw(rrect);
      redraw();
    }
  }
  else {
    if (moveData != mouseData_.moveData) {
      mouseData_.moveData = moveData;

      handleMouseMove();
    }
  }
}

void
CQModelView::
handleMouseMove()
{
  // horizontal header section pressed
  if      (mouseData_.pressData.hsection >= 0) {
    if (mouseData_.moveData.hsection < 0)
      return;

    selectColumnRange(mouseData_.pressData.hsection, mouseData_.moveData.hsection,
                      mouseData_.modifiers);

    redraw();
  }
  // horizontal header section handle pressed
  else if (mouseData_.pressData.hsectionh >= 0) {
    int dx = mouseData_.moveData.pos.x() - mouseData_.pressData.pos.x();

    ColumnData &columnData = columnDatas_[uint(mouseData_.pressData.hsectionh)];

    columnData.width = std::min(std::max(mouseData_.headerWidth + dx, 4), 9999);

    hh_->resizeSection(mouseData_.pressData.hsectionh, columnData.width);

    state_.updateAll();

    redraw();

    emit stateChanged();
  }
  // vertical header section pressed
  else if (mouseData_.pressData.vsection >= 0) {
    if (mouseData_.moveData.vsection < 0)
      return;

    selectRowRange(mouseData_.pressData.vsection, mouseData_.moveData.vsection,
                   mouseData_.modifiers);

    redraw();
  }
  // table row/column selected
  else if (mouseData_.pressData.ind.isValid() && mouseData_.moveData.ind.isValid()) {
    mouseData_.moveData.currentInd = sm_->currentIndex();

    // TODO: hierarchical will need more work
    if (isHierarchical()) {
      QRect rect(mouseData_.pressData.pos, mouseData_.moveData.pos);

      if (! (mouseData_.modifiers & Qt::ControlModifier))
        setSelection(rect, QItemSelectionModel::ClearAndSelect);
      else
        setSelection(rect, QItemSelectionModel::Select);

      sm_->setCurrentIndex(mouseData_.moveData.ind, QItemSelectionModel::NoUpdate);
    }
    else {
      selectCellRange(mouseData_.pressData.ind, mouseData_.moveData.ind,
                      mouseData_.modifiers);
    }

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

  cellPositionToIndex(mouseData_.releaseData);

  handleMouseRelease();

  mouseData_.pressData.reset();
  mouseData_.moveData .reset();
}

void
CQModelView::
handleMouseRelease()
{
  bool click = false;

  if      (mouseData_.pressData.iind.isValid())
    click = (mouseData_.pressData.iind == mouseData_.releaseData.iind);
  else if (mouseData_.pressData.ind.isValid())
    click = (mouseData_.pressData.ind == mouseData_.releaseData.ind);
  else if (mouseData_.pressData.hsection >= 0)
    click = (mouseData_.pressData.hsection == mouseData_.releaseData.hsection);
  else if (mouseData_.pressData.vsection >= 0)
    click = (mouseData_.pressData.vsection == mouseData_.releaseData.vsection);

  //---

  // vertical header section pressed
  if      (mouseData_.pressData.hsection >= 0) {
    if (mouseData_.modifiers & Qt::ShiftModifier) {
      int currentSection = -1;

      if (click) {
        auto start = mouseData_.pressData.currentInd;
        if (! start.isValid()) return;

        currentSection = start.column();
      }
      else {
        if (mouseData_.moveData.hsection < 0)
          return;

        currentSection = mouseData_.moveData.hsection;
      }

      selectColumnRange(mouseData_.pressData.hsection, currentSection, mouseData_.modifiers);

      redraw();
    }
  }
  // horizontal header section pressed
  else if (mouseData_.pressData.vsection >= 0) {
    if (mouseData_.modifiers & Qt::ShiftModifier) {
      int currentSection = -1;

      if (click) {
        auto start = mouseData_.pressData.currentInd;
        if (! start.isValid()) return;

        currentSection = start.row();
      }
      else {
        if (mouseData_.moveData.vsection < 0)
          return;

        currentSection = mouseData_.moveData.vsection;
      }

      selectRowRange(mouseData_.pressData.vsection, currentSection, mouseData_.modifiers);

      redraw();
    }
  }
  // table row/column selected
  else if (mouseData_.pressData.ind.isValid() && mouseData_.moveData.ind.isValid()) {
  }

  //---

  if (click) {
    if      (mouseData_.pressData.iind.isValid())
      emit clicked(mouseData_.pressData.iind);
    else if (mouseData_.pressData.ind.isValid())
      emit clicked(mouseData_.pressData.ind);
    else if (mouseData_.pressData.hsection >= 0) {
      hh_->emitSectionClicked(mouseData_.pressData.hsection);
    }
    else if (mouseData_.pressData.vsection >= 0) {
      vh_->emitSectionClicked(mouseData_.pressData.vsection);
    }
  }

  //---

  // mouse type for expand/collapse of tree
  if (isHierarchical()) {
    if (style()->styleHint(QStyle::SH_ListViewExpand_SelectMouseType, nullptr, this) ==
          QEvent::MouseButtonRelease) {
      if (mouseData_.pressData.iind.isValid()) {
        setExpanded(mouseData_.pressData.iind, ! isExpanded(mouseData_.pressData.iind));
        return;
      }
    }
  }
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
    auto persistent = QPersistentModelIndex(mouseData_.pressData.ind);

    if (edit(persistent, DoubleClicked, e))
      return;

    if (expandsOnDoubleClick())
      setExpanded(mouseData_.pressData.ind, ! isExpanded(mouseData_.pressData.ind));

    QStyleOptionViewItem option;

    if (style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, &option, this))
      emit activated(mouseData_.pressData.ind);
  }
  else if (mouseData_.pressData.iind.isValid()) {
    if (expandsOnDoubleClick())
      setExpanded(mouseData_.pressData.iind, ! isExpanded(mouseData_.pressData.iind));
  }
}

void
CQModelView::
handleMouseDoubleClick()
{
  if      (mouseData_.pressData.hsection >= 0) {
    if (headerEditor_) {
      headerEditor_->deleteLater();

      headerEditor_ = nullptr;
    }

    headerEditor_ = new CQModelViewHeaderEdit(this, mouseData_.pressData.hsection);

    headerEditor_->setGeometry(mouseData_.pressData.rect);

    auto text = model_->headerData(mouseData_.pressData.hsection, Qt::Horizontal, Qt::DisplayRole);

    headerEditor_->setText(text.toString());
    headerEditor_->setFocus();

    headerEditor_->show();
  }
  else if (mouseData_.pressData.hsectionh >= 0) {
    resizeColumnToContents(mouseData_.pressData.hsectionh);

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
  auto current = currentIndex();

  // search
  if      (e->key() == Qt::Key_Slash) {
    setShowFilter(! isShowFilter());
  }
  else if (e->key() == Qt::Key_Asterisk) {
    if (isHierarchical()) {
      std::vector<QModelIndex> parents;

      parents.push_back(current);

      while (! parents.empty()) {
        auto parent = parents.back();

        parents.pop_back();

        int nr = model()->rowCount(parent);

        for (int r = 0; r < nr; ++r) {
          auto child = model()->index(r, 0, parent);

          if (! child.isValid())
            break;

          parents.push_back(child);

          expand(child);
        }
      }

      expand(current);
    }
  }
  else if (e->key() == Qt::Key_Plus) {
    if (isHierarchical()) {
      expand(current);
    }
  }
  else if (e->key() == Qt::Key_Minus) {
    if (isHierarchical()) {
      collapse(current);
    }
  }

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

  auto *menu = new QMenu;

  //---

  addMenuActions(menu);

  //---

  (void) menu->exec(pos);

  delete menu;
}

void
CQModelView::
addMenuActions(QMenu *menu)
{
  int is = QFontMetrics(font()).height() + 6;

  //---

  int column = mouseData_.menuData.column();

  //---

  auto addMenu = [&](const QString &name) {
    auto *subMenu = new QMenu(name);

    menu->addMenu(subMenu);

    return subMenu;
  };

  auto addAction = [&](QMenu *menu, const QString &name, const char *slotName=nullptr) {
    auto *action = menu->addAction(name);

    if (slotName)
      connect(action, SIGNAL(triggered()), this, slotName);

    return action;
  };

  auto addCheckedAction = [&](QMenu *menu, const QString &name, bool checked,
                              const char *slotName=nullptr) {
    auto *action = menu->addAction(name);

    action->setCheckable(true);
    action->setChecked  (checked);

    if (slotName)
      connect(action, SIGNAL(triggered(bool)), this, slotName);

    return action;
  };

  auto addActionGroup = [&](QMenu *menu, const char *slotName) {
    auto *actionGroup = new QActionGroup(menu);

    connect(actionGroup, SIGNAL(triggered(QAction *)), this, slotName);

    return actionGroup;
  };

  //---

  auto *optionMenu = addMenu("Options");

  addCheckedAction(optionMenu, "Alternating Row Colors", alternatingRowColors(),
                   SLOT(alternatingRowColorsSlot(bool)));

  addCheckedAction(optionMenu, "Freeze First Column", isFreezeFirstColumn(),
                   SLOT(freezeFirstColumnSlot(bool)));
  addCheckedAction(optionMenu, "Stretch Last Column", isStretchLastColumn(),
                   SLOT(stretchLastColumnSlot(bool)));
  addCheckedAction(optionMenu, "Multi Header Lines", isMultiHeaderLines(),
                   SLOT(multiHeaderLinesSlot(bool)));

  if (isHierarchical())
    addCheckedAction(optionMenu, "Root Is Decorated", rootIsDecorated(),
                     SLOT(rootIsDecoratedSlot(bool)));

  addCheckedAction(optionMenu, "Grid", isShowGrid(),
                   SLOT(showGridSlot(bool)));

  //---

  auto *horizontalMenu = addMenu("Horizontal Header");

  addCheckedAction(horizontalMenu, "Bottom", isHeaderOnBottom(),
                   SLOT(setHeaderOnBottomSlot(bool)));

  //---

  auto *verticalMenu = addMenu("Vertical Header");

  addCheckedAction(verticalMenu, "Visible", isShowVerticalHeader(),
                   SLOT(showVerticalHeaderSlot(bool)));
  addCheckedAction(verticalMenu, "Text"      , (verticalType() == VerticalType::TEXT),
                   SLOT(showVerticalTextSlot(bool)));
  addCheckedAction(verticalMenu, "Row Number", (verticalType() == VerticalType::NUMBER),
                   SLOT(showVerticalNumberSlot(bool)));
  addCheckedAction(verticalMenu, "Empty"     , (verticalType() == VerticalType::EMPTY),
                   SLOT(showVerticalEmptySlot(bool)));
  addCheckedAction(verticalMenu, "Right"     , isHeaderOnRight(),
                   SLOT(setHeaderOnRightSlot(bool)));

  //---

  auto *sortMenu = addMenu("Sort");

  addCheckedAction(sortMenu, "Enabled", isSortingEnabled(),
                   SLOT(sortingEnabledSlot(bool)));

  if (column >= 0) {
    bool isCurrent = (isSortingEnabled() && column == hh_->sortIndicatorSection());

    bool isAscending  = (isCurrent && hh_->sortIndicatorOrder() == Qt::AscendingOrder );
    bool isDescending = (isCurrent && hh_->sortIndicatorOrder() == Qt::DescendingOrder);

    addCheckedAction(sortMenu, "Increasing", isAscending, SLOT(sortIncreasingSlot()))->
      setIcon(QIcon(QPixmap::fromImage(SORT_AZ_SVG().image(is, is))));

    addCheckedAction(sortMenu, "Decreasing", isDescending, SLOT(sortDecreasingSlot()))->
      setIcon(QIcon(QPixmap::fromImage(SORT_ZA_SVG().image(is, is))));
  }

  //---

  auto *selectMenu = addMenu("Select");

  auto *selectActionGroup = addActionGroup(selectMenu, SLOT(selectionBehaviorSlot(QAction *)));

  selectActionGroup->addAction(
    addCheckedAction(selectMenu, "Items"  , selectionBehavior() == SelectItems  ));
  selectActionGroup->addAction(
    addCheckedAction(selectMenu, "Rows"   , selectionBehavior() == SelectRows   ));
  selectActionGroup->addAction(
    addCheckedAction(selectMenu, "Columns", selectionBehavior() == SelectColumns));

  selectMenu->addActions(selectActionGroup->actions());

  //---

  auto *filterMenu = addMenu("Filter");

  addCheckedAction(filterMenu, "Show Filter", isShowFilter(), SLOT(showFilterSlot(bool)))->
    setIcon(QIcon(QPixmap::fromImage(FILTER_SVG().image(is, is))));

  if (mouseData_.menuData.calcInd().isValid())
    addAction(filterMenu, "Filter by Value", SLOT(filterByValueSlot()));

  //---

  auto *fitMenu = addMenu("Fit");

  addAction(fitMenu, "Fit All Columns", SLOT(fitAllColumnsSlot()))->
    setIcon(QIcon(QPixmap::fromImage(FIT_ALL_COLUMNS_SVG().image(is, is))));

  if (column >= 0) {
    addAction(fitMenu, "Fit Column", SLOT(fitColumnSlot()))->
      setIcon(QIcon(QPixmap::fromImage(FIT_COLUMN_SVG().image(is, is))));
  }

  addAction(fitMenu, "Fit No Scroll", SLOT(fitNoScrollSlot()));

  //--

  auto *showHideMenu = addMenu("Show/Hide");

  addAction(showHideMenu, "Hide Column", SLOT(hideColumnSlot()));

  if (int(visColumnDatas_.size()) != nc_)
    addAction(showHideMenu, "Show All Columns", SLOT(showAllColumnsSlot()));

  //--

  if (isHierarchical()) {
    auto *expandCollapseMenu = addMenu("Expand/Collapse");

    if (mouseData_.menuData.calcInd().isValid()) {
      bool expanded = isExpanded(mouseData_.menuData.calcInd());

      if (expanded)
        addAction(expandCollapseMenu, "Collapse", SLOT(collapseSlot()));
      else
        addAction(expandCollapseMenu, "Expand", SLOT(expandSlot()));
    }

    addAction(expandCollapseMenu, "Expand All"  , SLOT(expandAll()));
    addAction(expandCollapseMenu, "Collapse All", SLOT(collapseAll()));
  }

  //---

  if (column >= 0) {
    auto *delegate  = itemDelegate();
    auto *idelegate = qobject_cast<CQItemDelegate *>(delegate);

    if (idelegate && idelegate->isNumericColumn(column)) {
      auto *decorationMenu = addMenu("Decoration");

      const ColumnData &columnData = columnDatas_[uint(column)];

      addCheckedAction(decorationMenu, "Show Heatmap", columnData.heatmap,
                       SLOT(setHeatmapSlot(bool)));
    }
  }

  //---

  if (isHierarchical()) {
    auto *rootMenu = addMenu("Root");

    addAction(rootMenu, "Set Root", SLOT(setRootSlot()));

    if (rootIndex() != QModelIndex())
      addAction(rootMenu, "Reset Root", SLOT(resetRootSlot()));
  }
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
  ++numRedraws_;

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
showGridSlot(bool b)
{
  setShowGrid(b);
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
  if (! isSortingEnabled())
    setSortingEnabled(true);

  sortByColumn(mouseData_.menuData.column(), Qt::AscendingOrder);
}

void
CQModelView::
sortDecreasingSlot()
{
  if (! isSortingEnabled())
    setSortingEnabled(true);

  sortByColumn(mouseData_.menuData.column(), Qt::DescendingOrder);
}

void
CQModelView::
selectionBehaviorSlot(QAction *action)
{
  if      (action->text() == "Items")
    setSelectionBehavior(SelectItems);
  else if (action->text() == "Rows")
    setSelectionBehavior(SelectRows);
  else if (action->text() == "Columns")
    setSelectionBehavior(SelectColumns);
  else
    assert(false);
}

//---

void
CQModelView::
setHeaderOnBottomSlot(bool b)
{
  setHeaderOnBottom(b);
}

void
CQModelView::
setHeaderOnRightSlot(bool b)
{
  setHeaderOnRight(b);
}

//------

void
CQModelView::
hideColumnSlot()
{
  hideColumn(mouseData_.menuData.column());
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

  //---

  state_.updateAll();

  emit stateChanged();

  //---

  redraw();
}

void
CQModelView::
showColumn(int column)
{
  hh_->showSection(column);

  //---

  state_.updateAll();

  emit stateChanged();

  //---

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

  //---

  state_.updateAll();

  emit stateChanged();

  //---

  redraw();
}

//------

void
CQModelView::
hideRow(int row)
{
  vh_->hideSection(row);

  //--

  state_.updateAll();

  emit stateChanged();

  //--

  redraw();
}

void
CQModelView::
showRow(int row)
{
  vh_->showSection(row);

  //---

  state_.updateAll();

  emit stateChanged();

  //---

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

  //--

  state_.updateAll();

  emit stateChanged();

  //---

  redraw();
}

//------

void
CQModelView::
expand(const QModelIndex &index)
{
  if (isIndexExpanded(index))
    return;

  expanded_.insert(index);

  if (model_ && model_->canFetchMore(index))
    model_->fetchMore(index);

  //---

  state_.updateAll();

  emit expanded(index);

  emit stateChanged();

  //---

  redraw();
}

void
CQModelView::
collapse(const QModelIndex &index)
{
  if (! isIndexExpanded(index))
    return;

  auto p = expanded_.find(index);

  if (p != expanded_.end())
    expanded_.erase(p);

  //---

  state_.updateAll();

  emit collapsed(index);

  emit stateChanged();

  //---

  redraw();
}

bool
CQModelView::
isExpanded(const QModelIndex &index) const
{
  if (! model_) return false;

  assert(index.model() == model());

  return isIndexExpanded(index);
}

void
CQModelView::
setExpanded(const QModelIndex &index, bool expanded)
{
  if (! model_) return;

  assert(index.model() == model());

  if (expanded) {
    if (index.parent().isValid() && ! isExpanded(index.parent()))
      expand(index.parent());

    expand(index);
  }
  else
    collapse(index);
}

void
CQModelView::
expandAll()
{
  state_.updateAll();

  ignoreExpanded_ = true;

  updateRowDatas();

  ignoreExpanded_ = false;

  //---

  IndexSet inds;

  for (const auto &pr : rowDatas_) {
    const auto &rowData = pr.second;

    if (rowData.children && rowData.expanded) {
      auto index = model_->index(rowData.row, 0, rowData.parent);

      auto p = expanded_.find(index);

      if (p == expanded_.end()) {
        inds.insert(index);

        expanded_.insert(index);
      }
    }
  }

  //---

  state_.updateAll();

  for (const auto &index : inds)
    emit expanded(index);

  emit stateChanged();

  //---

  redraw();
}

void
CQModelView::
collapseAll()
{
  IndexSet inds = expanded_;

  expanded_.clear();

  //---

  state_.updateAll();

  for (const auto &index : inds)
    emit collapsed(index);

  emit stateChanged();

  //---

  redraw();
}

void
CQModelView::
expandToDepth(int /*depth*/)
{
  // TODO:
  assert(false);
}

bool
CQModelView::
isIndexExpanded(const QModelIndex &index) const
{
  auto p = expanded_.find(index);

  return (p != expanded_.end());
}

void
CQModelView::
expandSlot()
{
  expand(mouseData_.menuData.calcInd());
}

void
CQModelView::
collapseSlot()
{
  collapse(mouseData_.menuData.calcInd());
}

void
CQModelView::
expandedIndices(QModelIndexList &inds) const
{
  for (auto &ind : expanded_)
    inds.push_back(ind);
}

//------

void
CQModelView::
setRootSlot()
{
  setRootIndex(mouseData_.menuData.calcInd());
}

void
CQModelView::
resetRootSlot()
{
  setRootIndex(QModelIndex());
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
showVerticalTextSlot(bool b)
{
  setVerticalType(b ? VerticalType::TEXT : verticalType());
}

void
CQModelView::
showVerticalNumberSlot(bool b)
{
  setVerticalType(b ? VerticalType::NUMBER : VerticalType::TEXT);
}

void
CQModelView::
showVerticalEmptySlot(bool b)
{
  setVerticalType(b ? VerticalType::EMPTY : VerticalType::TEXT);
}

void
CQModelView::
showFilterSlot(bool b)
{
  setShowFilter(b);
}

void
CQModelView::
filterByValueSlot()
{
  if (! isShowFilter())
    setShowFilter(true);

  auto ind = mouseData_.menuData.calcInd();

  if (ind.isValid()) {
    auto str = model_->data(ind, Qt::DisplayRole).toString();

    auto *le = filterEdits_[uint(ind.column())];

    le->setText(str);

    filterColumn(ind.column());
  }
}

//------

void
CQModelView::
fitAllColumnsSlot()
{
  for (int c = 0; c < nc_; ++c)
    resizeColumnToContents(c);

  redraw();
}

void
CQModelView::
fitColumnSlot()
{
  resizeColumnToContents(mouseData_.menuData.column());

  redraw();
}

void
CQModelView::
fitNoScrollSlot()
{
  int vw = viewport()->rect().width();

  int w = (nc_ > 0 ? vw/nc_ : 0);

  for (int c = 0; c < nc_; ++c)
    setColumnWidth(c, w);

  redraw();
}

void
CQModelView::
resizeColumnToContents(int column)
{
  assert(column >= 0);

  int maxWidth = 0;

  //---

  // TODO: SizeHintRole

  auto data = model_->headerData(column, Qt::Horizontal, Qt::DisplayRole);

  auto str = data.toString();

  int w = 0;

  if (isMultiHeaderLines()) {
    auto strs = str.split(QRegExp("\n|\r\n|\r"));

    for (int i = 0; i < strs.length(); ++i)
      w = std::max(w, paintData_.fm.horizontalAdvance(strs[i]));
  }
  else
    w = std::max(w, paintData_.fm.horizontalAdvance(str));

  maxWidth = std::max(maxWidth, w);

  //---

  int margin = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, hh_);
  int ispace = 4;

  maxWidth += 2*margin + 2*ispace;

  if (hh_->isSortIndicatorShown())
    maxWidth += paintData_.fm.height() + margin;

  //---

  int maxRows = hh_->resizeContentsPrecision();

  int nvr = 0;

  auto parent = rootIndex();

  int dataMaxWidth = 0;

  maxColumnWidth(column, parent, 0, nvr, dataMaxWidth, maxRows);

  dataMaxWidth += 2*margin + 2*ispace;

  maxWidth = std::max(maxWidth, dataMaxWidth);

  //---

  ColumnData &columnData = columnDatas_[uint(column)];

  columnData.width = std::max(maxWidth, 16);

  hh_->resizeSection(column, columnData.width);

  //---

  state_.updateAll();

  redraw();

  emit stateChanged();
}

bool
CQModelView::
maxColumnWidth(int column, const QModelIndex &parent, int depth,
               int &nvr, int &maxWidth, int maxRows)
{
  auto *delegate = itemDelegate();

  //---

  // init style data
  QStyleOptionViewItem option;

  if (delegate) {
    option.init(this);

    option.state    &= ~QStyle::State_MouseOver;
    option.state    &= ~QStyle::State_HasFocus;
    option.features &= ~QStyleOptionViewItem::Alternate;

    option.widget              = this;
    option.rect                = QRect(0, 0, 100, paintData_.rowHeight);
    option.font                = font();
    option.fontMetrics         = QFontMetrics(option.font);
    option.decorationSize      = paintData_.decorationSize;
    option.decorationPosition  = QStyleOptionViewItem::Left;
    option.decorationAlignment = Qt::AlignCenter;
    option.displayAlignment    = Qt::AlignHCenter | Qt::AlignVCenter;
    option.textElideMode       = textElideMode();
  }

  //---

  if (model_ && model_->canFetchMore(parent))
    model_->fetchMore(parent);

  int nr = model_->rowCount(parent);

  for (int r = 0; r < nr; ++r) {
    if (isRowHidden(r, parent))
      continue;

    // TODO: SizeHintRole and delegate sizeHint

    auto ind = model_->index(r, column, parent);

    //---

    int  w       = 0;
    bool useData = true;

    if (delegate) {
      auto size = delegate->sizeHint(option, ind);

      if (size.isValid()) {
        w       = size.width();
        useData = false;
      }
    }

    //---

    if (useData) {
      // get cell data
      auto data = model_->data(ind, Qt::DisplayRole);

    //---

      // get max string width
#if 1
      auto strs = data.toString().split("\n");

      int nl = strs.length();

      for (int i = 0; i < nl; ++i) {
        const auto &str = strs[i];

        w = std::max(w, paintData_.fm.horizontalAdvance(str));
      }
#else
      w = paintData_.fm.horizontalAdvance(data.toString());
#endif
    }

    //---

    // add hierarchical depth indent
    if (isHierarchical() && column == 0)
      w += (depth + rootIsDecorated())*indentation();

    //---

    // update max width
    maxWidth = std::max(maxWidth, w);

    //---

    // update rows and stop if max
    ++nvr;

    if (nvr > maxRows)
      return false;

    //---

    // check expanded parent
    auto ind1 = model_->index(r, 0, parent);

    if (model_->hasChildren(ind1) && isExpanded(ind1)) {
      if (! maxColumnWidth(column, ind1, depth + 1, nvr, maxWidth, maxRows))
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
  ColumnData &columnData = columnDatas_[uint(mouseData_.menuData.column())];

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

  const auto &rowVisRowDatas = (*pp).second;

  auto pr = rowVisRowDatas.find(index.row());
  if (pr == rowVisRowDatas.end()) return;

  const auto &visRowData = (*pr).second;

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

  auto current = currentIndex();

  if (! current.isValid())
    return QModelIndex();

  //---

  updateVisColumns();
  updateVisRows   ();

  //---

  int  r      = current.row   ();
  int  c      = current.column();
  auto parent = current.parent();

  //---

  auto prevRow = [&](int r, int c, const QModelIndex &parent) {
    auto pp = visRowDatas_.find(parent);
    if (pp == visRowDatas_.end()) return QModelIndex();

    const auto &rowVisRowDatas = (*pp).second;

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

    const auto &rowVisRowDatas = (*pp).second;

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

  auto prevCol = [&](int r, int c, const QModelIndex &parent) {
    if (c == 0 && isHierarchical()) {
      auto pr = rowDatas_.find(currentFlatRow_);

      if (pr != rowDatas_.end()) {
        const auto &rowData = (*pr).second;

        if (rowData.children && rowData.expanded) {
          auto ind = model_->index(r, c, parent);

          collapse(ind);

          return ind;
        }
      }
    }

    auto p = visColumnDatas_.find(c);

    if (p == visColumnDatas_.begin())
      return prevRow(r, (nc_ > 0 ? nc_ - 1 : 0), parent);

    --p;

    return model_->index(r, (*p).first, parent);
  };

  auto nextCol = [&](int r, int c, const QModelIndex &parent) {
    if (c == 0 && isHierarchical()) {
      auto pr = rowDatas_.find(currentFlatRow_);

      if (pr != rowDatas_.end()) {
        const auto &rowData = (*pr).second;

        if (rowData.children && ! rowData.expanded) {
          auto ind = model_->index(r, c, parent);

          expand(ind);

          return ind;
        }
      }
    }

    auto p = visColumnDatas_.find(c);

    if (p != visColumnDatas_.end())
      ++p;

    if (p == visColumnDatas_.end())
      return nextRow(r, 0, parent);

    return model_->index(r, (*p).first, parent);
  };

  switch (action) {
    case MoveUp:
      return prevRow(r, c, parent);
    case MoveDown:
      return nextRow(r, c, parent);
    case MovePrevious:
    case MoveLeft:
      return prevCol(r, c, parent);
    case MoveNext:
    case MoveRight:
      return nextCol(r, c, parent);
    case MoveHome:
      return model_->index(0, 0, parent);
    case MoveEnd:
      // TODO: ensure valid column
      return model_->index((nr_ > 0 ? nr_ - 1 : 0), (nc_ > 0 ? nc_ - 1 : 0), parent);
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
    const auto &visCellData = cp.second;

    if (visCellData.rect.intersects(rect))
      selection.select(cp.first, cp.first);
  }

  sm_->select(selection, flags);

  // TODO: current index ?
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
    const auto &visCellData = cp.second;

    if (visCellData.rect.contains(posData.pos)) {
      posData.ind     = cp.first;
      posData.rect    = visCellData.rect;
      posData.flatRow = visCellData.flatRow;
      return true;
    }
  }

  for (const auto &cp : ivisCellDatas_) {
    const auto &visCellData = cp.second;

    if (visCellData.rect.contains(posData.pos)) {
      posData.iind    = cp.first;
      posData.rect    = visCellData.rect;
      posData.flatRow = visCellData.flatRow;
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
    const auto &visColumnData = vp.second;

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
    const auto &rowVisRowDatas = pp.second;

    for (const auto &pr : rowVisRowDatas) {
      const auto &visRowData = pr.second;

      if (visRowData.rect.contains(posData.pos)) {
        posData.vsection = visRowData.flatRow;
        posData.rect     = visRowData.rect;
        return true;
      }

#if 0
      if (visRowData.vrect.contains(posData.pos)) {
        posData.vsectionh = visRowData.flatRow;
        posData.rect      = visRowData.vrect;
        return true;
      }
#endif
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
      if (role == ColorRole::GridFg) {
        painter->setPen(paintData_.gridPen);
      }
      else {
        auto p = paintData_.roleColors.find(paintData_.penRole);

        if (p == paintData_.roleColors.end()) {
          auto c = roleColor(role);

          c.setAlphaF(alpha);

          p = paintData_.roleColors.insert(p, RoleColors::value_type(paintData_.penRole, c));
        }

        painter->setPen((*p).second);
      }
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
        auto c = roleColor(role);

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
    case ColorRole::Base        : return palette().color(QPalette::Base);
    case ColorRole::Text        : return palette().color(QPalette::Text);
    case ColorRole::HeaderBg    : return headerBg();
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
    case ColorRole::SelectionBg : return selectionBg();
    case ColorRole::SelectionFg : return selectionFg();
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

QSizeF
CQModelView::
viewItemTextLayout(QTextLayout &textLayout, int lineWidth) const
{
  double height    = 0;
  double widthUsed = 0;

  textLayout.beginLayout();

  while (true) {
    auto line = textLayout.createLine();

    if (! line.isValid())
      break;

    line.setLineWidth(lineWidth);
    line.setPosition(QPointF(0, height));

    height += line.height();

    widthUsed = std::max(widthUsed, line.naturalTextWidth());
  }

  textLayout.endLayout();

  return QSizeF(widthUsed, height);
}

//------

CQModelViewSelectionModel::
CQModelViewSelectionModel(CQModelView *view) :
 view_(view)
{
  assert(view_);
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
  if (! model()) return;

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

  setToolTip("Select All");

  setClickOp(ClickOp::FIT_ALL);

  int is = QFontMetrics(font()).height() + 6;

  setIconSize(QSize(is, is));

  connect(this, SIGNAL(clicked()), this, SLOT(clickSlot()));
}

void
CQModelViewCornerButton::
setClickOp(const ClickOp &o)
{
  clickOp_ = o;

  auto is = iconSize().height();

  switch (clickOp_) {
    case ClickOp::FIT_ALL:
      setToolTip("Fit All");
      pixmap_ = QPixmap::fromImage(FIT_ALL_COLUMNS_SVG().image(is, is));
      break;
    case ClickOp::SELECT_ALL:
    default:
      setToolTip("Select All");
      pixmap_ = QPixmap::fromImage(SELECT_ALL_SVG().image(is, is));
      break;
  }

  setIcon(QIcon(pixmap_));
}

void
CQModelViewCornerButton::
clickSlot()
{
  switch (clickOp_) {
    case ClickOp::FIT_ALL   : view_->fitAllColumnsSlot(); break;
    case ClickOp::SELECT_ALL: view_->selectAll        (); break;
  }
}

void
CQModelViewCornerButton::
contextMenuEvent(QContextMenuEvent *e)
{
  auto *menu = new QMenu;

  //---

  auto addAction = [&](QMenu *menu, const QString &name, const char *slotName=nullptr) {
    auto *action = menu->addAction(name);

    if (slotName)
      connect(action, SIGNAL(triggered()), view_, slotName);

    return action;
  };

  auto is = iconSize().height();

  addAction(menu, "Fit All"   , SLOT(fitNoScrollSlot()))->
    setIcon(QIcon(QPixmap::fromImage(FIT_ALL_COLUMNS_SVG().image(is, is))));
  addAction(menu, "Select All", SLOT(selectAllSlot()))->
    setIcon(QIcon(QPixmap::fromImage(SELECT_ALL_SVG().image(is, is))));

  //---

  (void) menu->exec(e->globalPos());

  delete menu;
}

void
CQModelViewCornerButton::
paintEvent(QPaintEvent*)
{
  QStyleOptionHeader opt;

  opt.init(this);

  auto state = QStyle::State(QStyle::State_None);

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

  painter.save();

  style()->drawControl(QStyle::CE_Header, &opt, &painter, this);

  painter.restore();

  //painter.drawPixmap(opt.rect, pixmap_);

  auto pw = pixmap_.width ();
  auto ph = pixmap_.height();

  auto c = rect().center();

  painter.drawPixmap(c.x() - pw/2, c.y() - ph/2, pixmap_);
}

//------

CQModelViewFilterEdit::
CQModelViewFilterEdit(CQModelView *view, int column) :
 QLineEdit(view), view_(view), column_(column)
{
  setObjectName("filterEdit");
}

void
CQModelViewFilterEdit::
keyPressEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Escape) {
    view_->setShowFilter(false);
    return;
  }

  QLineEdit::keyPressEvent(e);
}

//------

CQModelViewHeaderEdit::
CQModelViewHeaderEdit(CQModelView *view, int column) :
 QLineEdit(view->horizontalHeader()), view_(view), column_(column)
{
  setObjectName("headerEdit");

  setFrame(false);

  connect(this, SIGNAL(returnPressed()), this, SLOT(acceptSlot()));
}

void
CQModelViewHeaderEdit::
focusInEvent(QFocusEvent *)
{
  selectAll();
}

void
CQModelViewHeaderEdit::
focusOutEvent(QFocusEvent *)
{
  hide();
}

void
CQModelViewHeaderEdit::
keyPressEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Escape)
    view_->setFocus();

  QLineEdit::keyPressEvent(e);
}

void
CQModelViewHeaderEdit::
acceptSlot()
{
  auto *model = view_->model();

  model->setHeaderData(column_, Qt::Horizontal, text(), Qt::DisplayRole);

  view_->setFocus();
}
