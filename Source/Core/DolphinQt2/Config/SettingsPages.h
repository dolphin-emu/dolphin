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

class SettingPage : public QWidget
{
public:
    SettingPage(QWidget* parent = nullptr);
    void LoadConfig();

private slots:
    void SaveConfig();
};

class GeneralPage : public SettingPage
{
public:
    GeneralPage();
    void LoadConfig();

private slots:
    void SaveConfig();

private:
    void buildBasicSettings();
    void buildAdvancedSettings();

    QVBoxLayout* configLayout;

    // Basic Group
    QCheckBox* enableDualCore;
    QCheckBox* enableIdleSkip;
    QCheckBox* enableCheats;
    QComboBox* speedLimit;

    // Advanced Group
    QRadioButton* cpu_Interpreter;
    QRadioButton* cpu_CachedInterpreter;
    QRadioButton* cpu_JITRecompiler;
    QRadioButton* cpu_JITILRecompiler;

    QCheckBox* forceNTSC;
};

class GraphicsPage : public SettingPage
{
public:
    GraphicsPage();
    void LoadConfig();
    void SaveConfig();
};
