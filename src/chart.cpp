#include "chart.h"
#include <QPainter>
#include <QBrush>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <algorithm>
#include <cmath>

Chart::Chart(const ViewOptions *interfaceOptions, const double *uploadBuffer, const double *downloadBuffer,
			 const double *maxspeed, const int *ptr, int bufferSize)
	: QWidget(), mInterfaceOptions(interfaceOptions), mUplBuffer(uploadBuffer),
	  mDldBuffer(downloadBuffer), mMaxSpeed(maxspeed), mPtr(ptr), mBufferSize(bufferSize),
	  mFrameTimer(new QTimer(this)) {
	mFrameTimer->setInterval(33);
	mFrameTimer->setTimerType(Qt::PreciseTimer);
	connect(mFrameTimer, &QTimer::timeout, this, [this]() {
		emit maximumSpeedChanged(displayedMaximumSpeed());
		update();
		if (animationProgress() >= 1.0)
			mFrameTimer->stop();
	});
	snapshotHistory();
}

double Chart::animationProgress() const {
	return mAnimationClock.isValid()
		? std::min(1.0, double(mAnimationClock.elapsed()) / mAnimationDuration) : 1.0;
}

double Chart::displayedMaximumSpeed() const {
	return mScaleStart + (mScaleTarget - mScaleStart) * animationProgress();
}

void Chart::snapshotHistory() {
	mFrameTimer->stop();
	mAnimationClock.invalidate();
	mScrollStart = 0.0;
	mUpload.clear();
	mDownload.clear();
	// Copy the circular history in chronological order, oldest to newest.
	for (int i = 1; i <= mBufferSize; ++i) {
		const int index = (*mPtr + i) % mBufferSize;
		mUpload.append(mUplBuffer[index]);
		mDownload.append(mDldBuffer[index]);
	}
	mScaleStart = mScaleTarget = *mMaxSpeed;
	emit maximumSpeedChanged(mScaleTarget);
	update();
}

void Chart::sampleUpdated(bool reset) {
	if (!isVisible() || window()->isMinimized())
		return; // showEvent takes a fresh snapshot instead of replaying hidden samples.
	if (reset || mUpload.size() < mBufferSize) {
		snapshotHistory();
		return;
	}

	// Preserve the current scroll position if a sample arrives before the
	// previous transition finishes. Appending adds one interval of delay.
	const double remaining = mScrollStart * (1.0 - animationProgress());
	mScaleStart = displayedMaximumSpeed();
	mScaleTarget = *mMaxSpeed;
	mScrollStart = remaining + 1.0;
	mUpload.append(mUplBuffer[*mPtr]);
	mDownload.append(mDldBuffer[*mPtr]);
	const int retained = mBufferSize + int(std::ceil(mScrollStart));
	if (mUpload.size() > retained) {
		const int excess = mUpload.size() - retained;
		mUpload.remove(0, excess);
		mDownload.remove(0, excess);
	}
	mAnimationDuration = std::max(1, mInterfaceOptions->mUpdateInterval);
	mAnimationClock.start();
	if (!mFrameTimer->isActive())
		mFrameTimer->start();
	update();
}

void Chart::showEvent(QShowEvent *event) {
	snapshotHistory();
	QWidget::showEvent(event);
}

void Chart::hideEvent(QHideEvent *event) {
	mFrameTimer->stop();
	QWidget::hideEvent(event);
}

void Chart::paintEvent(QPaintEvent *event) {
	Q_UNUSED(event);
	QPainter paint(this);
	paint.setBackground(QColor(mInterfaceOptions->mChartBgColor));
	paint.setBackgroundMode(mInterfaceOptions->mChartTransparentBackground ? Qt::TransparentMode : Qt::OpaqueMode);
	QBrush brush(QColor(0x33, 0x33, 0x33), Qt::BrushStyle::CrossPattern);
	paint.fillRect(0, 0, width(), height(), brush);

	const double maximum = displayedMaximumSpeed();
	if (mBufferSize < 2 || mUpload.size() < 2 || width() <= 1 || height() <= 1 || maximum <= 0.0)
		return;

	const double right = width() - 1;
	const double chartHeight = height() - 1;
	const double spacing = right / (mBufferSize - 1);
	const double remaining = mScrollStart * (1.0 - animationProgress());
	paint.setClipRect(rect());
	paint.setRenderHint(QPainter::Antialiasing);
	for (int i = 1; i < mUpload.size(); ++i) {
		const double x = right + spacing * (remaining - (mUpload.size() - 1 - i));
		const double previousX = x - spacing;
		if (x < 0.0 || previousX > right)
			continue;
		paint.setPen(QColor(mInterfaceOptions->mChartDldColor));
		paint.drawLine(QPointF(previousX, chartHeight * (1.0 - mDownload[i - 1] / maximum)),
			QPointF(x, chartHeight * (1.0 - mDownload[i] / maximum)));
		paint.setPen(QColor(mInterfaceOptions->mChartUplColor));
		paint.drawLine(QPointF(previousX, chartHeight * (1.0 - mUpload[i - 1] / maximum)),
			QPointF(x, chartHeight * (1.0 - mUpload[i] / maximum)));
	}
}
