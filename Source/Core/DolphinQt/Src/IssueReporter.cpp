#include <QBoxLayout>
#include <QDesktopServices>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QUrl>
#include <QVariant>

#include "IssueReporter.h"

DIssueGeneralPage::DIssueGeneralPage(QWidget* parent): QWizardPage(parent)
{
	QLabel* summaryLabel = new QLabel(tr("Summary:"));
	QLineEdit* summaryEdit = new QLineEdit;

	QLabel* commentLabel = new QLabel(tr("Issue Description:"));
	QPlainTextEdit* commentEdit = new QPlainTextEdit;

	registerField("summary*", summaryEdit);
	registerField("comment*", commentEdit);

	QGridLayout* mainLayout = new QGridLayout;
	mainLayout->addWidget(summaryLabel, 0, 0);
	mainLayout->addWidget(summaryEdit, 0, 1);
	mainLayout->addWidget(commentLabel, 1, 0);
	mainLayout->addWidget(commentEdit, 1, 1, 2, 1);
	setLayout(mainLayout);
}

DIssueGeneralPage::~DIssueGeneralPage()
{

}


DIssueReporter::DIssueReporter(QWidget* parent): QWizard(parent)
{
	addPage(new DIssueGeneralPage);
}

DIssueReporter::~DIssueReporter()
{
}

void DIssueReporter::accept()
{
	QUrl url = QUrl("http://code.google.com/p/dolphin-emu/issues/entry");
	url.addQueryItem("template", "Defect report");
	url.addQueryItem("summary", field("summary").toString());
	url.addQueryItem("comment", field("comment").toString()); // TODO: Rather, ask the user to fill this out manually, since the url will get too long otherwise
	QDesktopServices::openUrl(url);
	QDialog::accept();
}
