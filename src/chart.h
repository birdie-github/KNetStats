#ifndef CHART_H
#define CHART_H

#include <QWidget>
#include <QVector>
#include <QElapsedTimer>
#include "configure.h"

class QPaintEvent;
class QShowEvent;
class QHideEvent;
class QTimer;

class Chart : public QWidget {
Q_OBJECT
public:
	Chart(const ViewOptions *interfaceOptions, const double *uploadBuffer, const double *downloadBuffer,
		  const double *maxspeed, const int *ptr, int bufferSize);
	void sampleUpdated(bool reset);
	double displayedMaximumSpeed() const;

signals:
	void maximumSpeedChanged(double speed);

protected:
	void paintEvent(QPaintEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	const ViewOptions *mInterfaceOptions;
	const double *mUplBuffer;
	const double *mDldBuffer;
	const double *mMaxSpeed;
	const int *mPtr;
	int mBufferSize;
	quint64 mNewestSample = 0;
	QVector<double> mUpload, mDownload;
	QTimer *mFrameTimer;
	QElapsedTimer mAnimationClock;
	double mScrollStart = 0.0;
	double mScaleStart = 0.0, mScaleTarget = 0.0;
	int mAnimationDuration = 1;

	double animationProgress() const;
	void snapshotHistory();
};

#endif
