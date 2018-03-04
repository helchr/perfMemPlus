#ifndef PERCENTDELEGATE_H
#define PERCENTDELEGATE_H

#include <QStyledItemDelegate>

class PercentDelegate : public QStyledItemDelegate
{
public:
  PercentDelegate(QObject* parent = 0);

  virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
  float relativeCost(const QModelIndex &index) const;
  QString displayText(const QModelIndex &index, const QLocale &locale) const;
};

#endif // PERCENTDELEGATE_H
