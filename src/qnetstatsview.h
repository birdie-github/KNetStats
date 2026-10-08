#ifndef QNETSTATSVIEW_H
#define QNETSTATSVIEW_H

#include <QSystemTrayIcon>
#include <QWidget>
#include <QIcon>
#include <QElapsedTimer>
#include <dirent.h>
#include "configure.h"

class QNetStats;
class Statistics;
class QMenu;
class QTimer;

class QNetStatsView : public QWidget {
Q_OBJECT

public:
	enum BUFFER_SIZES {
		HISTORY_SIZE = 50,    // Tamanho do historico.
		SPEED_BUFFER_SIZE = 10    // Tamanho do buffer usado para calcular a velocidade
	};

	//	Rx e Tx to bytes and packets
	unsigned long long mBRx{}, mBTx{}, mPRx{}, mPTx{};
	// Statistics
	unsigned long long mTotalBytesRx{}, mTotalBytesTx{}, mTotalPktRx{}, mTotalPktTx{};
	// Traffic deltas for the rolling speed window
	double mDeltaBufferRx[SPEED_BUFFER_SIZE]{}, mDeltaBufferTx[SPEED_BUFFER_SIZE]{};
	double mDeltaBufferPRx[SPEED_BUFFER_SIZE]{}, mDeltaBufferPTx[SPEED_BUFFER_SIZE]{};
	// pointer to current traffic delta buffer position
	int mDeltaBufferPtr{};
	int mSpeedHistoryPtr{};

	// History buffer TODO: Make it configurable!
	double mSpeedHistoryRx[HISTORY_SIZE]{};
	double mSpeedHistoryTx[HISTORY_SIZE]{};
	double mMaxSpeed{};
	QString mInterface;                // Current interface

	QNetStatsView(QNetStats *parent, const QString &interface);

	void updateViewOptions();
	void showStatistics();

	// read a value from /sys/class/net/interface/name
	bool readInterfaceNumValue(const char *name, unsigned long long &value);

	///	The current Update Interval in miliseconds
	inline int updateInterval() const;

	const ViewOptions *getViewOptions() const { return &mOptions; }

	// Calculate a rate from traffic deltas and their measured durations.
	inline double calcSpeed(const double *buffer) const;

	inline bool interfaceIsValid() {
		DIR *dir = opendir(mSysDevPath.toLatin1());
		if (!dir)
			return false;
		closedir(dir);
		return true;
	};

	inline bool trayIconVisible() { return mTrayIcon->isVisible(); }

private:
	QNetStats *mParent;
	QString mSysDevPath;            // Path to the device.
	bool mCarrier;                    // Interface carrier is on?
	QSystemTrayIcon *mTrayIcon;
	QMenu *mContextMenu;
	Statistics *mStatistics;        // Statistics window
	ViewOptions mOptions;            // View options
	// Icons
	QIcon mIconNone, mIconTx, mIconRx, mIconBoth;
	QIcon *mCurrentIcon{};            // Current state
	QTimer *mTimer;                    // Timer
	unsigned int mInterfaceIndex{};
	QElapsedTimer mSampleClock;
	double mSampleSeconds[SPEED_BUFFER_SIZE]{};

	void resetSampling();
	bool interfaceHasCarrier() const;
	unsigned int readInterfaceIndex() const;

	// set up the view.
	void setupTrayIcon();

	void updateStats();

	// calc tha max. speed stored in the history buffer
	inline void calcMaxSpeed();

private slots:

	void setupView();

	void checkMissingInterface();

	void iconActivated(QSystemTrayIcon::ActivationReason reason);

	void interfaceMissing();
};

void QNetStatsView::calcMaxSpeed() {
	mMaxSpeed = 0.0;
	for (int i = 0; i < HISTORY_SIZE; ++i) {
		if (mSpeedHistoryRx[i] > mMaxSpeed)
			mMaxSpeed = mSpeedHistoryRx[i];
		if (mSpeedHistoryTx[i] > mMaxSpeed)
			mMaxSpeed = mSpeedHistoryTx[i];
	}
}

double QNetStatsView::calcSpeed(const double *buffer) const {
	double total = 0.0;
	double seconds = 0.0;
	for (int i = 0; i < SPEED_BUFFER_SIZE; ++i) {
		total += buffer[i];
		seconds += mSampleSeconds[i];
	}
	return seconds > 0.0 ? total / seconds : 0.0;
}

int QNetStatsView::updateInterval() const {
	return mOptions.mUpdateInterval;
}

#endif
