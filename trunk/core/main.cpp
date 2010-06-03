#include <QtGui/QApplication>
#include "core.h"
#include <QSettings>
#include <QDateTime>

Core *core = 0;

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	QApplication::setApplicationName("Saje");
	QApplication::setOrganizationName("Saje");
	QApplication::setOrganizationDomain("scottellis.com.au");

	QApplication::setQuitOnLastWindowClosed(false);

	// make sure the settings file, and it's folder, exist
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings s;
	if(!s.contains("Started"))
		s.setValue("Started", QDateTime::currentDateTime());

	core = new Core(&a);

	int ret = a.exec();

	delete core;
	return ret;
}
