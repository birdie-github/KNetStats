#include "knetstatsview.h"
#include "knetstats.h"
#include <QTimer>
#include <qevent.h>
#include <qpainter.h>

#include <QMenu>
#include <fstream>
#include <algorithm>
#include "statistics.h"

extern const char *programName;

KNetStatsView::KNetStatsView(KNetStats *parent, const QString &interface)
		: mParent(parent), mSysDevPath("/sys/class/net/" + interface + "/") {
	mInterface = interface;
	mCarrier = interfaceIsValid();
	mFirstUpdate = true;

	KNetStats::readInterfaceConfig(interface, &mOptions);
	mTimer = new QTimer(this);
	mStatistics = new Statistics(this);
	mTrayIcon = new QSystemTrayIcon(this);
	mContextMenu = new QMenu(this);
	mContextMenu->addAction("Configure Interfaces", parent, &KNetStats::showConfigure);
	mContextMenu->addAction("Quit KNetStats", parent, []() { QApplication::quit(); });

	setupTrayIcon();
	setupView();

	mTimer->start(mOptions.mUpdateInterval);
	connect(mTrayIcon, &QSystemTrayIcon::activated, this, &KNetStatsView::iconActivated);
}

void KNetStatsView::setupView() {
	if (!interfaceIsValid()) {
		connect(mTimer, &QTimer::timeout, this, &KNetStatsView::checkMissingInterface);
		return;
	}

	mTrayIcon->show();
	connect(mTimer, &QTimer::timeout, this, &KNetStatsView::updateStats);
}

void KNetStatsView::setupTrayIcon() {
	// Load Icons
	mIconError = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_error.png");
	mIconNone = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_none.png");
	mIconTx = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_tx.png");
	mIconRx = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_rx.png");
	mIconBoth = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_both.png");
	mCurrentIcon = &mIconNone;
	mTrayIcon->setToolTip(QString("Monitoring %1").arg(mInterface));
	mTrayIcon->setContextMenu(mContextMenu);
	mTrayIcon->setIcon(*mCurrentIcon);
}

void KNetStatsView::checkMissingInterface() {
	if (interfaceIsValid()) {
		mTrayIcon->show();
		if (mOptions.mNotifications)
			mTrayIcon->showMessage(programName, QString("Interface %1 reappeared!").arg(mInterface),
								   QSystemTrayIcon::Information,
								   3000);
		disconnect(mTimer, &QTimer::timeout, this, &KNetStatsView::checkMissingInterface);
		connect(mTimer, &QTimer::timeout, this, &KNetStatsView::updateStats);
	}
	mParent->checkTrayIconsAvailable();
}

void KNetStatsView::interfaceMissing() {
	resetSampling();
	if (mOptions.mNotifications)
		mTrayIcon->showMessage(programName, QString("Interface %1 disappeared!").arg(mInterface),
							   QSystemTrayIcon::Information,
							   3000);
	mTrayIcon->hide();
	disconnect(mTimer, &QTimer::timeout, this, &KNetStatsView::updateStats);
	connect(mTimer, &QTimer::timeout, this, &KNetStatsView::checkMissingInterface);
	mParent->checkTrayIconsAvailable();
}

void KNetStatsView::updateViewOptions() {
	KNetStats::readInterfaceConfig(mInterface, &mOptions);
	mTimer->setInterval(mOptions.mUpdateInterval);
	mStatistics->updateTimerInterval();
	setupTrayIcon();
}

void KNetStatsView::updateStats() {
	if (!interfaceIsValid()) {
		interfaceMissing();
		return;
	}

	FILE *fp = fopen((mSysDevPath + "carrier").toLatin1(), "r");
	int carrierFlag = '0';

	if (fp) {
		carrierFlag = fgetc(fp);
		// /sys/net/<>/carrier can immediately read EOF if the network state is DOWN. Pin it to 0.
		carrierFlag = (carrierFlag < 0) ? '0' : carrierFlag;
		fclose(fp);
	}

	if (carrierFlag == '0') { // carrier down
		resetSampling();
		if (mCarrier) {
			mCarrier = false;
			if (mOptions.mNotifications)
				mTrayIcon->showMessage(programName, QString("Interface %1 is down!").arg(mInterface),
									   QSystemTrayIcon::Information,
									   3000);
			mTrayIcon->hide();
			mParent->checkTrayIconsAvailable();
		}
		return;
	} else if (!mCarrier) { // carrier up
		mCarrier = true;
		mTrayIcon->show();
		if (mOptions.mNotifications)
			mTrayIcon->showMessage(programName, QString("Interface %1 is up!").arg(mInterface),
								   QSystemTrayIcon::Information,
								   3000);
		mParent->checkTrayIconsAvailable();
	}

	unsigned long long brx{}, btx{}, prx{}, ptx{};
	if (!readInterfaceNumValue("rx_bytes", brx) ||
		!readInterfaceNumValue("tx_bytes", btx) ||
		!readInterfaceNumValue("rx_packets", prx) ||
		!readInterfaceNumValue("tx_packets", ptx)) {
		resetSampling();
		return;
	}

	const bool countersReset = brx < mBRx || btx < mBTx || prx < mPRx || ptx < mPTx;
	if (countersReset)
		resetSampling();

	if (mSampleClock.isValid()) {
		const qint64 elapsedNs = mSampleClock.nsecsElapsed();
		if (elapsedNs <= 0)
			return;
		const double perSecond = 1000000000.0 / double(elapsedNs);
		if (++mSpeedBufferPtr == SPEED_BUFFER_SIZE)
			mSpeedBufferPtr = 0;
		if (++mSpeedHistoryPtr == HISTORY_SIZE)
			mSpeedHistoryPtr = 0;

		mSpeedBufferTx[mSpeedBufferPtr] = (btx - mBTx) * perSecond;
		mSpeedBufferRx[mSpeedBufferPtr] = (brx - mBRx) * perSecond;
		mSpeedBufferPTx[mSpeedBufferPtr] = (ptx - mPTx) * perSecond;
		mSpeedBufferPRx[mSpeedBufferPtr] = (prx - mPRx) * perSecond;
		mSpeedHistoryRx[mSpeedHistoryPtr] = calcSpeed(mSpeedBufferRx);
		mSpeedHistoryTx[mSpeedHistoryPtr] = calcSpeed(mSpeedBufferTx);
		calcMaxSpeed();
	}
	mSampleClock.start();

	QIcon *newIcon;
	if (brx == mBRx) {
		if (btx == mBTx)
			newIcon = &mIconNone;
		else
			newIcon = &mIconTx;
	} else {
		if (btx == mBTx)
			newIcon = &mIconRx;
		else
			newIcon = &mIconBoth;
	}

	if (newIcon != mCurrentIcon) {
		mCurrentIcon = newIcon;
		mTrayIcon->setIcon(*mCurrentIcon);
	}

	// Include existing interface counters on startup, then accumulate safe deltas.
	mTotalBytesRx += mFirstUpdate || brx < mBRx ? brx : brx - mBRx;
	mTotalBytesTx += mFirstUpdate || btx < mBTx ? btx : btx - mBTx;
	mTotalPktRx += mFirstUpdate || prx < mPRx ? prx : prx - mPRx;
	mTotalPktTx += mFirstUpdate || ptx < mPTx ? ptx : ptx - mPTx;
	mFirstUpdate = false;

	mBRx = brx;
	mBTx = btx;
	mPRx = prx;
	mPTx = ptx;
}

bool KNetStatsView::readInterfaceNumValue(const char *name, unsigned long long &value) {
	std::ifstream file((mSysDevPath + "statistics/" + name).toLatin1());
	return bool(file >> value);
}

void KNetStatsView::resetSampling() {
	mSampleClock.invalidate();
	std::fill_n(mSpeedBufferRx, SPEED_BUFFER_SIZE, 0.0);
	std::fill_n(mSpeedBufferTx, SPEED_BUFFER_SIZE, 0.0);
	std::fill_n(mSpeedBufferPRx, SPEED_BUFFER_SIZE, 0.0);
	std::fill_n(mSpeedBufferPTx, SPEED_BUFFER_SIZE, 0.0);
	mSpeedHistoryRx[mSpeedHistoryPtr] = 0.0;
	mSpeedHistoryTx[mSpeedHistoryPtr] = 0.0;
	calcMaxSpeed();
}

void KNetStatsView::iconActivated(QSystemTrayIcon::ActivationReason reason) {
	if (reason == QSystemTrayIcon::ActivationReason::Trigger) {
		if (mStatistics->isVisible())
			mStatistics->hideWindow();
		else
			mStatistics->showWindow();
	} else if (reason == QSystemTrayIcon::ActivationReason::Context) {
		mContextMenu->exec();
	}
}
