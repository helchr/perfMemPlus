#include "percentdelegate.h"
#include <QPainter>
#include <QApplication>

PercentDelegate::PercentDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

float PercentDelegate::relativeCost(const QModelIndex &index) const
{
  bool ok = false;
  float cost = index.data(Qt::DisplayRole).toFloat(&ok);
  cost = cost / 100;
  return ok ? cost : 0;
}

QString PercentDelegate::displayText(const QModelIndex &index, const QLocale&) const
{
  return index.data().toString();
}

void PercentDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

  // Draw controls, but no text.
  opt.text.clear();
  style->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

  painter->save();

  // Draw bar.
  float ratio = qBound(0.0f, relativeCost(index), 1.0f);
  QRect barRect = opt.rect;
  barRect.setWidth(opt.rect.width() * ratio);
  painter->setPen(Qt::NoPen);
  painter->setBrush(QBrush(QColor::fromRgba(0xf0fff080),Qt::SolidPattern));
  painter->drawRect(barRect);

  // Draw text.
  QLocale loc = opt.locale;
  loc.setNumberOptions(0);
  const QString text = displayText(index, loc);
  const QBrush &textBrush = (option.state & QStyle::State_Selected ? opt.palette.highlightedText() : opt.palette.text());
  painter->setBrush(Qt::NoBrush);
  painter->setPen(textBrush.color());
  painter->drawText(opt.rect, Qt::AlignVCenter | Qt::AlignLeft, text);

  painter->restore();
}

QSize PercentDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);

  const QString text = displayText(index, opt.locale);
  const QSize size = QSize(option.fontMetrics.width(text),
  option.fontMetrics.height());
  return size;
}
