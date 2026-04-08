#include "dataloader.h"
#include <QFile>
#include <QVector>
#include <QByteArray>
#include <QDebug>
#include <QThread>
#include <QMutexLocker>

extern bool exportLowValuesToHexFile(const QString &filePath, const QList<int16_t> &lowValues);
DataLoader::DataLoader(QObject *parent)
    : QObject{parent}
{}

DataLoader::DataLoader(QObject *parent, int index): QObject{parent}, m_index(index)
{
}


void DataLoader::loadFile(int channel, const QString &filename, QVector<double> &xdata, QVector<double> &ydata, QDataStream::ByteOrder byteOrder)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件: " << filename;
        return;
    }

    // 计算x轴和y轴的最大值
    double x_max = 0;
    double y_max = 0;

    QDataStream in(&file);
    in.setByteOrder(byteOrder);

    QList<int16_t> allValues;
    int index = 0;
    double x = 0.0;
    double x_span = 0.05/1000;
    while (!in.atEnd()) {
        qint16 value;
        in >> value;

        // 检查读取是否成功
        if (in.status() != QDataStream::Ok) {
            qWarning() << "读取数据失败: " << index;
            break;
        }

        xdata.append(x);
        ydata.append(static_cast<double>(value)); // 存储y轴数据

        // 收集所有的 lowValue
        allValues.append(value);

        ++index;
        x += x_span;



        // if (index >= 10000) {
        //     if (!xdata.isEmpty()) {
        //         x_max = std::max(x_max, xdata.last());
        //     }
        //     if (!ydata.isEmpty()) {
        //         y_max = std::max(y_max, *std::max_element(ydata.begin(), ydata.end()));
        //     }

        //     emit dataLoaded(channel, xdata, ydata, x_max, y_max);

        //     // 调用函数导出所有 lowValue 到文件
        //     if (!allValues.isEmpty()) {
        //         QString exportFile = QString("card%1ddr_ch%2.txt").arg(m_index+1).arg(channel);
        //         bool success = exportLowValuesToHexFile(exportFile, allValues);
        //         if (success) {
        //             qDebug() << "成功导出 allValues 到 " << exportFile;
        //         } else {
        //             qWarning() << "导出 allValues 失败";
        //         }
        //     }

        //     break;
        // }

        // if (index % 10000 == 0) {
        //     if (xdata.size() != ydata.size()) {
        //         qWarning() << "xdata 和 ydata 大小不一致: " << index << xdata.size() << ydata.size();
        //         break;
        //     }

        //     emit dataLoaded(xdata, ydata, x_max, y_max);
        // }
    }
    qWarning() << "xdata 和 ydata 大小 " << xdata.size() << ydata.size();

        if (!xdata.isEmpty()) {
            x_max = std::max(x_max, xdata.last());
        }
        if (!ydata.isEmpty()) {
            y_max = std::max(y_max, *std::max_element(ydata.begin(), ydata.end()));
        }

        emit dataLoaded(channel, xdata, ydata, x_max, y_max);

        // 调用函数导出所有 lowValue 到文件
        if (!allValues.isEmpty()) {
            QString exportFile = QString("card%1ddr_ch%2.txt").arg(m_index+1).arg(channel);
            bool success = exportLowValuesToHexFile(exportFile, allValues);
            if (success) {
                qDebug() << "成功导出 allValues 到 " << exportFile;
            } else {
                qWarning() << "导出 allValues 失败";
            }
        }


    file.close();


    emit loadingFinished();
}
