#ifndef CUBIEFLASHER_H
#define CUBIEFLASHER_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QTimerEvent>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>

class flasher;

namespace Ui {
class CubieFlasher;
}

class CubieFlasher : public QMainWindow
{
        Q_OBJECT

public:
        explicit CubieFlasher(QWidget *parent = 0);
        ~CubieFlasher();

protected:
        void closeEvent(QCloseEvent *e);
        void timerEvent(QTimerEvent *e);

private slots:
        void quit();
        void flash_NAND();
        void toggleURBs(bool show);
        void about_CubieFlasher();
        void about_qt();

        void displayStatus(QString message);
        void displayError(QString message);
        void displayURB(int urb);
        void displayProgress(qreal percentage);
private:
        void setup_ui();
        void connect_actions();
        flasher* m_flasher;
        Ui::CubieFlasher* ui;
        QProgressBar* m_progress;
        QLabel* m_status;
        QLabel* m_connected;
        int m_timer;
};

#endif // CUBIEFLASHER_H
