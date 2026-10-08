#include "colorbutton.h"

#include <QColorDialog>
#include <QIcon>
#include <QPixmap>

ColorButton::ColorButton(QWidget *parent) : QPushButton(parent) {
	setColor(Qt::black);
	connect(this, &QPushButton::clicked, this, [this]() {
		const QColor selected = QColorDialog::getColor(mColor, this, tr("Select Color"));
		if (selected.isValid())
			setColor(selected);
	});
}

void ColorButton::setColor(const QColor &color) {
	if (!color.isValid())
		return;

	mColor = color;
	QPixmap swatch(24, 16);
	swatch.fill(mColor);
	setIcon(QIcon(swatch));
	setIconSize(swatch.size());
	setText(mColor.name());
}
