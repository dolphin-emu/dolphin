#include <QWidget>

#include "Common/CommonTypes.h"

class QCheckBox;

class PrimeCheatsWidget : public QWidget
{
public:
  explicit PrimeCheatsWidget();
protected:
  void enterEvent(QEvent*);
  void showEvent(QShowEvent*);
private:
  void CreateWidgets();
  void ConnectWidgets();
  void OnSaveConfig();
  void OnLoadConfig();
  void AddDescriptions();

  QCheckBox* m_checkbox_noclip;
  QCheckBox* m_checkbox_invulnerability;
  QCheckBox* m_checkbox_skipcutscenes;
  QCheckBox* m_checkbox_scandash;
  QCheckBox* m_checkbox_skipportalmp2;
  QCheckBox* m_checkbox_friendvouchers;
  QCheckBox* m_checkbox_hudmemo;
};
