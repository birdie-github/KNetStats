#include <QApplication>

#include "qnetstats.h"

const char *programName = "QNetStats";

int main(int argc, char **argv) {
	QApplication::setOrganizationName("QNetStats");
	QApplication::setApplicationName("QNetStats");
	QApplication::setDesktopFileName("com.birdie-github.QNetStats");
	QApplication::setQuitOnLastWindowClosed(false);
	QApplication app(argc, argv);
	QNetStats qnetstats;

	return QApplication::exec();
}
