#include <CQModelViewHeader.h>
#include <CQModelView.h>
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <iostream>

CQModelViewHeader::
CQModelViewHeader(Qt::Orientation orientation, CQModelView *view) :
 QHeaderView(orientation, view), view_(view)
{
  setObjectName(orientation == Qt::Horizontal ? "hheader" : "vheader");

  setContextMenuPolicy(Qt::DefaultContextMenu);

  setSectionsClickable(true);
  setHighlightSections(true);

  setMouseTracking(true);
}

void
CQModelViewHeader::
setModel(QAbstractItemModel *model)
{
  QHeaderView::setModel(model);
}

void
CQModelViewHeader::
setVisible(bool v)
{
  QHeaderView::setVisible(v);
}

void
CQModelViewHeader::
doItemsLayout()
{
  //std::cerr << "CQModelViewHeader::doItemsLayout\n";
  // Called on StyleChange

  QHeaderView::doItemsLayout();
}

void
CQModelViewHeader::
reset()
{
  //std::cerr << "CQModelViewHeader::reset\n";
  // Called on setModel

  QHeaderView::reset();
}

void
CQModelViewHeader::
currentChanged(const QModelIndex &current, const QModelIndex &old)
{
  //std::cerr << "CQModelViewHeader::currentChanged\n";
  // Called on setSelectionModel

  QHeaderView::currentChanged(current, old);
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

  QColor c = palette().color(QPalette::Base);

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

  mouseData.pressed   = true;
  mouseData.modifiers = e->modifiers();

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

bool
CQModelViewHeader::
event(QEvent *e)
{
#if 0
  switch (e->type()) {
    case QEvent::HoverEnter: {
      auto *he = static_cast<QHoverEvent*>(e);
      d->hover = logicalIndexAt(he->pos());
      if (d->hover != -1)
        updateSection(d->hover);
      break;
    }
    case QEvent::Leave:
    case QEvent::HoverLeave: {
      if (d->hover != -1)
        updateSection(d->hover);
      d->hover = -1;
      break;
    }
    case QEvent::HoverMove: {
      auto *he = static_cast<QHoverEvent*>(e);
      int oldHover = d->hover;
      d->hover = logicalIndexAt(he->pos());
      if (d->hover != oldHover) {
        if (oldHover != -1)
          updateSection(oldHover);
        if (d->hover != -1)
          updateSection(d->hover);
      }
      break;
    }
    default: {
      return QHeaderView::event(e);
    }
  }
#else
  return QHeaderView::event(e);
#endif
}

bool
CQModelViewHeader::
viewportEvent(QEvent *e)
{
  //std::cerr << "CQModelViewHeader::viewportEvent\n";

  // TODO: handle FontChange, StyleChange, Hide, Show (resizeSections, geometriesChanged)
  switch (e->type()) {
    case QEvent::ToolTip: {
      auto *he = static_cast<QHelpEvent*>(e);

      CQModelView::PositionData posData;

      posData.pos = he->pos();

      int section = -1;

      if (orientation() == Qt::Horizontal) {
        view_->hheaderPositionToIndex(posData);

        section = posData.hsection;
      }
      else {
        view_->vheaderPositionToIndex(posData);

        section = posData.vsection;
      }

      if (section != -1) {
        QVariant variant = view_->model()->headerData(section, orientation(), Qt::ToolTipRole);

        if (variant.isValid()) {
          QToolTip::showText(he->globalPos(), variant.toString(), this);
          return true;
        }
      }

      return true;
    }
    default:
      break;
  }

  return QHeaderView::viewportEvent(e);
}

int
CQModelViewHeader::
horizontalOffset() const
{
  //std::cerr << "CQModelViewHeader::horizontalOffset\n";

  if (orientation() == Qt::Horizontal)
    return view_->horizontalOffset();

  return 0;
}

int
CQModelViewHeader::
verticalOffset() const
{
  //std::cerr << "CQModelViewHeader::verticalOffset\n";

  if (orientation() == Qt::Vertical)
    return view_->verticalOffset();

  return 0;
}

void
CQModelViewHeader::
updateGeometries()
{
  //std::cerr << "CQModelViewHeader::updateGeometries\n";
  // Called by doItemsLayout

  return QHeaderView::updateGeometries();
}

void
CQModelViewHeader::
scrollContentsBy(int dx, int dy)
{
  //std::cerr << "CQModelViewHeader::scrollContentsBy\n";

  return QHeaderView::scrollContentsBy(dx, dy);
}

void
CQModelViewHeader::
dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
  //std::cerr << "CQModelViewHeader::dataChanged\n";

  return QHeaderView::dataChanged(topLeft, bottomRight, roles);
}

void
CQModelViewHeader::
rowsInserted(const QModelIndex &parent, int start, int end)
{
  //std::cerr << "CQModelViewHeader::rowsInserted\n";

  return QHeaderView::rowsInserted(parent, start, end);
}

QRect
CQModelViewHeader::
visualRect(const QModelIndex &index) const
{
  //std::cerr << "CQModelViewHeader::visualRect\n";

  return QHeaderView::visualRect(index);
}

void
CQModelViewHeader::
scrollTo(const QModelIndex &index, ScrollHint hint)
{
  //std::cerr << "CQModelViewHeader::scrollTo\n";

  return QHeaderView::scrollTo(index, hint);
}

QModelIndex
CQModelViewHeader::
indexAt(const QPoint &p) const
{
  //std::cerr << "CQModelViewHeader::indexAt\n";

  return QHeaderView::indexAt(p);
}

bool
CQModelViewHeader::
isIndexHidden(const QModelIndex &index) const
{
  //std::cerr << "CQModelViewHeader::isIndexHidden\n";

  return QHeaderView::isIndexHidden(index);
}

QModelIndex
CQModelViewHeader::
moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers)
{
  //std::cerr << "CQModelViewHeader::moveCursor\n";

  return QHeaderView::moveCursor(action, modifiers);
}

void
CQModelViewHeader::
setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags flags)
{
  //std::cerr << "CQModelViewHeader::setSelection\n";

  return QHeaderView::setSelection(rect, flags);
}

QRegion
CQModelViewHeader::
visualRegionForSelection(const QItemSelection &selection) const
{
  //std::cerr << "CQModelViewHeader::visualRegionForSelection\n";

  return QHeaderView::visualRegionForSelection(selection);
}

void
CQModelViewHeader::
emitSectionClicked(int section)
{
  emit sectionClicked(section);
}

void
CQModelViewHeader::
redraw()
{
  viewport()->update(viewport()->rect());

  update(rect());
}
