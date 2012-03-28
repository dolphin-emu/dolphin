#include <QBoxLayout>
#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopServices>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QUrl>
#include <QVariant>

#include "IssueReporter.h"
#include "Util/Util.h"

// Initial page
DIssueInitialPage::DIssueInitialPage(QWidget* parent) : QWizardPage(parent)
{
	// TODO: Add a note that only English reports are accepted
	QLabel* welcomeText = new QLabel(tr("This wizard will assist you at reporting an issue with the emulator to the Dolphin team."));

	QLabel* problemLabel = new QLabel(tr("What kind of issue do you want to report?"));
	QComboBox* problemBox = new QComboBox;
	problemBox->addItem(tr("---"));
	problemBox->addItem(tr("User Interface"));
	problemBox->addItem(tr("Broken Feature"));
	problemBox->addItem(tr("Emulation"));
	problemBox->addItem(tr("Program Stability (Crashes, etc...)"));

	QLabel* searchedLabel = new QLabel(tr("Did you search for an existing issue report about your problem, yet?"));
	QComboBox* searchedBox = new QComboBox;
	searchedBox->addItems(QStringList() << tr("--") << tr("No") << tr("Yes"));

	registerField("problemType*", problemBox);
	registerField("searched*", searchedBox);

	QGridLayout* mainLayout = new QGridLayout;
	mainLayout->addWidget(welcomeText, 0, 0, 1, 2);
	mainLayout->addWidget(problemLabel, 1, 0);
	mainLayout->addWidget(problemBox, 1, 1);
	mainLayout->addWidget(searchedLabel, 2, 0);
	mainLayout->addWidget(searchedBox, 2, 1);
	setLayout(mainLayout);
}

DIssueInitialPage::~DIssueInitialPage()
{

}

int DIssueInitialPage::nextId() const
{
	if (field("searched").toInt() == Searched_No) return DIssueReporter::Page_SearchBeforeReporting;
	else return QWizardPage::nextId();
}


// Description page
DIssueDescriptionPage::DIssueDescriptionPage(QWidget* parent) : QWizardPage(parent)
{
	QLabel* pageLabel = new QLabel(tr("Thank you for your feedback. Now you can specify more details about your issue."));

	QLabel* summaryLabel = new QLabel(tr("Please specify a short but meaningful summary of your problem."));
	QLineEdit* summaryEdit = new QLineEdit;

	// TODO: Add some advice what information to provide, etc
	QLabel* descLabel = new QLabel(tr("Provide detailed information about your problem, please."));
	QPlainTextEdit* descEdit = new QPlainTextEdit;

	registerField("summary*", summaryEdit);
	registerField("description*", descEdit);

	QGridLayout* mainLayout = new QGridLayout;
	mainLayout->addWidget(pageLabel, 0, 0, 1, 2);
	mainLayout->addWidget(summaryLabel, 1, 0);
	mainLayout->addWidget(summaryEdit, 1, 1);
	mainLayout->addWidget(descLabel, 2, 0, 1, 2);
	mainLayout->addWidget(descEdit, 3, 0, 1, 2);
	setLayout(mainLayout);
}

DIssueDescriptionPage::~DIssueDescriptionPage()
{

}

QWizardPage* DIssueReporter::CreatePage_ThanksForReporting()
{
	// TODO: Update addInfo when the problemType field changes...
	QString addInfo = "Build date: TODO\nOperating System: TODO\n";
	switch (field("problemType").toInt())
	{
		case DIssueInitialPage::Problem_UI:
			addInfo += "Qt Version: TODO\netc";
			break;

		case DIssueInitialPage::Problem_Feature:
			// TODO?
			break;

		case DIssueInitialPage::Problem_Emulation:
			// TODO: Post only the info for the exact problem type (graphics, sound, ...)
			addInfo += "GPU: TODO\n";
			break;

		case DIssueInitialPage::Problem_Stability:
			break;
	}

	// TODO: Elaborate
	QLabel* pageLabel = new QLabel(tr("Thanks for reporting! You will now be redirected to our web issue reporting form.\nPlease post the contents of the text field below into the \"Description\" field of the form."));

	QPlainTextEdit* descEdit = new QPlainTextEdit;
	descEdit->setReadOnly(true);
	descEdit->setPlainText(field("description").toString() + QString("-------\n\nAdditional information:\n") + addInfo);

	QBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(pageLabel);
	mainLayout->addWidget(descEdit);

	QWizardPage* page = new QWizardPage;
	page->setLayout(mainLayout);
	page->setFinalPage(true); // TODO: Put this into an own class, since it will still show a "next" button here...
	return page;
}

DIssueReporter::DIssueReporter(QWidget* parent): QWizard(parent)
{
	setDefaultProperty("QPlainTextEdit", "plainText", SIGNAL(textChanged()));

	addPage(new DIssueInitialPage);
	addPage(new DIssueDescriptionPage);
	QWizardPage* searchPage = new QWizardPage;
	QBoxLayout* searchLayout = new QVBoxLayout;
	searchLayout->addWidget(new QLabel(tr("Search for existing issue reports before reporting the issue to us. You will be redirected to our issue database.")));
	searchPage->setLayout(searchLayout);
	searchPage->setFinalPage(true);
	addPage(searchPage);
	addPage(CreatePage_ThanksForReporting());
}

DIssueReporter::~DIssueReporter()
{
}

void DIssueReporter::accept()
{
	if (field("searched").toInt() == DIssueInitialPage::Searched_No)
	{
		QUrl url = QUrl("http://code.google.com/p/dolphin-emu/issues/list");
		url.addQueryItem("can", "1"); // search for closed issues as well
		QDesktopServices::openUrl(url);
	}
	else
	{
		QUrl url = QUrl("http://code.google.com/p/dolphin-emu/issues/entry");
		url.addQueryItem("template", "Defect report");
		url.addQueryItem("summary", field("summary").toString());
		url.addQueryItem("comment", tr("POST YOUR ISSUE DESCRIPTION HERE")); // TODO: Rather, ask the user to fill this out manually, since the url will get too long otherwise
		QDesktopServices::openUrl(url);
	}
	QDialog::accept();
}
