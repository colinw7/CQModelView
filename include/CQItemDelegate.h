#ifndef CQItemDelegate_H
#define CQItemDelegate_H

#include <QItemDelegate>

class QAbstractItemView;

/*!
 * Delegate Abstract Item View with support for:
 *  . heatmap coloring depending on min/max real value of column
 */
class CQItemDelegate : public QItemDelegate {
  Q_OBJECT

  Q_PROPERTY(bool heatmap READ isHeatmap WRITE setHeatmap)

 public:
  CQItemDelegate(QAbstractItemView *view);

  bool isHeatmap() const { return heatmap_; }
  void setHeatmap(bool b) { heatmap_ = b; }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

 private:
  bool drawType(QPainter *painter, const QStyleOptionViewItem &option,
                const QModelIndex &index) const;

  bool drawRealInRange(QPainter *painter, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const;

  void drawEditImage(QPainter *painter, const QRect &rect, bool numeric) const;

 private:
  QAbstractItemView *view_        { nullptr };
  bool               heatmap_     { false };
  mutable bool       isEditable_  { false };
  mutable bool       isMouseOver_ { false };
};

#endif
