#include "textstatistics.h"
#include "configure.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <algorithm>
#include <cmath>

QString formatShortRate(double bytesPerSecond) {
    static const char units[] = {'\0', 'K', 'M', 'G', 'T', 'P'};
    double value = std::isfinite(bytesPerSecond) && bytesPerSecond > 0.0 ? bytesPerSecond : 0.0;
    int unit = 0;
    while (value >= 1024.0 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    if (value >= 999.5 && unit < 5) {
        value /= 1024.0;
        ++unit;
    }
    value = std::min(value, 999.0 + (unit < 5 ? 0.499 : 0.0));
    QString result = QString::number(value, 'f', unit > 0 && value < 9.95 ? 1 : 0);
    if (unit > 0)
        result += QLatin1Char(units[unit]);
    return result.rightJustified(4, QLatin1Char(' '));
}

namespace {
// Three pixels wide, five high; the identifier does not depend on a system font.
const unsigned char digits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}
};

int maximumRateWidth(const QFontMetrics &metrics) {
    QChar widest = QLatin1Char('0');
    for (char c = '1'; c <= '9'; ++c)
        if (metrics.horizontalAdvance(QLatin1Char(c)) > metrics.horizontalAdvance(widest))
            widest = QLatin1Char(c);
    int width = metrics.horizontalAdvance(QString(3, widest));
    for (char suffix : {'K', 'M', 'G', 'T', 'P'}) {
        width = std::max(width, metrics.horizontalAdvance(QString(3, widest) + QLatin1Char(suffix)));
        width = std::max(width, metrics.horizontalAdvance(QString(widest) + QLatin1Char('.') + widest + QLatin1Char(suffix)));
    }
    return width;
}

void drawRate(QPainter &painter, QRect area, QFont font, const QString &text, int shadowSize) {
    if (area.isEmpty())
        return;
    // Fit the largest size of the chosen family/style that accommodates every
    // short value. Font size stays constant when traffic changes.
    int bestSize = 1;
    for (int pixels = 1; pixels <= area.height(); ++pixels) {
        font.setPixelSize(pixels);
        const QFontMetrics metrics(font);
        if (metrics.height() <= area.height() && maximumRateWidth(metrics) <= area.width())
            bestSize = pixels;
    }
    font.setPixelSize(bestSize);
    painter.setFont(font);
    painter.save();
    painter.setClipRect(area);
    if (shadowSize > 0) {
        const QPen textPen = painter.pen();
        const QColor color = textPen.color();
        // A thin contrasting halo separates the glyph from the digit behind it.
        painter.setPen(qGray(color.rgb()) < 32 ? Qt::white : Qt::black);
        for (int y = -shadowSize; y <= shadowSize; y += shadowSize)
            for (int x = -shadowSize; x <= shadowSize; x += shadowSize)
                if (x != 0 || y != 0)
                    painter.drawText(area.translated(x, y), Qt::AlignRight | Qt::AlignVCenter, text.trimmed());
        painter.setPen(textPen);
    }
    painter.drawText(area, Qt::AlignRight | Qt::AlignVCenter, text.trimmed());
    painter.restore();
}
}

QImage renderTextStatistics(const ViewOptions &options, const QString &upload,
                           const QString &download, int size) {
    if (size < 8)
        return QImage();
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    if (options.mTextTransparentBackground)
        image.fill(Qt::transparent);
    else
        image.fill(QColor(options.mTextBackgroundColor));
    QPainter painter(&image);
    QRect uploadArea(0, 0, size, size / 2);
    QRect downloadArea(0, size / 2, size, size - size / 2);
    const int scale = std::max(1, size / 16);
    const bool right = options.mTextDigitPosition == 1 || options.mTextDigitPosition == 3;
    const bool bottom = options.mTextDigitPosition >= 2;
    const QPoint origin(right ? size - 3 * scale : 0, bottom ? size - 5 * scale : 0);
    // Both traffic rows use the full icon width, regardless of digit position.

    const int digit = std::clamp(options.mTextDigit, 0, 9);
    if (options.mTextShowDigit)
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 3; ++x)
                if (digits[digit][y] & (1 << (2 - x)))
                    painter.fillRect(origin.x() + x * scale, origin.y() + y * scale,
                                     scale, scale, QColor(options.mTextDigitColor));

    painter.setRenderHint(QPainter::TextAntialiasing);
    const int shadowSize = options.mTextShadow ? scale : 0;
    painter.setPen(QColor(options.mTextUploadColor));
    drawRate(painter, uploadArea, options.mTextFont, upload, shadowSize);
    painter.setPen(QColor(options.mTextDownloadColor));
    drawRate(painter, downloadArea, options.mTextFont, download, shadowSize);
    return image;
}

QIcon textStatisticsIcon(const ViewOptions &options, const QString &upload,
                         const QString &download) {
    QIcon icon;
    for (int size : {16, 22, 32, 44, 48, 64})
        icon.addPixmap(QPixmap::fromImage(renderTextStatistics(options, upload, download, size)));
    return icon;
}
