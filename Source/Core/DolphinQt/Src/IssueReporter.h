#include <QWizard>
#include <QWizardPage>

class DIssueGeneralPage : public QWizardPage
{
public:
	DIssueGeneralPage(QWidget* parent = 0);
	virtual ~DIssueGeneralPage();

protected:
	// TODO: int nextId() const
private:
};

class DIssueReporter : public QWizard
{
	Q_OBJECT

public:
	enum { Page_General, Page_Something };

	DIssueReporter(QWidget* parent = NULL);
	virtual ~DIssueReporter();

public slots:
	void accept();

private:
};
