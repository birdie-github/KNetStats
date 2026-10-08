#include <QtGlobal>
#ifdef Q_OS_WIN
#include <winsock2.h>
// MinGW exposes MIB_IF_ROW2 and GetIfEntry2 only with these definitions.
#include <ws2ipdef.h>
#include <iphlpapi.h>
// The Windows COM headers define this common C++ identifier as a macro.
#ifdef interface
#undef interface
#endif
#include <QUuid>
#else
#include <fstream>
#include <cstdio>
#include <QFileInfo>
#endif
#include <algorithm>
#include <QTimer>
#include <QMenu>
#include "qnetstatsview.h"
#include "qnetstats.h"
#include "statistics.h"
#include "textstatistics.h"
#include <QLocale>

extern const char *programName;

#ifdef Q_OS_WIN
namespace {
bool readWindowsInterface(const QString &name, MIB_IF_ROW2 &row) {
	row = {};
	// Qt can return a Windows interface name or fall back to the adapter
	// GUID. Resolve both to a LUID, avoiding mutable aliases.
	if (ConvertInterfaceNameToLuidW(reinterpret_cast<const wchar_t *>(name.utf16()),
								   &row.InterfaceLuid) != NO_ERROR) {
		const QUuid uuid(name);
		if (uuid.isNull())
			return false;
		GUID guid{};
		guid.Data1 = uuid.data1;
		guid.Data2 = uuid.data2;
		guid.Data3 = uuid.data3;
		std::copy_n(uuid.data4, 8, guid.Data4);
		if (ConvertInterfaceGuidToLuid(&guid, &row.InterfaceLuid) != NO_ERROR)
			return false;
	}
	return GetIfEntry2(&row) == NO_ERROR;
}
}
#endif

QNetStatsView::QNetStatsView(QNetStats *parent, const QString &interface)
		: QWidget(parent), mParent(parent)
#ifndef Q_OS_WIN
		, mSysDevPath("/sys/class/net/" + interface + "/")
#endif
		{
	mInterface = interface;
	mCarrier = false;

	QNetStats::readInterfaceConfig(interface, &mOptions);
	mTimer = new QTimer(this);
	mStatistics = new Statistics(this);
	mTrayIcon = new QSystemTrayIcon(this);
	mTextTrayIcon = new QSystemTrayIcon(this);
	mContextMenu = new QMenu(this);
	mContextMenu->addAction("Configure Interfaces", parent, &QNetStats::showConfigure);
	mContextMenu->addAction("Quit QNetStats", parent, []() { QApplication::quit(); });

	setupTrayIcon();
	setupView();

	mTimer->start(mOptions.mUpdateInterval);
	connect(mTrayIcon, &QSystemTrayIcon::activated, this, &QNetStatsView::iconActivated);
	connect(mTextTrayIcon, &QSystemTrayIcon::activated, this, &QNetStatsView::iconActivated);
}

void QNetStatsView::setupView() {
	if (!interfaceIsValid()) {
		connect(mTimer, &QTimer::timeout, this, &QNetStatsView::checkMissingInterface);
		return;
	}

	mCarrier = interfaceHasCarrier();
	mTrayIcon->setVisible(mCarrier && mOptions.mDisplayTrayIcon);
	connect(mTimer, &QTimer::timeout, this, &QNetStatsView::updateStats);
}

void QNetStatsView::setupTrayIcon() {
	// Load Icons
	mIconNone = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_none.png");
	mIconTx = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_tx.png");
	mIconRx = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_rx.png");
	mIconBoth = QIcon(":/img/theme" + QString::number(mOptions.mTheme) + "_both.png");
	mCurrentIcon = &mIconNone;
	mTrayIcon->setToolTip(QString("Monitoring %1").arg(displayName()));
	mTrayIcon->setContextMenu(mContextMenu);
	mTrayIcon->setIcon(*mCurrentIcon);
	mTrayIcon->setVisible(mOptions.mDisplayTrayIcon && mCarrier && interfaceIsValid());
	mTextTrayIcon->setContextMenu(mContextMenu);
	updateTextTrayIcon(true);
	mTextTrayIcon->setVisible(mOptions.mDisplayTextStatistics);
}

void QNetStatsView::checkMissingInterface() {
	if (interfaceIsValid()) {
		mCarrier = interfaceHasCarrier();
		mTrayIcon->setVisible(mCarrier && mOptions.mDisplayTrayIcon);
		if (mOptions.mNotifications)
			mParent->showInterfaceNotification(QString("Interface %1 reappeared!").arg(displayName()),
								   QSystemTrayIcon::Information,
								   3000);
		disconnect(mTimer, &QTimer::timeout, this, &QNetStatsView::checkMissingInterface);
		connect(mTimer, &QTimer::timeout, this, &QNetStatsView::updateStats);
		mParent->checkTrayIconsAvailable();
	}
}

void QNetStatsView::interfaceMissing() {
	mInterfaceIdentity = 0;
	resetSampling();
	if (mOptions.mNotifications)
		mParent->showInterfaceNotification(QString("Interface %1 disappeared!").arg(displayName()),
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
				mParent->showInterfaceNotification(QString("Interface %1 is down!").arg(displayName()),
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
	if (mOptions.mDisplayTrayIcon && !mTrayIcon->isVisible()) {
		mTrayIcon->show();
		mParent->checkTrayIconsAvailable();
	}
	if (carrierWasDown && mOptions.mNotifications)
		mParent->showInterfaceNotification(QString("Interface %1 is up!").arg(displayName()),
							   QSystemTrayIcon::Information,
							   3000);

	const quint64 interfaceIdentity = readInterfaceIdentity();
	if (interfaceIdentity == 0) {
		resetSampling();
		return;
	}
	unsigned long long brx{}, btx{}, prx{}, ptx{};
	if (!readInterfaceCounters(brx, btx, prx, ptx)) {
		resetSampling();
		return;
	}

	// Do not commit a sample collected across an interface replacement.
	if (readInterfaceIdentity() != interfaceIdentity) {
		resetSampling();
		return;
	}
	const bool newInterface = interfaceIdentity != mInterfaceIdentity;
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
	mInterfaceIdentity = interfaceIdentity;

	mBRx = brx;
	mBTx = btx;
	mPRx = prx;
	mPTx = ptx;
	mRatesAvailable = true;
	updateTextTrayIcon();
}

QString QNetStatsView::displayName() const {
	return QNetStats::interfaceDisplayName(mInterface);
}

bool QNetStatsView::interfaceIsValid() const {
#ifdef Q_OS_WIN
	MIB_IF_ROW2 row{};
	return readWindowsInterface(mInterface, row);
#else
	return QFileInfo(mSysDevPath).isDir();
#endif
}

bool QNetStatsView::interfaceHasCarrier() const {
#ifdef Q_OS_WIN
	MIB_IF_ROW2 row{};
	return readWindowsInterface(mInterface, row) && row.OperStatus == IfOperStatusUp;
#else
	FILE *file = fopen((mSysDevPath + "carrier").toLatin1(), "r");
	if (!file)
		return false;
	// EOF or an unreadable carrier is treated as down.
	const bool carrier = fgetc(file) == '1';
	fclose(file);
	return carrier;
#endif
}

quint64 QNetStatsView::readInterfaceIdentity() const {
#ifdef Q_OS_WIN
	MIB_IF_ROW2 row{};
	return readWindowsInterface(mInterface, row) ? row.InterfaceLuid.Value : 0;
#else
	unsigned int index{};
	std::ifstream file((mSysDevPath + "ifindex").toLatin1());
	if (!(file >> index))
		return 0;
	return index;
#endif
}

bool QNetStatsView::readInterfaceCounters(unsigned long long &brx, unsigned long long &btx,
										  unsigned long long &prx, unsigned long long &ptx) const {
#ifdef Q_OS_WIN
	MIB_IF_ROW2 row{};
	if (!readWindowsInterface(mInterface, row))
		return false;
	// One snapshot, using 64-bit counters. Non-unicast includes broadcast
	// and multicast packets, so packet totals cover all traffic.
	brx = row.InOctets;
	btx = row.OutOctets;
	prx = row.InUcastPkts + row.InNUcastPkts;
	ptx = row.OutUcastPkts + row.OutNUcastPkts;
	return true;
#else
	return readInterfaceNumValue("rx_bytes", brx) &&
		readInterfaceNumValue("tx_bytes", btx) &&
		readInterfaceNumValue("rx_packets", prx) &&
		readInterfaceNumValue("tx_packets", ptx);
#endif
}

#ifndef Q_OS_WIN
bool QNetStatsView::readInterfaceNumValue(const char *name, unsigned long long &value) const {
	std::ifstream file((mSysDevPath + "statistics/" + name).toLatin1());
	return bool(file >> value);
}
#endif

void QNetStatsView::resetSampling() {
	mRatesAvailable = false;
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
	updateTextTrayIcon();
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

void QNetStatsView::updateTextTrayIcon(bool force) {
	if (!mOptions.mDisplayTextStatistics)
		return;
	const double upload = calcSpeed(mDeltaBufferTx);
	const double download = calcSpeed(mDeltaBufferRx);
	const QString upl = mRatesAvailable ? formatShortRate(upload) : QString("-");
	const QString dld = mRatesAvailable ? formatShortRate(download) : QString("-");
	if (force || upl != mLastUpload || dld != mLastDownload) {
		mTextTrayIcon->setIcon(textStatisticsIcon(mOptions, upl, dld));
		mLastUpload = upl;
		mLastDownload = dld;
	}
	const QString rates = mRatesAvailable
		? tr("Upload: %1 bytes/s\nDownload: %2 bytes/s")
			.arg(QLocale().toString(upload, 'f', 1), QLocale().toString(download, 'f', 1))
		: tr("Traffic statistics unavailable");
	mTextTrayIcon->setToolTip(QString("%1: %2\n%3")
		.arg(mOptions.mTextDigit).arg(displayName(), rates));
}
