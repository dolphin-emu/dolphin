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
    SettingPage(QWidget *parent = 0);
    void LoadConfig();
    void SaveConfig();
};

class GeneralPage :public SettingPage
{
public:
    GeneralPage();
    void LoadConfig();
    void SaveConfig();
private:
    // Top layout
    QCheckBox* enableDualCore;
    QCheckBox* enableIdleSkip;
    QCheckBox* enableCheats;
    QCheckBox* forceNTSC;

    QComboBox *speedLimit;

    QRadioButton* cpu_Interpreter;
    QRadioButton* cpu_CachedInterpreter;
    QRadioButton* cpu_JITRecompiler;
    QRadioButton* cpu_JITILRecompiler;
};

class GraphicsPage : public SettingPage
{
public:
    GraphicsPage();
    void LoadConfig();
    void SaveConfig();
};
