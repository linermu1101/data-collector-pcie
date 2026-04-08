#include "pciefunction.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include "QsLog.h"

//初始化函数
static int verbose_msg(const char* const fmt, ...) {

    int ret = 0;
    va_list args;
    if (1) {
        va_start(args, fmt);
        ret = vprintf(fmt, args);
        va_end(args);
    }
    return ret;
}

//PCIE初始化
PcieFunction::PcieFunction(int popertyValue,QObject *parent) : QObject(parent)
{
    this->setProperty("设备",popertyValue);
    saveFileInf = new IniFileInf;
}

//PCIE缓存释放
int PcieFunction::allocMemory()
{

    h2c_align_mem_tmp = allocate_buffer(0x800000,4096*2);
    c2h_align_mem_tmp = allocate_buffer(0x800000,4096*2);

    if(NULL == h2c_align_mem_tmp || NULL == c2h_align_mem_tmp) return -1;

    return 0;
}

//打开PCIE设备
void PcieFunction::openPcieDev(QString devPath,int index)
{
    int val = this->property("设备").toInt();
    if(val != index)  return;
    wchar_t  user_path_w[MAX_PATH+1];
    wchar_t  c2h0_path_w[MAX_PATH+1];
    wchar_t  h2c0_path_w[MAX_PATH+1];
    QString user_path = devPath + "\\user";
    QString c2h0_path = devPath + "\\c2h_0";
    QString h2c0_path = devPath + "\\h2c_0";

    mbstowcs(user_path_w, user_path.toUtf8().data(),  user_path.size()+1);
    mbstowcs(h2c0_path_w, h2c0_path.toUtf8().data(),  h2c0_path.size()+1);
    mbstowcs(c2h0_path_w, c2h0_path.toUtf8().data(),  c2h0_path.size()+1);

    h_user = CreateFile(user_path_w,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h_user == INVALID_HANDLE_VALUE) return;

    h_c2h0 = CreateFile(c2h0_path_w,GENERIC_READ ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h_c2h0 == INVALID_HANDLE_VALUE) return;

    h_h2c0 = CreateFile(h2c0_path_w,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h_h2c0 == INVALID_HANDLE_VALUE) return;
    openFlag = true;
}

//关闭PCIE设备
void PcieFunction::closePcieDev()
{
    if(!openFlag) return;
    CloseHandle(h_user);
    CloseHandle(h_c2h0);
    CloseHandle(h_h2c0);
    openFlag = false;
}


int PcieFunction::cmdReset()
{
    return writeRegister(RESET_REGISTER, 1);
}

void PcieFunction::cmdStartBysoft(int samplingTime)
{
    writeRegister(START_SOFTWARE_REGISTER, samplingTime);
}


void PcieFunction::cmdStartByHardWare(int samplingTime)
{
    writeRegister(START_HARDWARE_REGISTER, samplingTime);
}

void PcieFunction::cmdSetReadMode(int mode)
{
    writeRegister(READ_MODE_REGISTER, mode);
}

void PcieFunction::cmdSetSampleInterval(int sampleInterval)
{
    writeRegister(PHASE_PICKUP_REGISTER, sampleInterval);
}

void PcieFunction::cmdSetFlip(int value)
{
    writeRegister(PHASE_FLIP_REGISTER, value);
}

void PcieFunction::cmdSetFrequencyPostion(int value)
{
    writeRegister(BASE_FREQUENCY_POSITION_REGISTER, value);
}

void PcieFunction::cmdSetFilterWindowWidth(int value)
{
    writeRegister(FILTER_WINDOW_WIDTH_REGISTER, value);
}

int PcieFunction::writeRegister(UINT32 address, int value)
{
    unsigned char buf[4];
    buf[0] = value;
    buf[1] = value>>8;
    buf[2] = value>>16;
    buf[3] = value>>24;
    return write_device(h_user,address,4,buf);
}

QString PcieFunction::bytesToHex(const unsigned char *buf, int size)
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


void PcieFunction::delayMs(int mes)
{
    QTime dieTime = QTime::currentTime().addMSecs(mes);
    while(dieTime>QTime::currentTime()){
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
    }
}

quint32 PcieFunction::getChannelAddress(int channelIndex)
{
    switch (channelIndex) {
    case 1:
        return V2_SINGEL_CHANNEL1_DDRADDRESS;
    case 2:
        return V2_SINGEL_CHANNEL2_DDRADDRESS;
    default:
        // 处理无效的通道序号
        qDebug()<< "pcieInit getChannelAddress invalid " << channelIndex;
        return 0;
    }
}


void PcieFunction::collectOfflineData(int triggerMode, int channelIndex, int collectTime, int samplingRate, QString datFilePath)
{
    int size = (samplingRate /1000 /1000 ) * collectTime *2;
    int loopNumber = (size/MAX_BYTES_PER_TRANSFER)+1;
    int bytesLength = size/loopNumber;
    int triggeradd = 0; // DDR指针地址 此处先设置0 如动态获取则加寄存器获取

    //cmdSetReadMode(1);
    delayMs(50);
    //软件触发
    if(triggerMode == 0)
    {
        //cmdStartBysoft(collectTime);
    }
    else if(triggerMode == 1){
        //cmdStartByHardWare(collectTime);
    }

    qDebug()<< "等待采集 triggerMode " << triggerMode;
    delayMs(collectTime/1000);

    //QFile* tempFile = new QFile(datFilePath);

    quint32 CHANNEL_ADDRESS = getChannelAddress(channelIndex);

    //if (tempFile->open(QIODevice::WriteOnly))
    if(true)
    {
        for( int i = 0; i < loopNumber; i++){

            unsigned char *buffer;
            if (i == loopNumber - 1) {
                buffer = (unsigned char*)malloc(size - bytesLength * i);
            } else {
                buffer = (unsigned char*)malloc(bytesLength);
            }

            if (i == loopNumber - 1) {
                //pcieDevice.c2hTransfer(CHANNEL_ADDRESS + triggeradd + i * bytesLength, size - bytesLength * i, buffer);
                int size = read_device(h_c2h0, CHANNEL_ADDRESS + triggeradd + i * bytesLength, size - bytesLength * i, buffer);
            } else {
                //pcieDevice.c2hTransfer(CHANNEL_ADDRESS + triggeradd + i * bytesLength, bytesLength, buffer);
                int size = read_device(h_c2h0, CHANNEL_ADDRESS + triggeradd + i * bytesLength, bytesLength, buffer);
            }

            if (i == loopNumber - 1) {
                //tempFile->write(reinterpret_cast<const char *>(buffer), size - bytesLength * i);
            } else {
                //tempFile->write(reinterpret_cast<const char *>(buffer), bytesLength);
            }

            qDebug()<<QString("卡通道%1  触发起始位置:").arg(channelIndex)  <<(CHANNEL_ADDRESS + triggeradd + i * bytesLength);
            qDebug()<<QString("卡通道%1  触发结束位置:").arg(channelIndex)  <<(CHANNEL_ADDRESS + triggeradd + i * bytesLength + bytesLength);

            free(buffer);
        }
    }
    //tempFile->close();

    //emit progress(channelIndex);
}


//数据传输函数
void PcieFunction::c2h_transfer(PlanarGraph *plot,SettingPara cardPara)
{
    int val = this->property("设备").toInt();
    if(val != cardPara.cardIndex)  return;


    // 以下变量改用新版含义
    // int cnt=0;
    // int total_cnt = 0;
    // int RT_Length;
    double timeLine = 0;
    if(cardPara.address == RealTime_Phase_Addr){
        // switch(cardPara.pickUpMode)
        // {
        // case 0:
        //     RT_Length = 10000*2;
        //     break;
        // case 1:
        //     RT_Length = 1000*2;
        //     break;
        // case 2:
        //     RT_Length = 500*2;
        //     break;
        // case 3:
        //     RT_Length = 200*2;
        //     break;
        // case 4:
        //     RT_Length = 100*2;
        //     break;
        // }
        ddrFlag = false;
    }else{
        //RT_Length = cardPara.singleLength/100;
        ddrFlag = true;
    }
    // if(!ddrFlag && (cardPara.mode == 1))
    // {
    //     cardPara.totalLength *= 5;
    //     RT_Length *= 5;
    //     specturm = cardPara.c_time/(cardPara.totalLength/(5*2));
    // }else{
    //     specturm = cardPara.c_time / (cardPara.totalLength/2);
    // }

    // //这里总长度等于单次的长度x时间
    // //而时间片等于总时间/总长度/2
    // //总循环数 = 总长度/单词的长度
    // total_cnt = cardPara.totalLength/RT_Length;

    QVector<QCPGraphData> vecdata1,vecdata2,vecdata3,vecdata4,vecdata5,pNullVec;
    QCPGraphData gp_data1,gp_data2,gp_data3,gp_data4,gp_data5;
    QSharedPointer<QCPGraphDataContainer> ch1_graph,ch2_graph,ch3_graph,ch4_graph,ch5_graph;
    ch1_graph = plot->plot->graph(0)->data();
    ch2_graph = plot->plot->graph(1)->data();
    ch3_graph = plot->plot->graph(2)->data();
    ch4_graph = plot->plot->graph(3)->data();
    ch5_graph = plot->plot->graph(4)->data();
    bool first = true;
    bool ddrSave = false;
    if(cardPara.saveCnt)
    {
        ddrSave = saveFileInf->save_List[cardPara.saveCnt-1];
    }
    unsigned char *t_buf;   //重组数据的buf
    if(cardPara.mode)
        t_buf=(unsigned char*)malloc(cardPara.totalLength);
    else
        t_buf =(unsigned char*)malloc(1);
    int addr1,addr2,addr3,addr4,addr5; //数据首地址
    addr1 = 0;
    addr2 = cardPara.totalLength/5;
    addr3 = cardPara.totalLength*2/5;
    addr4 = cardPara.totalLength*3/5;
    addr5 = cardPara.totalLength*4/5;


    /************2024.12.28新增*************/
    int singleSampleTime = 1000;  // 单次采集时间，单位为微秒 (us) 此处1ms
    int sampleInterval = 1000;      // 挑点采样间隔，单位为微秒 (us) 100 50 20 10 1 此处1ms


    // 计算采样点数
    int sampleCount = singleSampleTime / sampleInterval; // 1
    int perSize = 4;
    int sampleLength = sampleCount * perSize;
    int cnt = 0;    // 采样序列
    int totalCnt =  cardPara.c_time * 1000 / singleSampleTime;  // collectTime 要传递过来是us 所以乘以1000
    QElapsedTimer timer;

    //软件触发
    cmdReset();
    delayMs(100);
    cmdSetFlip(0);
    delayMs(50);
    cmdSetFilterWindowWidth(10);
    delayMs(50);
    cmdSetFrequencyPostion(3);
    delayMs(50);

    cmdSetReadMode(0);
    delayMs(50);
    cmdSetSampleInterval(sampleInterval);
    delayMs(50);

    // 这里进行写入时间命令cardPara.c_time
    int collectTime = cardPara.c_time * 1000;

    QLOG_INFO () << " c2h_transfer " << collectTime << " triggerMode " << cardPara.triggerMode;

    if(cardPara.triggerMode == 1)
    {
        writeRegister(V2_SINGEL_HARDWARE_TRIG_Addr, collectTime);

        // 若是实时读取&硬件触发  则需要等待信号进来
        if ( (cardPara.triggerMode == 1) && !ddrFlag )
        {
            QLOG_INFO () << " c2h_transfer hardware wait...";
            emit report("等待硬件触发...");
            for (;;)
            {
                byte hardTriggerbuf[4];
                memset(hardTriggerbuf, 0, 4);
                read_device(h_user, V2_SINGEL_HARDWARE_TAG_Addr, 4, hardTriggerbuf);
                int32_t hardTriggerTag = (static_cast<int32_t>(hardTriggerbuf[3]) << 24) |
                                         (static_cast<int32_t>(hardTriggerbuf[2]) << 16) |
                                         (static_cast<int32_t>(hardTriggerbuf[1]) << 8)  |
                                         (static_cast<int32_t>(hardTriggerbuf[0]));

                if(hardTriggerTag == 1)
                {
                    QLOG_INFO () << " c2h_transfer hardware trigger...";
                    emit report("监测已硬件触发...");
                    break;
                }
                QThread::usleep(1);
            }
        }
    }
    else{
        emit report("马上软件触发...");
        timer.start();
        writeRegister(V2_SINGEL_SOFTWARE_TRIG_Addr, collectTime);
    }



    // 无限循环，直到满足退出条件
    while (true)
    {
        // 判断是否采集完成
        if(cnt >= totalCnt)
        {
            qDebug() << "collectRealTimeData over";
            break;
        }

        // 检查退出条件：计数达到总数或停止标志位为真
        // if ((cnt == totalCnt) || isStop)
        // {
        //     // 关闭数据处理文件
        //     fclose(dataHandle);
        //     // 如果不是DDR模式，执行设备写入操作以重置状态
        //     if (!ddrFlag)
        //     {
        //         // 旧版复位命令
        //         // unsigned char val;
        //         // val = 1;
        //         // write_device(h_user, 0x04, 4, &val);
        //         // val = 0;
        //         // write_device(h_user, 0x04, 4, &val);

        //         // 发出重新绘制图表的信号
        //         emit replotChart(cardPara.cardIndex);
        //     }
        //     // 如果不是停止状态，发出DDR结束的信号
        //     if (!isStop)
        //     {
        //         emit ddrEnd(cardPara.cardIndex);
        //     }
        //     // 退出循环
        //     return;
        // }

        // 定义临时变量和缓冲区用于数据处理
        //int16_t temp1, temp2, temp3, temp4, temp5;
        // unsigned char *buffer;
        // buffer = (unsigned char *)malloc(RT_Length);
        // 根据DDR标志位决定读取设备的方式
        if (ddrFlag)
        {
            // DDR 取数开始...........................................................
            // unsigned char buffer[sampleLength];
            // memset(buffer, 0, sampleLength);


            // // // 读取设备数据，如果读取长度不足则跳过本次循环
            // // int size = read_device(h_c2h0, cardPara.address + cnt * RT_Length, RT_Length, buffer);
            // // if (size < RT_Length)
            // //     continue;


            // // 如果需要保存DDR数据，写入文件
            // if (ddrSave)
            // {
            //     fwrite(buffer, 1, sampleLength, dataHandle);
            // }
        }
        else
        {
            // 实时取数开始...........................................................
            // 这里改成新版 实时取数
            // 读0x38长度 取数0x3c地址
            // 分配缓冲区
            unsigned char buffer[sampleLength];
            memset(buffer, 0, sampleLength);

            byte buf[4];
            memset(buf, 0, 4);
            // read length
            int readRightSize = read_device(h_user, V2_SINGEL_FIFO_LEFT_Addr, 4, buf);
            // 将 buf[4] 转换为 int32_t
            int32_t readSizeFromboard = (static_cast<int32_t>(buf[3]) << 24) |
                                        (static_cast<int32_t>(buf[2]) << 16) |
                                        (static_cast<int32_t>(buf[1]) << 8)  |
                                        (static_cast<int32_t>(buf[0]));

            // 读取数据
            if(readSizeFromboard > 0)
            {
                read_device(h_user, V2_SINGEL_REALDATA_Addr , sampleLength, buffer);
            }
            //QLOG_INFO()<<" readbuffer= "  << buffer << " readSizeFromboard= "<< readSizeFromboard << " sampleCount "<<sampleCount;
            if(readSizeFromboard < sampleCount)
            {
                //QThread::usleep(1);

                int nelapsed = timer.elapsed();
                if (nelapsed >= (collectTime / 1000 +900)) {
                    timer.restart();
                    QString regRealData = bytesToHex(buffer, sampleLength);
                    QString regRealDataLength = bytesToHex(buf, 4);
                    QString errorMsg = QString("采集超时 \n#### 0x3c实时数据 ：%1 \n#### 0x38返回长度：%2 获取内容：%3 总共耗时%4毫秒")
                                           .arg(regRealData)
                                           .arg(readRightSize).arg(regRealDataLength).arg(nelapsed);
                    emit report(errorMsg);
                    QLOG_INFO()<<" errorMsg= "  << errorMsg;


                    break; // break;
                }

                //                 QString tryMsg = QString("采集太快 等待fifo新增 #### 0x38返回：%1")
                //                                       .arg(readSizeFromboard);
                //                 emit report(tryMsg);

                continue;
            }

            int32_t  *dBuffer = (int32_t*)buffer;
            // 下面新版RT_Length 改成 sampleLength


            // 读取设备数据，如果读取长度不足则跳过本次循环
            // int size = read_device(h_c2h0, cardPara.address, RT_Length, buffer);
            // if (size < RT_Length)
            //     continue;
            // // 将缓冲区转换为int16_t类型指针
            // int16_t *dBuffer = (int16_t *)buffer;


            // 根据模式处理数据
//            if (cardPara.mode == 0)
//            {
//                // 写入数据到文件并处理数据到向量中

//                //fwrite(buffer, 1, RT_Length, dataHandle);
//                fwrite(buffer, 1, sampleLength, dataHandle);
//                for (int m = 0; m < sampleLength / 2; m++)
//                {
//                    temp1 = *(dBuffer + m);
//                    gp_data1.key = timeLine;
//                    gp_data1.value = (double)temp1 * cardPara.coefficient;
//                    timeLine += specturm;
//                    vecdata1.append(gp_data1);
//                }
//                // 根据是否是第一次处理数据，设置或添加数据到图表
//                if (first)
//                {
//                    ch1_graph->set(vecdata1, true);
//                    first = false;
//                }
//                else
//                    ch1_graph->add(vecdata1, true);
//                // 清空向量并交换为空向量
//                vecdata1.clear();
//                vecdata1.swap(pNullVec);
//            }
//            else
//            {
//                // 处理多通道数据
//                int k = 0;
//                for (int m = 0; m < sampleLength / 10; m++)
//                {
//                    temp1 = *(dBuffer + k);
//                    temp2 = *(dBuffer + k + 1);
//                    temp3 = *(dBuffer + k + 2);
//                    temp4 = *(dBuffer + k + 3);
//                    temp5 = *(dBuffer + k + 4);
//                    *(t_buf + addr1 + k * 2) = temp1;
//                    *(t_buf + addr2 + k * 2) = temp2;
//                    *(t_buf + addr3 + k * 2) = temp3;
//                    *(t_buf + addr4 + k * 2) = temp4;
//                    *(t_buf + addr5 + k * 2) = temp5;

//                    gp_data1.key = timeLine;
//                    gp_data1.value = temp1 / 8 * cardPara.coefficient;
//                    gp_data2.key = timeLine;
//                    gp_data2.value = temp2 / 8 * cardPara.coefficient;
//                    gp_data3.key = timeLine;
//                    gp_data3.value = temp3 / 8 * cardPara.coefficient;
//                    gp_data4.key = timeLine;
//                    gp_data4.value = temp4 / 8 * cardPara.coefficient;
//                    gp_data5.key = timeLine;
//                    gp_data5.value = temp5 / 8 * cardPara.coefficient;
//                    timeLine += specturm;
//                    vecdata1.append(gp_data1);
//                    vecdata2.append(gp_data2);
//                    vecdata3.append(gp_data3);
//                    vecdata4.append(gp_data4);
//                    vecdata5.append(gp_data5);
//                    k += 5;
//                }
//                // 添加数据到多个图表
//                ch1_graph->add(vecdata1, true);
//                ch2_graph->add(vecdata2, true);
//                ch3_graph->add(vecdata3, true);
//                ch4_graph->add(vecdata4, true);
//                ch5_graph->add(vecdata5, true);

//                // 清空向量并交换为空向量
//                vecdata1.clear();
//                vecdata2.clear();
//                vecdata3.clear();
//                vecdata4.clear();
//                vecdata5.clear();
//                vecdata1.swap(pNullVec);
//                vecdata2.swap(pNullVec);
//                vecdata3.swap(pNullVec);
//                vecdata4.swap(pNullVec);
//                vecdata5.swap(pNullVec);
//            }

            //emit realdataLoaded(cnt, readSizeFromboard);
            // qDebug () << " c2h_transfer realdata cnt：" << cnt << " totalCnt：" << totalCnt << " readSizeFromboard " << readSizeFromboard;
        }
        // 计数增加，检查是否达到总数
        cnt++;
        //free(buffer);
        // if (cnt == totalCnt)
        // {
        //     // 如果是模式1且不是DDR模式，写入所有数据到文件
        //     if (cardPara.mode && !ddrFlag)
        //     {
        //         fwrite(t_buf, 1, cardPara.totalLength, dataHandle);
        //     }
        // }
    }


    if(cardPara.mode && !ddrFlag)
    {
        fwrite(t_buf,1,cardPara.totalLength,dataHandle);
    }
    free(t_buf);
}

//h2c_transfer
void PcieFunction::h2c_transfer(unsigned int address,unsigned int size,unsigned char *buffer,int index)
{
   int val = this->property("设备").toInt();

   if(val != index)  return;

    memcpy(h2c_align_mem_tmp,buffer,size);
    write_device(h_h2c0,address,size,buffer);
}

//嵌套写入函数
void PcieFunction::user_write(unsigned int address,unsigned int size,unsigned char *buffer,int index)
{
  int val = this->property("设备").toInt();
  if(val != index)  return;
  write_device(h_user,address,size,buffer);
}

//嵌套读取函数
void PcieFunction::user_read(unsigned int address,unsigned int size,unsigned char *buffer,int index)
{
  int val = this->property("设备").toInt();
  if(val != index)  return;
   read_device(h_user,address,size,buffer);
}

//检索硬件触发
void PcieFunction::user_read_IRQ(int index,int timeout)
{
    int val = this->property("设备").toInt();
    if(val != index)  return;
    unsigned char buf[4];
    UINT32 data;
    int elapsed;
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    for(;;)
    {
        if(trigEnd)
        {
            trigEnd = false;
            return;
        }
        read_device(h_user,0x0C,4,buf);
        data = buf[3]<<24|buf[2]<<16|buf[1]<<8|buf[0];

        if(data == 1)
        {
            emit startHardCollection();
            unsigned char val;
            val = 0;
            user_write(12,4,&val,index);
            return;
        }

        elapsed = elapsedTimer.elapsed()/1000;
        if(elapsed > timeout)
        {
            emit timeoutIRQ();
            trigEnd = true;
        }
    }
}

//分配buff
BYTE* PcieFunction::allocate_buffer(UINT32 size, UINT32 alignment)
{
    if (size == 0) {
        size = 4;
    }

    if (alignment == 0) {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        alignment = sys_info.dwPageSize;
    }
    return (BYTE*)_aligned_malloc(size, alignment);
}

//读取设备
int PcieFunction::read_device(HANDLE device, unsigned int address, DWORD size, BYTE *buffer)
{
    DWORD rd_size = 0;
    unsigned int transfers;
    unsigned int i=0;

    LARGE_INTEGER addr;
    memset(&addr,0,sizeof(addr));
    addr.QuadPart = address;
    if ((int)INVALID_SET_FILE_POINTER == SetFilePointerEx(device, addr, NULL, FILE_BEGIN)) {
        return -3;
    }
    transfers = (unsigned int)(size / MAX_BYTES_PER_TRANSFER);
    for (i = 0; i<transfers; i++)
    {
        if (!ReadFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), (DWORD)MAX_BYTES_PER_TRANSFER, &rd_size, NULL))
        {
            return -1;
        }
        if (rd_size != MAX_BYTES_PER_TRANSFER)
        {
            return -2;
        }
    }
    if (!ReadFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), (DWORD)(size - i*MAX_BYTES_PER_TRANSFER), &rd_size,NULL))
    {
        return -1;
    }
    if (rd_size != (size - i * MAX_BYTES_PER_TRANSFER))
    {
        return -2;
    }
    return size;
}

//写入设备
int PcieFunction::write_device(HANDLE device, unsigned int address, DWORD size, BYTE *buffer)
{
    DWORD wr_size = 0;
    unsigned int transfers;
    unsigned int i;
    transfers = (unsigned int)(size / MAX_BYTES_PER_TRANSFER);

    if (INVALID_SET_FILE_POINTER == SetFilePointer(device, address, NULL, FILE_BEGIN)) {
      fprintf(stderr, "Error setting file pointer, win32 error code: %ld\n", GetLastError());
        return -3;
    }

    for (i = 0; i<transfers; i++)
    {
        if (!WriteFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), MAX_BYTES_PER_TRANSFER, &wr_size, NULL))
        {
            return -1;
        }
        if (wr_size != MAX_BYTES_PER_TRANSFER)
        {
            return -2;
        }
    }
    if (!WriteFile(device, (void *)(buffer + i*MAX_BYTES_PER_TRANSFER), (DWORD)(size - i*MAX_BYTES_PER_TRANSFER), &wr_size, NULL))
    {
        return -1;
    }
    if (wr_size != (size - i*MAX_BYTES_PER_TRANSFER))
    {
        return -2;
    }
    return size;
}

//PCIE析构函数
PcieFunction::~PcieFunction()
{
    delete  saveFileInf;
}
