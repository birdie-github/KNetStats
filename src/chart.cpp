#include "chart.h"
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QPolygonF>
#include <QPainterPath>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include <algorithm>
#include <cmath>

namespace {
QPainterPath roundedTrace(const QPolygonF &points) {
	QPainterPath path;
	if (points.isEmpty())
		return path;
	path.moveTo(points.first());
	if (points.size() < 2)
		return path;

	QVector<double> slopes(points.size() - 1);
	for (int i = 0; i < slopes.size(); ++i) {
		const double width = points[i + 1].x() - points[i].x();
		slopes[i] = width > 0.0 ? (points[i + 1].y() - points[i].y()) / width : 0.0;
	}
	QVector<double> tangents(points.size(), 0.0);
	tangents.first() = slopes.first();
	tangents.last() = slopes.last();
	for (int i = 1; i < points.size() - 1; ++i) {
		const double before = slopes[i - 1], after = slopes[i];
		// Flatten peaks and troughs. Else limit the shared tangent to both
		// adjacent slopes, keeping each curve inside its measured endpoints.
		if ((before > 0.0 && after > 0.0) || (before < 0.0 && after < 0.0))
			tangents[i] = std::copysign(std::min(std::abs(before), std::abs(after)), before);
	}
	for (int i = 0; i < points.size() - 1; ++i) {
		const QPointF &start = points[i], &end = points[i + 1];
		const double third = (end.x() - start.x()) / 3.0;
		if (third <= 0.0) {
			// Very narrow charts can put adjacent samples on the same pixel.
			path.lineTo(end);
			continue;
		}
		path.cubicTo(QPointF(start.x() + third, start.y() + tangents[i] * third),
			QPointF(end.x() - third, end.y() - tangents[i + 1] * third), end);
	}
	return path;
}
}

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
	mNewestSample = mBufferSize > 0 ? quint64(mBufferSize - 1) : 0;
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
	++mNewestSample;
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

	const double progress = animationProgress();
	const double maximum = mScaleStart + (mScaleTarget - mScaleStart) * progress;
	if (mBufferSize < 2 || mUpload.size() < 2 || width() <= 1 || height() <= 1 || maximum <= 0.0)
		return;

	const double right = width() - 1;
	const double chartHeight = height() - 1;
	const double spacing = right / (mBufferSize - 1);
	const double remaining = mScrollStart * (1.0 - progress);
	const double ratio = devicePixelRatioF();
	// Give each sample a stable pixel position. Scroll the whole trace by an
	// integer number of device pixels, instead of changing every segment's
	// subpixel coverage on every frame. This also works with fractional DPI.
	const double origin = std::round((double(mNewestSample) - remaining) * spacing * ratio);
	QPolygonF upload, download;
	upload.reserve(mUpload.size());
	download.reserve(mDownload.size());
	for (int i = 0; i < mUpload.size(); ++i) {
		const double sample = double(mNewestSample) - (mUpload.size() - 1 - i);
		const double x = std::round(right * ratio) + std::round(sample * spacing * ratio) - origin;
		upload.append(QPointF(x, std::round(chartHeight * (1.0 - mUpload[i] / maximum) * ratio)));
		download.append(QPointF(x, std::round(chartHeight * (1.0 - mDownload[i] / maximum) * ratio)));
	}
	paint.setClipRect(rect());
	paint.setRenderHint(QPainter::Antialiasing, false);
	paint.scale(1.0 / ratio, 1.0 / ratio);
	QPen pen(QColor(mInterfaceOptions->mChartDldColor));
	pen.setWidth(1);
	pen.setCosmetic(true);
	paint.setPen(pen);
	paint.drawPath(roundedTrace(download));
	pen.setColor(QColor(mInterfaceOptions->mChartUplColor));
	paint.setPen(pen);
	paint.drawPath(roundedTrace(upload));
}
