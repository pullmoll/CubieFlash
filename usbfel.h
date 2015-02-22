#ifndef USBFEL_H
#define USBFEL_H

#include <QObject>
#include <QFile>
#include <QLocale>
#include <QtEndian>
#include <libusb.h>


#define SUNXI_FEL_DEVICE_MAJOR  0x1f3a
#define SUNXI_FEL_DEVICE_MINOR  0xefe8
#define SUNXI_SOC_ID_FLASHMODE  0x1610
#define SUNXI_SOC_ID_A10        0x1623
#define SUNXI_SOC_ID_A13        0x1625
#define SUNXI_SOC_ID_A31        0x1633
#define SUNXI_SOC_ID_A20        0x1651
#define SUNXI_SOC_ID_A23        0x1650

typedef struct aw_fel_version_s {
        char		signature[8];
        quint32		soc_id;		/* 0x00162300 */
        quint32		unknown_0a;	/* 1 */
        quint16		protocol;	/* 1 */
        quint8		unknown_12;	/* 0x44 */
        quint8		unknown_13;	/* 0x08 */
        quint32		scratchpad;	/* 0x7e00 */
        quint32		pad[2];		/* unused */
}       aw_fel_version_t;


class usb_FEL : public QObject
{
        Q_OBJECT
public:
        usb_FEL(quint16 major = SUNXI_FEL_DEVICE_MAJOR, quint16 minor = SUNXI_FEL_DEVICE_MINOR, int timeout = 60000, QObject* parent = 0);
        ~usb_FEL();

        typedef enum {
                AW_USB_READ = 0x11,
                AW_USB_WRITE = 0x12,
                AW_USB_FEL_BULK_EP_OUT = 0x01,
                AW_USB_FEL_BULK_EP_IN = 0x82
        }       AW_USB_EP;

        typedef enum {
                AW_FEL_VERSION  = 0x0001,
                AW_FEL_1_WRITE  = 0x0101,
                AW_FEL_1_EXEC   = 0x0102,
                AW_FEL_1_READ   = 0x0103
        }       AW_FEL_1_CMD;

        typedef enum {
                AW_FEL_2_DRAM   = 0,
                AW_FEL_2_NAND   = (1 << 5),
                AW_FEL_2_RDWR   = 0x0201,
                AW_FEL_2_EXEC   = 0x0202,
                AW_FEL_2_0203   = 0x0203,
                AW_FEL_2_0204   = 0x0204,
                AW_FEL_2_0205   = 0x0205,
                AW_FEL_2_WR     = (1 << 12),
                AW_FEL_2_RD     = (1 << 13),
                AW_FEL_2_IO     = (AW_FEL_2_RD | AW_FEL_2_WR),
                AW_FEL_2_FIRST  = (1 << 14),
                AW_FEL_2_LAST   = (1 << 15)
        }       AW_FEL_2_CMD;

        void setDevice(quint16 major, quint32 minor);
        bool find_device();
        bool usb_open();
        bool usb_close();

        bool aw_send_usb_request(quint16 type, qint64 size);
        bool aw_read_usb_response();
        bool aw_usb_write(const void *data, size_t len);
        bool aw_usb_read(void *data, size_t len);
        quint32 aw_fel_get_version(aw_fel_version_t* pver = 0);
        bool aw_fel_read(quint32 offset, void *buf, size_t len);
        bool aw_fel_write(quint32 offset, const void *buf, size_t len);
        bool aw_fel_execute(quint32 offset, quint32 param1 = 0, quint32 param2 = 0);
        bool aw_fel_send_file(quint32 offset, const QString &filename, quint32 chunk_size = 65536, quint32 min_bytes = 0);
        bool aw_send_fel_request(int type, quint32 addr, quint32 length, quint32 pad = 0);
        bool aw_send_fel_4uints(quint32 param1, quint32 param2, quint32 param3, quint32 param4);
        bool aw_read_fel_status();
        bool aw_pad_read(void *buf, size_t len);
        bool aw_pad_write(const void *buf, size_t len);
        bool aw_fel2_read(quint32 offset, void *buf, size_t len, quint32 specs);
        bool aw_fel2_write(quint32 offset, const void *buf, size_t len, quint32 specs);
        bool aw_fel2_send_file(quint32 offset, quint32 specs, const QString &filename, quint32 chunk_size = 65536, quint32 min_bytes = 0);
        bool aw_fel2_exec(quint32 offset = 0, quint32 param1 = 0, quint32 param2 = 0);
        bool aw_fel2_send_4uints(quint32 param1, quint32 param2, quint32 param3, quint32 param4);
        bool aw_fel2_0203(quint32 offset = 0, quint32 param1 = 0, quint32 param2 = 0);
        bool aw_fel2_0203_until_ok();
        bool aw_fel2_0204(quint32 length = 0, quint32 param1 = 0, quint32 param2 = 0);
        bool aw_fel2_0205(quint32 param1 = 0, quint32 param2 = 0, quint32 param3 = 0);

        void aw_fel_hexdump(quint32 offset, size_t size);
        bool aw_fel_dump(quint32 offset, size_t size);
        bool aw_fel_fill(quint32 offset, size_t size, unsigned char value);

        static QString hexdump(const void *data, quint32 offset, size_t size);
signals:
        void Progress(qreal percentage);
        void Error(QString message);
        void Status(QString message);

private:
        int m_rc;
        libusb_context* m_ctx;
        libusb_device_handle* m_usb;
        bool m_detached_iface;
        int m_timeout;
        quint16 m_major;
        quint16 m_minor;
        bool usb_bulk_send(int ep, const void *buff, size_t length);
        bool usb_bulk_recv(int ep, void *buff, size_t length);
        qint64 save_file(const QString &filename, void *data, size_t size);
        QByteArray load_file(const QString &filename, size_t *psize);
};

#endif // USBFEL_H
