#ifndef STATISTICS_H
#define STATISTICS_H

#include "ui_statisticsbase.h"
#include <qstring.h>
#include <QNetworkInterface>

class KNetStatsView;

class Statistics : public QDialog, public Ui::StatisticsBase {
Q_OBJECT
public:
	explicit Statistics(KNetStatsView *parent);

	void show();

private:
	QTimer *mTimer;
	KNetStatsView *mParent;

public slots:

	void accept();

private slots:

	void update();
};

#endif
