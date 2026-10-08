#include "statistics.h"
#include "chart.h"
#include "qnetstatsview.h"
#include <QNetworkInterface>
#include <QHostAddress>
#include <QStringList>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWindow>

namespace {
QPoint mouseGlobalPosition(const QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event->globalPosition().toPoint();
#else
	return event->globalPos();
#endif
}
}

Statistics::Statistics(QNetStatsView *parent)
		: QDialog(parent), Ui::StatisticsBase(), mParent(parent) {

	setupUi(this);
	this->setWindowTitle(QString("Monitoring Interface %1 - QNetStats").arg(parent->mInterface));

	auto *chart = new Chart(parent->getViewOptions(), parent->mSpeedHistoryTx, parent->mSpeedHistoryRx,
							&parent->mMaxSpeed,
							&parent->mSpeedHistoryPtr, QNetStatsView::HISTORY_SIZE);
	mChart->addWidget(chart);
	mChartWidget = chart;
	chart->installEventFilter(this);
	chart->setToolTip(tr("Double-click for compact chart mode"));

	// Keep the existing UI and its margins together. A hidden container also
	// removes all of its spacers and minimum-size requirements from the dialog.
	mNormalContent = new QWidget(this);
	mNormalContent->setLayout(gridLayout_3);
	mRootLayout = new QVBoxLayout(this);
	mRootLayout->setContentsMargins(0, 0, 0, 0);
	mRootLayout->setSpacing(0);
	mRootLayout->addWidget(mNormalContent);

	mInterfaceLabel = new QLabel(parent->mInterface, chart);
	mInterfaceLabel->setFont(font());
	mInterfaceLabel->setAutoFillBackground(true);
	mInterfaceLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
	mInterfaceLabel->adjustSize();
	mInterfaceLabel->move(6, 4);
	mInterfaceLabel->hide();
	this->update();

	mTimer = new QTimer(this);
	updateTimerInterval();
	connect(tabWidget, &QTabWidget::currentChanged, this, &Statistics::updateTabSize);
	updateTabSize(tabWidget->currentIndex());
	connect(mTimer, &QTimer::timeout, this, &Statistics::updateStatistics);
	connect(mTimer, &QTimer::timeout, chart, qOverload<>(&Chart::repaint));
	connect(mOk, &QPushButton::clicked, this, &Statistics::hideWindow);
}

void Statistics::updateStatistics() {
	mMaxSpeed->setText(this->locale().formattedDataSize(mParent->mMaxSpeed) + +"/s");
	mBRx->setText(this->locale().formattedDataSize(mParent->mTotalBytesRx));
	mBTx->setText(this->locale().formattedDataSize(mParent->mTotalBytesTx));
	mByteSpeedRx->setText(this->locale().formattedDataSize(mParent->mSpeedHistoryRx[mParent->mSpeedHistoryPtr]) + "/s");
	mByteSpeedTx->setText(this->locale().formattedDataSize(mParent->mSpeedHistoryTx[mParent->mSpeedHistoryPtr]) + "/s");

	mPRx->setText(QString::number(mParent->mTotalPktRx));
	mPTx->setText(QString::number(mParent->mTotalPktTx));
	mPktSpeedRx->setText(QString::number(mParent->calcSpeed(mParent->mDeltaBufferPRx), 'f', 1) + " pkts/s");
	mPktSpeedTx->setText(QString::number(mParent->calcSpeed(mParent->mDeltaBufferPTx), 'f', 1) + " pkts/s");

	auto interface = QNetworkInterface::interfaceFromName(mParent->mInterface);
	mMTU->setNum(interface.maximumTransmissionUnit());
	mMAC->setText(interface.hardwareAddress());
	if (interface.flags() & QNetworkInterface::IsRunning) {
		QStringList ips, netmasks;
		for (const QNetworkAddressEntry &addr: interface.addressEntries()) {
			ips.append(addr.ip().toString());
			netmasks.append(addr.netmask().toString());
		}
		mIP->setText(ips.join(QLatin1Char('\n')));
		mNetmask->setText(netmasks.join(QLatin1Char('\n')));
		return;
	}
	mIP->setText("Not Connected");
	mNetmask->setText("Not Connected");
	if (!mParent->interfaceIsValid()) {
		mMTU->setText("N/A");
		mMAC->setText("N/A");
	}
}

void Statistics::updateTabSize(int tabIndex) {
	if (tabIndex == -1)
			return;
	for (int i = 0; i < tabWidget->count(); i++) {
		if (i != tabIndex)
			tabWidget->widget(i)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	}

	tabWidget->widget(tabIndex)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	tabWidget->widget(tabIndex)->adjustSize();
}

void Statistics::updateTimerInterval() {
	mTimer->setInterval(mParent->updateInterval());
}

void Statistics::showEvent(QShowEvent *event) {
	updateStatistics();
	mTimer->start(mParent->updateInterval());
	QDialog::showEvent(event);
}

void Statistics::hideEvent(QHideEvent *event) {
	mTimer->stop();
	mDragPending = false;
	mManualDrag = false;
	QDialog::hideEvent(event);
}

void Statistics::showWindow() {
	this->show();
}

void Statistics::hideWindow() {
	this->hide();
}

void Statistics::setCompact(bool compact) {
	if (mCompact == compact)
		return;

	const bool wasVisible = isVisible();
	mDragPending = false;
	mManualDrag = false;
	if (compact) {
		mNormalGeometry = saveGeometry();
		mNormalFlags = windowFlags();
		const QRect chartGeometry(mChartWidget->mapToGlobal(QPoint(0, 0)), mChartWidget->size());
		mCompact = true;
		mChart->removeWidget(mChartWidget);
		mNormalContent->hide();
		mRootLayout->addWidget(mChartWidget);
		mInterfaceLabel->show();
		mInterfaceLabel->raise();
		setWindowFlags(mNormalFlags | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
		setWindowState(Qt::WindowNoState);
		mRootLayout->activate();
		setGeometry(chartGeometry);
		mChartWidget->setToolTip(tr("Drag to move; double-click or press Escape to restore"));
	} else {
		mCompact = false;
		mInterfaceLabel->hide();
		mRootLayout->removeWidget(mChartWidget);
		mChart->addWidget(mChartWidget);
		mNormalContent->show();
		setWindowFlags(mNormalFlags);
		mRootLayout->activate();
		restoreGeometry(mNormalGeometry);
		mChartWidget->setToolTip(tr("Double-click for compact chart mode"));
	}
	// Changing window flags hides the dialog, and reparenting hides the chart.
	mChartWidget->show();
	if (wasVisible)
		show();
}

void Statistics::keyPressEvent(QKeyEvent *event) {
	if (mCompact && event->key() == Qt::Key_Escape) {
		setCompact(false);
		event->accept();
		return;
	}
	QDialog::keyPressEvent(event);
}

bool Statistics::eventFilter(QObject *object, QEvent *event) {
	if (object != mChartWidget)
		return QDialog::eventFilter(object, event);

	if (event->type() == QEvent::MouseButtonDblClick) {
		auto *mouse = static_cast<QMouseEvent *>(event);
		if (mouse->button() == Qt::LeftButton) {
			setCompact(!mCompact);
			return true;
		}
	}
	if (!mCompact)
		return QDialog::eventFilter(object, event);

	if (event->type() == QEvent::MouseButtonPress) {
		auto *mouse = static_cast<QMouseEvent *>(event);
		if (mouse->button() == Qt::LeftButton) {
			mDragPending = true;
			mManualDrag = false;
			mDragStart = mouseGlobalPosition(mouse);
			mDragWindowStart = pos();
			return true;
		}
	} else if (event->type() == QEvent::MouseMove) {
		auto *mouse = static_cast<QMouseEvent *>(event);
		if ((mDragPending || mManualDrag) && (mouse->buttons() & Qt::LeftButton)) {
			const QPoint delta = mouseGlobalPosition(mouse) - mDragStart;
			if (mDragPending && delta.manhattanLength() >= QApplication::startDragDistance()) {
				mDragPending = false;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
				if (windowHandle() && windowHandle()->startSystemMove())
					return true;
#endif
				mManualDrag = true;
			}
			if (mManualDrag)
				move(mDragWindowStart + delta);
			return true;
		}
	} else if (event->type() == QEvent::MouseButtonRelease) {
		auto *mouse = static_cast<QMouseEvent *>(event);
		if (mouse->button() == Qt::LeftButton) {
			mDragPending = false;
			mManualDrag = false;
			return true;
		}
	}
	return QDialog::eventFilter(object, event);
}
