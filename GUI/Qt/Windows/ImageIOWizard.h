#ifndef IMAGEIOWIZARD_H
#define IMAGEIOWIZARD_H

#include <QDebug>
#include <QWizard>
#include <QThread>
#include <QProgressDialog>

#include "SNAPCommon.h"
#include "ImageIOWizardModel.h"
#include "QtReporterDelegates.h"

class QLineEdit;
class QPushButton;
class QComboBox;
class QLabel;
class QFileDialog;
class QStandardItemModel;
class QMenu;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QSpinBox;
class QDoubleSpinBox;
class FileChooserPanelWithHistory;
class OptimizationProgressRenderer;
class QtVTKRenderWindowBox;
class DICOMListingTable;

// Helper classes in their own namespace, so I can use simple class names
namespace imageiowiz
{

/*
 * TODO/NOTE:
 *
 * This was one of the first elements ported to Qt. For that reason, it does
 * not follow the same design patterns as the newer Qt code. Namely, it does
 * not make use of .ui files (all the Qt elements are coded in C++ directly)
 * and it does not use model couplings in the way that newer code does.
 *
 * Eventually, this code should be brought in line with the newer code.
 */

class AbstractPage : public QWizardPage
{
  Q_OBJECT

public:
  explicit AbstractPage(QWidget *parent = 0);

  irisSetMacro(Model, ImageIOWizardModel *)
  irisGetMacro(Model, ImageIOWizardModel *)

protected:

  bool ErrorMessage(const char *subject, const char *detail = NULL);
  bool ConditionalError(bool rc, const char *subject, const char *detail);
  bool ErrorMessage(const IRISException &exc);
  void WarningMessage(const IRISWarningList &wl);
  ImageIOWizardModel *m_Model;

  // Perform the actual save or load
  bool PerformIO();

  // A qlabel for displaying error/warning messages. The children are
  // responsible for placing this control on their layouts
  QLabel *m_OutMessage;

  // Main area for children to layout
  QWidget *m_Canvas;

  static const QString m_HtmlTemplate;
};

class SelectFilePage : public AbstractPage
{
  Q_OBJECT

public:
  explicit SelectFilePage(QWidget *parent = 0);

  // Externally set the filename
  void SetFilename(const std::string &filename,
                   GuidedNativeImageIO::FileFormat format);

  void initializePage();
  bool validatePage();
  // bool isComplete() const;

public slots:

  void onFilenameChanged(QString absoluteFilename);

  QString customFormatOracle(QString filename);

private:
  FileChooserPanelWithHistory *m_FilePanel;
};

class SummaryPage : public AbstractPage
{
  Q_OBJECT

public:
  explicit SummaryPage(QWidget *parent = 0);

  void initializePage();

  bool validatePage();


private:
  // Helper for building the tree
  void AddItem(QTreeWidgetItem *parent, const char *key, ImageIOWizardModel::SummaryItem si);
  void AddItem(QTreeWidget *parent, const char *key, ImageIOWizardModel::SummaryItem si);

  QTreeWidget *m_Tree;
  QLabel *m_Warnings;
};


class DICOMPage : public AbstractPage
{
  Q_OBJECT

public:

  explicit DICOMPage(QWidget *parent = 0);
  void initializePage();
  bool validatePage();

  bool isComplete() const;
  void cleanupPage();

public slots:

  void processDicomDirectory();
  void updateTable();

private:

  DICOMListingTable *m_Table;
};

class RawPage : public AbstractPage
{
  Q_OBJECT

public:

  explicit RawPage(QWidget *parent = 0);
  void initializePage();
  bool validatePage();
  virtual bool isComplete() const;

public slots:
  void onHeaderSizeChange();

private:
  QSpinBox *m_Dims[3], *m_HeaderSize;
  QDoubleSpinBox *m_Spacing[3];
  QComboBox *m_InFormat, *m_InEndian;
  QSpinBox *m_OutImpliedSize, *m_OutActualSize;
  unsigned long m_FileSize;
};

/**
 * @brief The ImageIOProgressDialog class
 * Customized progress dialog for Image IO Progress Display
 */
class ImageIOProgressDialog : public QProgressDialog
{
	Q_OBJECT
public:
	explicit ImageIOProgressDialog(QWidget *parent = 0);

	#if QT_VERSION >= 0x050000
	typedef QScopedPointer<ImageIOProgressDialog, QScopedPointerDeleteLater> ScopedPointer;
	#else
	typedef QScopedPointer<ImageIOProgressDialog> ScopedPointer;
	#endif

	void SetSaveMode()
	{
		m_SaveMode = true;
		this->setLabelText("Saving Image...");
	}
	void SetReadMode()
	{
		m_SaveMode = false;
		this->setLabelText("Reading Image...");
	} // default

	void display();

	SmartPtr<itk::Command> createCommand();
private:
	bool m_SaveMode = false;
	QtProgressReporterDelegate m_Delegate;
};



} // end namespace




class ImageIOWizard : public QWizard
{
  Q_OBJECT




public:

  enum { Page_File, Page_Raw, Page_DICOM, Page_Coreg, Page_OverlayRole, Page_Summary };

  explicit ImageIOWizard(QWidget *parent = 0);

  void SetModel(ImageIOWizardModel *model);

  void SetFilename(const std::string &filename,
                   GuidedNativeImageIO::FileFormat format = GuidedNativeImageIO::FORMAT_COUNT);

  virtual int nextId() const;

  /** Helper function to get the next page that should be opened after the image has
      been loaded */
  int nextPageAfterLoad();

signals:

public slots:

protected:


  // The model
  ImageIOWizardModel *m_Model;
};

#endif // IMAGEIOWIZARD_H
