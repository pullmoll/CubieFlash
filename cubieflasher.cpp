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
#include "ui_cubieflasher.h"
#include "about.h"
#include "flasher.h"

CubieFlasher::CubieFlasher(QWidget *parent) :
        QMainWindow(parent),
        m_flasher(0),
        ui(new Ui::CubieFlasher),
        m_progress(0),
        m_status(0),
        m_connected(0),
        m_timer(-1)
{
        setup_ui();

        m_flasher = new flasher(this);
        connect(m_flasher, SIGNAL(URB(int)), this, SLOT(displayURB(int)));
        connect(m_flasher, SIGNAL(Progress(qreal)), this, SLOT(displayProgress(qreal)));
        connect(m_flasher, SIGNAL(Status(QString)), this, SLOT(displayStatus(QString)));
        connect(m_flasher, SIGNAL(Error(QString)), this, SLOT(displayError(QString)));

        m_timer = startTimer(250);
}

CubieFlasher::~CubieFlasher()
{
        delete ui;
}

void CubieFlasher::closeEvent(QCloseEvent *e)
{
        QSettings s;
        s.setValue(QLatin1String("windowState"), saveState());
        s.setValue(QLatin1String("windowGeometry"), saveGeometry());
        s.setValue(QLatin1String("showURBs"), ui->action_Show_URBs->isChecked());
        e->accept();
}

void CubieFlasher::timerEvent(QTimerEvent *e)
{
        if (e->timerId() != m_timer)
                return;
        if (m_flasher->connected()) {
                m_connected->setStyleSheet(QLatin1String("background-color: #00a020;"));
        } else {
                m_connected->setStyleSheet(QLatin1String("background-color: #ff4040;"));
        }
}

void CubieFlasher::quit()
{
        close();
}

void CubieFlasher::flash_NAND()
{
        while (!m_flasher->connected()) {
                int res = QMessageBox::warning(this,
                                               tr("Waiting for Cubietruck FEL connection"),
                                               tr("Please connect the Cubietruck to your PC using the"
                                                  "USB cable plugged in to the OTG port on the front"
                                                  "side of your board."),
                                               QMessageBox::Ok, QMessageBox::Cancel);
                if (QMessageBox::Cancel == res)
                        return;
        }
        m_flasher->flash();
}

void CubieFlasher::about_CubieFlasher()
{
        about dlg;
        dlg.setWindowTitle(tr("%1 Version %2")
                           .arg(qApp->applicationName())
                           .arg(qApp->applicationVersion()));
        dlg.exec();
}

void CubieFlasher::about_qt()
{
        qApp->aboutQt();
}

void CubieFlasher::toggleURBs(bool show)
{
        m_flasher->showURBs(show);
}

void CubieFlasher::displayStatus(QString message)
{
        QEventLoop loop(this);
        ui->textBrowser->setTextColor(qRgb(0x00,0xa0,0x20));
        ui->textBrowser->append(message);
        loop.processEvents();
}

void CubieFlasher::displayError(QString message)
{
        QEventLoop loop(this);
        ui->textBrowser->setTextColor(qRgb(0xff,0x40,0x40));
        ui->textBrowser->append(message);
        loop.processEvents();
}

void CubieFlasher::displayURB(int urb)
{
        QEventLoop loop(this);
        m_status->setText(QString("URB(%1)").arg(urb, 6, 10, QChar('0')));
        loop.processEvents();
}

void CubieFlasher::displayProgress(qreal percentage)
{
        QEventLoop loop(this);
        m_progress->setValue(percentage);
        loop.processEvents();
}

void CubieFlasher::setup_ui()
{
        ui->setupUi(this);
        connect_actions();

        m_connected = new QLabel(tr("[FEL]"));
        ui->statusBar->addWidget(m_connected);

        m_status = new QLabel(tr("Status"));
        m_status->setFrameStyle(QFrame::Panel);
        ui->statusBar->addWidget(m_status, 1);

        m_progress = new QProgressBar;
        m_progress->setMinimumWidth(160);
        ui->statusBar->addPermanentWidget(m_progress);

        QSettings s;
        restoreState(s.value(QLatin1String("windowState")).toByteArray());
        restoreGeometry(s.value(QLatin1String("windowGeometry")).toByteArray());
        ui->action_Show_URBs->setChecked(s.value(QLatin1String("showURBs")).toBool());
}

void CubieFlasher::connect_actions()
{
        connect(ui->action_Flash_NAND, SIGNAL(triggered()), this, SLOT(flash_NAND()));
        connect(ui->action_Quit, SIGNAL(triggered()), this, SLOT(quit()));
        connect(ui->action_Show_URBs, SIGNAL(triggered(bool)), this, SLOT(toggleURBs(bool)));
        connect(ui->action_About_CubieFlasher, SIGNAL(triggered()), this, SLOT(about_CubieFlasher()));
        connect(ui->action_About_Qt, SIGNAL(triggered()), this, SLOT(about_qt()));

        ui->mainToolBar->setIconSize(QSize(32,32));
        ui->mainToolBar->addAction(ui->action_Flash_NAND);
        ui->mainToolBar->addAction(ui->action_Show_URBs);
        ui->mainToolBar->addSeparator();
        ui->mainToolBar->addAction(ui->action_Quit);
        ui->mainToolBar->addSeparator();
        ui->mainToolBar->addAction(ui->action_About_CubieFlasher);
        ui->mainToolBar->addAction(ui->action_About_Qt);
}
