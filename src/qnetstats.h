#ifndef QNETSTATS_H
#define QNETSTATS_H

#include "configure.h"
#include <QHash>
#include <QSystemTrayIcon>

class QNetStatsView;

class QNetStats : public QDialog {
Q_OBJECT
public:
	QNetStats();

	static void readInterfaceConfig(const QString &ifName, ViewOptions *opts);

	void checkTrayIconsAvailable();

public slots:

	void showConfigure() { mConfigure->show(); };

	/// Configure dialog Apply button
	void configApply();

private:
	typedef QHash<QString, QNetStatsView *> TrayIconMap;
	QSystemTrayIcon *mBackupTrayIcon;
	TrayIconMap mViews;
	Configure *mConfigure;

	void setup();

	void setupBackupTrayIcon();

	void saveConfig(const OptionsMap &options);
};

#endif
