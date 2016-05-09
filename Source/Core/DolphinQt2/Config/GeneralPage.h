#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "SettingsPages.h"

class GeneralPage : public SettingPage
{
public:
    GeneralPage();
    void LoadConfig();

private slots:
    void SaveConfig();

private:
    void BuildBasicSettings();
    void BuildAdvancedSettings();
    void BuildOptions();
    QVBoxLayout* mainLayout;

    // Basic Group
    QComboBox* language;

    QCheckBox* enableCheats;
    QComboBox* speedLimit;

    // Advanced Group

    QCheckBox* forceNTSC;

    /* To move to CPU
    QCheckBox* enableDualCore;
    QCheckBox* enableIdleSkip;

    QRadioButton* cpu_Interpreter;
    QRadioButton* cpu_CachedInterpreter;
    QRadioButton* cpu_JITRecompiler;
    QRadioButton* cpu_JITILRecompiler;
    */

};