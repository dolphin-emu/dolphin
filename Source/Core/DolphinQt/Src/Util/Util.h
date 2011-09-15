#pragma once

#include <QWidget>
#include <QLayout>
#include <QGroupBox>
#include <QMap>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QString;
class QRadioButton;
class QVBoxLayout;

template<class Layout>
class DGroupBox : public QGroupBox
{
public:
	DGroupBox(const QString& title, QWidget* parent = NULL) : QGroupBox(title, parent)
	{
		layout = new Layout;
		setLayout(layout);
	}
	~DGroupBox() {}

	void addWidget(QWidget* widget)
	{
		layout->addWidget(widget);
	}

	void addLayout(QLayout* child)
	{
		layout->addLayout(child);
	}

private:
	Layout* layout;
};
typedef DGroupBox<QHBoxLayout> DGroupBoxH;
typedef DGroupBox<QVBoxLayout> DGroupBoxV;



template<class Layout>
class DLayoutWidget : public QWidget
{
public:
	DLayoutWidget(QWidget* parent = NULL)
	{
		layout = new Layout;
		setLayout(layout);
	}

	void addWidget(QWidget* widget)
	{
		layout->addWidget(widget);
	}
	void addLayout(QLayout* child)
	{
		layout->addLayout(child);
	}

	void setSizeConstraint(QLayout::SizeConstraint constraint)
	{
		layout->setSizeConstraint(constraint);
	}

private:
	Layout* layout;
};
typedef DLayoutWidget<QHBoxLayout> DLayoutWidgetH;
typedef DLayoutWidget<QVBoxLayout> DLayoutWidgetV;



// keeps track of initial button state and resets it on request.
// also can notify other widgets when a control's value was changed
class DControlStateManager : public QObject
{
	Q_OBJECT

public:
	DControlStateManager(QObject* parent);

	void RegisterControl(QCheckBox* control, bool checked);
	void RegisterControl(QRadioButton* control, bool checked);
	void RegisterControl(QComboBox* control, int choice);
	void RegisterControl(QComboBox* control, const QString& choice);

public slots:
	void OnReset();
	//void OnApply(); // TODO: This should be used to change the behavior of OnReset()

private:
	QMap<QCheckBox*, bool> checkbox_states;
	QMap<QRadioButton*, bool> radiobutton_states;
	QMap<QComboBox*, int> combobox_states_int;
	QMap<QComboBox*, QString> combobox_states_string;

signals:
	void settingChanged(); // TODO!
};
