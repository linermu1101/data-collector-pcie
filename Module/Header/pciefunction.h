#ifndef PCIEFUNCTION_H
#define PCIEFUNCTION_H

#include <QObject>
#include "windows.h"
#include "setupapi.h"
#include "initguid.h"
#include "planargraph.h"
#include <strsafe.h>
#include "header.h"

#pragma comment(lib, "setupapi.lib")

// DEFINE_GUID(GUID_DEVINTERFACE_XDMA,
//             0x74c7e4a9, 0x6d5d, 0x4a70, 0xbc, 0x0d, 0x20, 0x69, 0x1d, 0xff, 0x9e, 0x9d);

#define MAX_PATH_LENGEH 260
#define FPGA_DDR_START_ADDR 0x00000000
#define MAX_BYTES_PER_TRANSFER  0x800000

// 读取区间段 24新版板卡 仅用到名称 实际地址不从这里取 note@huiche
enum DataRegAddress{
    RealTime_Phase_Addr     =   0x0,
    DDR3_Reference_Addr     =   0x99000000,
    DDR3_Measure_Addr       =   0x80000000,
    DDR3_Phase1_Addr        =   0xB2000000,
    DDR3_Phase2_Addr        =   0xCB000000,
    DDR3_Phase3_Addr        =   0xE4000000,
};

class IniFileInf{
public:
    //实时通道
    QStringList realTime_ini_List;
    QStringList realTime_id_List;

    //挑点通道
    QStringList ini_List;
    QList<BOOL> save_List;
    QStringList id_List;
};

class PcieFunction : public QObject
{
    Q_OBJECT
public:
    explicit PcieFunction(int popertyValue,QObject *parent = nullptr);
    ~PcieFunction();
    bool isStop = false;
    FILE *dataHandle;
    FILE *infHandle;
    bool isSave=false;
    bool trigEnd = false;
    double specturm;
    int ddrcnt = 0;
    int ddrcount = 0; //record the total length of each card
    QList<int> a;

    IniFileInf *saveFileInf;


    /// @brief 复位寄存器
    static const quint32 RESET_REGISTER = 0x0;

    /// @brief 软件触发采集寄存器
    static const quint32 START_SOFTWARE_REGISTER = 0x04;

    /// @brief 硬件触发采集寄存器
    static const quint32 START_HARDWARE_REGISTER = 0x08;

    /// @brief 是否翻转寄存器
    static const quint32 PHASE_FLIP_REGISTER = 0x0c;

    /// @brief 间隔挑点数寄存器
    static const quint32 PHASE_PICKUP_REGISTER = 0x10;

    /// @brief 基频位置寄存器
    static const quint32 BASE_FREQUENCY_POSITION_REGISTER = 0x14;

    /// @brief 滤波窗宽度寄存器
    static const quint32 FILTER_WINDOW_WIDTH_REGISTER = 0x18;

    /// @brief 实时读/读DDR设置寄存器
    static const quint32 READ_MODE_REGISTER = 0x1c;

    /// @brief 通道1DDR起始采集地址
    static const quint32 CHANNEL1_DDRADDRESS = 0x00000000;

    /// @brief 通道2DDR起始采集地址
    static const quint32 CHANNEL2_DDRADDRESS = 0x20000000;

    /// @brief 通道3DDR起始采集地址 此处无效
    static const quint32 CHANNEL3_DDRADDRESS = 0x00000000;

    /// @brief 通道4DDR起始采集地址 此处无效
    static const quint32 CHANNEL4_DDRADDRESS = 0x00000000;

    /// @brief 实时数据起始采集地址
    static const quint32 REALDATA_ADDRESS = 0x3c;

    static const quint32 REALDATA_SIZE_ADDRESS = 0x38;

    /// @brief 测量信号三频位置地址
    static const quint32 POSTIONMEASURE_ADDRESS = 0x34;

    /// @brief 参考信号三频位置地址
    static const quint32 POSTIONREF_ADDRESS = 0x30;


    /// @brief 复位板卡
    int cmdReset();

    /// @brief 软件触发采集
    void cmdStartBysoft(int samplingTime);
    void cmdStartByHardWare(int samplingTime);

    void cmdSetReadMode(int mode);
    void cmdSetSampleInterval(int sampleInterval);
    void cmdSetFlip(int value);
    void cmdSetFrequencyPostion(int value);
    void cmdSetFilterWindowWidth(int value);


public slots:
    void openPcieDev(QString devPath,int index);
    int  allocMemory();
    int writeRegister(UINT32 address, int value); // 新增 寄存器操作
    QString bytesToHex(const unsigned char *buf, int size);
    void delayMs(int mes);
    quint32 getChannelAddress(int channelIndex);
    void collectOfflineData(int triggerMode, int channelIndex, int collectTime, int samplingRate, QString datFilePath);
    void c2h_transfer(PlanarGraph *plot,SettingPara cardPara);
    void h2c_transfer(UINT32 address,UINT32 size,unsigned char *buffer,int index);
    void user_write(UINT32 address,UINT32 size,unsigned char *buffer,int index);
    void user_read(UINT32 address,UINT32 size,unsigned char *buffer,int index);
    void user_read_IRQ(int index,int timeout);
    void closePcieDev();

private:  
    unsigned char *h2c_align_mem_tmp;
    unsigned char *c2h_align_mem_tmp;

    bool openFlag = false;
    bool ddrFlag = false;
    HANDLE h_user;
    HANDLE h_c2h0;
    HANDLE h_h2c0;

    static int scanPcieDevice(GUID guid, QStringList *dev_path, DWORD32 len_devpath);
    static BYTE* allocate_buffer(UINT32 size, UINT32 alignment);
    static int read_device(HANDLE device, unsigned int address, DWORD size, BYTE *buffer);
    static int write_device(HANDLE device, unsigned int address, DWORD size, BYTE *buffer);

signals:
    void finished();
    void report(QString msg);
    void progress(int value);
//    void realdataLoaded(int index, int fifoLen);

    void replotChart(int index);
    void startHardCollection();
    void ddrEnd(int index);
    void write2RFM2g(unsigned char *buffer,unsigned long length);
    void timeoutIRQ();

};

#endif // PCIEFUNCTION_H
