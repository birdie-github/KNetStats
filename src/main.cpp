#include <QApplication>

#include "knetstats.h"

const char *programName = "KNetStats";

int main(int argc, char **argv) {
	QApplication::setOrganizationName("KNetStats");
	QApplication::setApplicationName("KNetStats");
	QApplication::setDesktopFileName("com.birdie-github.KNetStats");
	QApplication::setQuitOnLastWindowClosed(false);
	QApplication app(argc, argv);
	KNetStats knetstats;

	return QApplication::exec();
}
