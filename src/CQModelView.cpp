#include <CQModelView.h>
#include <CQBaseModelTypes.h>
#include <CQItemDelegate.h>

#include <QAbstractItemModel>
#include <QItemSelectionModel>
#include <QAbstractButton>
#include <QScrollBar>
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>

#include <set>
#include <iostream>
#include <cassert>

//---

class CQModelViewCornerButton : public QAbstractButton {
 public:
  CQModelViewCornerButton(QWidget *parent);

  void paintEvent(QPaintEvent*) override;
};

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

  cornerWidget_->setFocusPolicy(Qt::NoFocus);

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

  state_.updateScrollBars = true;

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
  const ColumnData &columnData = columnDatas_[column];

  return columnData.width;
}

void
CQModelView::
setColumnWidth(int column, int width)
{
  ColumnData &columnData = columnDatas_[column];

  columnData.width = width;
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
}

//---

void
CQModelView::
resizeEvent(QResizeEvent *)
{
  state_.updateScrollBars = true;

  updateGeometries();
}

void
CQModelView::
modelChangedSlot()
{
}

void
CQModelView::
updateGeometries()
{
  globalColumnData_.headerWidth  = fm_.width("X") + 4;
  globalRowData_   .headerHeight = fm_.height() + globalRowData_.headerMargin;
  globalRowData_   .height       = fm_.height() + globalRowData_.margin;

  // space for headers
  setViewportMargins(globalColumnData_.headerWidth, globalRowData_.headerHeight, 0, 0);

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
}

void
CQModelView::
updateScrollBars()
{
  if (! state_.updateScrollBars)
    return;

  state_.updateScrollBars = false;

  //---

  int rowHeight = this->rowHeight(0);

  int sw = vs_->sizeHint().width();
  int sh = hs_->sizeHint().height();

  int w = 0;
  int h = nr_*rowHeight;

  for (int c = 0; c < nc_; ++c)
    w += columnWidth(c);

  //---

  int ah = height() - sh;

  if (h > ah) {
    scrollData_.nv = (ah + rowHeight - 1)/rowHeight;

    vs_->setPageStep(scrollData_.nv);
    vs_->setRange(0, nr_ - vs_->pageStep());

    vs_->setVisible(true);
  }
  else {
    scrollData_.nv = -1;

    vs_->setVisible(false);
  }

  //---

  int aw = width() - sw;

  if (w > aw) {
    hs_->setPageStep(100);
    hs_->setRange(0, width() - hs_->pageStep());

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
    QModelIndex hind = model_->index(ind.column(), 0, QModelIndex());
    QModelIndex vind = model_->index(ind.row   (), 0, QModelIndex());

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

  std::set<int> columns, rows;

  for (auto it = selection.constBegin(); it != selection.constEnd(); ++it) {
    const QItemSelectionRange &range = *it;

    for (int c = range.left(); c <= range.right(); ++c)
      columns.insert(c);

    for (int r = range.top(); r <= range.bottom(); ++r)
      rows.insert(r);
  }

  QItemSelection hselection, vselection;

  for (auto &c : columns) {
    QModelIndex hind = model_->index(c, 0, QModelIndex());

    hselection.select(hind, hind);
  }

  for (auto &r : rows) {
    QModelIndex vind = model_->index(r, 0, QModelIndex());

    vselection.select(vind, vind);
  }

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

  state_.updateScrollBars = (nc_ != nc || nr_ !=  nr);

  updateScrollBars();

  //---

  drawCells(&painter);
}

void
CQModelView::
drawCells(QPainter *painter)
{
  initVisibleColumns(/*clear*/ false);

  visCellDatas_.clear();

  visCellDataAll_ = (scrollData_.nv >= nr_);

  r1_ = (scrollData_.nv > 0 ? verticalOffset()                      : 0      );
  r2_ = (scrollData_.nv > 0 ? verticalOffset() + scrollData_.nv - 1 : nr_ - 1);

  //---

  // draw rows
  int rowHeight = this->rowHeight(0);

  int y = 0;

  for (int r = r1_; r <= r2_; ++r) {
    drawRow(painter, r, y);

    y += rowHeight;
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

    if (! visCellDataAll_) {
      if (range.top() < r1_ || range.bottom() > r2_)
        initVisCellDatas();
    }

    const VisColumnData &visColumnData1 = visColumnDatas_[range.left ()];
    const VisColumnData &visColumnData2 = visColumnDatas_[range.right()];

    const VisRowData &visRowData1 = visRowDatas_[range.top   ()];
    const VisRowData &visRowData2 = visRowDatas_[range.bottom()];

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
  paintData_.reset();

  paintData_.w = viewport()->width();

  paintData_.margin = style()->pixelMetric(QStyle::PM_HeaderMargin, 0, hh_);

  painter->fillRect(viewport()->rect(), roleColor(ColorRole::HeaderBg));

  //---

  initVisibleColumns(/*clear*/true);

  //---

  int freezeWidth = 0;

  if (nc_ > 1 && isFreezeFirstColumn()) {
    if (! isColumnHidden(0))
      freezeWidth = columnWidth(0);
  }

  if (freezeWidth > 0)
    painter->save();

  int x = -horizontalOffset();

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    ColumnData &columnData = columnDatas_[c];

    bool valid = (! isFreezeFirstColumn() || c != 0);

    if (valid) {
      if (isFreezeFirstColumn()) {
        const VisColumnData &visColumnData = visColumnDatas_[c];

        bool isLast = visColumnData.last;

        int x1 = std::max(x, freezeWidth);
        int x2 = (! isLast ? std::max(x + columnData.width, freezeWidth) : paintData_.w - 1);

        if (x1 < x2)
          painter->setClipRect(QRect(x1, 0, x2 - x1 + 1, globalRowData_.headerHeight));
        else
          valid = false;
      }
    }

    if (valid)
      drawHHeaderSection(painter, c, x);

    x += columnData.width;
  }

  //---

  if (freezeWidth > 0) {
    if (! isColumnHidden(0)) {
      painter->setClipRect(QRect(0, 0, freezeWidth, globalRowData_.headerHeight));

      drawHHeaderSection(painter, 0, 0);
    }

    painter->restore();
  }
}

void
CQModelView::
drawHHeaderSection(QPainter *painter, int c, int x)
{
  QModelIndex ind = model_->index(c, 0, QModelIndex());

  //---

  ColumnData &columnData = columnDatas_[c];

  VisColumnData &visColumnData = visColumnDatas_[c];

  bool isLast = visColumnData.last;

  QVariant data = model_->headerData(c, Qt::Horizontal, Qt::DisplayRole);

  QString str = data.toString();

  int xl = x + paintData_.margin;
  int xr = xl + columnData.width - 2*paintData_.margin - 1;

  if (isLast && isStretchLastColumn() && x + columnData.width < paintData_.w)
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

  if (c == hsm_->currentIndex().row())
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

    painter->drawLine(x1, y - 2, x2, y - 2);
    painter->drawLine(x1, y - 1, x2, y - 1);
    painter->drawLine(x1, y    , x2, y    );
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

    painter->drawLine(x - 2, y1, x - 2, y2);
    painter->drawLine(x - 1, y1, x - 1, y2);
    painter->drawLine(x    , y1, x    , y2);
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
  paintData_.reset();

  visRowDatas_.clear();

  //---

  int rowHeight = this->rowHeight(0);

  int y = 0;

  for (int r = r1_; r <= r2_; ++r) {
    VisRowData &visRowData = visRowDatas_[r];

    visRowData.rect = QRect(0, y, globalColumnData_.headerWidth, rowHeight);

    //---

    QModelIndex ind = model_->index(r, 0, QModelIndex());

    //---

    // init style data
    QStyleOptionHeader option;

    if (r == mouseData_.moveData.vsection)
      option.state |= QStyle::State_MouseOver;

    if (vsm_->isSelected(ind))
      option.state |= QStyle::State_Selected;

    if (r == vsm_->currentIndex().row())
      option.state |= QStyle::State_HasFocus;

    option.rect             = visRowData.rect;
    option.icon             = QIcon();
    option.iconAlignment    = Qt::AlignCenter;
    option.position         = QStyleOptionHeader::Middle; // TODO
    option.section          = r;
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

      painter->drawLine(x - 2, y1, x - 2, y2);
      painter->drawLine(x - 1, y1, x - 1, y2);
      painter->drawLine(x    , y1, x    , y2);
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

    //---

    y += rowHeight;
  }
}

void
CQModelView::
drawRow(QPainter *painter, int r, int y)
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

  int rowHeight = this->rowHeight(0);

  //---

  int freezeWidth = 0;

  if (nc_ > 1 && isFreezeFirstColumn()) {
    if (! isColumnHidden(0))
      freezeWidth = columnWidth(0);
  }

  if (freezeWidth > 0)
    painter->save();

  int x = -horizontalOffset();

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    ColumnData &columnData = columnDatas_[c];

    bool valid = (! isFreezeFirstColumn() || c != 0);

    if (valid) {
      if (isFreezeFirstColumn()) {
        const VisColumnData &visColumnData = visColumnDatas_[c];

        bool isLast = visColumnData.last;

        int x1 = std::max(x, freezeWidth);
        int x2 = (! isLast ? std::max(x + columnData.width, freezeWidth) : paintData_.w - 1);

        if (x1 < x2)
          painter->setClipRect(QRect(x1, y, x2 - x1 + 1, rowHeight));
        else
          valid = false;
      }
    }

    if (valid)
      drawCell(painter, r, c, x, y);

    x += columnData.width;
  }

  //---

  if (freezeWidth > 0) {
    if (! isColumnHidden(0)) {
      painter->setClipRect(QRect(0, y, freezeWidth, rowHeight));

      drawCell(painter, r, 0, 0, y);
    }

    painter->restore();
  }
}

void
CQModelView::
drawCell(QPainter *painter, int r, int c, int x, int y)
{
  QModelIndex ind = model_->index(r, c, QModelIndex());

  //paintData_.resetRole();

  ColumnData &columnData = columnDatas_[c];

  const VisColumnData &visColumnData = visColumnDatas_[c];

  bool isLast = visColumnData.last;

  //---

  // get column label
  QVariant data = model_->data(ind, Qt::DisplayRole);

  QString str = data.toString();

  //--

  int rowHeight = this->rowHeight(0);

  VisCellData &visCellData = visCellDatas_[ind];

  if (isLast && isStretchLastColumn() && x + columnData.width < paintData_.w)
    visCellData.rect = QRect(x, y, paintData_.w - x, rowHeight);
  else
    visCellData.rect = QRect(x, y, columnData.width, rowHeight);

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

  if (alternatingRowColors() && (r & 1))
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

    setRolePen(painter, ColorRole::HeaderFg);

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
initVisCellDatas() const
{
  int rowHeight = this->rowHeight(0);

  int y = -verticalOffset()*rowHeight;

  for (int r = 0; r < nr_; ++r) {
    int x = -horizontalOffset();

    for (int c = 0; c < nc_; ++c) {
      if (isColumnHidden(c))
        continue;

      const ColumnData &columnData = columnDatas_[c];

      const VisColumnData &visColumnData = visColumnDatas_[c];

      bool isLast = visColumnData.last;

      QModelIndex ind1 = model_->index(r, c, QModelIndex());

      VisCellData &visCellData = visCellDatas_[ind1];

      if (isLast && isStretchLastColumn() && x + columnData.width < paintData_.w)
        visCellData.rect = QRect(x, y, paintData_.w - x, rowHeight);
      else
        visCellData.rect = QRect(x, y, columnData.width, rowHeight);

      x += columnData.width;
    }

    //---

    y += rowHeight;
  }

  visCellDataAll_ = true;
}

void
CQModelView::
initVisibleColumns(bool clear)
{
  int nv    = 0;
  int lastC = -1;

  if (clear)
    visColumnDatas_.clear();

  for (int c = 0; c < nc_; ++c) {
    if (isColumnHidden(c))
      continue;

    VisColumnData &visColumnData = visColumnDatas_[c];

    visColumnData.first = (nv == 0);

    ++nv;

    lastC = c;
  }

  if (lastC > 0) {
    VisColumnData &visColumnData = visColumnDatas_[lastC];

    visColumnData.last = true;
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

    mouseData_.headerWidth = columnData.width;
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

    state_.updateScrollBars = true;

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
  vh_->redraw();
  hh_->redraw();

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

void
CQModelView::
hideColumnSlot()
{
  hideColumn(mouseData_.menuData.hsection);

  redraw();
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
}

void
CQModelView::
showColumn(int column)
{
  hh_->showSection(column);
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

  QModelIndex parent;

  int nr = std::min(std::max(nr_, 0), 100);

  for (int r = 0; r < nr; ++r) {
    QModelIndex ind = model_->index(r, column, parent);

    QVariant data = model_->data(ind, Qt::DisplayRole);

    int w = fm_.width(data.toString());

    maxWidth = std::max(maxWidth, w);
  }

  //---

  ColumnData &columnData = columnDatas_[column];

  columnData.width = std::max(maxWidth + 2*globalColumnData_.margin, 16);

  //---

  state_.updateScrollBars = true;
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

  if (! visCellDataAll_) {
    initVisCellDatas();

    auto p = visCellDatas_.find(ind);

    if (p != visCellDatas_.end())
      return (*p).second.rect;
  }

  return QRect();
}

void
CQModelView::
scrollTo(const QModelIndex &index, ScrollHint)
{
  if (! index.isValid())
    return;

  int r = index.row();

  if (r >= r1_ && r < r2_)
    return;

  int rowHeight = this->rowHeight(0);

  int y = r*rowHeight;

  int y1 =  r1_     *rowHeight;
//int y2 = (r2_ + 1)*rowHeight;

  if (y >= y1)
    y -= viewport()->height() - 2*rowHeight;

  int vpos = y/rowHeight;

  vs_->setValue(std::min(std::max(vpos, vs_->minimum()), vs_->maximum()));

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

  int         r      = current.row   ();
  int         c      = current.column();
  QModelIndex parent = current.parent();

  auto prevRow = [&](int r) {
    return std::max(r - 1, 0);
  };

  auto nextRow = [&](int r) {
    return std::min(r + 1, nr_ - 1);
  };

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
      return model_->index(prevRow(r), c, parent);
    case MoveDown:
      return model_->index(nextRow(r), c, parent);
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
  // TODO: parent ?
  int c = index.column();

  if (isColumnHidden(c))
    return true;

  int r = index.row();

  auto pr = rowDatas_.find(r);

  if (pr != rowDatas_.end() && (*pr).second.hidden)
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

  if (! visCellDataAll_) {
    initVisCellDatas();

    for (const auto &cp : visCellDatas_) {
      const VisCellData &visCellData = cp.second;

      if (visCellData.rect.contains(posData.pos)) {
        posData.ind = cp.first;
        return true;
      }
    }
  }

  return false;
}

bool
CQModelView::
hheaderPositionToIndex(PositionData &posData) const
{
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

  for (const auto &hp : visRowDatas_) {
    const VisRowData &visRowData = hp.second;

    if (visRowData.rect.contains(posData.pos)) {
      posData.vsection = hp.first;
      return true;
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
CQModelViewCornerButton(QWidget *parent) :
 QAbstractButton(parent)
{
  setObjectName("corner");
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
