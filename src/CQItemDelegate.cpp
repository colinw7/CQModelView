#define QT_KEYPAD_NAVIGATION 1

#include <CQItemDelegate.h>
#include <CQBaseModelTypes.h>

#include <QAbstractItemView>
#include <QLineEdit>
#include <QLayout>
#include <QPainter>
#include <QEvent>

#include <cassert>

#include <svg/edit_item_svg.h>

CQItemDelegate::
CQItemDelegate(QAbstractItemView *view) :
 view_(view)
{
  //setClipping(false);
}

void
CQItemDelegate::
paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  isEditable_  = (index.flags() & Qt::ItemIsEditable) &&
                 (option.state & QStyle::State_HasEditFocus);
  isMouseOver_ = (option.state & QStyle::State_MouseOver);

  int itype;

  if (! drawType(painter, option, index, itype)) {
    QItemDelegate::paint(painter, option, index);

    if (isEditable_ && isMouseOver_) {
      CQBaseModelType type = (CQBaseModelType) itype;

      bool numeric = (type == CQBaseModelType::REAL || type == CQBaseModelType::INTEGER);

      drawEditImage(painter, option.rect, numeric);
    }
  }
}

bool
CQItemDelegate::
drawType(QPainter *painter, const QStyleOptionViewItem &option,
         const QModelIndex &index, int &itype) const
{
  QAbstractItemModel *model = view_->model();
  if (! model) return false;

  //---

  // get type
  // TODO: validate cast
  auto type = CQBaseModelType::STRING;

  QVariant tvar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::Type);

  if (! tvar.isValid())
    tvar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::BaseType);

  if (tvar.isValid()) {
    bool ok;
    int itype = tvar.toInt(&ok);
    if (ok)
      type = CQBaseModelType(itype);
  }

  itype = int(type);

  //---

  // draw mapped real
  if (type == CQBaseModelType::REAL || type == CQBaseModelType::INTEGER) {
    if (isHeatmap()) {
      if (isMouseOver_)
        return false;

      if (! drawRealInRange(painter, option, index))
        return false;
    }
    else {
      QItemDelegate::paint(painter, option, index);

      if (isEditable_ && isMouseOver_)
        drawEditImage(painter, option.rect, /*numeric*/true);
    }
  }
  else {
    return false;
  }

  return true;
}

bool
CQItemDelegate::
drawRealInRange(QPainter *painter, const QStyleOptionViewItem &option,
                const QModelIndex &index) const
{
  QStyleOptionViewItem option1 = option;

  auto initSelected = [&]() {
    QColor c = option.palette.color(QPalette::Highlight);

    option1.palette.setColor(QPalette::Highlight, QColor(0,0,0,0));
    option1.palette.setColor(QPalette::HighlightedText, c);
    option1.palette.setColor(QPalette::Text, c);

    option1.state &= ~QStyle::State_Selected;
  };

  auto drawSelected = [&]() {
    painter->setPen(option.palette.color(QPalette::Highlight));
    painter->setBrush(Qt::NoBrush);

    painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
  };

  auto clamp = [](int i, int low, int high) {
    return std::min(std::max(i, low), high);
  };

  auto blendColors = [&](const QColor &c1, const QColor &c2, double f) {
    double f1 = 1.0 - f;

    double r = c1.redF  ()*f + c2.redF  ()*f1;
    double g = c1.greenF()*f + c2.greenF()*f1;
    double b = c1.blueF ()*f + c2.blueF ()*f1;

    return QColor(clamp(int(255*r), 0, 255),
                  clamp(int(255*g), 0, 255),
                  clamp(int(255*b), 0, 255));
  };

  //---

  // get min/max for column
  QAbstractItemModel *model = view_->model();

  QVariant minVar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::Min);

  if (! minVar.isValid())
    minVar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::DataMin);

  QVariant maxVar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::Max);

  if (! maxVar.isValid())
    maxVar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::DataMax);

  if (! minVar.isValid() || ! maxVar.isValid())
    return false;

  bool ok1, ok2;

  double min = minVar.toReal(&ok1);
  double max = maxVar.toReal(&ok2);

  if (! ok1 || ! ok2)
    return false;

  //---

  // get model value
  QVariant var = model->data(index, Qt::EditRole);

  if (! var.isValid())
    var = model->data(index, Qt::DisplayRole);

  bool ok;
  double r = var.toReal(&ok);
  if (! ok) return false;

  //---

  // blend map color with background color using normalized value
  double norm = (max > min ? (r - min)/(max - min) : 0.0);

  QColor bg1(255, 0, 0);

  QColor bg2 = option.palette.color(QPalette::Window);

  QColor bg = blendColors(bg1, bg2, norm);

  //---

  // draw cell
  painter->fillRect(option.rect, bg);

  if (option.state & QStyle::State_Selected)
    initSelected();
  else {
    int g = qGray(bg.red(), bg.green(), bg.blue());

    QColor tc = (g < 127 ? Qt::white : Qt::black);

    option1.palette.setColor(QPalette::Text, tc);
  }

  QItemDelegate::drawDisplay(painter, option1, option.rect, QString("%1").arg(r));

  if (option.state & QStyle::State_Selected)
    drawSelected();

  QItemDelegate::drawFocus(painter, option, option.rect);

  return true;
}

void
CQItemDelegate::
drawEditImage(QPainter *painter, const QRect &rect, bool numeric) const
{
  QImage image = s_EDIT_ITEM_SVG.image(32, 32);

  int dy = (rect.height() - image.height())/2;

  QRect rect1;

  if (numeric)
    rect1 = QRect(rect.left() + 2, rect.top() + dy, image.width(), image.height());
  else
    rect1 = QRect(rect.right() - image.width() - 2, rect.top() + dy, image.width(), image.height());

  painter->drawImage(rect1, image);
}

bool
CQItemDelegate::
isNumericColumn(int column) const
{
  QAbstractItemModel *model = view_->model();
  if (! model) return false;

  CQBaseModelType type = CQBaseModelType::STRING;

  QVariant tvar = model->headerData(column, Qt::Horizontal, (int) CQBaseModelRole::Type);

  if (! tvar.isValid())
    tvar = model->headerData(column, Qt::Horizontal, (int) CQBaseModelRole::BaseType);

  if (! tvar.isValid())
    return false;

  bool ok;
  type = (CQBaseModelType) tvar.toInt(&ok);
  if (! ok) return false;

  //---

  return (type == CQBaseModelType::REAL || type == CQBaseModelType::INTEGER);
}

QWidget *
CQItemDelegate::
createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const
{
  QWidget *w;

  if (isNumericColumn(index.column()))
    w = new QLineEdit(parent);
  else
    w = new QLineEdit(parent);

  assert(w);

  w->updateGeometry();

  if (w->layout())
    w->layout()->invalidate();

  CQItemDelegate *th = const_cast<CQItemDelegate *>(this);

  w->installEventFilter(th);

  th->editor_      = w;
  th->editing_     = true;
  th->editorIndex_ = index;

  return w;
}

bool
CQItemDelegate::
eventFilter(QObject *obj, QEvent *event)
{
  return QObject::eventFilter(obj, event);
}
