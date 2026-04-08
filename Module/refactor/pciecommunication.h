#ifndef PCIECOMMUNICATION_H
#define PCIECOMMUNICATION_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QTimer>
#include <windows.h>
#include <setupapi.h>
#include <strsafe.h>
#include <INITGUID.H>

DEFINE_GUID(GUID_DEVINTERFACE_XDMA,
            0x74c7e4a9, 0x6d5d, 0x4a70, 0xbc, 0x0d, 0x20, 0x69, 0x1d, 0xff, 0x9e, 0x9d);

#define MAX_PATH_LENGEH 260
#define MAX_BYTES_PER_TRANSFER  0x800000  // 数字太大，导致DDR读取失败
//#define MAX_BYTES_PER_TRANSFER  0x800000
#define ONE_MB (1024UL * 1024UL)

/**
 * @brief The PcieCommunication class @huiche
 *
 */
class PcieCommunication : public QObject
{
    Q_OBJECT
public:
    explicit PcieCommunication(QObject *parent = nullptr);

    ~PcieCommunication();

    /**
     * 打开PCIE设备
     * 
     * @param basePath 设备的路径串
     * @return 返回打开设备的结果，0表示成功，非0表示失败
     */
    int pcieOpen(QString basePath);

    /**
     * 从主机到设备传输数据
     * 
     * @param address 设备端的接收地址
     * @param size 传输数据的大小，以字节为单位
     * @param buffer 主机端的数据缓冲区指针
     * @return 返回传输结果，0表示成功，非0表示失败
     */
    int h2cTransfer(unsigned int address, unsigned int size, unsigned char *buffer);

    /**
     * 从设备到主机传输数据
     * 
     * @param address 设备端的发送地址
     * @param size 传输数据的大小，以字节为单位
     * @param buffer 主机端的数据缓冲区指针
     * @return 返回传输结果，0表示成功，非0表示失败
     */
    int c2hTransfer(unsigned int address, unsigned int size, unsigned char *buffer);

    /**
     * 写入用户空间数据到设备
     * 
     * @param address 设备端的写入地址
     * @param size 写入数据的大小，以字节为单位
     * @param buffer 用户数据的缓冲区指针
     * @return 返回写入结果，0表示成功，非0表示失败
     */
    int writeUser(unsigned int address, unsigned int size, unsigned char *buffer);

    /**
     * 从设备读取用户空间数据
     * 
     * @param address 设备端的读取地址
     * @param size 读取数据的大小，以字节为单位
     * @param buffer 存放读取数据的缓冲区指针
     * @return 返回读取结果，0表示成功，非0表示失败
     */
    int readUser(unsigned int address, unsigned int size, unsigned char *buffer);

    /**
     * 关闭PCIE设备连接
     */
    void pcieClose();

signals:

private:
    int writeDevice(HANDLE device, unsigned int address, DWORD size, BYTE *buffer);
    int readDevice(HANDLE device, unsigned int address, DWORD size, BYTE *buffer);

    struct XdmaDevice {
        QString base_path;  // path to first found Xdma device
        QByteArray buffer_c2h;  // buffer for card to host
        QByteArray buffer_h2c;  // buffer for host to card
        quint32 buf_c2h_size;  // size of the buffer c2h in bytes
        quint32 buf_h2c_size;  // size of the buffer h2c in bytes
        HANDLE c2h0;  // handle to c2h0 file
        HANDLE h2c0;  // handle to h2c0 file
        HANDLE user;  // handle to user file
    };

    XdmaDevice xdma;
    QStringList pathList;
};

#endif // PCIECOMMUNICATION_H
