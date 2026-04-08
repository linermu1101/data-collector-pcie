#include "pcieprotocol.h"
#include <QDebug>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include "QsLog.h"
#include "string.h"

QStringList PcieProtocol::devicePathList;

PcieProtocol::PcieProtocol(QObject *parent, QString pciePath, int index)
    : QObject{parent}, m_index(index)
{
    pcieDevice.pcieOpen(pciePath);
}

PcieProtocol::~PcieProtocol()
{
    pcieDevice.pcieClose();
}


/**
 * @brief 查找XDMA设备
 * @param guid
 * @param dev_path
 * @param len_devpath
 * @return
 */
int PcieProtocol::scanPcieDevice(GUID guid, QStringList *dev_path, DWORD32 len_devpath)
{
    SP_DEVICE_INTERFACE_DATA device_interface;
    PSP_DEVICE_INTERFACE_DETAIL_DATA dev_detail;
    HDEVINFO device_info;
    DWORD index = 0;
    wchar_t tmp[MAX_PATH_LENGEH+1];
    char devpath[MAX_PATH_LENGEH+1];

    device_info = SetupDiGetClassDevs((LPGUID)&guid,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(device_info == INVALID_HANDLE_VALUE){
        return 0;
    }

    device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (index = 0; SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, index, &device_interface); ++index) {
        ULONG detailLength = 0;
        if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &detailLength, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            break;
        }
        dev_detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, detailLength);
        if (!dev_detail) {
            break;
        }
        dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, dev_detail, detailLength, NULL, NULL)) {
            HeapFree(GetProcessHeap(), 0, dev_detail);
            break;
        }
        StringCchCopy(tmp, len_devpath, dev_detail->DevicePath);
        wcstombs(devpath, tmp, 256);
        dev_path->append(devpath);
        HeapFree(GetProcessHeap(), 0, dev_detail);
    }
    SetupDiDestroyDeviceInfoList(device_info);
    return index;
}

QStringList PcieProtocol::pcieScan()
{
    devicePathList.clear();
    DWORD numDevice;
    numDevice = scanPcieDevice(GUID_DEVINTERFACE_XDMA, &devicePathList , 261);
    qDebug()<< "pcieInit devicePathList " << devicePathList;
    return devicePathList;
}

int PcieProtocol::cmdReset()
{
    return writeRegister(RESET_REGISTER, 1);
}

void PcieProtocol::cmdStartBysoft(int samplingTime)
{
    // 软件触发 配置文件板卡需要从1开始计数 主卡发0 其他发背板2
    if(m_regParams.trigcard_main_index == m_index+1)
    {
        writeRegister(0x60, 0);     //  主卡：进行配置0
        delayMs(5);
    }
    else{
        writeRegister(0x60, 2);     //  非主卡：进行背板配置2
        delayMs(5);
    }

    // 软件触发mode
    // writeRegister(0x60, 0);
    // delayMs(5);

    // 触发时间
    writeRegister(0x34, samplingTime/10);
    delayMs(5);

    // 开启复位 启动采集
    writeRegister(0x40, 0);
    delayMs(5);
    writeRegister(0x40, 1);
}


void PcieProtocol::cmdStartByHardWare(int samplingTime)
{
    // 配置文件板卡需要从1开始计数 主卡发1 其他发背板2
    if(m_regParams.trigcard_main_index == m_index+1)
    {
        writeRegister(0x60, 1);     //  主卡：进行配置1
        delayMs(5);
    }
    else{
        writeRegister(0x60, 2);     //  非主卡：进行背板配置2
        delayMs(5);
    }


    // 硬件触发mode
    // writeRegister(0x60, 1);
    // delayMs(5);

    writeRegister(0x34, samplingTime/10);
    delayMs(5);
}

void PcieProtocol::cmdSetReadMode(int mode)
{
    writeRegister(READ_MODE_REGISTER, mode);
}

void PcieProtocol::cmdSetSampleInterval(int sampleInterval)
{
    writeRegister(PHASE_PICKUP_REGISTER, sampleInterval);
}

void PcieProtocol::cmdSetFlip(int value)
{
    writeRegister(PHASE_FLIP_REGISTER, value);
}

void PcieProtocol::cmdSetFrequencyPostion(int value)
{
    writeRegister(BASE_FREQUENCY_POSITION_REGISTER, value);
}

void PcieProtocol::cmdSetFilterWindowWidth(int value)
{
    writeRegister(FILTER_WINDOW_WIDTH_REGISTER, value);
}

/**
 * @brief PcieProtocol::cmdSetDefaultParams
 * 设置默认参数
 */
void PcieProtocol::cmdSetDefaultParams()
{
    // 读取配置文件
    QSettings settings(QDir::currentPath() + "/config.ini", QSettings::IniFormat);

    // 加载主副卡
    settings.beginGroup("BASIC_PARA");
    m_regParams.trigcard_main_index = settings.value("HardTrigCard").toInt();
    qDebug() << " m_regParams.trigcard_main_index" << m_regParams.trigcard_main_index;
    settings.endGroup();

    settings.beginGroup("RegisterParams");

    // 读取参数源选择
    int parameter_source = settings.value("parameter_source", 0).toInt();

    // 根据参数源选择读取不同的参数
    if (parameter_source == 0) {
        // 使用计算参数
        m_regParams.nf0_r = settings.value("nf0_r").toInt();
        m_regParams.nf1_r = settings.value("nf1_r").toInt();
        m_regParams.nf2_r = settings.value("nf2_r").toInt();
        m_regParams.nf0_s = settings.value("nf0_s").toInt();
        m_regParams.nf1_s = settings.value("nf1_s").toInt();
        m_regParams.nf2_s = settings.value("nf2_s").toInt();
        qDebug() << "使用计算参数";
    } else {
        // 使用人工参数
        m_regParams.nf0_r = settings.value("nf0_r_manual").toInt();
        m_regParams.nf1_r = settings.value("nf1_r_manual").toInt();
        m_regParams.nf2_r = settings.value("nf2_r_manual").toInt();
        m_regParams.nf0_s = settings.value("nf0_s_manual").toInt();
        m_regParams.nf1_s = settings.value("nf1_s_manual").toInt();
        m_regParams.nf2_s = settings.value("nf2_s_manual").toInt();
        qDebug() << "使用人工参数";
    }

    // 读取其他参数（不受参数源影响）
    m_regParams.density_ratio_b = settings.value("density_ratio_b").toInt();
    m_regParams.change_rt = settings.value("change_rt").toInt();
    m_regParams.symb_on = settings.value("symb_on").toInt();
    m_regParams.symbol = settings.value("symbol").toInt();
    m_regParams.sym_time = settings.value("sym_time").toInt();
    m_regParams.singal_mode = settings.value("singal_mode").toInt();

    qDebug() << " m_regParams.sym_time " << m_regParams.sym_time << " trigcard_main_index " << m_regParams.trigcard_main_index;
    qDebug() << " 下发给板卡的六个极值为 " << m_regParams.nf0_r << m_regParams.nf1_r<< m_regParams.nf2_r<< m_regParams.nf0_s<< m_regParams.nf1_s<< m_regParams.nf2_s;
    settings.endGroup();

    // 复位和其他寄存器写入操作保持不变
    writeRegister(0x60, 3); // 默认背板复位
    delayMs(5);

    writeRegister(0x40, 0);
    delayMs(5);

    // rst_fifo
    writeRegister(0, 1);
    QThread::usleep(1);
    delayMs(5);
    writeRegister(0, 0);
    delayMs(5);

    // rst_xfft
    writeRegister(0x4, 0);
    QThread::usleep(1);
    delayMs(5);
    writeRegister(0x4, 1);
    delayMs(5);

    // 写入选择的参数
    // para2: nf0_r
    writeRegister(0x8, m_regParams.nf0_r);
    delayMs(5);

    // para3: nf1_r
    writeRegister(0xc, m_regParams.nf1_r);
    delayMs(5);

    // para4: nf2_r
    writeRegister(0x10, m_regParams.nf2_r);
    delayMs(5);

    // para5: nf0_s
    writeRegister(0x14, m_regParams.nf0_s);
    delayMs(5);

    // para6: nf1_s
    writeRegister(0x18, m_regParams.nf1_s);
    delayMs(5);

    // para7: nf2_s
    writeRegister(0x1c, m_regParams.nf2_s);
    delayMs(5);

    // para8: density_ratio_b
    writeRegister(0x20, m_regParams.density_ratio_b);
    delayMs(5);

    // para9: change_rt
    writeRegister(0x24, m_regParams.change_rt);
    delayMs(5);

    // para10: symb_on
    writeRegister(0x28, m_regParams.symb_on);
    delayMs(5);

    // para11: symbol
    writeRegister(0x2c, m_regParams.symbol);
    delayMs(5);

    // para12: sym_time
    writeRegister(0x30, m_regParams.sym_time);
    delayMs(5);

    // 34 放采集函数里面
    // rst_ddr3
    writeRegister(0x38, 0);
    delayMs(5);
    writeRegister(0x38, 1);
    delayMs(50);

    // rst_adc 4.25改成负脉冲用于解决第一次波形不对的情形 延时需要100ms太短不行
    writeRegister(0x44, 1);
    delayMs(100);
    writeRegister(0x44, 0);
    delayMs(100);
    writeRegister(0x44, 1);
    delayMs(100);

    // para13: singal_mode
    writeRegister(0x3c, m_regParams.singal_mode);
    delayMs(5);
}


void PcieProtocol::delayMs(int mes)
{
    QTime dieTime = QTime::currentTime().addMSecs(mes);
    while(dieTime>QTime::currentTime()){
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
    }
}

void PcieProtocol::collectRealTimeData(int triggerMode, float collectTime, int singleSampleTime, int sampleInterval, QString datFilePath, QString infFilePath, QDataStream::ByteOrder byteOrder)
{
    qDebug() << "PcieProtocol collectRealTimeData 线程地址: " << QThread::currentThread();

    // int singleSampleTime = 500;  // 单次采集时间，单位为微秒 (us) 此处0.5ms
    // int sampleInterval = 500;      // 挑点采样间隔，单位为微秒 (us) 100 50 20 10 1 此处0.5ms

    // collectTime 要传递过来是us  25.4给板卡设置采样时间的话是ms*100
    // 计算采样点数
    int sampleCount = singleSampleTime / sampleInterval;//目前500/500等于1
    int perSize = 4;
    int sampleLength = sampleCount * perSize;//目前为4
    int cnt = 0;    // 采样序列
    int totalCnt =  collectTime / singleSampleTime;//*980/1024（加上这个实时显示无误但原始数据又会出问题）;//输入采样时间*1000/singleSampleTime（500），也就是总点数为输入时间（ms）*2

    QFile* tempFile = new QFile(datFilePath);
    QElapsedTimer timer;
    
    double x_max = 0;
    double y_max = 0;
    double x_max_1 = 0;
    double y_max_1 = 0;
    double x_max_2 = 0;
    double y_max_2 = 0;
    int xindex = 0;

    //软件触发
    // cmdSetReadMode(0);
    // delayMs(50);
    // cmdSetSampleInterval(sampleInterval);
    // delayMs(50);

    if (!tempFile->isOpen() && tempFile->open(QIODevice::WriteOnly))
    {
        if(triggerMode == 0)
        {
            cmdStartBysoft(collectTime);
        }
        else if(triggerMode == 1)
        {
            cmdStartByHardWare(collectTime);

            QLOG_INFO () << " c2h_transfer hardware wait...";
            QString msg = QString("卡%1等待硬件触发...").arg(m_index+1);
            emit report(msg);
            for (;;)
            {
                byte hardTriggerbuf[4];
                memset(hardTriggerbuf, 0, 4);

                pcieDevice.readUser(HARDWARE_TAG_ADDRESS, 4, hardTriggerbuf);
                int32_t hardTriggerTag = (static_cast<int32_t>(hardTriggerbuf[3]) << 24) |
                                         (static_cast<int32_t>(hardTriggerbuf[2]) << 16) |
                                         (static_cast<int32_t>(hardTriggerbuf[1]) << 8)  |
                                         (static_cast<int32_t>(hardTriggerbuf[0]));

                if(hardTriggerTag > 0 )
                {
                    QString tagbuf = bytesToHex(hardTriggerbuf,4);
                    //QLOG_INFO () << " c2h_transfer hardware trigger...";
                    msg = QString("卡%1 获取信号%2 已硬件触发...").arg(m_index).arg(tagbuf);
                    emit report(msg);
                    break;
                }
                //QThread::usleep(1);
            }
        }


        timer.start();
        while (true) {
            // 判断是否采集完成
            if(cnt >= totalCnt)
            {
                // 结束复位
                writeRegister(0x60, 3);
                delayMs(5);
                qDebug() << "collectRealTimeData over";
                break;
            }

            // 分配缓冲区
            unsigned char buffer[sampleLength];
            memset(buffer, 0, sampleLength);

            byte buf[4];
            memset(buf, 0, 4);

            // read length
            int readRightSize = pcieDevice.readUser(REALDATA_SIZE_ADDRESS, 4, buf);
            // 将 buf[4] 转换为 int32_t
            int32_t readSizeFromboard = (static_cast<int32_t>(buf[3]) << 24) |
                            (static_cast<int32_t>(buf[2]) << 16) |
                            (static_cast<int32_t>(buf[1]) << 8)  |
                            (static_cast<int32_t>(buf[0]));

            // 读取数据
            if(readSizeFromboard > 0)
            {
                pcieDevice.readUser(REALDATA_ADDRESS, sampleLength, buffer);
            }

             //qDebug()<<" readbuffer= "  << buffer << " readSizeFromboard= "<< readSizeFromboard;
             if(readSizeFromboard < sampleCount)
             {
                 // 添加动态等待策略，根据实际FIFO填充率来调整等待时间
                 // 填充率越高，等待时间越长，给系统更多时间处理数据
                 int waitTime = 0;
                 if(readSizeFromboard > 0) {
                     // 计算填充率并动态调整等待时间
                     double fillRatio = (double)readSizeFromboard / sampleCount;
                     if(fillRatio < 0.3) {
                         waitTime = 1;  // 填充低，短暂等待
                     } else if(fillRatio < 0.7) {
                         waitTime = 2;  // 填充中等，适中等待
                     } else {
                         waitTime = 5;  // 填充高，等待较长时间
                     }
                     QThread::usleep(waitTime);
                 }

                 int nelapsed = timer.elapsed();
                 if (nelapsed >= (collectTime / 1000 +5000)) {
                     timer.restart();
                     QString regRealData = bytesToHex(buffer, sampleLength);
                     QString regRealDataLength = bytesToHex(buf, 4);
                     QString errorMsg = QString("采集超时 \n#### 0x54实时数据 ：%1 \n#### 0x50返回长度：%2 获取内容：%3 总共耗时%4毫秒")
                                            .arg(regRealData)
                                            .arg(readRightSize).arg(regRealDataLength).arg(nelapsed);
                     emit report(errorMsg);

                     break; // break;
                 }

                 // 检查FIFO状态并发出必要的警告
                 checkFifoStatus(readSizeFromboard, sampleCount);

                 continue;
             }

            // 更新信号
            qint64 res = tempFile->write(reinterpret_cast<const char *>(buffer), sampleLength);
            if (res != sampleLength) {
                qCritical() << "Failed to write data to file";
                break;
            }

            // 优化：仅处理必要的数据点进行显示，减少计算负担
            int16_t highValueTemp = 0;
            int16_t lowValueTemp = 0;
            int32_t valueTemp = 0;

            // 获取首个样本点值用于显示
            int32_t  *dBuffer = reinterpret_cast<int32_t*>(buffer);
            valueTemp = dBuffer[0];
            
            // 只在需要时解析和处理数据点（例如首个数据点或日志显示点）
            if (xindex == 0 || cnt % 100 == 0) {
                int32_t value = valueTemp;
                int16_t highValue = (int16_t)((value >> 16) & 0xFFFF);
                int16_t lowValue = (int16_t)(value & 0xFFFF);

                highValueTemp = highValue;
                lowValueTemp = lowValue;

                if (xindex == 0) {
                    QString infoMsg = QString("首个数据点高16：0x%1 当前数据点低16：0x%2")
                                           .arg(highValue, 4, 16, QLatin1Char('0')).arg(lowValue, 4, 16, QLatin1Char('0'));
                    emit report(infoMsg);

                    QString regRealData = bytesToHex(buffer, sampleLength);
                    QString regRealDataLength = bytesToHex(buf, 4);
                    QString debugMsg = QString("#### 0x54实时数据 获取内容：%1 \n#### 0x50返回长度：%2 获取内容：%3")
                                           .arg(regRealData)
                                           .arg(readRightSize).arg(regRealDataLength);

                    emit report(debugMsg);
                }
            }
            
            xindex++;


            emit realdataLoaded(m_index, cnt, readSizeFromboard, valueTemp);
            
            // 仅在日志级别为详细时记录
            if (cnt % 100 == 0) {
                QLOG_INFO() << " realdataLoaded " << cnt << " " << valueTemp;
            }

            cnt++;
        }
    }
    else {
        QString errorMsg = QString("实时文件打开错误");
        emit report(errorMsg);
        return;
    }

    tempFile->close();

    // 使用新的方法生成多个INF文件并合并
    QString baseFilename = QFileInfo(infFilePath).baseName();
    QString basePath = QFileInfo(infFilePath).path();
    generateMultipleInfFiles(m_index, basePath, baseFilename, totalCnt * perSize, true);

    //结束信号
    realFinished(m_index);
}

quint32 PcieProtocol::getChannelAddress(int channelIndex)
{
    switch (channelIndex) {
    case 1:
        return CHANNEL1_DDRADDRESS;
    case 2:
        return CHANNEL2_DDRADDRESS;
    case 3:
        return CHANNEL3_DDRADDRESS;
    case 4:
        return CHANNEL4_DDRADDRESS;
    default:
        // 处理无效的通道序号
        qDebug()<< "pcieInit getChannelAddress invalid " << channelIndex;
        return 0;
    }
}

/**
 * @brief 离线采样
 */
void PcieProtocol::collectOfflineData(int triggerMode, int channelIndex, float collectTime, int samplingRate, QString datFilePath, QString infFilePath, bool skipCMD)
{
    qDebug() << "PcieProtocol 线程地址: " << QThread::currentThread();

    // 这里采用256/2=128 是硬件比12.5MHZ采样率多采集了一点（125*1024/1000=128） 特意设置的 生成inf的时候也会同时按照这个来偏移 如果客户有特殊要求记得全局恢复
    int size = 256*5*(collectTime /10); //125 * collectTime ;// 256*5*(collectTime /10); // (samplingRate /1000 /1000 ) * collectTime *2;
    int loopNumber = qCeil(static_cast<double>(size) / MAX_BYTES_PER_TRANSFER);
    int bytesLength = loopNumber>1 ? MAX_BYTES_PER_TRANSFER:(size/loopNumber);
    int triggeradd = 0; // DDR指针地址 此处先设置0 如动态获取则加寄存器获取

//    cmdSetReadMode(1);
//    delayMs(50);

    if(!skipCMD)
    {
        //软件触发
        if(triggerMode == 0)
        {
            cmdStartBysoft(collectTime);
        }
        else if(triggerMode == 1){
            cmdStartByHardWare(collectTime);
        }

        qDebug()<< "等待采集 triggerMode " << triggerMode;
        delayMs(collectTime/1000);
    }


    QFile* tempFile = new QFile(datFilePath);

    quint32 CHANNEL_ADDRESS = getChannelAddress(channelIndex);

    if (tempFile->open(QIODevice::WriteOnly))
    {
        qint64 totalWritten = 0;
        for( int i = 0; i < loopNumber; i++){

            unsigned char *buffer;
            if (i == loopNumber - 1) {
                buffer = (unsigned char*)malloc(size - bytesLength * i);
            } else {
                buffer = (unsigned char*)malloc(bytesLength);
            }

            // 读取数据
            int currentLength;
            if (i == loopNumber - 1) {
                currentLength = size - bytesLength * i;
                pcieDevice.c2hTransfer(CHANNEL_ADDRESS + triggeradd + i * bytesLength, currentLength, buffer);
            } else {
                currentLength = bytesLength;
                pcieDevice.c2hTransfer(CHANNEL_ADDRESS + triggeradd + i * bytesLength, bytesLength, buffer);
            }

            // 每128位(16字节)为一组进行交换
            const int GROUP_SIZE = 16;  // 128位 = 16字节
            for(int pos = 0; pos < currentLength; pos += GROUP_SIZE) {
                // 确保剩余数据至少有一个完整的128位组
                if(pos + GROUP_SIZE <= currentLength) {
                    // 交换头尾16位(2字节)
                    for(int j = 0; j < GROUP_SIZE/4; j++) {
                        // 交换第j个16位和倒数第j+1个16位
                        unsigned char temp[2];
                        // 保存前面的16位
                        temp[0] = buffer[pos + j*2];
                        temp[1] = buffer[pos + j*2 + 1];

                        // 将后面的16位移到前面
                        buffer[pos + j*2] = buffer[pos + GROUP_SIZE - (j*2 + 2)];
                        buffer[pos + j*2 + 1] = buffer[pos + GROUP_SIZE - (j*2 + 1)];

                        // 将保存的前面的16位放到后面
                        buffer[pos + GROUP_SIZE - (j*2 + 2)] = temp[0];
                        buffer[pos + GROUP_SIZE - (j*2 + 1)] = temp[1];
                    }
                }
            }

            qint64 wrote = 0;
            if (i == loopNumber - 1) {
                wrote = tempFile->write(reinterpret_cast<const char *>(buffer), currentLength);
            } else {
                wrote = tempFile->write(reinterpret_cast<const char *>(buffer), bytesLength);
            }
            totalWritten += wrote;

            QLOG_INFO()<<QString("卡%1通道%2  触发起始位置:").arg(m_index).arg(channelIndex)  <<(CHANNEL_ADDRESS + triggeradd + i * bytesLength) << (size - bytesLength * i);
            QLOG_INFO()<<QString("卡%1通道%2  触发结束位置:").arg(m_index).arg(channelIndex)  <<(CHANNEL_ADDRESS + triggeradd + i * bytesLength + bytesLength);

            free(buffer);
        }

        // 确保所有数据刷新到磁盘
        tempFile->flush();

        // --------------- 新增：截断文件，只保留前 1000/1024 的数据 ---------------
        // size 是预期的总字节数，按要求保留前 1000/1024 部分
        qint64 desiredBytes = static_cast<qint64>( ( (qint64)size * 990 ) / 1024 ); // 向下取整
        qint64 actualSize = tempFile->size();
        QLOG_INFO() << QString("准备截断文件: 目标字节 = %1, 当前文件字节 = %2").arg(desiredBytes).arg(actualSize);

        if (desiredBytes <= 0) {
            QLOG_WARN() << "Desired bytes calculated <= 0, skip truncation.";
        } else if (actualSize >= desiredBytes) {
            bool ok = tempFile->resize(desiredBytes);
            if (!ok) {
                QLOG_ERROR() << "Failed to resize file to" << desiredBytes;
            } else {
                QLOG_INFO() << "File truncated to" << desiredBytes << "bytes successfully.";
            }
        } else {
            // 如果实际写入的比期望的还少，打印警告（不做扩展）
            QLOG_WARN() << "Actual file size less than desired truncation size, skipping resize.";
        }
        // --------------------------------------------------------------------

    }
    tempFile->close();

    // 结束后复位，调试用
//    writeRegister(0x60, 3);
//    delayMs(5);

    // 使用新的方法生成多个INF文件并合并
    QString baseFilename = QFileInfo(infFilePath).baseName();
    QString basePath = QFileInfo(infFilePath).path();
    generateMultipleInfFiles(m_index, basePath, baseFilename, bytesLength, false);

    emit offlineProgress(m_index, channelIndex);
}


void PcieProtocol::generateInfFile(const QString& infFilePath, const InfPara& infpara) {
    FILE* file = fopen(infFilePath.toLocal8Bit().constData(), "wb");
    if (!file) {
        QLOG_ERROR() << "无法打开文件写入：" << infFilePath;
        return;
    }

    // 创建一个122字节的缓冲区，确保完全初始化为0
    char buffer[122];
    memset(buffer, 0, sizeof(buffer));
    
    int offset = 0;
    
    // 1. filetype[10]
    memcpy(buffer + offset, "swip_das", strlen("swip_das"));
    offset += 10;
    
    // 2. channelId (short int - 2字节)
    short int channelId = (short int)infpara.channelId;
    // 考虑字节序问题，手动写入
    buffer[offset] = channelId & 0xFF;
    buffer[offset+1] = (channelId >> 8) & 0xFF;
    offset += 2;
    
    // 3. channelName[12]
    memcpy(buffer + offset, infpara.channelName, strlen(infpara.channelName) < 12 ? strlen(infpara.channelName) : 12);
    offset += 12;
    
    // 4. addr (int - 4字节)
    int addr = infpara.addr;
    // 手动写入，确保字节序正确
    buffer[offset] = addr & 0xFF;
    buffer[offset+1] = (addr >> 8) & 0xFF;
    buffer[offset+2] = (addr >> 16) & 0xFF;
    buffer[offset+3] = (addr >> 24) & 0xFF;
    offset += 4;
    
    // 5. freq (float - 4字节) - 这是一个问题字段
    float freq = infpara.freq;
    // 使用联合体确保字节序正确
    union {
        float f;
        unsigned char bytes[4];
    } freq_union;
    freq_union.f = freq;
    
    // 手动写入，确保字节序正确
    buffer[offset] = freq_union.bytes[0];
    buffer[offset+1] = freq_union.bytes[1];
    buffer[offset+2] = freq_union.bytes[2];
    buffer[offset+3] = freq_union.bytes[3];
    offset += 4;
    
    // 6. len (int - 4字节) - 这是一个问题字段
    int len = infpara.len;
    // 手动写入，确保字节序正确
    buffer[offset] = len & 0xFF;
    buffer[offset+1] = (len >> 8) & 0xFF;
    buffer[offset+2] = (len >> 16) & 0xFF;
    buffer[offset+3] = (len >> 24) & 0xFF;
    offset += 4;
    
    // 7. post (int - 4字节) - 这是一个问题字段
    int post = infpara.post;
    // 手动写入，确保字节序正确
    buffer[offset] = post & 0xFF;
    buffer[offset+1] = (post >> 8) & 0xFF;
    buffer[offset+2] = (post >> 16) & 0xFF;
    buffer[offset+3] = (post >> 24) & 0xFF;
    offset += 4;
    
    // 8. maxDat (unsigned short int - 2字节)
    unsigned short int maxDat = infpara.maxDat;
    // 手动写入，确保字节序正确
    buffer[offset] = maxDat & 0xFF;
    buffer[offset+1] = (maxDat >> 8) & 0xFF;
    offset += 2;
    
    // 9-12. 浮点数值 (每个4字节)
    // 使用联合体确保字节序正确
    union {
        float f;
        unsigned char bytes[4];
    } float_union;
    
    // 9. lowRang
    float_union.f = 0.0f;
    buffer[offset] = float_union.bytes[0];
    buffer[offset+1] = float_union.bytes[1];
    buffer[offset+2] = float_union.bytes[2];
    buffer[offset+3] = float_union.bytes[3];
    offset += 4;
    
    // 10. highRang
    float_union.f = 0.0f;
    buffer[offset] = float_union.bytes[0];
    buffer[offset+1] = float_union.bytes[1];
    buffer[offset+2] = float_union.bytes[2];
    buffer[offset+3] = float_union.bytes[3];
    offset += 4;
    
    // 11. factor
    float_union.f = 1.0f;
    buffer[offset] = float_union.bytes[0];
    buffer[offset+1] = float_union.bytes[1];
    buffer[offset+2] = float_union.bytes[2];
    buffer[offset+3] = float_union.bytes[3];
    offset += 4;
    
    // 12. offset
    float_union.f = 0.0f;
    buffer[offset] = float_union.bytes[0];
    buffer[offset+1] = float_union.bytes[1];
    buffer[offset+2] = float_union.bytes[2];
    buffer[offset+3] = float_union.bytes[3];
    offset += 4;
    
    // 13. unit[8]
    memcpy(buffer + offset, "DEG", strlen("DEG"));
    offset += 8;
    
    // 14. dly (float - 4字节)
    float_union.f = 0.0f;
    buffer[offset] = float_union.bytes[0];
    buffer[offset+1] = float_union.bytes[1];
    buffer[offset+2] = float_union.bytes[2];
    buffer[offset+3] = float_union.bytes[3];
    offset += 4;
    
    // 15. attribDt (short int - 2字节)
    short int attribDt = infpara.attribDt; // 整形数据
    buffer[offset] = attribDt & 0xFF;
    buffer[offset+1] = (attribDt >> 8) & 0xFF;
    offset += 2;
    
    // 16. datWth (short int - 2字节)
    short int datWth = infpara.datWth;
    buffer[offset] = datWth & 0xFF;
    buffer[offset+1] = (datWth >> 8) & 0xFF;
    offset += 2;
    
    // 17-19. sparI1-3 (每个short int - 2字节)
    // 17. sparI1
    short int sparI1 = infpara.sparI1;
    buffer[offset] = sparI1 & 0xFF;
    buffer[offset+1] = (sparI1 >> 8) & 0xFF;
    offset += 2;
    
    // 18. sparI2
    short int sparI2 = infpara.sparI2;
    buffer[offset] = sparI2 & 0xFF;
    buffer[offset+1] = (sparI2 >> 8) & 0xFF;
    offset += 2;
    
    // 19. sparI3
    short int sparI3 = infpara.sparI3;
    buffer[offset] = sparI3 & 0xFF;
    buffer[offset+1] = (sparI3 >> 8) & 0xFF;
    offset += 2;
    
    // 20-21. sparF1-2 (每个float - 4字节)
    // 20. sparF1
    float_union.f = infpara.sparF1;
    buffer[offset] = float_union.bytes[0];
    buffer[offset+1] = float_union.bytes[1];
    buffer[offset+2] = float_union.bytes[2];
    buffer[offset+3] = float_union.bytes[3];
    offset += 4;
    
    // 21. sparF2
    float_union.f = infpara.sparF2;
    buffer[offset] = float_union.bytes[0];
    buffer[offset+1] = float_union.bytes[1];
    buffer[offset+2] = float_union.bytes[2];
    buffer[offset+3] = float_union.bytes[3];
    offset += 4;
    
    // 22-24. 字符串字段，保持为0
    // 22. sparC1[8]
    offset += 8;
    
    // 23. sparC2[16]
    offset += 16;
    
    // 24. sparC3[10]
    offset += 10;
    
    // 确认总大小是否为122字节
    if (offset != 122) {
        QLOG_ERROR() << "结构体大小不匹配：" << offset << " 字节，预期 122 字节";
    }
    
    // 写入整个缓冲区
    size_t written = fwrite(buffer, 1, 122, file);
    if (written != 122) {
        QLOG_ERROR() << "写入INF文件失败：" << infFilePath;
    }
    
    fclose(file);
    
    QLOG_INFO() << "INF文件生成完成：" << infFilePath;
    QLOG_INFO() << "文件大小：" << written << " 字节";
}

void PcieProtocol::collectOfflineAlldata(int cardIndex, int channels, int triggerMode, int samplingRate, float collectTime, QString datPathPre, QString infPathPre, QString gunNumber)
{
    if(cardIndex == m_index)
    {
        // 循环遍历通道
        for (int channel = 0; channel < channels; ++channel)
        {
            // 构造文件路径
            QString fileDDrDatPath = QString("%1/%2DDR3_DATA_CARD%3_CH%4.DAT").arg(datPathPre).arg(gunNumber).arg(cardIndex+1).arg(channel + 1);
            // 不再生成单独的INF文件，传入空字符串
            collectOfflineData(triggerMode, channel + 1, collectTime, samplingRate, fileDDrDatPath, "", true);
        }
        
        // 为所有通道生成一个合并的INF文件
        QString baseFilename = QString("%1DDR3_DATA_CARD%2").arg(gunNumber).arg(cardIndex+1);
        
        // 读取配置文件获取信号模式
        QSettings settings(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
        settings.beginGroup("RegisterParams");
        int signalMode = settings.value("singal_mode").toInt();
        settings.endGroup();
        
        // 根据信号模式计算数据大小
        int dataSize;
        if (signalMode == 0) { // 单频模式
            dataSize = 256*3*(collectTime/10);//传过来的collectTime乘以了1000，在这里除了10，也就是协议里的ms*100，256*3为1ms数据大小，再乘以后面的就是总大小
        } else { // 多频模式
            dataSize = 256*5*(collectTime/10);
        }
        
        generateMultipleInfFiles(m_index, infPathPre, baseFilename, dataSize, false);

        emit offlineFinished(m_index);
    }
}




int PcieProtocol::writeRegister(UINT32 address, int value)
{
    unsigned char buf[4];
    buf[0] = value;
    buf[1] = value>>8;
    buf[2] = value>>16;
    buf[3] = value>>24;

    return pcieDevice.writeUser(address, 4, buf);
}

QString PcieProtocol::readRegister(UINT32 address)
{
    int data0 = -1;
    int data1 = -1;
    int data2 = -1;
    int data3 = -1;
    int byteLength = 4;
    byte buf[4];

    pcieDevice.readUser(address, byteLength ,buf);
    //data =buf[3]|buf[2]|buf[1]|buf[0];
    data0 = buf[0];
    QString Data0 =QString("%1").arg(data0,2,16,QChar('0').toUpper());
    data1 = buf[1];
    QString Data1 =QString("%1").arg(data1,2,16,QChar('0').toUpper());
    data2 = buf[2];
    QString Data2 =QString("%1").arg(data2,2,16,QChar('0').toUpper());
    data3 = buf[3];
    QString Data3 =QString("%1").arg(data3,2,16,QChar('0').toUpper());
    QString result =Data3+Data2+Data1+Data0;
    return result;
}

int PcieProtocol::readRegisterOrBram(unsigned int address, unsigned int size, unsigned char *buffer)
{
    return pcieDevice.readUser(address, size, buffer);
}

int PcieProtocol::c2hTrans(unsigned int address, unsigned int size, unsigned char *buffer)
{
    return pcieDevice.c2hTransfer(address, size, buffer);
}

QString PcieProtocol::bytesToHex(const unsigned char *buf, int size)
{
    // QByteArray byteArray(reinterpret_cast<const char*>(buf), size);
    // QByteArray hexString = byteArray.toHex();

    // QString result = QString::fromLatin1(hexString);

    QString result;
    for (int i = 0; i < size; ++i) {
        result.append(QString("%1").arg(buf[i], 2, 16, QChar('0')).toUpper());
    }
    return result;
}


// 封装的小端到大端字节序转换函数
int32_t PcieProtocol::swapEndian(int32_t value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x000000FF) << 24);
}

void PcieProtocol::checkFifoStatus(int currentSize, int maxSize)
{
    if (currentSize <= 0 || maxSize <= 0) {
        return;
    }
    
    double fillRatio = static_cast<double>(currentSize) / maxSize;
    
    // 当FIFO填充率超过阈值时，发出警告
    if (isNearOverflow(fillRatio)) {
        m_fifoWarningCount++;
        
        // 只在计数为1或5的倍数时发送警告，避免过多日志
        if (m_fifoWarningCount == 1 || m_fifoWarningCount % 5 == 0) {
            emit fifoOverflowWarning(m_index, fillRatio);
            
            QString warningMsg = QString("警告：卡%1的FIFO接近溢出! 填充率：%2% (当前:%3/%4)")
                            .arg(m_index+1)
                            .arg(fillRatio * 100, 0, 'f', 2)
                            .arg(currentSize)
                            .arg(maxSize);
            emit report(warningMsg);
        }
    }
    else {
        // 重置警告计数
        if (m_fifoWarningCount > 0) {
            m_fifoWarningCount = 0;
        }
    }
}

/**
 * @brief 根据配置文件中的通道参数生成多个inf文件
 * @param cardIndex 卡片索引
 * @param basePath inf文件基本路径
 * @param baseFilename 基本文件名
 * @param dataSize 数据大小
 * @param isRealTime 是否是实时数据
 * @return 合并后的inf文件路径
 */
QString PcieProtocol::generateMultipleInfFiles(int cardIndex, const QString& basePath, const QString& baseFilename, int dataSize, bool isRealTime)
{
    // 读取配置文件
    QSettings settings(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
    
    // 准备存储所有生成的INF文件路径
    QStringList infFilePaths;
    
    // 根据卡片索引构造前缀
    QString cardPrefix = QString("Card%1_").arg(cardIndex + 1);
    
    // 获取singal_mode值决定单频还是三频模式
    int signalMode = 0; // 默认单频模式
    settings.beginGroup("RegisterParams");
    signalMode = settings.value("singal_mode").toInt();
    settings.endGroup();
    
    // 获取通道参数组
    settings.beginGroup("CHANNEL_PARA");
    
    // 确定要处理的通道类型
    QStringList channelTypes;
    if (isRealTime) {
        // 实时数据根据信号模式选择通道
        if (signalMode == 0) { // 单频模式
            // 只使用第一个RT_a1通道
            channelTypes << "RT_a1";
            QLOG_INFO() << "单频模式：使用RT_a1通道";
        } else { // 三频模式
            // 使用RT_a1, RT_a2, RT_density1三个通道
            channelTypes << "RT_a1" << "RT_a2" << "RT_density1";
            QLOG_INFO() << "三频模式：使用RT_a1, RT_a2, RT_density1通道";
        }
    } else {
        // 离线数据通道，根据信号模式选择通道
        if (signalMode == 0) { // 单频模式
            // 只使用前3个通道
            channelTypes << "a1" << "a2" << "density1";
            QLOG_INFO() << "单频模式：使用a1, a2, density1通道";
        } else { // 多频模式
            // 使用全部5个通道
            channelTypes << "a1" << "a2" << "density1" << "density2" << "density3";
            QLOG_INFO() << "多频模式：使用全部5个通道";
        }
    }
    
    // 为每个通道类型生成INF文件
    for (int i = 0; i < channelTypes.size(); i++) {
        const QString& channelType = channelTypes[i];
        
        // 构造配置键名
        QString iniKey = cardPrefix + channelType + "_ini";
        QString idKey = cardPrefix + channelType + "_id";
        
        // 获取通道名称和ID
        QString channelName = settings.value(iniKey).toString();
        int channelId = settings.value(idKey).toInt();
        
        if (!channelName.isEmpty()) {
            // 构造INF文件路径
            QString infFilePath = QString("%1/%2_%3.INF").arg(basePath).arg(baseFilename).arg(channelType);
            
            // 创建INF参数
            InfPara infpara;
            strncpy(infpara.filetype, "swip_das", sizeof(infpara.filetype) - 1);
            infpara.filetype[sizeof(infpara.filetype) - 1] = '\0';
            infpara.channelId  = channelId;
            
            // 设置通道名称
            strncpy(infpara.channelName, channelName.toLocal8Bit().constData(), sizeof(infpara.channelName) - 1);
            infpara.channelName[sizeof(infpara.channelName) - 1] = '\0';
            
            // 设置其他参数
            int baseAddr = dataSize * cardIndex;


            // 数据中心对应格式： 
            /**
             * attribDt：1为无符号整数，2为有符号整数，3为浮点数
             * DDR应该是2
             * 实时应该是3   无论单频还是三频 数据dat文件进行转换 均在数据通道重组的时候进行float转换 均占4个字节
             * datWth：U16 需要设置成 2； float 型数据，需要设置成 4
             * DDR应该是2 实时应该是4
            */
            if (isRealTime) {
                if (signalMode == 0 || channelTypes.size() == 1) {
                    // 单频实时配置
                    // 单频模式或只有一个通道时，使用整个数据范围
                    int N = dataSize / 4; //采样点数（datasize是totalcnt*persize，persize是4）
                    infpara.addr = 0 + baseAddr;
                    infpara.len = N;
                    infpara.post = N;
                    // 单通道是float
                    infpara.attribDt = 3;
                    infpara.datWth = 4;
                    infpara.freq = 12500000/6250;   //实时单频挑点后采样率
                } else {                    
                    // 三频实时数据，分割为三个通道 对三频实时数据进行处理改造成三通道均为4字节浮点数数据 
                    int N = dataSize / 12; //采样点数（每个点数含三个相位）
                    infpara.addr = i * N * 4 + baseAddr;
                    infpara.len = N;
                    infpara.post = N;
                    // 单通道是float
                    infpara.attribDt = 3;
                    infpara.datWth = 4;
                    infpara.freq = 12500000/6250/2;   //实时三频挑点后采样率
                }
            } else {
                // 离线数据，按通道分割
                if (signalMode == 0) { // 单频模式
                    // 每个通道占用总数据大小的1/3
                    int channelDataSize = dataSize / 3;
                    infpara.addr = i * channelDataSize + baseAddr;
                    // 设置通道数据长度为总长度的1/3
                    infpara.len = channelDataSize/2;
                    infpara.post = channelDataSize/2;
                    infpara.attribDt = 2;
                    infpara.datWth = 2;
                    infpara.freq = 12500000; // DDR底层12.5MHZ
                } else { // 多频模式
                    // 每个通道占用总数据大小的1/5
                    int channelDataSize = dataSize / 5;
                    infpara.addr = i * channelDataSize + baseAddr;
                    // 设置通道数据长度为总长度的1/5
                    infpara.len = channelDataSize/2;
                    infpara.post = channelDataSize/2;
                    infpara.attribDt = 2;
                    infpara.datWth = 2;
                    infpara.freq = 12500000; // DDR底层12.5MHZ
                }
            }
            
            infpara.maxDat = 0;
            infpara.lowRang = 0;
            infpara.highRang = 0;
            infpara.factor = 1;
            infpara.Offset = 0;
            strncpy(infpara.unit, "DEG", sizeof(infpara.unit) - 1);
            infpara.unit[sizeof(infpara.unit) - 1] = '\0';
            infpara.Dly = 0;

            infpara.sparI1 = 0;
            infpara.sparI2 = 0;
            infpara.sparI3 = 0;
            infpara.sparF1 = 0;
            infpara.sparF2 = 0;

            // 生成INF文件
            generateInfFile(infFilePath, infpara);
            
            // 添加到文件路径列表
            infFilePaths.append(infFilePath);
            
            QLOG_INFO() << "为通道 " << channelName << " (ID:" << channelId << ") 生成INF文件: " << infFilePath;
            QLOG_INFO() << "通道数据指针位置: " << infpara.addr << " 长度: " << infpara.len;
        }
    }
    
    settings.endGroup();
    
    // 合并所有INF文件到一个文件
    QString mergedInfPath = QString("%1/%2.INF").arg(basePath).arg(baseFilename);
    
    if (isRealTime && infFilePaths.size() == 1) {
        // 实时模式下，如果只有一个文件（单频模式），直接重命名
        // 直接复制第一个文件到合并文件
        QFile::copy(infFilePaths.first(), mergedInfPath);
        QLOG_INFO() << "实时模式：直接使用单个通道文件: " << infFilePaths.first();
        
        // 删除原始文件
        QFile::remove(infFilePaths.first());
        QLOG_INFO() << "已删除原始INF文件: " << infFilePaths.first();
        
        return mergedInfPath;
    }
    
    // 离线模式或实时多文件情况（三频模式）：需要合并
    QFile mergedFile(mergedInfPath);
    
    if (mergedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // 合并所有INF文件内容
        for (const QString &filePath : infFilePaths) {
            QFile sourceFile(filePath);
            if (sourceFile.open(QIODevice::ReadOnly)) {
                QByteArray infData = sourceFile.readAll();
                mergedFile.write(infData);
                sourceFile.close();
                QLOG_INFO() << "添加INF文件到合并文件: " << filePath;
            } else {
                QLOG_INFO() << "无法打开INF文件: " << filePath;
            }
        }
        
        mergedFile.close();
        QLOG_INFO() << "INF文件合并完成: " << mergedInfPath;
        
        // 删除原始的单个INF文件
        for (const QString &filePath : infFilePaths) {
            if (QFile::remove(filePath)) {
                QLOG_INFO() << "已删除原始INF文件: " << filePath;
            } else {
                QLOG_INFO() << "无法删除原始INF文件: " << filePath;
            }
        }
    } else {
        QLOG_INFO() << "无法创建合并INF文件: " << mergedInfPath;
    }
    
    return mergedInfPath;
}

