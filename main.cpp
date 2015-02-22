#include "cubieflasher.h"
#include <QApplication>

int main(int argc, char *argv[])
{
        QApplication a(argc, argv);

        a.setApplicationName(QLatin1String("CubieFlasher"));
        a.setApplicationVersion(QLatin1String("0.1.0"));
        a.setOrganizationName(QLatin1String("pullmoll"));
        a.setOrganizationDomain(QLatin1String("mame.myds.me"));

        CubieFlasher w;
        w.show();

        return a.exec();
}
