#include "realdataloader.h"
#include <QFile>
#include <QVector>
#include <QByteArray>
#include <QDebug>
#include <QThread>
#include <QMutexLocker>


RealDataLoader::RealDataLoader(QObject *parent)
    : QObject{parent}
{}

RealDataLoader::RealDataLoader(QObject *parent, int index): QObject{parent}, m_index(index)
{
}


// 封装的小端到大端字节序转换函数
uint32_t RealDataLoader::swapEndian(uint32_t value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x000000FF) << 24);
}

// 定义一个函数来导出 lowValue 到文本文件
bool exportLowValuesToHexFile(const QString &filePath, const QList<int16_t> &lowValues) {
    QFile outFile(filePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件进行写入:" << filePath;
        return false;
    }

    QTextStream out(&outFile);
    foreach (int16_t value, lowValues) {
        // 将 int16_t 的值转换为 uint16_t 以正确处理负数
        uint16_t unsignedValue = static_cast<uint16_t>(value);

        // 将 lowValue 转换为大写的16进制字符串，确保至少有4个字符，不足的前面补0
        out << QString::number(unsignedValue, 16).toUpper().rightJustified(4, '0') << "\n";
    }
    outFile.close();
    return true;
}

void RealDataLoader::loadFile(int cardIndex, const QString &filename, QMap<int, QVector<QVector<double>>> &mapXdata, QMap<int, QVector<QVector<double>>> &mapYdata, QDataStream::ByteOrder byteOrder)
{
    QVector<QVector<double>> xdata;
    QVector<QVector<double>> ydata;
    xdata.resize(2);
    ydata.resize(2);

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件: " << filename;
        return;
    }

    qDebug() << " RealDataLoader::loadFile " << filename;

    // 计算x轴和y轴的最大值
    double x_max = 0;
    double y_max = 0;
    double x1_max = 0;
    double y1_max = 0;
    double x2_max = 0;
    double y2_max = 0;

    QDataStream in(&file);
    in.setByteOrder(byteOrder);

    QList<int16_t> allLowValues;

    int index = 0;
    while (!in.atEnd()) {
        int32_t value;
        in >> value;

        // 检查读取是否成功
        if (in.status() != QDataStream::Ok) {
            qWarning() << "读取数据失败: " << index;
            break;
        }

        int16_t highValue = (int16_t)((value >> 16) & 0xFFFF);
        int16_t lowValue = (int16_t)(value & 0xFFFF);  //这个数在单频有效

        double valueHighf = (double)highValue/8; //除以8
        double valueLowf = (double)lowValue/8; //除以8
        double valuef = (double)value/8192*180/3.1415926; //除以8  /8192*180/3.1415926

        if(index == 0)
        {
            qDebug() << " first point is " << valueHighf << valueLowf;
        }

        xdata[0].append(index/2);
        ydata[0].append(static_cast<double>(valuef));

        xdata[1].append(index);
        ydata[1].append(static_cast<double>(valueLowf));

        // 收集所有的 lowValue
        allLowValues.append(lowValue);

        ++index;
    }
    qWarning() << "xdata 和 ydata 大小 " << xdata[0].size() << ydata[0].size();

    // 调用函数导出所有 lowValue 到文件
//    if (!allLowValues.isEmpty()) {
//        QString exportFile = QString("card%1real_lowValues.txt").arg(m_index+1);
//        bool success = exportLowValuesToHexFile(exportFile, allLowValues);
//        if (success) {
//            qDebug() << "成功导出 lowValues 到 real_lowValues.txt";
//        } else {
//            qWarning() << "导出 lowValues 失败";
//        }
//    }

    if (!xdata[0].isEmpty()) {
        x1_max = std::max(x1_max, xdata[0].last());
    }
    if (!ydata[0].isEmpty()) {
        y1_max = std::max(y1_max, *std::max_element(ydata[0].begin(), ydata[0].end()));
    }

    if (!xdata[1].isEmpty()) {
        x2_max = std::max(x2_max, xdata[1].last());
    }
    if (!ydata[1].isEmpty()) {
        y2_max = std::max(y2_max, *std::max_element(ydata[1].begin(), ydata[1].end()));
    }

    x_max = (x1_max > x2_max)? x1_max : x2_max;
    y_max = (y1_max > y2_max)? y1_max : y2_max;

    emit dataLoaded(m_index, xdata, ydata, x_max, y_max);

    file.close();

    mapXdata.insert(cardIndex, xdata);
    mapYdata.insert(cardIndex, ydata);


    emit loadingFinished(m_index);
}


void RealDataLoader::loadThreeFile(int cardIndex, const QString &filename, QMap<int, QVector<QVector<double>>> &mapXdata, QMap<int, QVector<QVector<double>>> &mapYdata, QDataStream::ByteOrder byteOrder)
{
    QVector<QVector<double>> xdata;
    QVector<QVector<double>> ydata;
    xdata.resize(4); // 调整为4，支持多频模式
    ydata.resize(4); // 调整为4，支持多频模式

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件: " << filename;
        return;
    }

    qDebug() << " RealDataLoader::loadFile " << filename;

    // 计算x轴和y轴的最大值
    double x_max = 0;
    double y_max = 0;
    double x1_max = 0, y1_max = 0;
    double x2_max = 0, y2_max = 0;
    double x3_max = 0, y3_max = 0;
    double x4_max = 0, y4_max = 0;

    QDataStream in(&file);
    in.setByteOrder(byteOrder);

    QList<int16_t> allLowValues;

    int index = 0;
    while (!in.atEnd()) {
        int64_t value64; // 改用64位整数读取
        in >> value64;

        // 检查读取是否成功
        if (in.status() != QDataStream::Ok) {
            qWarning() << "读取数据失败: " << index;
            break;
        }

        // 提取前32位数据，保持与原有处理方式一致
        int32_t value32High = static_cast<int32_t>((value64 >> 32) & 0xFFFFFFFF);

        // 提取后32位，并分为两个16位数据
        int16_t valueMid = static_cast<int16_t>((value64 >> 16) & 0xFFFF);
        int16_t valueLow = static_cast<int16_t>(value64 & 0xFFFF);

        // 处理前32位数据，与原来相同
        // int16_t highValue = static_cast<int16_t>((value32High >> 16) & 0xFFFF);
        // int16_t lowValue = static_cast<int16_t>(value32High & 0xFFFF);

        // double valueHighf = static_cast<double>(highValue)/8; // 除以8
        // double valueLowf = static_cast<double>(lowValue)/8; // 除以8
        double valuef = static_cast<double>(value32High); // 除以8  /8192*180/3.1415926

        // 处理中间16位数据
        double valueMidf = static_cast<double>(valueMid); // 与valuef相同处理 /8192*180/3.1415926

        // 处理最后16位数据
        double valueLastf = static_cast<double>(valueLow); // 与valuef相同处理 /8192*180/3.1415926

        if(index == 0)
        {
            qDebug() << " first point is " << value32High << " mid: " << valueMidf << " last: " << valueLastf;
        }

        // 第一组数据，与原来相同
        xdata[0].append(index);
        ydata[0].append(static_cast<double>(valuef));//换成valueLastf可以

        // 第二组数据，没用到随便填
        xdata[1].append(index);
        ydata[1].append(static_cast<double>(valuef));

        // 第三组数据，中间16位
        xdata[2].append(index);
        ydata[2].append(static_cast<double>(valueMidf));


        // 第四组数据，最后16位
        xdata[3].append(index);
        ydata[3].append(static_cast<double>(valueLastf));

        // // 收集所有的 lowValue
        // allLowValues.append(lowValue);

        ++index;
    }
    qWarning() << "xdata 和 ydata 大小 " << xdata[0].size() << ydata[0].size();

    // 调用函数导出所有 lowValue 到文件
    // if (!allLowValues.isEmpty()) {
    //     QString exportFile = QString("card%1real_lowValues.txt").arg(m_index+1);
    //     bool success = exportLowValuesToHexFile(exportFile, allLowValues);
    //     if (success) {
    //         qDebug() << "成功导出 lowValues 到 real_lowValues.txt";
    //     } else {
    //         qWarning() << "导出 lowValues 失败";
    //     }
    // }

    // 更新最大值计算，包括所有四组数据
    if (!xdata[0].isEmpty()) {
        x1_max = std::max(x1_max, xdata[0].last());
    }
    if (!ydata[0].isEmpty()) {
        y1_max = std::max(y1_max, *std::max_element(ydata[0].begin(), ydata[0].end()));
    }

    if (!xdata[1].isEmpty()) {
        x2_max = std::max(x2_max, xdata[1].last());
    }
    if (!ydata[1].isEmpty()) {
        y2_max = std::max(y2_max, *std::max_element(ydata[1].begin(), ydata[1].end()));
    }

    if (!xdata[2].isEmpty()) {
        x3_max = std::max(x3_max, xdata[2].last());
    }
    if (!ydata[2].isEmpty()) {
        y3_max = std::max(y3_max, *std::max_element(ydata[2].begin(), ydata[2].end()));
    }

    if (!xdata[3].isEmpty()) {
        x4_max = std::max(x4_max, xdata[3].last());
    }
    if (!ydata[3].isEmpty()) {
        y4_max = std::max(y4_max, *std::max_element(ydata[3].begin(), ydata[3].end()));
    }

    x_max = std::max(std::max(x1_max, x2_max), std::max(x3_max, x4_max));
    y_max = std::max(std::max(y1_max, y2_max), std::max(y3_max, y4_max));

    emit dataLoaded(m_index, xdata, ydata, x_max, y_max);

    file.close();

    mapXdata.insert(cardIndex, xdata);
    mapYdata.insert(cardIndex, ydata);


    emit loadingFinished(m_index);
}
