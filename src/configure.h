#ifndef CONFIGURE_H
#define CONFIGURE_H

#include <QString>
#include <QMap>
#include <QIcon>
#include <QFont>

#include "ui_configurebase.h"

struct ViewOptions {
	// general
	int mUpdateInterval;
	bool mMonitoring;
	bool mNotifications;
	bool mDisplayTrayIcon;
	bool mDisplayTextStatistics;
	int mTextDigit;
	int mTextDigitPosition;
	bool mTextShowDigit;
	bool mTextShadow;
	QString mTextDigitColor;
	QString mTextUploadColor;
	QString mTextDownloadColor;
	QString mTextBackgroundColor;
	bool mTextTransparentBackground;
	QFont mTextFont;
	// icon view
	int mTheme;
	// chart view
	QString mChartUplColor;
	QString mChartDldColor;
	QString mChartBgColor;
	bool mChartTransparentBackground;
};

typedef QMap<QString, ViewOptions> OptionsMap;

class QShowEvent;
class QGroupBox;
class QComboBox;
class ColorButton;
class QPushButton;
class QLabel;

class Configure : public QDialog, public Ui::ConfigureBase {
Q_OBJECT
public:
	explicit Configure(QWidget *parent);

	bool canSaveConfig();

	const OptionsMap &options() const { return mConfig; }

protected:
	void showEvent(QShowEvent *event) override;

protected slots:

	void changeInterface(QListWidgetItem *item);

	void changeTheme(int theme);

private:
	void storeCurrentOptions();
	void setupTextStatisticsControls();
	void updateTextStatisticsControls();
	void updateTextStatisticsPreview();
	QGroupBox *mTextStatisticsGroup;
	QComboBox *mTextDigit;
	QComboBox *mTextDigitPosition;
	QCheckBox *mTextShowDigit;
	QCheckBox *mTextShadow;
	ColorButton *mTextDigitColor;
	ColorButton *mTextUploadColor;
	ColorButton *mTextDownloadColor;
	ColorButton *mTextBackgroundColor;
	QCheckBox *mTextTransparentBackground;
	QPushButton *mTextFontButton;
	QLabel *mTextPreview16;
	QLabel *mTextPreview22;
	QFont mSelectedTextFont;
	bool mLoadingOptions = false;
	QString mCurrentItem;
	OptionsMap mConfig;
	QIcon mInterfaceIcon;

private slots:

	void refreshInterfaces();
};

#endif
