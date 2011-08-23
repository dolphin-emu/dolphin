#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include "Util.h"

DControlStateManager::DControlStateManager(QObject* parent) : QObject(parent)
{
}

void DControlStateManager::RegisterControl(QCheckBox* control, bool checked)
{
	checkbox_states.insert(control, checked);
	control->setChecked(checked);
}

void DControlStateManager::RegisterControl(QRadioButton* control, bool checked)
{
	radiobutton_states.insert(control, checked);
	control->setChecked(checked);
}

void DControlStateManager::RegisterControl(QComboBox* control, int choice)
{
	combobox_states_int.insert(control, choice);
	control->setCurrentIndex(choice);
}

void DControlStateManager::RegisterControl(QComboBox* control, const QString& choice)
{
	combobox_states_string.insert(control, choice);
	control->setCurrentIndex(control->findText(choice)); // TODO: Make sure the text is valid
}

void DControlStateManager::OnReset()
{
	for (QMap<QCheckBox*,bool>::iterator cb = checkbox_states.begin(); cb != checkbox_states.end(); ++cb)
		cb.key()->setChecked(cb.value());

	for (QMap<QRadioButton*,bool>::iterator rb = radiobutton_states.begin(); rb != radiobutton_states.end(); ++rb)
		rb.key()->setChecked(rb.value());

	for (QMap<QComboBox*,int>::iterator cb = combobox_states_int.begin(); cb != combobox_states_int.end(); ++cb)
		cb.key()->setCurrentIndex(cb.value());

	for (QMap<QComboBox*,QString>::iterator cb = combobox_states_string.begin(); cb != combobox_states_string.end(); ++cb)
		cb.key()->setCurrentIndex(cb.key()->findText(cb.value())); // TODO: Make sure the text is valid
}
