#include "chart.h"
#include <QPainter>
#include <QBrush>

void Chart::paintEvent(QPaintEvent *event) {
	QPainter paint(this);
	paint.setBackground(QColor(mInterfaceOptions->mChartBgColor));
	paint.setBackgroundMode(mInterfaceOptions->mChartTransparentBackground ? Qt::TransparentMode : Qt::OpaqueMode);
	QBrush brush(QColor(0x33, 0x33, 0x33), Qt::BrushStyle::CrossPattern);
	paint.fillRect(0, 0, width(), height(), brush);

	if (mBufferSize < 2 || width() <= 1 || height() <= 1 || *mMaxSpeed <= 0.0)
		return;

	const int right = width() - 1;
	const int HEIGHT = height() - 1;

	int lastX = right;
	int lastRxY = HEIGHT - int(HEIGHT * (mDldBuffer[*mPtr] / (*mMaxSpeed)));
	int lastTxY = HEIGHT - int(HEIGHT * (mUplBuffer[*mPtr] / (*mMaxSpeed)));

	int i = *mPtr;
	for (int count = 1; count < mBufferSize; ++count) {
		if (--i < 0)
			i = mBufferSize - 1;
		const int x = right - int(qint64(right) * count / (mBufferSize - 1));
		int rxY = HEIGHT - int(HEIGHT * (mDldBuffer[i] / (*mMaxSpeed)));
		int txY = HEIGHT - int(HEIGHT * (mUplBuffer[i] / (*mMaxSpeed)));
		paint.setPen(QColor(mInterfaceOptions->mChartDldColor));
		paint.drawLine(lastX, lastRxY, x, rxY);
		paint.setPen(QColor(mInterfaceOptions->mChartUplColor));
		paint.drawLine(lastX, lastTxY, x, txY);

		lastX = x;
		lastRxY = rxY;
		lastTxY = txY;
	}
}

