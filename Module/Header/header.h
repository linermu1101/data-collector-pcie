#ifndef HEADER_H
#define HEADER_H
#ifdef __cplusplus
#include "stdint.h"
#include "windows.h"
extern "C"
{
#endif
#ifdef __cplusplus
#include "rfm2g_api.h"
}
#endif
#include <QDebug>


enum LayoutStyle{
    Layout_4x2  =   0,
    Layout_8x1  =   1,
};

enum WorkMode{
    Single_Frequency    =   0,
    Three_Frequency     =   1,
};

enum PhasePick{
    PickUp_1us      =   0x0a,
    PickUp_10us     =   0x64,
    PickUp_20us     =   0xc8,
    PickUp_50us     =   0x1f4,
    PickUp_100us    =   0x3e8,
};

enum PhaseFilp_50ms{
    Auto_Judge  =   0,
    Negative    =   1,
};

enum ConfigRegAddress{
    Overflow_Warning_Addr       =   0x00,
    Trigger_Addr                =   0x04,
    HardTrig_MainCard_Addr      =   0x1c,
    Working_Mode_Addr           =   0x24,
    Phase_PickUp_Addr           =   0x28,
    Phase_Filp_Addr             =   0x2c,
    Frequency_Reference_Addr    =   0x30,
    Measuring_Frequency_Addr    =   0x34,
    V2_SINGEL_RESET_Addr                =   0x00,   // 新版单频复位地址
    V2_SINGEL_SOFTWARE_TRIG_Addr        =   0x04,   // 新版单频软件触发地址
    V2_SINGEL_HARDWARE_TRIG_Addr        =   0x08,   // 新版单频硬件触发地址
    V2_SINGEL_HARDWARE_TAG_Addr         =   0x40,    // 新版单频硬件触发标志
    V2_SINGEL_FIFO_LEFT_Addr            =   0x38,   // 新版单频 实时上传FIFO剩余点数
    V2_SINGEL_REALDATA_Addr             =   0x3c,   // 新版单频 实时上传数据
    V2_SINGEL_CHANNEL1_DDRADDRESS       =   0x00000000,   // 新版单频 DDR通道一地址
    V2_SINGEL_CHANNEL2_DDRADDRESS       =   0x20000000,   // 新版单频 DDR通道二地址
};

enum CtlStatus{
    Init    =   0,
    Start   =   1,
    Stop    =   2,
};


struct SettingPara{
    uint32_t address;
    int cardIndex;
    int triggerMode;    // 触发模式 0 软件 1硬件
    int mode;           // 工作模式 单频或者三频
    int saveCnt;
    int coefficient;
    int pickUpMode;
    //float c_time;
    int c_time;         // 采样时间
    int singleLength;   // 单帧字节数
    int totalLength;    // 字节总数
    bool startFlag = false;
};

//注：如果这里不设置2字节对齐， 默认为4字节对齐，则结构体的总字节大小并不是122byte，而是128byte
#pragma pack(push) //  保存原来的字节对齐状态
#pragma pack(2) // 两字节对齐
struct InfPara{
    char filetype[10];
    int16_t channelId;
    char channelName[12];
    long addr;
    float freq;
    long len;
    long post;
    uint16_t maxDat;
    float lowRang;
    float highRang;
    float factor;
    float Offset;
    char unit[8];
    float Dly;
    int16_t attribDt;
    int16_t datWth;
    int16_t sparI1=0;
    int16_t sparI2=0;
    int16_t sparI3=0;
    float sparF1=0;
    float sparF2=0;
    char sparC1[8]={""};
    char sparC2[16]={""};
    char sparC3[10]={""};
};
#pragma pack(pop)

#endif // HEADER_H
