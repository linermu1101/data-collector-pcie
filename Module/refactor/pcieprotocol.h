#ifndef PCIEPROTOCOL_H
#define PCIEPROTOCOL_H



#include <QObject>
#include <QDateTime>
#include <QDataStream>
#include <QtMath>

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QString>
#include <QByteArray>
#include <QSettings>
#include "pciecommunication.h"
#include <strsafe.h>





class PcieProtocol : public QObject
{
    Q_OBJECT
public:
    explicit PcieProtocol(QObject *parent = nullptr);
    PcieProtocol(QObject *parent, QString pciePath, int index);

    ~PcieProtocol();

    struct RegisterParams {
        int trigcard_main_index;   // 主卡编号
        int nf0_r;          // 0x08 参考信号第一极值点
        int nf1_r;          // 0x0C 参考信号第二极值点
        int nf2_r;          // 0x10 参考信号第三极值点
        int nf0_s;          // 0x14 测量信号第一极值点
        int nf1_s;          // 0x18 测量信号第二极值点
        int nf2_s;          // 0x1C 测量信号第三极值点
        int density_ratio_b; // 0x20 相位输出动态范围
        int change_rt;      // 0x24 反馈间隔点数
        int symb_on;        // 0x28 自动判断相位取反开关
        int symbol;         // 0x2C 相位取反控制
        int sym_time;       // 0x30 相位翻转判断时间
        int singal_mode;    // 0x3C 单频/三频选择
        RegisterParams() {
            memset(this, 0, sizeof(RegisterParams));
        }
    } m_regParams;


    #pragma pack(push, 1)  // 使用1字节对齐，确保结构体布局与文件格式完全匹配
    struct InfPara {
        char filetype[10];         // 文件类型：固定为"swip_das"
        short int channelId;       // 通道 ID
        char channelName[12];      // 通道信号名称
        int addr;                  // 数据指针
        float freq;                // 采样率
        int len;                   // 数据采集长度
        int post;                  // 触发后采集长度
        unsigned short int maxDat; // 满量程时的 A/D 转换值
        float lowRang;             // 量程下限
        float highRang;            // 量程上限
        float factor;              // 系数因子
        float Offset;              // 信号偏移量
        char unit[8];              // 物理量单位
        float Dly;                 // 采数延时(ms)
        short int attribDt;        // 数据属性：A/D数据1、整形数据2、实型数据3、数字量数据6
        short int datWth;          // 数据字宽度
        short int sparI1;          // 备用2字节整数 1
        short int sparI2;          // 备用2字节整数 2
        short int sparI3;          // 备用2字节整数 3
        float sparF1;              // 备用4字节浮点 1
        float sparF2;              // 备用4字节浮点 2
        char sparC1[8];            // 备用字符串 1
        char sparC2[16];           // 备用字符串 2
        char sparC3[10];           // 备用字符串 3
    };
    #pragma pack(pop)

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
    static const quint32 REALDATA_ADDRESS = 0x4c;       //0x3c;

    static const quint32 REALDATA_SIZE_ADDRESS = 0x48;  //0x38;

    /// @brief 硬件触发标记地址
    static const quint32 HARDWARE_TAG_ADDRESS = 0x48;   //0x40;

    /// @brief 测量信号三频位置地址
    static const quint32 POSTIONMEASURE_ADDRESS = 0x34;

    /// @brief 参考信号三频位置地址
    static const quint32 POSTIONREF_ADDRESS = 0x30;

    /// @brief 扫描PCIe设备并返回设备列表
    /// @return 设备路径列表
    static QStringList pcieScan();

    /// @brief 复位板卡
    int cmdReset();

    /// @brief 软件触发采集
    void cmdStartBysoft(int samplingTime);
    /// @brief 硬件触发采集
    void cmdStartByHardWare(int samplingTime);

    void cmdSetReadMode(int mode);
    void cmdSetSampleInterval(int sampleInterval);
    void cmdSetFlip(int value);
    void cmdSetFrequencyPostion(int value);
    void cmdSetFilterWindowWidth(int value);
    void cmdSetDefaultParams();

    /**
     * @brief 离线数据采集（DDR缓存）
     * @param triggerMode 触发模式 0软件触发 1硬件触发
     * @param channelIndex 通道序号 从1开始 通道1=1 通道2=2
     * @param collectTime 采样时间 单位us
     * @param samplingRate 当前采样率 20*10^6
     * @param skipCMD 是否跳过采集指令
     */
    void collectOfflineData(int triggerMode, int channelIndex, float collectTime, int samplingRate, QString datFilePath, QString infFilePath, bool skipCMD);
    void generateInfFile(const QString& infFilePath, const InfPara& infpara);
    
    /**
     * @brief 根据配置文件中的通道参数生成多个inf文件
     * @param cardIndex 卡片索引
     * @param basePath inf文件基本路径
     * @param baseFilename 基本文件名
     * @param dataSize 数据大小
     * @param isRealTime 是否是实时数据
     * @return 合并后的inf文件路径
     */
    QString generateMultipleInfFiles(int cardIndex, const QString& basePath, const QString& baseFilename, int dataSize, bool isRealTime);
    
    void collectOfflineAlldata(int cardIndex, int channels, int triggerMode, int samplingRate, float collectTime, QString datPathPre, QString infPathPre, QString gunNumber);
    /**
     * @brief 实时数据采集
     * @param collectTime 采样时间 单位us
     * @param singleSampleTime 单次采集时间片，单位为微秒 (us) 此处0.5ms 1ms
     * @param sampleInterval 挑点采样间隔，单位为微秒 (us) 100 50 20 10 1 此处0.5ms  1ms
     */
    void collectRealTimeData(int triggerMode, float collectTime, int singleSampleTime, int sampleInterval, QString datFilePath, QString infFilePath, QDataStream::ByteOrder byteOrder);

    /// @brief 写入设备寄存器
    /// @param address 寄存器地址
    /// @param value 值
    /// @return 写入操作的结果，成功返回0，失败返回非0值
    int writeRegister(UINT32 address, int value);

    /// @brief 设备寄存器读取值
    /// @param address 寄存器地址
    /// @return 读取值，以字符串形式返回
    QString readRegister(UINT32 address);
    int readRegisterOrBram(unsigned int address, unsigned int size, unsigned char *buffer);

    /// @brief 卡传主机
    int c2hTrans(unsigned int address, unsigned int size, unsigned char *buffer);

    /// @brief 延时毫秒
    /// @param mes 毫秒数
    void delayMs(int mes);

    /// @brief 根据通道序号获取寄存器地址
    quint32 getChannelAddress(int channelIndex);

    QString bytesToHex(const unsigned char *buf, int size);
    int32_t swapEndian(int32_t value);




protected:
    /// @brief 扫描PCIe设备
    /// @param guid 设备GUID
    /// @param dev_path 存储设备路径的列表
    /// @param len_devpath 列表长度
    /// @return 操作结果
    static int scanPcieDevice(GUID guid, QStringList *dev_path, DWORD32 len_devpath);

signals:
    void offlineFinished(int cardindex);
    void offlineProgress(int cardindex, int value);
    void report(QString msg);
    void realFinished(int cardindex);
    void realdataLoaded(int cardindex, int index, int fifoLen, int32_t valueAll);
    void fifoOverflowWarning(int cardindex, double fillRatio);

private:
    PcieCommunication pcieDevice;
    static QStringList devicePathList;
    int m_index;// 线程编号
    
    // FIFO监测相关
    void checkFifoStatus(int currentSize, int maxSize);
    bool isNearOverflow(double fillRatio) const { return fillRatio > 0.8; }
    int m_fifoWarningCount = 0; // FIFO警告计数
};

#endif // PCIEPROTOCOL_H
