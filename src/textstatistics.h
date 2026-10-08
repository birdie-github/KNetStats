#ifndef TEXTSTATISTICS_H
#define TEXTSTATISTICS_H

#include <QIcon>
#include <QImage>
#include <QString>

struct ViewOptions;

QString formatShortRate(double bytesPerSecond);
QImage renderTextStatistics(const ViewOptions &options, const QString &upload,
                           const QString &download, int size);
QIcon textStatisticsIcon(const ViewOptions &options, const QString &upload,
                         const QString &download);

#endif
