#include "qnetstatsview.h"
#include "qnetstats.h"
#include <QTimer>

#include <QMenu>
#include <fstream>
#include <cstdio>
#include <algorithm>
#include "statistics.h"

extern const char *programName;

QNetStatsView::QNetStatsView(QNetStats *parent, const QString &interface)
		: QWidget(parent), mParent(parent), mSysDevPath("/sys/class/net/" + interface + "/") {
	mInterface = interface;
	mCarrier = false;

	QNetStats::readInterfaceConfig(interface, &mOptions);
	mTimer = new QTimer(this);
	mStatistics = new Statistics(this);
	mTrayIcon = new QSystemTrayIcon(this);
	mContextMenu = new QMenu(this);
	mContextMenu->addAction("Configure Interfaces", parent, &QNetStats::showConfigure);
	mContextMenu->addAction("Quit QNetStats", parent, []() { QApplication::quit(); });

	setupTrayIcon();
	setupView();

	mTimer->start(mOptions.mUpdateInterval);
	connect(mTrayIcon, &QSystemTrayIcon::activated, this, &QNetStatsView::iconActivated);
}

void QNetStatsView::setupView() {
	if (!interfaceIsValid()) {
		connect(mTimer, &QTimer::timeout, this, &QNetStatsView::checkMissingInterface);
		return;
	}

	mCarrier = interfaceHasCarrier();
	mTrayIcon->setVisible(mCarrier);
	connect(mTimer, &QTimer::timeout, this, &QNetStatsView::updateStats);
}

void QNetStatsView::setupTrayIcon() {
	// Load Icons
	mIconNone = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_none.png");
	mIconTx = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_tx.png");
	mIconRx = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_rx.png");
	mIconBoth = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_both.png");
	mCurrentIcon = &mIconNone;
	mTrayIcon->setToolTip(QString("Monitoring %1").arg(mInterface));
	mTrayIcon->setContextMenu(mContextMenu);
	mTrayIcon->setIcon(*mCurrentIcon);
}

void QNetStatsView::checkMissingInterface() {
	if (interfaceIsValid()) {
		mCarrier = interfaceHasCarrier();
		mTrayIcon->setVisible(mCarrier);
		if (mOptions.mNotifications)
			mTrayIcon->showMessage(programName, QString("Interface %1 reappeared!").arg(mInterface),
								   QSystemTrayIcon::Information,
								   3000);
		disconnect(mTimer, &QTimer::timeout, this, &QNetStatsView::checkMissingInterface);
		connect(mTimer, &QTimer::timeout, this, &QNetStatsView::updateStats);
	}
	mParent->checkTrayIconsAvailable();
}

void QNetStatsView::interfaceMissing() {
	mInterfaceIndex = 0;
	resetSampling();
	if (mOptions.mNotifications)
		mTrayIcon->showMessage(programName, QString("Interface %1 disappeared!").arg(mInterface),
							   QSystemTrayIcon::Information,
							   3000);
	mTrayIcon->hide();
	disconnect(mTimer, &QTimer::timeout, this, &QNetStatsView::updateStats);
	connect(mTimer, &QTimer::timeout, this, &QNetStatsView::checkMissingInterface);
	mParent->checkTrayIconsAvailable();
}

void QNetStatsView::updateViewOptions() {
	QNetStats::readInterfaceConfig(mInterface, &mOptions);
	mTimer->setInterval(mOptions.mUpdateInterval);
	mStatistics->updateTimerInterval();
	setupTrayIcon();
}

void QNetStatsView::updateStats() {
	if (!interfaceIsValid()) {
		interfaceMissing();
		return;
	}

	if (!interfaceHasCarrier()) { // carrier down
		resetSampling();
		if (mCarrier) {
			mCarrier = false;
			if (mOptions.mNotifications)
				mTrayIcon->showMessage(programName, QString("Interface %1 is down!").arg(mInterface),
									   QSystemTrayIcon::Information,
									   3000);
		}
		if (mTrayIcon->isVisible()) {
			mTrayIcon->hide();
			mParent->checkTrayIconsAvailable();
		}
		return;
	}
	const bool carrierWasDown = !mCarrier;
	mCarrier = true;
	if (!mTrayIcon->isVisible()) {
		mTrayIcon->show();
		mParent->checkTrayIconsAvailable();
	}
	if (carrierWasDown && mOptions.mNotifications)
		mTrayIcon->showMessage(programName, QString("Interface %1 is up!").arg(mInterface),
							   QSystemTrayIcon::Information,
							   3000);

	const unsigned int interfaceIndex = readInterfaceIndex();
	if (interfaceIndex == 0) {
		resetSampling();
		return;
	}
	unsigned long long brx{}, btx{}, prx{}, ptx{};
	if (!readInterfaceNumValue("rx_bytes", brx) ||
		!readInterfaceNumValue("tx_bytes", btx) ||
		!readInterfaceNumValue("rx_packets", prx) ||
		!readInterfaceNumValue("tx_packets", ptx)) {
		resetSampling();
		return;
	}

	// Do not commit a sample collected across an interface replacement.
	if (readInterfaceIndex() != interfaceIndex) {
		resetSampling();
		return;
	}
	const bool newInterface = interfaceIndex != mInterfaceIndex;
	const bool countersReset = brx < mBRx || btx < mBTx || prx < mPRx || ptx < mPTx;
	if (newInterface || countersReset)
		resetSampling();

	if (mSampleClock.isValid()) {
		const qint64 elapsedNs = mSampleClock.nsecsElapsed();
		if (elapsedNs <= 0)
			return;
		const double elapsedSeconds = double(elapsedNs) / 1000000000.0;
		if (++mDeltaBufferPtr == SPEED_BUFFER_SIZE)
			mDeltaBufferPtr = 0;
		if (++mSpeedHistoryPtr == HISTORY_SIZE)
			mSpeedHistoryPtr = 0;

		mSampleSeconds[mDeltaBufferPtr] = elapsedSeconds;
		mDeltaBufferTx[mDeltaBufferPtr] = btx - mBTx;
		mDeltaBufferRx[mDeltaBufferPtr] = brx - mBRx;
		mDeltaBufferPTx[mDeltaBufferPtr] = ptx - mPTx;
		mDeltaBufferPRx[mDeltaBufferPtr] = prx - mPRx;
		mSpeedHistoryRx[mSpeedHistoryPtr] = calcSpeed(mDeltaBufferRx);
		mSpeedHistoryTx[mSpeedHistoryPtr] = calcSpeed(mDeltaBufferTx);
		calcMaxSpeed();
	}
	mSampleClock.start();

	QIcon *newIcon;
	if (newInterface) {
		newIcon = &mIconNone;
	} else if (brx == mBRx) {
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

	// Include the first counters of each interface lifetime, then safe deltas.
	mTotalBytesRx += newInterface || brx < mBRx ? brx : brx - mBRx;
	mTotalBytesTx += newInterface || btx < mBTx ? btx : btx - mBTx;
	mTotalPktRx += newInterface || prx < mPRx ? prx : prx - mPRx;
	mTotalPktTx += newInterface || ptx < mPTx ? ptx : ptx - mPTx;
	mInterfaceIndex = interfaceIndex;

	mBRx = brx;
	mBTx = btx;
	mPRx = prx;
	mPTx = ptx;
}

bool QNetStatsView::interfaceHasCarrier() const {
	FILE *file = fopen((mSysDevPath + "carrier").toLatin1(), "r");
	if (!file)
		return false;
	// EOF or an unreadable carrier is treated as down.
	const bool carrier = fgetc(file) == '1';
	fclose(file);
	return carrier;
}

unsigned int QNetStatsView::readInterfaceIndex() const {
	unsigned int index{};
	std::ifstream file((mSysDevPath + "ifindex").toLatin1());
	if (!(file >> index))
		return 0;
	return index;
}

bool QNetStatsView::readInterfaceNumValue(const char *name, unsigned long long &value) {
	std::ifstream file((mSysDevPath + "statistics/" + name).toLatin1());
	return bool(file >> value);
}

void QNetStatsView::resetSampling() {
	mSampleClock.invalidate();
	mDeltaBufferPtr = 0;
	std::fill_n(mSampleSeconds, SPEED_BUFFER_SIZE, 0.0);
	std::fill_n(mDeltaBufferRx, SPEED_BUFFER_SIZE, 0.0);
	std::fill_n(mDeltaBufferTx, SPEED_BUFFER_SIZE, 0.0);
	std::fill_n(mDeltaBufferPRx, SPEED_BUFFER_SIZE, 0.0);
	std::fill_n(mDeltaBufferPTx, SPEED_BUFFER_SIZE, 0.0);
	mSpeedHistoryRx[mSpeedHistoryPtr] = 0.0;
	mSpeedHistoryTx[mSpeedHistoryPtr] = 0.0;
	calcMaxSpeed();
}

void QNetStatsView::showStatistics() {
	mStatistics->showWindow();
	mStatistics->raise();
	mStatistics->activateWindow();
}

void QNetStatsView::iconActivated(QSystemTrayIcon::ActivationReason reason) {
	if (reason == QSystemTrayIcon::ActivationReason::Trigger) {
		if (mStatistics->isVisible())
			mStatistics->hideWindow();
		else
			showStatistics();
	}
}
