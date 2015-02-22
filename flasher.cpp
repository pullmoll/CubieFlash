/*
 * Copyright (C) Jürgen Buchmüller <pullmoll@t-online.de>
 *
 * Based on code by Steven Saunderson's project CTNandBoot
 * (check <http://phelum.net/> for contact details).
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
#include "flasher.h"

#define ADDR_CRC_TABLE  0x40100000      //!< address of the CRC table
#define ADDR_FES_1      0x40200000      //!< address of fes_2-1.fex
#define ADDR_FES_2      0x00007220
#define ADDR_MAGIC_DE   0x40360000
#define ADDR_FED_NAND   0x40430000
#define ADDR_DRAM_BUFF  0x40600000

flasher::flasher(QObject *parent) :
        QObject(parent),
        m_rc(0),
        m_show_urbs(true),
        m_usb(0),
        m_version(),
        m_scratchpad(0x00007e00)
{
        m_usb = new usb_FEL(SUNXI_FEL_DEVICE_MAJOR, SUNXI_FEL_DEVICE_MINOR, 60000, this);
        connect(m_usb, SIGNAL(Progress(qreal)), this, SIGNAL(Progress(qreal)));
        connect(m_usb, SIGNAL(Status(QString)), this, SIGNAL(Status(QString)));
        connect(m_usb, SIGNAL(Error(QString)), this, SIGNAL(Error(QString)));
}

flasher::~flasher()
{
        delete m_usb;
        m_usb = 0;
}

bool flasher::connected()
{
        return m_usb->find_device();
}

bool flasher::open_usb()
{
        return m_usb->usb_open();
}

bool flasher::close_usb()
{
        return m_usb->usb_close();
}

void flasher::showURBs(bool show)
{
        m_show_urbs = show;
}

/**
 * @brief return a resource path name for a resource name
 * @param name name of the resource
 * @return QString with the filename
 */
QString flasher::resource(const QString &name)
{
        return QString(":/cubietruck/data/%1").arg(name);
}

/**
 * @brief show the URB
 * @param urb URB number of the original tracing
 */
void flasher::showURB(int urb)
{
        qDebug("%s: URB (%06d)", __func__, urb);
        if (m_show_urbs)
                emit URB(urb);
}

/**
 * @brief read a trace log file's hex data into the QByteArray at dest
 * @param dest reference to a QByteArray to fill with data
 * @param bytes number of bytes expected (dest is zero padded to this size)
 * @param name name of the resource to load
 * @return
 */
bool flasher::read_log(QByteArray& dest, size_t bytes, const QString& filename)
{
        QFile in(filename);
        size_t nbyte = 0;
        dest.clear();

        if (!in.open(QIODevice::ReadOnly)) {
                emit Error (tr("Failed to open input file: %1").arg(filename));
                return false;
        }

        while (!in.atEnd()) {
                QString line = in.readLine();
                line = line.remove(QRegExp(QLatin1String("^.*:")));
                line = line.remove(QChar(' '));
                QByteArray data = QByteArray::fromHex(line.toLatin1());
                nbyte += data.size();
                dest.append(data);
        }
        in.close();
        QLocale l = QLocale::system();
        emit Status(tr("Read log file %1 (%2 bytes)")
                    .arg(filename)
                    .arg(l.toString(static_cast<quint64>(nbyte))));
        if (nbyte < bytes) {
                QByteArray pad;
                pad.fill('\0', bytes - nbyte);
                dest.append(pad);
        }
        return true;
}


bool flasher::stage_1_prep()
{
        qDebug("%s: ******** START ********", __func__);

        QByteArray buf;
        showURB(5);
        quint32 version = m_usb->aw_fel_get_version(&m_version);

        // 0x1651 = Cubietruck
        if (version != SUNXI_SOC_ID_A20) {
                emit Error(tr("Expected ID 0x%1, got 0x%2")
                           .arg(SUNXI_SOC_ID_A20, 4, 16, QChar('0'))
                           .arg(version, 4, 16, QChar('0')));
                return false;
        }
        m_scratchpad = m_version.scratchpad;

        showURB(14);
        version = m_usb->aw_fel_get_version();
        qDebug("%s: version=0x%04x", __func__, version);

        // URB 23 - 27
        showURB(23);
        buf.fill('\0', 256);
        // get 256 0xCC
        m_usb->aw_fel_read(m_scratchpad, buf.data(), buf.size());

        for (int i = 0; i < buf.size(); i++) {
                if (buf.at(i) == '\xCC')
                        continue;
                emit Error(tr("Non 0xCC at entry %1 (0x%2)")
                           .arg(i).arg(static_cast<uchar>(buf.at(i)), 2, 16, QChar('0')));
                return false;
        }

        showURB(32);
        version = m_usb->aw_fel_get_version();
        qDebug("%s: version=0x%04x", __func__, version);

        showURB(41);
        buf[0] = '\0';
        buf[1] = '\0';
        buf[2] = '\0';
        buf[3] = '\0';
        m_usb->aw_fel_write(m_scratchpad, buf.data(), buf.size());

        showURB(50);
        version = m_usb->aw_fel_get_version();
        qDebug("%s: version=0x%04x", __func__, version);

        return version == 0x1651;
}


bool flasher::install_fes_1_1()
{
        qDebug("%s: ******** START ********", __func__);

        static const QByteArray DRAM0("DRAM\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);
        QEventLoop loop(this);
        QByteArray buf1;

        QString name = resource(QLatin1String("fes_1-1.fex"));
        QFile in(name);
        if (!in.open(QIODevice::ReadOnly)) {
                emit Error(tr("Failed to open file to send: %1").arg(name));
                return false;
        }
        QByteArray fes_1_1_orig = in.readAll();
        in.close();
        size_t fsize = fes_1_1_orig.size();

        showURB(63);
        if (!read_log(buf1, 0x200, resource(QLatin1String("pt1_000063"))))
                return false;
        if (!m_usb->aw_fel_write(0x7010, buf1.data(), buf1.size()))
                return false;

        showURB(72);
        buf1.fill('\0', 16);
        if (!m_usb->aw_fel_write(0x7210, buf1.data(), buf1.size()))
                return false;

        showURB(77);
        // Load buffer as per URB 81 (0xae0 = 2784) FES_1-1 with nulls after.
        // We do lots of sanity checks here (relic from testing).

        // data from log
        if (!read_log(buf1, 0x0ae0, resource(QLatin1String("pt1_000081"))))
                return false;

        if (buf1.left(fsize) != fes_1_1_orig) {
                emit Error(tr("Dump / fes_1-1 file mismatch"));
                qDebug("%s: file buffer:\n%s", __func__, qPrintable(m_usb->hexdump(buf1.constData(), 0, buf1.size())));
                qDebug("%s: read buffer:\n%s", __func__, qPrintable(m_usb->hexdump(fes_1_1_orig.constData(), 0, fes_1_1_orig.size())));
                return false;
        }

        if (!m_usb->aw_fel_send_file(0x7220, resource(QLatin1String("fes_1-1.fex")), 4000, 2784)) {
                return false;
        }

        // sanity test
        QByteArray buf2;
        buf2.fill('\0', 2784);
        m_usb->aw_fel_read(0x7220, buf2.data(), buf2.size());
        if (buf1.left(fsize) != buf2.left(fsize)) {
                emit Error(tr("Readback mismatch of fes_1-1"));
                qDebug("%s: file buffer:\n%s", __func__, qPrintable(m_usb->hexdump(buf1.constData(), 0, buf1.size())));
                qDebug("%s: read buffer:\n%s", __func__, qPrintable(m_usb->hexdump(buf2.constData(), 0, buf2.size())));
                return false;
        }

        showURB(87);
        if (!m_usb->aw_fel_execute(0x7220))
                return false;

        // need this to avoid error on next USB I/O
        qint64 timeout = QDateTime::currentMSecsSinceEpoch() + Q_INT64_C(500);
        while (QDateTime::currentMSecsSinceEpoch() < timeout) {
                loop.processEvents();
        }

        showURB(96);
        buf1.fill('\0', 16);
        // expect 'DRAM' then nulls
        if (!m_usb->aw_fel_read(0x7210, buf1.data(), buf1.size()))
                return false;

        if (buf1 != DRAM0) {
                emit Error(tr("Compare to DRAM0 lit failed"));
                return false;
        }

        return true;
}


bool flasher::install_fes_1_2()
{
        qDebug("%s: ******** START ********", __func__);

        static const QByteArray DRAM1("DRAM\1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);
        QByteArray buf1;

        showURB(105);
        buf1.fill('\0', 16);
        if (!m_usb->aw_fel_write(0x7210, buf1.data(), buf1.size()))
                return false;

        showURB(114);
        if (!m_usb->aw_fel_send_file(0x2000, resource(QLatin1String("fes_1-2.fex"))))
                return false;

        showURB(120);
        if (!m_usb->aw_fel_execute(0x2000))
                return false;

        showURB(129);
        // expect 'DRAM', 0x01 then nulls
        buf1.fill('\0', 16);
        if (!m_usb->aw_fel_read(0x7210, buf1.data(), buf1.size()))
                return false;

        if (buf1 != DRAM1) {
                emit Error(tr("Compare to DRAM1 lit failed"));
                return false;
        }

        showURB(138);
        // expect as per URB 138
        buf1.fill('\0', 0x200);
        if (!m_usb->aw_fel_read(0x7010, buf1.data(), buf1.size()))
                return false;

        QByteArray buf2;
        if (!read_log(buf2, 0x0200, resource(QLatin1String("pt1_000138"))))
                return false;

        if (buf1 != buf2) {
                emit Error(tr("Compare to pt1_000138 failed"));
                return false;
        }
        return true;
}


bool flasher::send_crc_table()
{
        qDebug("%s: ******** START ********", __func__);

        QByteArray buf1;

        showURB(147);
        if (!read_log(buf1, 0x2000, resource(QLatin1String("pt1_000147"))))
                return false;

        if (!m_usb->aw_fel_write(ADDR_CRC_TABLE, buf1.data(), buf1.size()))
                return false;

        showURB(153);
        // read it back to make sure it's correct
        QByteArray buf2;
        buf2.fill('\0', buf1.size());
        if (!m_usb->aw_fel_read(ADDR_CRC_TABLE, buf2.data(), buf2.size()))
                return false;
        if (buf1 != buf2) {
                emit Error(tr("Compare to pt1_000147 failed"));
                return false;
        }
        return true;
}


bool flasher::install_fes_2()
{
        qDebug("%s: ******** START ********", __func__);

        QByteArray buf;

        showURB(165);
        buf.fill('\0', 16);
        if (!m_usb->aw_fel_write(0x7210, buf.data(), buf.size()))
                return false;

        showURB(174);
        if (!m_usb->aw_fel_send_file(ADDR_FES_1, resource(QLatin1String("fes.fex"))))
                return false;

        showURB(192);
        if (!m_usb->aw_fel_send_file(ADDR_FES_2, resource(QLatin1String("fes_2.fex"))))
                return false;

        showURB(198);
        if (!m_usb->aw_fel_execute(0x7220))
                return false;

        return true;
}


bool flasher::stage_2_prep()
{
        qDebug("%s: ******** START ********", __func__);

        QByteArray buf;
        quint32 version;

        showURB(5);
        version = m_usb->aw_fel_get_version();
        // 0x1651 = Cubietruck
        if (version != SUNXI_SOC_ID_FLASHMODE) {
                emit Error(tr("Expected ID 0x%1, got 0x%2")
                           .arg(SUNXI_SOC_ID_FLASHMODE, 4, 16, QChar('0'))
                           .arg(version, 4, 16, QChar('0')));
                return false;
        }

        showURB(14);
        version = m_usb->aw_fel_get_version();
        qDebug("%s: version=0x%04x", __func__, version);

        showURB(24);
        buf.fill('\0', 256);
        if (!m_usb->aw_fel2_read(m_scratchpad, buf.data(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        for (int i = 0; i < 256; i++) {
                if (buf[i] != static_cast<char>((i < 4) ? '\0' : '\xCC')) {
                        emit Error(tr("Scratchpad incorrect:\n%1")
                                   .arg(m_usb->hexdump(buf.constData(), 0, buf.size())));
                        return false;
                }
        }

        showURB(32);
        version = m_usb->aw_fel_get_version();
        qDebug("%s: version=0x%04x", __func__, version);

        showURB(42);
        if (!m_usb->aw_fel2_write(m_scratchpad, buf.data(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        return true;
}


bool flasher::install_fed_nand()
{
        qDebug("%s: ***************************", __func__);
        QByteArray buf;

        showURB(51);

        if (!read_log(buf, 0x2760, resource(QLatin1String("pt2_000054"))))
                return false;

        if (!m_usb->aw_fel2_write(0x40a00000, buf.data(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        showURB(60);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_start.fex"))))
                return false;

        showURB(69);
        if (!m_usb->aw_fel2_send_file(ADDR_FED_NAND, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("FED_NAND_0000000"))))
                return false;

        showURB(123);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_end.fex"))))
                return false;

        showURB(132);
        if (!m_usb->aw_fel2_exec(ADDR_FED_NAND, 0x31))
                return false;

        showURB(135);
        if (!m_usb->aw_fel2_send_4uints(0x40a00000, 0x40a01000, 0, 0))
                return false;

        showURB(140);
        if (!m_usb->aw_fel2_0203_until_ok())
                return false;

        showURB(150);
        if (!m_usb->aw_fel2_0204(0x0400))
                return false;

        showURB(153);

        // what info is here ?
        buf.fill('\0', 0x0400);
        if (!m_usb->aw_pad_read(buf.data(), buf.size()))
                return false;

        qDebug("%s: unknown buffer:\n %s", __func__,
               qPrintable(m_usb->hexdump(buf.constData(), 0, buf.size())));

        return true;
}


bool flasher::send_partition(const QString& filename, quint32 sector, quint32 sectors)
{
        qDebug("%s: ***************************", __func__);

        QByteArray buf;
        QFile fin(filename);
        qint64 file_size;			// allow for file > 2GB, all untested !!!
        uint file_sectors;
        const uint nand_sec_size = 512;
        uint usb_rec_size;
        uint usb_rec_secs;
        uint sector_key;
        uint sector_limit;
        uint usb_flags;

        usb_rec_size = 65536;
        usb_rec_secs = usb_rec_size / nand_sec_size;
        usb_rec_size = usb_rec_secs * nand_sec_size;

        if (fin.open(QIODevice::ReadOnly)) {
                emit Error(tr("Failed to open file to send: %1").arg(filename));
                return false;
        }

        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_cr_start.fex"))))
                return false;

        file_size = fin.size();
        emit Status(tr("Sending %1 (%2 bytes)...").arg(filename));

        file_sectors = (file_size + nand_sec_size - 1) / nand_sec_size;

        sector_key = sector;
        if (sectors < file_sectors)
                sectors = file_sectors;
        sector_limit = sector_key + sectors;

        while (sector_key < sector_limit) {
                uint read_secs = qMin(usb_rec_secs, sector_limit - sector_key);
                uint read_size = read_secs * nand_sec_size;
                QByteArray data = fin.read(read_size);
                uint bytes_read = data.size();
                if (bytes_read < read_size) {
                        QByteArray pad;
                        pad.fill('\0', read_size - bytes_read);
                        data.append(pad);
                }

                usb_flags = usb_FEL::AW_FEL_2_NAND | usb_FEL::AW_FEL_2_WR;
                if (sector_key == sector)
                        usb_flags |= usb_FEL::AW_FEL_2_FIRST;
                if (sector_key + read_secs == sector_limit)
                        usb_flags |= usb_FEL::AW_FEL_2_LAST;
                if (!m_usb->aw_fel2_write(sector_key, buf.constData(), buf.size(), usb_flags)) {
                        emit Error(trUtf8("Error writing sector(s) %1…%2 of %3")
                                   .arg(sector_key)
                                   .arg(sector_key + read_secs - 1)
                                   .arg(sector_limit));
                        break;
                }
                sector_key += read_secs;
        }
        fin.close();

        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_cr_end.fex"))))
                return false;

        emit Status(tr("Sending %1 done.").arg(filename));
        return true;
}


bool flasher::send_partitions_and_MBR()
{
        qDebug("%s: ******** START ********", __func__);

        return true;    // for now

        static const QLatin1String part1("RFSFAT16_BOOTLOADER_FEX00");
        static const QLatin1String part2("RFSFAT16_ROOTFS_FEX000000");
        static const QLatin1String mbr("1234567890___MBR");
        QByteArray buf;

        buf.fill('\0', 12);
        // reset CRC
        if (!m_usb->aw_fel2_write(0x40023c00, buf.constData(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;
        if (!send_partition(resource(part1), 0x008000))
                return false;
        // read CRC
        if (!m_usb->aw_fel2_read(0x40023c00, buf.data(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;
        qDebug("%s: CRC for %s:\n%s", __func__,
               qPrintable(part1), qPrintable(m_usb->hexdump(buf.constData(), 0, buf.size())));

        buf.fill('\0', 12);
        // reset CRC
        if (!m_usb->aw_fel2_write(0x40023c00, buf.constData(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;
        if (!send_partition(resource(part2), 0x028000))
                return false;
#if     0
        aw_fel2_read (0x40023c00, buf, 0x0c, usb_fel::AW_FEL_2_DRAM);	// read CRC

        buf.fill('\0', 12);
        aw_fel2_write(0x40023c00, buf, 0x0c, usb_fel::AW_FEL_2_DRAM);	// reset CRC
#endif
        if (!send_partition(resource(mbr), 0x000000))
                return false;
        // read CRC
        if (!m_usb->aw_fel2_read(0x40023c00, buf.data(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        // 2 partitions ?
        if (!m_usb->aw_fel2_0205(0x02))
                return false;

        return true;
}


bool flasher::install_uboot()
{
        qDebug("%s: ******** START ********", __func__);
        static const QByteArray reply("updateBootxOk000");
        QByteArray buf;

        showURB(113241);
        if (!m_usb->aw_fel2_send_file(ADDR_DRAM_BUFF, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("UBOOT_0000000000"))))
                return false;

        showURB(113303);
        // pt2_113307 == pt2_000054
        if (!read_log(buf, 0x2760, resource(QLatin1String("pt2_113307"))))
                return false;
        if (!m_usb->aw_fel2_write(0x40400000, buf.constData(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        showURB(113547);
        if (!read_log(buf, 0x00ac, resource(QLatin1String("pt2_113316"))))
                return false;
        if (!m_usb->aw_fel2_write(0x40410000, buf.constData(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        showURB(113322);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_start.fex"))))
                return false;

        showURB(113331);
        if (!m_usb->aw_fel2_send_file(ADDR_FED_NAND, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("UPDATE_BOOT1_000"))))
                return false;

        showURB(113384);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_end.fex"))))
                return false;

        showURB(113394);
        if (!m_usb->aw_fel2_exec(ADDR_FED_NAND, 0x11))
                return false;

        showURB(113397);
        if (!m_usb->aw_fel2_send_4uints(ADDR_DRAM_BUFF, 0x40400000, 0x40410000, 0))
                return false;

        showURB(113402);
        if (!m_usb->aw_fel2_0203_until_ok())
                return false;

        showURB(113502);
        if (!m_usb->aw_fel2_0204(0x0400))
                return false;

        showURB(113505);
        buf.fill('\0', 0x0400);
        if (!m_usb->aw_pad_read(buf.data(), buf.size()))
                return false;

        bool success = buf.mid(24, reply.size()) == reply;
        qDebug("install_uboot result (%s)", success ? "SUCCESS" : "FAILURE");
        if (!success) {
                qDebug("%s", qPrintable(m_usb->hexdump(buf.constData(), 0, buf.size())));
        }

        return success;
}


bool flasher::install_boot0()
{
        qDebug("%s: ******** START ********", __func__);
        static const QByteArray reply("updateBootxOk000");
        QByteArray buf;

        showURB(113514);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_start.fex"))))
                return false;

        showURB(113523);
        if (!m_usb->aw_fel2_send_file(ADDR_DRAM_BUFF, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("BOOT0_0000000000"))))
                return false;

        showURB(113532);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_end.fex"))))
                return false;

        showURB(113541);
        // pt2_113541 == pt2_000054
        if (!read_log(buf, 0x2760, resource(QLatin1String("pt2_113541"))))
                return false;
        if (!m_usb->aw_fel2_write(0x40400000, buf.constData(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        showURB(113547);
        buf.clear();
        if (!read_log(buf, 0x00ac, resource(QLatin1String("pt2_113550"))))
                return false;
        if (!m_usb->aw_fel2_write(0x40410000, buf.constData(), buf.size(), usb_FEL::AW_FEL_2_DRAM))
                return false;

        showURB(113559);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_start.fex"))))
                return false;

        showURB(113565);
        if (!m_usb->aw_fel2_send_file(ADDR_FED_NAND, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("UPDATE_BOOT0_000"))))
                return false;

        showURB(113610);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_end.fex"))))
                return false;

        showURB(113619);
        if (!m_usb->aw_fel2_exec(ADDR_FED_NAND, 0x11))
                return false;

        showURB(113622);
        if (!m_usb->aw_fel2_send_4uints(ADDR_DRAM_BUFF, 0x40400000, 0x40410000, 0))
                return false;

        showURB(113628);
        if (!m_usb->aw_fel2_0203_until_ok())
                return false;

        showURB(113655);
        if (!m_usb->aw_fel2_0204(0x0400))
                return false;

        showURB(113658);
        buf.fill('\0', 0x0400);
        if (!m_usb->aw_pad_read(buf.data(), buf.size()))
                return false;

        bool success = buf.mid(24, reply.size()) == reply;
        qDebug("install_boot0 result (%s)",
               success ? "SUCCESS" : "FAILURE");
        if (!success) {
                qDebug("%s", qPrintable(m_usb->hexdump(buf.constData(), 0, buf.size())));
        }
        return success;
}


bool flasher::restore_system()
{
        qDebug("%s: ******** START ********", __func__);

        QByteArray buf;
        quint32 version;

        showURB(113664);
        version = m_usb->aw_fel_get_version();
        qDebug("%s: version=0x%04x", __func__, version);

        showURB(113673);
        if (!m_usb->aw_fel2_write(m_scratchpad + 4, "\xcd\xa5\x34\x12", 0x04, usb_FEL::AW_FEL_2_DRAM))
                return false;

        showURB(113682);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_start.fex"))))
                return false;

        showURB(113691);
        if (!m_usb->aw_fel2_send_file(ADDR_FED_NAND, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("FET_RESTORE_0000"))))
                return false;

        showURB(113703);
        if (!m_usb->aw_fel2_send_file(ADDR_MAGIC_DE, usb_FEL::AW_FEL_2_DRAM, resource(QLatin1String("magic_de_end.fex"))))
                return false;

        showURB(113709);
        if (!m_usb->aw_fel2_exec(ADDR_FED_NAND, 0x11))
                return false;

        showURB(113712);
        buf.fill('\0', 16);
        if (!m_usb->aw_pad_write(buf.constData(), buf.size()))
                return false;

        // added by me, still in flash mode
        // even though restore done.
//	aw_fel_get_version (handle);

        return true;
}


bool flasher::stage_1()
{
        emit Status(tr("Start of stage %1").arg(1));
        if (!stage_1_prep())
                return false;
        if (!install_fes_1_1())
                return false;
        if (!install_fes_1_2())
                return false;
        if (!send_crc_table())
                return false;
        if (!install_fes_2())
                return false;
        emit Status(tr("End of stage %1").arg(1));
        return true;
}


bool flasher::stage_2()
{
        emit Status(tr("Start of stage %1").arg(2));
        if (!stage_2_prep())
                return false;
        if (!install_fed_nand())
                return false;
        if (!send_partitions_and_MBR())				// not done yet !!!
                return false;
        if (!install_uboot())
                return false;
        if (!install_boot0())
                return false;
        if (!restore_system())
                return false;
        emit Status(tr("End of stage %1").arg(2));
        return true;
}


bool flasher::flash()
{
        QEventLoop loop(this);
        const qint64 msec = 20000;
        bool success;

        success = m_usb->usb_open();
        if (success)
                success &= stage_1();
        if (success)
                success &= m_usb->usb_close();
        if (!success) {
                emit Error(tr("Stage %1 failed - aborting.").arg(1));
                return false;
        }

        emit Status(tr("Waiting up to %1 seconds").arg(.001 * msec, 0, 'g', 2));
        qint64 timeout = QDateTime::currentMSecsSinceEpoch() + 1000;
        qint64 now;
        while ((now = QDateTime::currentMSecsSinceEpoch()) < timeout) {
                emit Progress(100.0 - (timeout - now) * 100.0 / msec);
                loop.processEvents();
        }
        timeout += msec - 1000;
        while ((now = QDateTime::currentMSecsSinceEpoch()) < timeout) {
                emit Progress(100.0 - (timeout - now) * 100.0 / msec);
                loop.processEvents();
                if (m_usb->find_device())
                        break;
        }
        emit Progress(100.0);
        if (success)
                success &= m_usb->usb_open();
        if (success)
                success &= stage_2();
        if (success)
                success &= m_usb->usb_close();
        if (!success) {
                emit Error(tr("Stage %1 failed - aborting.").arg(2));
                return false;
        }

        emit Status(tr("All done!"));

        return 0;
}

