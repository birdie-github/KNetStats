#ifndef STATISTICS_H
#define STATISTICS_H

#include "ui_statisticsbase.h"

class QNetStatsView;
class QShowEvent;
class QHideEvent;

class Statistics : public QDialog, public Ui::StatisticsBase {
Q_OBJECT
public:
	explicit Statistics(QNetStatsView *parent);
	void updateTimerInterval();

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	QTimer *mTimer;
	QNetStatsView *mParent;

public slots:

	void showWindow();

	void hideWindow();

private slots:

	void updateStatistics();

	void updateTabSize(int tabIndex);
};

#endif
