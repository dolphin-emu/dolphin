#pragma once

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


class InterfacePage : public QWidget
{
public:
    InterfacePage(QWidget* parent = nullptr);
    void LoadConfig();

private slots:
    void SaveConfig();
private:
	void ConnectOptions();
    void BuildOptions();
    void BuildUIOptions();
    void BuildInGameOptions();

    QVBoxLayout* mainLayout;

    QCheckBox* autoAdjustWindowSize;
    QCheckBox* keepDolphinOnTop;
    QCheckBox* renderToMainWindow;
    QComboBox* themeSelector;
    QCheckBox* confirmOnStop;
    QCheckBox* usePanicHandlers;
    QCheckBox* enableOnScreenDisplay;
    QCheckBox* pauseOnFocusLost;
    QCheckBox* hideMouseCursor;
};
