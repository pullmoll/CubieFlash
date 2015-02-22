/*
 * Copyright (C) 2012  Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * Changed by Steven Saunderson (check <http://phelum.net/> for contact details).
 * Converted to C++ class by Jürgen Buchmüller <pullmoll@t-online.de>
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
#include "usbfel.h"
#include <errno.h>

/* Needs _BSD_SOURCE for htole and letoh  */
//#define _BSD_SOURCE
#define _NETBSD_SOURCE

#define HOST_TO_LE(x) qToLittleEndian(x)
#define LE_TO_HOST(x) qFromLittleEndian(x)

typedef struct aw_fel_request_s {
        quint32		request;
        quint32		address;
        quint32		length;
        quint32         pad;
}       aw_fel_request_t;

typedef struct aw_fel_generic_s {
        quint32		param1;
        quint32		param2;
        quint32		param3;
        quint32         param4;
}       aw_fel_generic_t;

usb_FEL::usb_FEL(quint16 major, quint16 minor, int timeout, QObject *parent) :
        QObject(parent),
        m_rc(0),
        m_ctx(0),
        m_usb(0),
        m_detached_iface(false),
        m_timeout(timeout),
        m_major(major),
        m_minor(minor)
{
}

usb_FEL::~usb_FEL()
{
        if (m_usb)
                usb_close();
}

void usb_FEL::setDevice(quint16 major, quint32 minor)
{
        usb_close();
        m_major = major;
        m_minor = minor;
}

bool usb_FEL::find_device()
{
        bool success = false;
        libusb_context* ctx = 0;

        m_rc = libusb_init(&ctx);
        Q_ASSERT(m_rc == 0);
        libusb_device** list = 0;
        ssize_t ndevices = libusb_get_device_list(ctx, &list);
        for (ssize_t i = 0; i < ndevices; i++) {
                libusb_device* device = list[i];
                libusb_device_descriptor desc;
                int rc = libusb_get_device_descriptor(device, &desc);
                if (0 == rc) {
                        if (desc.idVendor == m_major && desc.idProduct == m_minor)
                                success = true;
                }
        }
        libusb_free_device_list(list, 1);
        libusb_exit(ctx);
        return success;
}

bool usb_FEL::usb_open()
{
        m_rc = libusb_init(&m_ctx);
        Q_ASSERT(m_rc == 0);

        m_usb = libusb_open_device_with_vid_pid(m_ctx, m_major , m_minor);
        if (!m_usb) {
                switch (errno) {
                case EACCES:
                        emit Error(tr("You don't have permission to access Allwinner USB FEL device."));
                        emit Status(tr("Root privileges are required to run this tool."));
                        break;
                default:
                        emit Error(tr("ERROR: Allwinner USB FEL device (%1:%2) not found!\n")
                                   .arg(m_major, 4, 16, QChar('0'))
                                   .arg(m_minor, 4, 16, QChar('0')));
                        break;
                }
                return false;
        }

        m_rc = libusb_claim_interface(m_usb, 0);

#if defined(Q_OS_UNIX)
        if (m_rc != LIBUSB_SUCCESS) {
                libusb_detach_kernel_driver(m_usb, 0);
                m_detached_iface = true;
                m_rc = libusb_claim_interface(m_usb, 0);
        }
#endif
        Q_ASSERT(m_rc == 0);
        return m_usb != 0;
}

bool usb_FEL::usb_close()
{
        if (!m_usb) {
                qDebug("%s: not opened", __func__);
                return false;
        }

        libusb_close(m_usb);

#if defined(Q_OS_UNIX)
        if (m_detached_iface) {
                libusb_attach_kernel_driver(m_usb, 0);
                m_detached_iface = false;
        }
#endif
        m_usb = 0;

        libusb_exit(m_ctx);
        m_ctx = 0;

        return m_usb == 0;
}


bool usb_FEL::usb_bulk_send(int ep, const void *buff, size_t length)
{
        uchar* data = (uchar *)(buff);
        int rc = 0;
        qDebug("%s: ep=%02x buff=%p length=%u", __func__, ep, buff, static_cast<unsigned>(length));
        while (length > 0) {
                int sent = 0;
                rc = libusb_bulk_transfer(m_usb, ep, data, length, &sent, m_timeout);
                if (0 != rc) {
                        emit Error(tr("libusb usb_bulk_send error (%1)").arg(rc));
                        break;
                }
                length -= sent;
                data += sent;
        }
        bool success = 0 == rc;
        qDebug("%s: %s", __func__, success ? "SUCCESS" : "FAILED");
        return success;
}

bool usb_FEL::usb_bulk_recv(int ep, void *buff, size_t length)
{
        quint8 *data = reinterpret_cast<quint8 *>(buff);
        int rc = 0;
        qDebug("%s: ep=%02x buff=%p length=%u", __func__, ep, buff, static_cast<unsigned>(length));
        while (length > 0) {
                int recv = 0;
                rc = libusb_bulk_transfer(m_usb, ep, data, length, &recv, m_timeout);
                if (0 != rc) {
                        emit Error(tr("libusb usb_bulk_recv error (%1)").arg(rc));
                        break;
                }
                length -= recv;
                data += recv;
        }
        bool success = 0 == rc;
        qDebug("%s: %s", __func__, success ? "SUCCESS" : "FAILED");
        return success;
}

bool usb_FEL::aw_send_usb_request(quint16 type, qint64 size)
{
        uchar buf[32];
        memset(buf, 0, sizeof(buf));
        buf[ 0] = 'A';
        buf[ 1] = 'W';
        buf[ 2] = 'U';
        buf[ 3] = 'C';
        buf[ 8] = static_cast<uchar>(size >>  0);
        buf[ 9] = static_cast<uchar>(size >>  8);
        buf[10] = static_cast<uchar>(size >> 16);
        buf[11] = static_cast<uchar>(size >> 24);
        buf[12] = static_cast<uchar>(size >> 32);
        buf[13] = static_cast<uchar>(size >> 40);
        buf[14] = static_cast<uchar>(size >> 48);
        buf[15] = static_cast<uchar>(size >> 56);
        buf[16] = static_cast<uchar>(type >>  0);
        buf[17] = static_cast<uchar>(type >>  8);
        buf[18] = static_cast<uchar>(size >> 16);
        buf[19] = static_cast<uchar>(size >> 24);
        buf[20] = static_cast<uchar>(size >> 16);
        buf[21] = static_cast<uchar>(size >> 24);
        bool success     = usb_bulk_send(AW_USB_FEL_BULK_EP_OUT, buf, sizeof(buf));
        qDebug("%s: %s", __func__, success ? "SUCCESS" : "FAILED");
        return success;
}

bool usb_FEL::aw_read_usb_response()
{
        quint32 status;
        uchar buf[13];
        memset(buf, 0, sizeof(buf));

        bool success = usb_bulk_recv(AW_USB_FEL_BULK_EP_IN, buf, sizeof(buf));
        qDebug("%s: %s", __func__, success ? "SUCCESS" : "FAILED");
        if (success) {
                success &=
                        buf[0] == 'A' &&
                        buf[1] == 'W' &&
                        buf[2] == 'U' &&
                        buf[3] == 'S';
                status =
                        static_cast<quint32>(buf[ 8] <<  0) |
                        static_cast<quint32>(buf[ 9] <<  8) |
                        static_cast<quint32>(buf[10] << 16) |
                        static_cast<quint32>(buf[11] << 24);
        }
        qDebug("%s: response %.8s status=0x%08x %s", __func__, buf, status, success ? "SUCCESS" : "FAILED");
        return success;
}

bool usb_FEL::aw_usb_write(const void *data, size_t len)
{
        qDebug("%s: data=%p len=%u", __func__, data, static_cast<unsigned>(len));
        if (!aw_send_usb_request(AW_USB_WRITE, len))
                return false;
        if (!usb_bulk_send(AW_USB_FEL_BULK_EP_OUT, data, len))
                return false;
        return aw_read_usb_response();
}

bool usb_FEL::aw_usb_read(void *data, size_t len)
{
        qDebug("%s: data=%p len=%u", __func__, data, static_cast<unsigned>(len));
        if (!aw_send_usb_request(AW_USB_READ, len))
                return false;
        if (!usb_bulk_send(AW_USB_FEL_BULK_EP_IN, data, len))
                return false;
        return aw_read_usb_response();
}

bool usb_FEL::aw_send_fel_request(int type, quint32 addr, quint32 length, quint32 pad)
{
        aw_fel_request_t req;
        memset (&req, 0, sizeof (req));
        req.request = HOST_TO_LE(type);
        req.address = HOST_TO_LE(addr);
        req.length  = HOST_TO_LE(length);
        req.pad     = HOST_TO_LE(pad);
        qDebug("%s: aw_fel_request", __func__);
        qDebug("%s:     request  : %08x", __func__, req.request);
        qDebug("%s:     address  : %08x", __func__, req.address);
        qDebug("%s:     length   : %08x", __func__, req.length);
        qDebug("%s:     pad      : %08x", __func__, req.pad);
        bool success = aw_usb_write (&req, sizeof(req));
        qDebug("%s: %s", __func__, success ? "SUCCESS" : "FAILED");
        return success;
}

bool usb_FEL::aw_send_fel_4uints( quint32 param1, quint32 param2, quint32 param3, quint32 param4)
{
        aw_fel_generic_t req;
        memset (&req, 0, sizeof (req));
        req.param1 = HOST_TO_LE(param1);
        req.param2 = HOST_TO_LE(param2);
        req.param3 = HOST_TO_LE(param3);
        req.param4 = HOST_TO_LE(param4);
        qDebug("%s: aw_fel_request", __func__);
        qDebug("%s:     param1   : %08x", __func__, req.param1);
        qDebug("%s:     param2   : %08x", __func__, req.param2);
        qDebug("%s:     param3   : %08x", __func__, req.param3);
        qDebug("%s:     param4   : %08x", __func__, req.param4);
        bool success = aw_usb_write (&req, sizeof(req));
        qDebug("%s: %s", __func__, success ? "SUCCESS" : "FAILED");
        return success;
}

bool usb_FEL::aw_read_fel_status()
{
        static const QByteArray status_ok("\xff\xff\x00\x00\x00\x00\x00\x00", 8);
        QByteArray buf;

        buf.fill('\0', 8);
        if (!aw_usb_read(buf.data(), buf.size()))
                return false;

        if (buf != status_ok) {
                emit Error(tr("ERROR: aw_read_fel_status"));
        }
        return buf == status_ok;
}

quint32 usb_FEL::aw_fel_get_version(aw_fel_version_t *pver)
{
        aw_fel_version_t ver;
        if (!aw_send_fel_request(AW_FEL_VERSION, 0, 0))
                return 0;
        if (!aw_usb_read(&ver, sizeof(ver)))
                return 0;
        if (!aw_read_fel_status())
                return 0;

        ver.soc_id     = LE_TO_HOST(ver.soc_id);
        ver.unknown_0a = LE_TO_HOST(ver.unknown_0a);
        ver.protocol   = LE_TO_HOST(ver.protocol);
        ver.scratchpad = LE_TO_HOST(ver.scratchpad);
        ver.pad[0]     = LE_TO_HOST(ver.pad[0]);
        ver.pad[1]     = LE_TO_HOST(ver.pad[1]);

        const char *soc_name = "unknown";
        switch ((ver.soc_id >> 8) & 0xFFFF) {
        case SUNXI_SOC_ID_FLASHMODE:
                soc_name = "A?? flash mode";
                break;
        case SUNXI_SOC_ID_A10:
                soc_name = "A10";
                break;
        case SUNXI_SOC_ID_A13:
                soc_name = "A13";
                break;
        case SUNXI_SOC_ID_A31:
                soc_name = "A31";
                break;
        case SUNXI_SOC_ID_A20:
                soc_name = "A20";
                break;
        case SUNXI_SOC_ID_A23:
                soc_name = "A23";
                break;
        }

        if (pver)
                memcpy(pver, &ver, sizeof(*pver));

        qDebug("signature    : '%.8s'", ver.signature);
        qDebug("soc_id       : %08x (%s)", ver.soc_id, soc_name);
        qDebug("unknown_0a   : %08x", ver.unknown_0a);
        qDebug("protocol     : %04x", ver.protocol);
        qDebug("unknown_12   : %04x", ver.unknown_12);
        qDebug("unknown_13   : %04x", ver.unknown_13);
        qDebug("scratchpad   : %08x", ver.scratchpad);
        qDebug("pad[0]       : %08x", ver.pad[0]);
        qDebug("pad[1]       : %08x", ver.pad[1]);

        return ((ver.soc_id >> 8) & 0xFFFF);
}


bool usb_FEL::aw_fel_read(quint32 offset, void *buf, size_t len)
{
        if (!aw_send_fel_request(AW_FEL_1_READ, offset, len))
                return false;
        if (!aw_usb_read(buf, len))
                return false;
        return aw_read_fel_status();
}


bool usb_FEL::aw_fel_write(quint32 offset, const void *buf, size_t len)
{
        if (!aw_send_fel_request(AW_FEL_1_WRITE, offset, len))
                return false;
        if (!aw_usb_write(buf, len))
                return false;
        return aw_read_fel_status();
}


bool usb_FEL::aw_fel_execute(quint32 offset, quint32 param1, quint32 param2)
{
        if (!aw_send_fel_request(AW_FEL_1_EXEC, offset, param1, param2))
                return false;
        return aw_read_fel_status();
}


bool usb_FEL::aw_fel_send_file(quint32 offset, const QString& filename, quint32 chunk_size, quint32 min_bytes)
{
        QFile fin(filename);
        quint32	file_size;
        quint32	total_written;

        if (!fin.open(QIODevice::ReadOnly)) {
                emit Error(tr("Failed to open file to send: %1").arg(filename));
                return false;
        }
        file_size = fin.size();

        QLocale l = QLocale::system();
        emit Status(tr("Sending %1 (%2 bytes)...")
                    .arg(filename)
                    .arg(l.toString(file_size)));

        if (min_bytes < file_size)
                min_bytes = file_size;

        total_written = 0;
        emit Progress(0);
        while (min_bytes > 0) {
                quint32 read_size = min_bytes < chunk_size ? min_bytes : chunk_size;
                QByteArray buf = fin.read(read_size);
                quint32 bytes_read = buf.size();
                min_bytes -= bytes_read;
                qDebug("%s: offset=0x%08x bytes_read=%u min_bytes=%u chunk_size=%u", __func__,
                       offset, bytes_read, min_bytes, chunk_size);
                while (min_bytes > 0 && bytes_read < chunk_size) {
                        buf.append('\0');
                        min_bytes--;
                }
                total_written += bytes_read;
                emit Progress(100.0 * total_written / file_size);
                if (!aw_fel_write(offset, buf.constData(), bytes_read)) {
                        emit Error(tr("Abort file send at offset %1 of %2.")
                                   .arg(fin.pos()).arg(fin.size()));
                        return false;
                }
                offset += bytes_read;
        }

        fin.close();
        emit Status(tr("Successfully sent %1.").arg(filename));
        return true;
}

bool usb_FEL::aw_pad_read(void *buf, size_t len)
{
        if (!aw_usb_read(buf, len))
                return false;
        return aw_read_fel_status();
}


bool usb_FEL::aw_pad_write(const void *buf, size_t len)
{
        if (!aw_usb_write(buf, len))
                return false;
        return aw_read_fel_status();
}


bool usb_FEL::aw_fel2_read(quint32 offset, void *buf, size_t len, quint32 specs)
{
        specs &= ~AW_FEL_2_IO;
        specs |=  AW_FEL_2_RD;
        if (!aw_send_fel_request(AW_FEL_2_RDWR, offset, len, specs))
                return false;
        if (!aw_usb_read(buf, len))
                return false;
        return aw_read_fel_status();
}


bool usb_FEL::aw_fel2_write(quint32 offset, const void *buf, size_t len, quint32 specs)
{
        specs &= ~AW_FEL_2_IO;
        specs |=  AW_FEL_2_WR;
        if (!aw_send_fel_request(AW_FEL_2_RDWR, offset, len, specs))
                return false;
        if (!aw_usb_write(buf, len))
                return false;
        return aw_read_fel_status();
}


bool usb_FEL::aw_fel2_send_file(quint32 offset, quint32 specs, const QString& filename, quint32 chunk_size, quint32 min_bytes)
{
        QFile fin(filename);
        quint32	file_size;
        quint32	total_written;

        if (!fin.open(QIODevice::ReadOnly)) {
                emit Error(tr("Failed to open file to send: %1").arg(filename));
                return false;
        }
        file_size = fin.size();

        QLocale l = QLocale::system();
        emit Status(tr("Sending %1 (%2 bytes)...")
                    .arg(filename)
                    .arg(l.toString(file_size)));

        if (min_bytes < file_size)
                min_bytes = file_size;

        total_written = 0;
        emit Progress(0);
        while (min_bytes > 0) {
                quint32 read_size = min_bytes < chunk_size ? min_bytes : chunk_size;
                QByteArray buf = fin.read(read_size);
                quint32 bytes_read = buf.size();
                min_bytes -= bytes_read;
                qDebug("%s: offset=0x%08x bytes_read=%u min_bytes=%u chunk_size=%u", __func__,
                       offset, bytes_read, min_bytes, chunk_size);
                while (min_bytes > 0 && bytes_read < chunk_size) {
                        buf.append('\0');
                        min_bytes--;
                }
                total_written += bytes_read;
                emit Progress(100.0 * total_written / file_size);
                if (!aw_fel2_write(offset, buf.constData(), bytes_read, specs)) {
                        emit Error(tr("Abort file send at offset %1 of %2.")
                                   .arg(fin.pos()).arg(fin.size()));
                        return false;
                }
                offset += bytes_read;
        }

        fin.close();
        emit Status(tr("Successfully sent %1.").arg(filename));
        return true;
}


bool usb_FEL::aw_fel2_exec(quint32 offset, quint32 param1, quint32 param2)
{
        return aw_send_fel_request(AW_FEL_2_EXEC, offset, param1, param2);
//	return aw_read_fel_status();
}


bool usb_FEL::aw_fel2_send_4uints(quint32 param1, quint32 param2, quint32 param3, quint32 param4)
{
        aw_send_fel_4uints (param1, param2, param3, param4);
        return aw_read_fel_status();
}


bool usb_FEL::aw_fel2_0203(quint32 offset, quint32 param1, quint32 param2)
{
        return aw_send_fel_request(AW_FEL_2_0203, offset, param1, param2);
//	return aw_read_fel_status();
}


bool usb_FEL::aw_fel2_0203_until_ok()
{
        static const QByteArray reply("\x00\x01", 2);
        QByteArray buf;

        buf.fill('\0', 64);
        buf[0] = '\xff';
        emit Status(tr("...waiting"));
        while (buf.left(2) != reply) {
                if (!aw_fel2_0203())
                        return false;
                if (!aw_pad_read(buf.data(), 32))
                        return false;
        }

        emit Status(tr("Done."));
        return true;
}


bool usb_FEL::aw_fel2_0204(quint32 length, quint32 param1, quint32 param2)
{
        return aw_send_fel_request (AW_FEL_2_0204, length, param1, param2);
//	return aw_read_fel_status();
}


bool usb_FEL::aw_fel2_0205(quint32 param1, quint32 param2, quint32 param3)
{
        if (!aw_send_fel_request(AW_FEL_2_0205, param1, param2, param3))
                return false;
        return aw_read_fel_status();
}

qint64 usb_FEL::save_file(const QString& filename, void *data, size_t size)
{
        QFile out(filename);
        if (!out.open(QIODevice::WriteOnly)) {
                emit Error(tr("Failed to open output file: %1").arg(filename));
                return false;
        }
        qint64 written = out.write(reinterpret_cast<const char *>(data), size);
        out.close();
        return written;
}

QByteArray usb_FEL::load_file(const QString& filename, size_t *psize)
{
        QFile in(filename);
        QByteArray data;
        if (!in.open(QIODevice::ReadOnly)) {
                emit Error(tr("Failed to open input file: %1").arg(filename));
                return data;
        }
        data = in.readAll();
        if (psize)
                *psize = data.size();
        in.close();
        return data;
}

void usb_FEL::aw_fel_hexdump(quint32 offset, size_t size)
{
        QByteArray buf;
        buf.fill('\0', size);
        if (!aw_fel_read(offset, buf.data(), buf.size()))
                return;
        hexdump(buf.constData(), offset, size);
}

bool usb_FEL::aw_fel_dump(quint32 offset, size_t size)
{
        QByteArray buf;
        buf.fill('\0', size);
        if (!aw_fel_read(offset, buf.data(), buf.size()))
                return false;
        int written = fwrite(buf.constData(), buf.size(), 1, stdout);
        return written == buf.size();
}

bool usb_FEL::aw_fel_fill( quint32 offset, size_t size, unsigned char value)
{
        QByteArray buf;
        buf.fill(static_cast<char>(value), size);
        return aw_fel_write(offset, buf, size);
}


QString usb_FEL::hexdump(const void *data, quint32 offset, size_t size)
{
        QString dump;
        const uchar *buf = reinterpret_cast<const uchar *>(data);
        for (size_t j = 0; j < size; j += 16) {
                QString line;
                line += QString("%1: ").arg(offset + j, 8, 16, QChar('0'));
                for (size_t i = 0; i < 16; i++) {
                        if (j + i >= size) {
                                line += QLatin1String("   ");
                        } else {
                                line += QString("%1 ").arg(static_cast<uchar>(buf[j+i]), 2, 16, QChar('0'));
                        }
                }
                line += QLatin1String("  ");
                for (size_t i = 0; i < 16; i++) {
                        if (j + i >= size) {
                                line += QChar(' ');
                        } else if (isprint(buf[j+i])) {
                                line += QChar(buf[j+i]);
                        } else {
                                line += QChar('.');
                        }
                }
                line += QChar('\n');
                dump += line;
        }
        return dump;
}
