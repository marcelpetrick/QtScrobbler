/***************************************************************************
 *   Copyright (C) 2010 by Robert Keevil                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; under version 2 of the License.         *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#include "app.h"

int main(int argc, char **argv)
{
    app cli(argc, argv);
    if (!cli.parse_cmd(argc, argv))
        return 2;

    QTranslator translator;
    if (translator.load(":/language"))
        cli.installTranslator(&translator);

    // Start work after the event loop is active so synchronous failures can set its exit code.
    QTimer::singleShot(0, &cli, &app::run);
    return cli.exec();
}
