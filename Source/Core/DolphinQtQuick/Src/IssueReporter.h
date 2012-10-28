#include <QWizard>
#include <QWizardPage>

class DIssueInitialPage : public QWizardPage
{
public:
	enum { Problem_UI = 1, Problem_Feature, Problem_Emulation, Problem_Stability };
	enum { Searched_No = 1, Searched_Yes };

    DIssueInitialPage(QWidget* parent = NULL);
    virtual ~DIssueInitialPage();

protected:
    virtual int nextId() const;
};

class DIssueDescriptionPage : public QWizardPage
{
public:
	DIssueDescriptionPage(QWidget* parent = NULL);
    virtual ~DIssueDescriptionPage();
};

class DIssueReporter : public QWizard
{
	Q_OBJECT

public:
	enum { Page_Initial, Page_DetailDescription, Page_SearchBeforeReporting, Page_ThanksForReporting };

	DIssueReporter(QWidget* parent = NULL);
	virtual ~DIssueReporter();

public slots:
	void accept();

private:
	QWizardPage* CreatePage_ThanksForReporting();
};
