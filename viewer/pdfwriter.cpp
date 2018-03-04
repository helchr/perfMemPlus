#include "pdfwriter.h"
#include <QHeaderView>
#include <QPainter>
#include <QPageLayout>
#include <QPicture>
#include <QPdfWriter>
#include <QTableView>
#include <QFileDialog>

PdfWriter::PdfWriter(const QString& dbPath)
{
  this->dbPath = dbPath;
  fd = new QFileDialog();
}

PdfWriter::~PdfWriter()
{
  delete fd;
}

void PdfWriter::writeTableToPdf(QTableView* view, const QString& path) const
{
  QTableView tmpView;
  tmpView.setModel(view->model());
  for(int i = 0; i < view->model()->columnCount(); i++)
  {
    tmpView.setItemDelegateForColumn(i,view->itemDelegateForColumn(i));
  }
  tmpView.resizeColumnsToContents();
  tmpView.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  tmpView.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tmpView.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tmpView.setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  tmpView.verticalHeader()->hide();

  for(int i = 0; i < tmpView.model()->rowCount(); i++)
  {
    tmpView.setRowHidden(i,true);
    tmpView.setColumnWidth(i,view->columnWidth(i));
  }
  for(auto row : view->selectionModel()->selectedRows())
  {
    tmpView.setRowHidden(row.row(),false);
  }
  writeWidgetToPdf(&tmpView,path);
}

void PdfWriter::writeWidgetToPdf(QWidget* w, const QString& path) const
{
  QPicture pic;
  QPainter painter(&pic);
  w->render(&painter);
  painter.end();
  QPdfWriter pdfWriter(path);
  QPageLayout layout;
  layout.setOrientation(QPageLayout::Landscape);
  QPageSize ps(QSize(pic.heightMM()+1,pic.widthMM()+1),QPageSize::Millimeter,"",QPageSize::ExactMatch);
  layout.setPageSize(ps);
  pdfWriter.setPageLayout(layout);
  painter.begin(&pdfWriter);
  painter.drawPicture(QPointF(0,0),pic);
  painter.end();
}

void PdfWriter::setupSavePdfFileDialog()
{
  fd->setWindowTitle("Select pdf output file");
  QFileInfo fi(dbPath);
  fd->setDirectory(fi.absoluteDir());
  fd->setAcceptMode(QFileDialog::AcceptSave);
  fd->setNameFilter("PDF (*.pdf)");
}

void PdfWriter::writeWidgetToPdf(QWidget* w)
{
  setupSavePdfFileDialog();
  if(fd->exec())
    {
      auto path = fd->selectedFiles().first();
      writeWidgetToPdf(w,path);
    }

}

void PdfWriter::writeTableToPdf(QTableView* view)
{
  setupSavePdfFileDialog();
  if(fd->exec())
    {
      auto path = fd->selectedFiles().first();
      writeTableToPdf(view,path);
    }
}
