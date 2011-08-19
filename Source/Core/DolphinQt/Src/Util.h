#pragma once

#include <QWidget>
#include <QLayout>
class QGroupBox;
class QHBoxLayout;
class QString;
class QVBoxLayout;
class QWidget;

template<class Layout>
class DGroupBox : public QGroupBox
{
public:
	DGroupBox(const QString& title, QWidget* parent = NULL)
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
