#ifndef TRANSFER_H
#define TRANSFER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QRegExp>
#include <QFile>
#include <QDateTime>
#include <QEventLoop>
#include "usbfel.h"

class flasher : public QObject
{
        Q_OBJECT
public:
        flasher(QObject* parent = 0);
        ~flasher();

        bool connected();
        bool flash();
        void showURBs(bool show);

signals:
        void URB(int urb);
        void Progress(qreal percentage);
        void Status(QString message);
        void Error(QString message);

private:
        int m_rc;
        bool m_show_urbs;
        usb_FEL* m_usb;
        aw_fel_version_t m_version;
        quint32 m_scratchpad;
        QString resource(const QString& name);
        bool open_usb();
        bool close_usb();
        void showURB(int urb);
        bool read_log(QByteArray &dest, size_t bytes, const QString &filename);
        bool stage_1_prep();
        bool install_fes_1_1();
        bool install_fes_1_2();
        bool send_crc_table();
        bool install_fes_2();
        bool stage_2_prep();
        bool install_fed_nand();
        bool send_partition(const QString &filename, quint32 sector = 0, quint32 sectors = 0);
        bool send_partitions_and_MBR();
        bool install_uboot();
        bool install_boot0();
        bool restore_system();
        bool stage_1();
        bool stage_2();
};

#endif // TRANSFER_H
