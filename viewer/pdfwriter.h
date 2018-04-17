#ifndef PDFWRITER_H
#define PDFWRITER_H

#include <QString>

class QFileDialog;
class QWidget;
class QTableView;

class PdfWriter
{
public:
  PdfWriter(const QString &dbPath = "");
  ~PdfWriter();
  void writeTableToPdf(QTableView *view, const QString &path);
  void writeTableToPdf(QTableView *view);
  void writeWidgetToPdf(QWidget *w, const QString &path);
  void writeWidgetToPdf(QWidget *w);

private:
  QFileDialog* fd;
  QString dbPath;
  void setupSavePdfFileDialog();
};

#endif // PDFWRITER_H
