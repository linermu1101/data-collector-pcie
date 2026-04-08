#ifndef RFM2G_CARD_H
#define RFM2G_CARD_H

#include <QObject>
#include "header.h"

// 反射内存卡地址规划结构体 - 优化版本
struct RFM2G_MemoryLayout {
    // 总内存容量：128MB或256MB，支持最多15张卡
    static const uint32_t TOTAL_MEMORY_SIZE = 128 * 1024 * 1024;  // 128MB总容量
    static const uint32_t MAX_CARDS = 15;                         // 最大支持卡数
    static const uint32_t CARD_BASE_SIZE = 8 * 1024 * 1024;      // 每张卡分配8MB空间
    
    // 每张卡内存分区（紧凑设计）
    static const uint32_t REALTIME_DATA_SIZE = 6 * 1024 * 1024;  // 实时数据区6MB
    static const uint32_t PARAM_DATA_SIZE = 1 * 1024 * 1024;     // 参数数据区1MB
    static const uint32_t STATUS_DATA_SIZE = 1 * 1024 * 1024;    // 状态数据区1MB
    
    // 各区域在卡内的偏移地址
    static const uint32_t REALTIME_OFFSET = 0;                    // 实时数据偏移
    static const uint32_t PARAM_OFFSET = REALTIME_DATA_SIZE;       // 参数数据偏移
    static const uint32_t STATUS_OFFSET = REALTIME_DATA_SIZE + PARAM_DATA_SIZE; // 状态数据偏移
};

// 简化的实时数据结构体 - 针对500us高频采集优化
struct RFM2G_RealtimeData {
    uint32_t cardIndex;        // 卡索引
    uint32_t sampleIndex;      // 采样序号
    uint32_t mode;             // 工作模式(0单频1三频)
    uint32_t timestamp;        // 时间戳(简化为32位)
    
    // 采样数据（根据模式不同含义不同）
    union {
        struct {  // 单频模式 (mode=0)
            float singleValue;     // 单频处理后的角度值
            uint32_t reserved[2];  // 保留字段
        } single;
        
        struct {  // 三频模式 (mode=1)
            float phase1;          // 第一相位值
            float phase2;          // 第二相位值  
            float phase3;          // 第三相位值
        } triple;
    } data;
};

// 简化的参数数据结构体
struct RFM2G_ParamData {
    uint32_t cardIndex;        // 卡索引
    uint32_t triggerMode;      // 触发模式
    uint32_t workMode;         // 工作模式(0单频1三频)
    uint32_t sampleInterval;   // 采样间隔(us)
    uint32_t startTime;        // 开始时间戳
    uint32_t reserved[3];      // 保留字段
};

// 简化的状态数据结构体
struct RFM2G_StatusData {
    uint32_t cardIndex;        // 卡索引
    uint32_t isActive;         // 是否活跃(1活跃0停止)
    uint32_t totalSamples;     // 总采样数
    uint32_t currentFifo;      // 当前FIFO长度
    uint32_t lastUpdateTime;   // 最后更新时间戳
    uint32_t errorCount;       // 错误计数
    uint32_t reserved[2];      // 保留字段
};

class RFM2G_CARD : public QObject
{
    Q_OBJECT
public:
    explicit RFM2G_CARD(QObject *parent = nullptr);

    int openRFM2g();
    void releaseRFM2g();
    
    // 设置最大支持卡数（最多15张卡）
    void setMaxCardCount(int count) { 
        m_maxCardCount = (count > RFM2G_MemoryLayout::MAX_CARDS) ? 
                        RFM2G_MemoryLayout::MAX_CARDS : count; 
    }
    int getMaxCardCount() const { return m_maxCardCount; }
    
    // 检查设备是否已打开
    bool isOpened() const { return m_isOpened; }
    
    // 初始化指定卡的内存映射
    bool initializeCardMemory(int cardIndex);
    
    // 写入实时数据
    bool writeRealtimeData(int cardIndex, const RFM2G_RealtimeData& data);
    
    // 写入参数数据
    bool writeParamData(int cardIndex, const RFM2G_ParamData& data);
    
    // 写入状态数据
    bool writeStatusData(int cardIndex, const RFM2G_StatusData& data);

public slots:
    void write(unsigned char *buffer,unsigned long length);

private:
    char *DevicePath  = "\\\\.\\rfm2g1";
    RFM2GHANDLE *handle;
    int m_maxCardCount = RFM2G_MemoryLayout::MAX_CARDS;
    bool m_isOpened = false;  // 设备打开状态
    
    // 计算指定卡和数据类型的内存地址
    uint32_t calculateAddress(int cardIndex, int dataType) const;

signals:
    void errorOccurred(const QString& errorMessage);

};

#endif // RFM2G_CARD_H
