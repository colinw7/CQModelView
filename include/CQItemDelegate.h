#ifndef CQItemDelegate_H
#define CQItemDelegate_H

#include <QItemDelegate>
#include <QPointer>

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

  bool isNumericColumn(int column) const;

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const override;

 private:
  bool drawType(QPainter *painter, const QStyleOptionViewItem &option,
                const QModelIndex &index) const;

  bool drawRealInRange(QPainter *painter, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const;

  void drawEditImage(QPainter *painter, const QRect &rect, bool numeric) const;

  bool eventFilter(QObject *obj, QEvent *event) override;

 private:
  using WidgetP = QPointer<QWidget>;

  QAbstractItemView* view_        { nullptr }; //!< parent view
  bool               heatmap_     { false };   //!< is heatmap enabled
  mutable bool       isEditable_  { false };   //!< is editable
  mutable bool       isMouseOver_ { false };   //!< is mouse over
  WidgetP            editor_;                  //!< current editor
  bool               editing_     { false };   //!< is editing
  QModelIndex        editorIndex_;             //!< editor model index
};

#endif
