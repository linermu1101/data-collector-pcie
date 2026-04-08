#include "SignalProcessing.h"
#include <QtEndian> // qFromLittleEndian
#include <QtMath>   // 提供 qFloor

SignalProcessing::SignalProcessing()
{
    in = nullptr;
    out = nullptr;
    p = nullptr;
}

SignalProcessing::~SignalProcessing()
{
    if (p) {
        fftw_destroy_plan(p);
    }
    if (in) {
        fftw_free(in);
    }
    if (out) {
        fftw_free(out);
    }
    fftw_cleanup();
}


QVector<double> SignalProcessing::readRegionData(const QString& filePath, int regionIndex, int numRegions)
{
    QVector<double> regionData;
    QFile file(filePath);
qDebug()<<"numregions为"<< numRegions;
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << filePath;
        return regionData;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian); // 根据实际情况调整字节序

    const int pointsPerRegion = 128; // 每个区域的数据点数量
    const int bytesPerPoint = 2; // 每个数据点2个字节
    const int totalPoints = numRegions * pointsPerRegion; // 需要读取的总数据点数量

    // 计算每个完整ABCDE区域的字节数
    const int bytesPerABCDEGroup = 5 * pointsPerRegion * bytesPerPoint;

    // 检查文件大小是否足够
    if (file.size() < bytesPerABCDEGroup * 2) {
        qWarning() << "File does not contain enough data to read" << numRegions << "regions.";
        file.close();
        return regionData; // 返回空的QVector<double>
    }

    QByteArray buffer(pointsPerRegion * bytesPerPoint, 0);
    regionData.reserve(totalPoints);

    for (int index = 0; index < numRegions; ++index) {
        // 计算当前区域的偏移量
        qint64 offset = index * bytesPerABCDEGroup + (regionIndex - 1) * pointsPerRegion * bytesPerPoint;

        // 移动到当前区域的位置
        if (!file.seek(offset)) {
            qWarning() << "Failed to seek to region" << regionIndex << (index + 1) << "in file:" << filePath;
            file.close();
            return regionData;
        }

        // 读取当前区域数据
        if (in.readRawData(buffer.data(), buffer.size()) != buffer.size()) {
            qWarning() << "Failed to read region" << regionIndex << (index + 1) << "data from file:" << filePath;
            file.close();
            return regionData;
        }

        for (int i = 0; i < pointsPerRegion; ++i) {
            qint16 value = *reinterpret_cast<qint16*>(buffer.data() + i * bytesPerPoint);
            regionData.append(static_cast<double>(value));
        }
    }

    file.close();

    return regionData;
}
//newReadRegionData：此函数已废弃
QVector<double> SignalProcessing::newReadRegionData(const QString &filePath, int regionIndex, int numRegions)
{
    QVector<double> regionData;
    QFile file(filePath);
    //qDebug() << "numRegions为" << numRegions;

    // 参数检查
    if (numRegions <= 0) {
        qWarning() << "numRegions must be > 0";
        return regionData;
    }
    if (regionIndex < 1 || regionIndex > 5) {
        qWarning() << "regionIndex must be in [1,5]";
        return regionData;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << filePath;
        return regionData;
    }

    const int pointsPerRegion = 128; // 每个区域的数据点数量
    const int bytesPerPoint = 2;     // 每个数据点2个字节
    const qint64 bytesPerRegion = qint64(pointsPerRegion) * bytesPerPoint;
    const qint64 bytesPerABCDEGroup = 5 * bytesPerRegion; // 每个完整组( A..E )字节数

    // 检查文件大小是否足够读取 numRegions 个完整组
    qint64 neededBytes = qint64(numRegions) * bytesPerABCDEGroup;
    if (file.size() < neededBytes) {
        qWarning() << "File does not contain enough data. required bytes =" << neededBytes
                   << " but file.size() =" << file.size();
        file.close();
        return regionData;
    }

    // 计算总样点数以及需要处理的样点上限（只处理前 1000/1024 部分）
    const qint64 totalPoints = qint64(numRegions) * pointsPerRegion;
    const qint64 processLimit = qFloor(totalPoints * (990.0 / 1024.0));
    if (processLimit <= 0) {
        qWarning() << "Process limit <= 0, nothing to do.";
        file.close();
        return regionData;
    }

    // 预留空间
    regionData.reserve((int)qMin(totalPoints, processLimit));

    // 用于每次读取的缓冲区（每区域256字节）
    QByteArray buffer(bytesPerRegion, 0);

    qint64 appendedCount = 0; // 已加入 regionData 的样点计数

    for (int g = 0; g < numRegions; ++g) {
        if (appendedCount >= processLimit) break; // 达到上限就退出

        // 计算偏移到第 g 组中的 regionIndex 区域起始处
        qint64 offset = qint64(g) * bytesPerABCDEGroup + qint64(regionIndex - 1) * bytesPerRegion;

        if (!file.seek(offset)) {
            qWarning() << "Failed to seek to region" << regionIndex << "group" << g << "in file:" << filePath;
            break;
        }

        qint64 bytesRead = file.read(buffer.data(), buffer.size());
        if (bytesRead != buffer.size()) {
            qWarning() << "Failed to read region" << regionIndex << "group" << g << "data from file. expected"
                       << buffer.size() << "but read" << bytesRead;
            break;
        }

        // 逐个样本解析为小端 qint16，并追加，注意不超过 processLimit
        const uchar *udata = reinterpret_cast<const uchar *>(buffer.constData());
        for (int i = 0; i < pointsPerRegion; ++i) {
            if (appendedCount >= processLimit) break; // 满足总上限后退出

            // 安全地从两个字节构造小端 qint16
            qint16 val = qFromLittleEndian<qint16>(udata + i * bytesPerPoint);
            regionData.append(static_cast<double>(val));
            ++appendedCount;
        }
    }

    file.close();
    qDebug() << "Finished reading. totalPoints =" << totalPoints << "processLimit =" << processLimit
             << "appended =" << appendedCount;
    return regionData;
}

void SignalProcessing::fftTransform(int N, double fs, const QVector<double>& input, QVector<double>& frequencies, QVector<double>& magnitudes)
{
    this->N = N;
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);

    // 初始化输入数据
    for (int i = 0; i < N; i++) {
        in[i][0] = input[i];
        in[i][1] = 0;
    }

    // 创建FFT计划
    p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // 执行FFT变换
    fftw_execute(p);

    // 计算频率和幅值数据
    frequencies.resize(N / 2);
    magnitudes.resize(N / 2);

    double delta_f = fs / N; // 频率分辨率

    for (int i = 0; i < N / 2; ++i) {
        frequencies[i] = i * delta_f; // 实际频率
        magnitudes[i] = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]); // 幅值
    }
}

QVector<std::complex<double>> SignalProcessing::getFFTOutput() const
{
    QVector<std::complex<double>> output;
    if (out) {
        for (int i = 0; i < N; ++i) {
            output.append(std::complex<double>(out[i][0], out[i][1]));
        }
    }
    return output;
}

QVector<double> SignalProcessing::smooth(const QVector<double>& data, int windowSize)
{
    QVector <double> smoothedData;
    int n = data.size();
    if (windowSize <= 0 || windowSize > n) {
        return data;
    }

    smoothedData.resize(n);
    for (int i = 0; i < n; ++i) {
        double sum = 0.0;
        int count = 0;
        for (int j = -windowSize / 2; j <= windowSize / 2; ++j) {
            int index = i + j;
            if (index >= 0 && index < n) {
                sum += data[index];
                count++;
            }
        }
        smoothedData[i] = sum / count;
    }
    return smoothedData;
}

// void SignalProcessing::findExtremes(const QVector<double>& data, double& maxValue, double& minValue)
// {
//     if (data.isEmpty()) {
//         maxValue = 0;
//         minValue = 0;
//         return;
//     }

//     maxValue = *std::max_element(data.begin(), data.end());
//     minValue = *std::min_element(data.begin(), data.end());
// }

// QVector<QPair<double, double>> SignalProcessing::findTopThreeExtremes(const QVector<double>& data, const QVector<double>& frequencies)
// {
//     QVector<QPair<double, double>> maxExtremes;
//     if (data.size() < 3) {
//         return maxExtremes; // 如果数据点少于3个，返回空
//     }

//     // 检测极大值
//     for (int i = 1; i < data.size() - 1; ++i) {
//         if (data[i] > data[i - 1] && data[i] > data[i + 1]) { // 局部最大值
//             maxExtremes.append(qMakePair(frequencies[i], data[i]));
//         }
//     }

//     // 找到最大的三个极大值
//     if (maxExtremes.size() < 3) {
//         return maxExtremes; // 如果极大值少于3个，返回所有极大值
//     }

//     std::sort(maxExtremes.begin(), maxExtremes.end(), [](const QPair<double, double>& a, const QPair<double, double>& b) {
//         return a.second > b.second;
//     });

//     // 取出前三个极大值
//     QVector<QPair<double, double>> topThree = maxExtremes.mid(0, 3);

//     // 按频率升序排列前三个极大值
//     std::sort(topThree.begin(), topThree.end(), [](const QPair<double, double>& a, const QPair<double, double>& b) {
//         return a.first < b.first; // 按频率升序排列
//     });

//     return topThree;
// }


QVector<Extremum> SignalProcessing::findTopThreeExtremes(const QVector<double>& data, const QVector<double>& frequencies)
{
    QVector<Extremum> maxExtremes;
    if (data.size() < 3) {
        return maxExtremes; // 如果数据点少于3个，返回空
    }

    // 检测极大值
    for (int i = 3; i < data.size() - 1; ++i) { // 从第4个点（索引3）开始遍历数据
        if (data[i] > data[i - 1] && data[i] > data[i + 1]) { // 局部最大值
            maxExtremes.append(Extremum(frequencies[i], data[i], i+1)); // 下发序号是从1计数 1-256
        }
    }

    // 找到最大的三个极大值
    if (maxExtremes.size() < 3) {
        return maxExtremes; // 如果极大值少于3个，返回所有极大值
    }

    std::sort(maxExtremes.begin(), maxExtremes.end(), [](const Extremum& a, const Extremum& b) {
        return a.magnitude > b.magnitude;
    });

    // 取出前三个极大值
    QVector<Extremum> topThree = maxExtremes.mid(0, 3);

    // 按频率升序排列前三个极大值
    std::sort(topThree.begin(), topThree.end(), [](const Extremum& a, const Extremum& b) {
        return a.frequency < b.frequency; // 按频率升序排列
    });

    return topThree;
}


Extremum SignalProcessing::findMaxExtreme(const QVector<double>& data, const QVector<double>& frequencies)
{
    if (data.isEmpty()) {
        return Extremum(0, 0, -1); // 返回一个无效的 Extremum
    }

    // 创建一个新的 QVector，排除前三个点
    QVector<double> filteredData;
    if (data.size() > 3) {
        filteredData = data.mid(3); // 从第四个点开始复制数据
    } else {
        return Extremum(0, 0, -1); // 如果数据不足4个点，返回一个无效的 Extremum
    }

    int maxIndex = std::distance(filteredData.begin(), std::max_element(filteredData.begin(), filteredData.end())) + 3; // 计算原始索引
    return Extremum(frequencies[maxIndex], data[maxIndex], maxIndex);
}
