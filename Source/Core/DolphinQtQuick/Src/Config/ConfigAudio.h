#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDiag;

class DConfigAudio : public QWidget
{
	Q_OBJECT

public:
	DConfigAudio(QWidget* parent = NULL);

public slots:
	void Reset();
	void Apply();

private:
	QButtonGroup* rbDSPEngine;
	QComboBox* chBackend;
	QComboBox* chSamplingRate;
	QCheckBox* cbEnableDTK;
	QCheckBox* cbRecordAudio;
	QCheckBox* cbLLEOnThread;
	QDiag* dgVolume;
};
