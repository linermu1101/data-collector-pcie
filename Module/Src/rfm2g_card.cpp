#include "rfm2g_card.h"
#include <QDebug>
#include <QMutex>

// 全局互斥锁，保证线程安全
static QMutex g_rfmMutex;

RFM2G_CARD::RFM2G_CARD(QObject *parent) : QObject(parent)
{
    handle = nullptr;
    m_isOpened = false;
}

int RFM2G_CARD::openRFM2g()
{
    QMutexLocker locker(&g_rfmMutex);
    
    if (m_isOpened) {
        qDebug() << "反射内存卡已经打开";
        return 0;
    }
    
    int res = RFM2gOpen(DevicePath, handle);
    if (res == 0) {
        m_isOpened = true;
        qDebug() << "反射内存卡打开成功";
    } else {
         m_isOpened = false;
         QString errorMsg = QString("反射内存卡打开错误！错误码: %1").arg(res);
         qDebug() << errorMsg;
         emit errorOccurred(errorMsg);
     }
    
    return res;
}

void RFM2G_CARD::releaseRFM2g()
{
    QMutexLocker locker(&g_rfmMutex);
    
    if (m_isOpened && handle) {
        RFM2gClose(handle);
        m_isOpened = false;
        qDebug() << "反射内存卡已关闭";
    }
}

void RFM2G_CARD::write(unsigned char *buffer,unsigned long length)
{
    qDebug()<<"========";
    int res = RFM2gWrite(*handle,120*1024*1024,buffer,length);
    if(res)
    {
        releaseRFM2g();
        res = openRFM2g();
        if(res) return;
        else
           RFM2gWrite(*handle,120*1024*1024,buffer,length);
    }
}

// 计算指定卡和数据类型的内存地址
uint32_t RFM2G_CARD::calculateAddress(int cardIndex, int dataType) const
{
    if (cardIndex < 0 || cardIndex >= m_maxCardCount) {
        qDebug() << "无效的卡索引:" << cardIndex;
        return 0;
    }
    
    uint32_t cardBaseAddr = cardIndex * RFM2G_MemoryLayout::CARD_BASE_SIZE;
    
    switch (dataType) {
        case 0: // 实时数据
            return cardBaseAddr + RFM2G_MemoryLayout::REALTIME_OFFSET;
        case 1: // 参数数据
            return cardBaseAddr + RFM2G_MemoryLayout::PARAM_OFFSET;
        case 2: // 状态数据
            return cardBaseAddr + RFM2G_MemoryLayout::STATUS_OFFSET;
        default:
            qDebug() << "无效的数据类型:" << dataType;
            return 0;
    }
}

// 初始化指定卡的内存映射
bool RFM2G_CARD::initializeCardMemory(int cardIndex)
{
    if (!m_isOpened || !handle) {
        qDebug() << "反射内存卡未打开，无法初始化卡" << cardIndex;
        return false;
    }
    
    if (cardIndex < 0 || cardIndex >= m_maxCardCount) {
        qDebug() << "无效的卡索引:" << cardIndex;
        return false;
    }
    
    // 清零该卡的所有内存区域
    uint32_t cardBaseAddr = cardIndex * RFM2G_MemoryLayout::CARD_BASE_SIZE;
    char zeroBuffer[1024] = {0}; // 1KB零缓冲区
    
    // 分块清零整个卡空间（8MB）
    for (uint32_t offset = 0; offset < RFM2G_MemoryLayout::CARD_BASE_SIZE; offset += 1024) {
        int res = RFM2gWrite(*handle, cardBaseAddr + offset, (unsigned char*)zeroBuffer, 1024);
        if (res != 0) {
             QString errorMsg = QString("初始化卡%1内存失败，地址: 0x%2, 错误码: %3")
                               .arg(cardIndex).arg(cardBaseAddr + offset, 0, 16).arg(res);
             qDebug() << errorMsg;
             emit errorOccurred(errorMsg);
             return false;
         }
    }
    
    qDebug() << "卡" << cardIndex << "内存初始化成功";
    return true;
}

// 写入实时数据
bool RFM2G_CARD::writeRealtimeData(int cardIndex, const RFM2G_RealtimeData& data)
{
    QMutexLocker locker(&g_rfmMutex);
    
    if (!m_isOpened || !handle) {
        return false;
    }
    
    uint32_t addr = calculateAddress(cardIndex, 0); // 实时数据区
    if (addr == 0) {
        return false;
    }
    
    // 计算写入地址（基于采样序号）
    uint32_t writeAddr = addr + (data.sampleIndex % (RFM2G_MemoryLayout::REALTIME_DATA_SIZE / sizeof(RFM2G_RealtimeData))) * sizeof(RFM2G_RealtimeData);
    
    int res = RFM2gWrite(*handle, writeAddr, (unsigned char*)&data, sizeof(RFM2G_RealtimeData));
    return (res == 0);
}

// 写入参数数据
bool RFM2G_CARD::writeParamData(int cardIndex, const RFM2G_ParamData& data)
{
    QMutexLocker locker(&g_rfmMutex);
    
    if (!m_isOpened || !handle) {
        return false;
    }
    
    uint32_t addr = calculateAddress(cardIndex, 1); // 参数数据区
    if (addr == 0) {
        return false;
    }
    
    int res = RFM2gWrite(*handle, addr, (unsigned char*)&data, sizeof(RFM2G_ParamData));
    return (res == 0);
}

// 写入状态数据
bool RFM2G_CARD::writeStatusData(int cardIndex, const RFM2G_StatusData& data)
{
    QMutexLocker locker(&g_rfmMutex);
    
    if (!m_isOpened || !handle) {
        return false;
    }
    
    uint32_t addr = calculateAddress(cardIndex, 2); // 状态数据区
    if (addr == 0) {
        return false;
    }
    
    int res = RFM2gWrite(*handle, addr, (unsigned char*)&data, sizeof(RFM2G_StatusData));
    return (res == 0);
}
