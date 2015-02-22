/*
 * Copyright (C) Jürgen Buchmüller <pullmoll@t-online.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "cubieflasher.h"
#include <QApplication>

int main(int argc, char *argv[])
{
        QApplication a(argc, argv);

        a.setApplicationName(QLatin1String("CubieFlasher"));
        a.setApplicationVersion(QLatin1String("0.1.1"));
        a.setOrganizationName(QLatin1String("pullmoll"));
        a.setOrganizationDomain(QLatin1String("mame.myds.me"));

        CubieFlasher w;
        w.show();

        return a.exec();
}
