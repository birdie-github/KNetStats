#ifndef COLORBUTTON_H
#define COLORBUTTON_H

#include <QColor>
#include <QPushButton>

class ColorButton : public QPushButton {
public:
	explicit ColorButton(QWidget *parent = nullptr);

	QColor color() const { return mColor; }
	void setColor(const QColor &color);

private:
	QColor mColor;
};

#endif
