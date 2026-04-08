#ifndef SIGNALPROCESSING_H
#define SIGNALPROCESSING_H

#include <fftw3.h>
#include <QVector>
#include <cmath>
#include <algorithm>
#include <complex>
#include <QFile>
#include <QDataStream>
#include <QVector>
#include <QDebug>


struct Extremum {
    double frequency;   // 频率
    double magnitude;   // 幅值
    int index;          // 序号 1-256 从1开始

    Extremum(double f, double m, int i) : frequency(f), magnitude(m), index(i) {}
};


class SignalProcessing
{
public:
    SignalProcessing();
    ~SignalProcessing();

    /**
     * @brief 解析ddr数据
     * @param filePath
     * @param regionIndex 五组序号分别是1-5
     * @param numRegions  取多少个128的数据点 默认填2 取256
     * @return QVector<double>
     */
    QVector<double> readRegionData(const QString& filePath, int regionIndex, int numRegions);
    QVector<double> newReadRegionData(const QString& filePath, int regionIndex, int numRegions);

    // 执行FFT变换，输入是N个点的数据，输出是频谱值（频率和幅值）
    void fftTransform(int N, double fs, const QVector<double>& input, QVector<double>& frequencies, QVector<double>& magnitudes);

    // 获取FFT变换后的输出数据
    QVector<std::complex<double>> getFFTOutput() const;

    // 平滑算法（示例：简单移动平均）
    QVector<double> smooth(const QVector<double>& data, int windowSize);

    // 求极值算法（示例：求最大值和最小值）
    Extremum findMaxExtreme(const QVector<double>& data, const QVector<double>& frequencies);

    // 找到最大的三个极大值 先幅值降序 然后按频率升序排列
    QVector<Extremum> findTopThreeExtremes(const QVector<double>& data, const QVector<double>& frequencies);


    // void findExtremes(const QVector<double>& data, double& maxValue, double& minValue);
    // QVector<QPair<double, double>> findTopThreeExtremes(const QVector<double>& data, const QVector<double>& frequencies);

private:
    fftw_complex *in;
    fftw_complex *out;
    fftw_plan p;
    int N;
};

#endif // SIGNALPROCESSING_H
