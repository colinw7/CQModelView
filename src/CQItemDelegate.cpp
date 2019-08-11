#include <CQItemDelegate.h>
#include <CQBaseModelTypes.h>
#include <QAbstractItemView>
#include <QPainter>

CQItemDelegate::
CQItemDelegate(QAbstractItemView *view) :
 view_(view)
{
  setClipping(false);
}

void
CQItemDelegate::
paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if (! drawType(painter, option, index))
    QItemDelegate::paint(painter, option, index);
}

bool
CQItemDelegate::
drawType(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QAbstractItemModel *model = view_->model();
  if (! model) return false;

  //---

  // get type
  // TODO: validate cast
  CQBaseModelType type = CQBaseModelType::STRING;

  QVariant tvar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::Type);

  if (! tvar.isValid())
    tvar = model->headerData(index.column(), Qt::Horizontal, (int) CQBaseModelRole::BaseType);

  if (! tvar.isValid())
    return false;

  bool ok;
  type = (CQBaseModelType) tvar.toInt(&ok);
  if (! ok) return false;

  //---

  // draw mapped real
  if (type == CQBaseModelType::REAL || type == CQBaseModelType::INTEGER) {
    if (! isHeatmap())
      return false;

    if (! drawRealInRange(painter, option, index))
      return false;
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
