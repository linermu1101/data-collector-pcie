#include "signalvaluewindow.h"
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>
#include <QLabel>
#include <algorithm>

SignalValueWindow::SignalValueWindow(const QList<PlanarGraph*>& plotList, QWidget *parent)
    : QWidget(parent),
      m_regionIndex(1),
      m_numRegions(2),
      m_frequencyMode(0)
{
    // 设置窗口属性
    setWindowTitle("信号值显示");
    setWindowFlags(windowFlags() | Qt::Window);
    resize(1200, 800);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // 创建选项卡控件
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);

    // 为每个plotList项创建对应的选项卡
    for (int i = 0; i < plotList.size(); ++i) {
        QWidget *tabWidget = new QWidget();
        QVBoxLayout *tabLayout = new QVBoxLayout(tabWidget);

        if (plotList[i] != nullptr) {
            // 创建信号图表
            PlanarGraph* signalPlot = new PlanarGraph(QString("卡%1信号值").arg(i + 1), tabWidget);

            // 初始化图表
            QCustomPlot* customPlot = signalPlot->getCustomPlot();
            if (customPlot) {
                customPlot->setBackground(Qt::white);
                customPlot->xAxis->setLabel("序号");
                customPlot->yAxis->setLabel("信号值");
                customPlot->xAxis->setRange(0, 255);
                customPlot->yAxis->setRange(0, 100);

                customPlot->addGraph();
                customPlot->graph(0)->setPen(QPen(Qt::blue));
                customPlot->graph(0)->setName("原始信号");
                customPlot->replot();
            }

            tabLayout->addWidget(signalPlot);
            m_signalPlotList.append(signalPlot);
            customPlot->setVisible(false);
        } else {
            // 空的plotList项，显示提示
            QLabel *label = new QLabel(QString("卡%1未启用或不存在").arg(i + 1), tabWidget);
            label->setAlignment(Qt::AlignCenter);
            tabLayout->addWidget(label);
            m_signalPlotList.append(nullptr);
        }

        tabWidget->setLayout(tabLayout);
        m_tabWidget->addTab(tabWidget, QString("卡%1").arg(i + 1));
    }

    qDebug() << "信号值窗口创建完成，共" << m_signalPlotList.size() << "个选项卡";
}

SignalValueWindow::~SignalValueWindow()
{
    for (PlanarGraph* plot : m_signalPlotList) {
        if (plot) plot->deleteLater();
    }
    m_signalPlotList.clear();
}

void SignalValueWindow::updateSignalData(int plotIndex, const QString &filePath,
                                       int regionIndex, int numRegions, int frequencyMode)
{
    // 检查索引有效性
    if (plotIndex < 0 || plotIndex >= m_signalPlotList.size()) {
        qWarning() << "无效的图表索引:" << plotIndex;
        return;
    }

    if (m_signalPlotList[plotIndex] == nullptr) {
        qWarning() << "索引" << plotIndex << "的图表为空，无法更新数据";
        return;
    }

    // 更新参数
    m_currentFilePath = filePath;
    m_regionIndex = regionIndex;
    m_numRegions = numRegions;
    m_frequencyMode = frequencyMode;

    // 检查文件是否存在
    if (!QFile::exists(m_currentFilePath)) {
        qWarning() << "文件不存在:" << m_currentFilePath;
        QMessageBox::warning(this, "警告", QString("文件不存在: %1").arg(m_currentFilePath));
        return;
    }

    // 更新窗口标题
    QFileInfo fileInfo(m_currentFilePath);
    setWindowTitle(QString("信号处理窗口"));

    // 处理信号数据
    processRefSignalData(plotIndex);
    processMesSignalData(plotIndex);


    // 切换到对应的选项卡
    if (plotIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(plotIndex);
    }
}

void SignalValueWindow::processRefSignalData(int plotIndex)
{
    qDebug() << "处理卡" << plotIndex + 1 << "的信号数据，文件:" << m_currentFilePath;

    try {
        // 参数准备
        int N = 256;
        double fs = 12.5e6;

        // 解析DDR数据
        SignalProcessing signalProcessing;
         //Failed to read region 2 20001 data from file(是因为20000*128=2560000个点，到这就是文件的总大小了256000*128太大了，后面超出文件大小了)（以200ms为例）
        QVector<double> aRegionData_ref = signalProcessing.readRegionData(m_currentFilePath, 1, 2);//256000(要显示所有点的话换回m_numRegions，所有时间均适用，不用200000了)
      //  qDebug() << aRegionData_ref;
        if (aRegionData_ref.isEmpty()) {
            qWarning() << "读取的数据为空";
            return;
        }

        // FFT变换
        QVector<double> frequencies;
        QVector<double> magnitudes;
        signalProcessing.fftTransform(N, fs, aRegionData_ref, frequencies, magnitudes);

        // 获取FFT输出
        QVector<std::complex<double>> fftOutput = signalProcessing.getFFTOutput();

        // 处理极值点
        QVector<Extremum> topThreeMaxExtremes;
        if (m_frequencyMode == 0) {
            Extremum maxExtreme = signalProcessing.findMaxExtreme(magnitudes, frequencies);
            topThreeMaxExtremes.append(maxExtreme);
            emit sendExtremeIndices(plotIndex, maxExtreme.index, maxExtreme.index, maxExtreme.index);
        } else {
            topThreeMaxExtremes = signalProcessing.findTopThreeExtremes(magnitudes, frequencies);
            if (topThreeMaxExtremes.size() >= 3) {
                emit sendExtremeIndices(plotIndex,
                    topThreeMaxExtremes[0].index,
                    topThreeMaxExtremes[1].index,
                    topThreeMaxExtremes[2].index);
            }
        }

        // 去掉前两个点
        if (aRegionData_ref.size() > 2) aRegionData_ref.remove(0, 2);
        if (frequencies.size() > 2) frequencies.remove(0, 2);
        if (magnitudes.size() > 2) magnitudes.remove(0, 2);
        if (fftOutput.size() > 2) fftOutput.remove(0, 2);

        // 调整极值点索引
        for (Extremum& extremum : topThreeMaxExtremes) {
            if (extremum.index >= 2) extremum.index -= 2;
            else extremum.index = 0;
        }

        // 显示结果
         displayRefResults(plotIndex, aRegionData_ref, frequencies, magnitudes, fftOutput, topThreeMaxExtremes);
    } catch (const std::exception& e) {
        qCritical() << "处理信号数据错误:" << e.what();
        QMessageBox::critical(this, "错误", QString("处理信号数据时发生错误: %1").arg(e.what()));
    }
}
void SignalValueWindow::processMesSignalData(int plotIndex)
{
    qDebug() << "处理卡" << plotIndex + 1 << "的信号数据，文件:" << m_currentFilePath;

    try {
        // 参数准备
        int N = 256;
        double fs = 12.5e6;

        // 解析DDR数据
        SignalProcessing signalProcessing;
         //Failed to read region 2 20001 data from file(是因为20000*128=2560000个点，到这就是文件的总大小了256000*128太大了，后面超出文件大小了)（以200ms为例）
        QVector<double> aRegionData_mes = signalProcessing.readRegionData(m_currentFilePath, 2, 2);//256000(要显示所有点的话换回m_numRegions)
      //  qDebug() << aRegionData_mes;
        if (aRegionData_mes.isEmpty()) {
            qWarning() << "读取的数据为空";
            return;
        }

        // FFT变换
        QVector<double> frequencies;
        QVector<double> magnitudes;
        signalProcessing.fftTransform(N, fs, aRegionData_mes, frequencies, magnitudes);

        // 获取FFT输出
        QVector<std::complex<double>> fftOutput = signalProcessing.getFFTOutput();

        // 处理极值点
        QVector<Extremum> topThreeMaxExtremes;
        if (m_frequencyMode == 0) {
            Extremum maxExtreme = signalProcessing.findMaxExtreme(magnitudes, frequencies);
            topThreeMaxExtremes.append(maxExtreme);
            emit sendExtremeIndices(plotIndex, maxExtreme.index, maxExtreme.index, maxExtreme.index);
        } else {
            topThreeMaxExtremes = signalProcessing.findTopThreeExtremes(magnitudes, frequencies);
            if (topThreeMaxExtremes.size() >= 3) {
                emit sendExtremeIndices(plotIndex,
                    topThreeMaxExtremes[0].index,
                    topThreeMaxExtremes[1].index,
                    topThreeMaxExtremes[2].index);
            }
        }

        // 去掉前两个点
        if (aRegionData_mes.size() > 2) aRegionData_mes.remove(0, 2);
        if (frequencies.size() > 2) frequencies.remove(0, 2);
        if (magnitudes.size() > 2) magnitudes.remove(0, 2);
        if (fftOutput.size() > 2) fftOutput.remove(0, 2);

        // 调整极值点索引
        for (Extremum& extremum : topThreeMaxExtremes) {
            if (extremum.index >= 2) extremum.index -= 2;
            else extremum.index = 0;
        }

        // 显示结果
        displayMesResults(plotIndex, aRegionData_mes, frequencies, magnitudes, fftOutput, topThreeMaxExtremes);

    } catch (const std::exception& e) {
        qCritical() << "处理信号数据错误:" << e.what();
        QMessageBox::critical(this, "错误", QString("处理信号数据时发生错误: %1").arg(e.what()));
    }
}

void SignalValueWindow::displayResults(int plotIndex,
                                     const QVector<double>& aRegionData,
                                     const QVector<double>& frequencies,
                                     const QVector<double>& magnitudes,
                                     const QVector<std::complex<double>>& fftOutput,
                                     const QVector<Extremum>& topThreeMaxExtremes)
{
    if (aRegionData.isEmpty()) return;

    PlanarGraph* plot = m_signalPlotList[plotIndex];
    if (!plot) return;

    QCustomPlot* customPlot = plot->getCustomPlot();
    if (!customPlot) return;

    // 准备X轴数据
    QVector<double> sampleIndices(aRegionData.size());
    for (int i = 0; i < aRegionData.size(); i++) {
        sampleIndices[i] = i;
    }

    // 设置曲线数据
    if (customPlot->graphCount() == 0) {
        customPlot->addGraph();
        customPlot->graph(0)->setPen(QPen(Qt::blue));
        customPlot->graph(0)->setName("原始信号");
    }

    customPlot->graph(0)->setData(sampleIndices, aRegionData);

    // 设置坐标轴
    customPlot->xAxis->setLabel("序号");
    customPlot->yAxis->setLabel("信号值");
    customPlot->xAxis->setRange(0, aRegionData.size() - 1);

    // 计算Y轴范围
    double minVal = *std::min_element(aRegionData.begin(), aRegionData.end());
    double maxVal = *std::max_element(aRegionData.begin(), aRegionData.end());
    double margin = (maxVal - minVal) * 0.1;
    if (margin == 0) margin = 1.0;

    customPlot->yAxis->setRange(minVal - margin, maxVal + margin);
    customPlot->replot();

    qDebug() << "卡" << plotIndex + 1 << "数据显示完成，数据点数:" << aRegionData.size();
}

void SignalValueWindow::displayRefResults(int plotIndex,
                                          const QVector<double>& aRegionData,
                                          const QVector<double>& frequencies,
                                          const QVector<double>& magnitudes,
                                          const QVector<std::complex<double>>& fftOutput,
                                          const QVector<Extremum>& topThreeMaxExtremes)
{
    if (aRegionData.isEmpty()) return;
    PlanarGraph* pg = m_signalPlotList[plotIndex];
    if (!pg) return;

    // --- 时域 ---
    QCustomPlot* refPlot = pg->getRefTimePlot();
    refPlot->setVisible(true);
    if (refPlot) {
        refPlot->clearGraphs();
        QVector<double> x(aRegionData.size());
        for (int i=0;i<aRegionData.size();++i) x[i]=i;

        refPlot->addGraph();
        refPlot->graph(0)->setData(x, aRegionData);
        refPlot->graph(0)->setPen(QPen(Qt::blue));
        refPlot->graph(0)->setName("参考-时域");
        refPlot->xAxis->setLabel("序号");
        refPlot->yAxis->setLabel("参考信号时域信号值(mv)");
        refPlot->xAxis->setRange(0, aRegionData.size() - 1);
        refPlot->yAxis->setRange(*std::min_element(aRegionData.begin(), aRegionData.end()),
                                 *std::max_element(aRegionData.begin(), aRegionData.end()));
        refPlot->replot();
    }

    // --- 频谱 ---
    QCustomPlot* refFftPlot = pg->getRefFftPlot();
    refFftPlot->setVisible(true);
    if (refFftPlot) {
        refFftPlot->clearGraphs();
        refFftPlot->clearItems();

        // 转换为 dB，横轴 MHz
        QVector<double> magnitudesDb(magnitudes.size());
        QVector<double> freqMHz(frequencies.size());
        for (int i=0;i<magnitudes.size();++i) {
            magnitudesDb[i] = 20 * log10(magnitudes[i] + 1e-12);
            freqMHz[i] = frequencies[i] / 1e6;
        }

        refFftPlot->addGraph();
        refFftPlot->graph(0)->setData(freqMHz, magnitudesDb);
        refFftPlot->graph(0)->setPen(QPen(Qt::red));
        refFftPlot->graph(0)->setName("参考-频谱");
        refFftPlot->xAxis->setLabel("参考信号频率 (MHz)");
        refFftPlot->yAxis->setLabel("参考信号幅值 (dB)");
        double fs = 12.5e6;
        refFftPlot->xAxis->setRange(0, fs/2/1e6);
        double maxDb = *std::max_element(magnitudesDb.begin(), magnitudesDb.end());
        refFftPlot->yAxis->setRange(maxDb - 60, maxDb + 5);

        // 标注极大值
        if (!topThreeMaxExtremes.isEmpty()) {
            refFftPlot->addGraph();
            refFftPlot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 8));
            refFftPlot->graph(1)->setPen(QPen(Qt::green));
            refFftPlot->graph(1)->setLineStyle(QCPGraph::lsNone);

            for (int i=0;i<topThreeMaxExtremes.size();++i) {
                double freqMHzVal = topThreeMaxExtremes[i].frequency / 1e6;
                double magDb = 20 * log10(topThreeMaxExtremes[i].magnitude + 1e-12);

                refFftPlot->graph(1)->addData(freqMHzVal, magDb);

                // 添加标签
                QCPItemText* textLabel = new QCPItemText(refFftPlot);
                textLabel->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
                textLabel->position->setType(QCPItemPosition::ptPlotCoords);
                textLabel->position->setCoords(freqMHzVal, magDb);
                textLabel->setText(QString("极大值 %1\n%2 MHz\n%3 dB")
                                   .arg(i+1)
                                   .arg(freqMHzVal, 0, 'f', 2)
                                   .arg(magDb, 0, 'f', 2));
                textLabel->setFont(QFont("Arial", 8));
                textLabel->setColor(Qt::darkGreen);
            }
        }

        refFftPlot->replot();
    }
}

void SignalValueWindow::displayMesResults(int plotIndex,
                                          const QVector<double>& aRegionData,
                                          const QVector<double>& frequencies,
                                          const QVector<double>& magnitudes,
                                          const QVector<std::complex<double>>& fftOutput,
                                          const QVector<Extremum>& topThreeMaxExtremes)
{
    if (aRegionData.isEmpty()) return;
    PlanarGraph* pg = m_signalPlotList[plotIndex];
    if (!pg) return;

    // --- 时域 ---
    QCustomPlot* mesPlot = pg->getMesTimePlot();
    mesPlot->setVisible(true);
    if (mesPlot) {
        mesPlot->clearGraphs();
        QVector<double> x(aRegionData.size());
        for (int i=0;i<aRegionData.size();++i) x[i]=i;

        mesPlot->addGraph();
        mesPlot->graph(0)->setData(x, aRegionData);
        mesPlot->graph(0)->setPen(QPen(Qt::blue));
        mesPlot->graph(0)->setName("测量-时域");
        mesPlot->xAxis->setLabel("序号");
        mesPlot->yAxis->setLabel("测量信号时域信号值(mv)");
        mesPlot->xAxis->setRange(0, aRegionData.size() - 1);
        mesPlot->yAxis->setRange(*std::min_element(aRegionData.begin(), aRegionData.end()),
                                 *std::max_element(aRegionData.begin(), aRegionData.end()));
        mesPlot->replot();
    }

    // --- 频谱 ---
    QCustomPlot* mesFftPlot = pg->getMesFftPlot();
    mesFftPlot->setVisible(true);
    if (mesFftPlot) {
        mesFftPlot->clearGraphs();
        mesFftPlot->clearItems();

        // 转换为 dB，横轴 MHz
        QVector<double> magnitudesDb(magnitudes.size());
        QVector<double> freqMHz(frequencies.size());
        for (int i=0;i<magnitudes.size();++i) {
            magnitudesDb[i] = 20 * log10(magnitudes[i] + 1e-12);
            freqMHz[i] = frequencies[i] / 1e6;
        }

        mesFftPlot->addGraph();
        mesFftPlot->graph(0)->setData(freqMHz, magnitudesDb);
        mesFftPlot->graph(0)->setPen(QPen(Qt::red));
        mesFftPlot->graph(0)->setName("测量-频谱");
        mesFftPlot->xAxis->setLabel("测量信号频率 (MHz)");
        mesFftPlot->yAxis->setLabel("测量信号幅值 (dB)");
        double fs = 12.5e6;
        mesFftPlot->xAxis->setRange(0, fs/2/1e6);
        double maxDb = *std::max_element(magnitudesDb.begin(), magnitudesDb.end());
        mesFftPlot->yAxis->setRange(maxDb - 60, maxDb + 5);

        // 标注极大值
        if (!topThreeMaxExtremes.isEmpty()) {
            mesFftPlot->addGraph();
            mesFftPlot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 8));
            mesFftPlot->graph(1)->setPen(QPen(Qt::green));
            mesFftPlot->graph(1)->setLineStyle(QCPGraph::lsNone);

            for (int i=0;i<topThreeMaxExtremes.size();++i) {
                double freqMHzVal = topThreeMaxExtremes[i].frequency / 1e6;
                double magDb = 20 * log10(topThreeMaxExtremes[i].magnitude + 1e-12);

                mesFftPlot->graph(1)->addData(freqMHzVal, magDb);

                QCPItemText* textLabel = new QCPItemText(mesFftPlot);
                textLabel->setPositionAlignment(Qt::AlignTop|Qt::AlignHCenter);
                textLabel->position->setType(QCPItemPosition::ptPlotCoords);
                textLabel->position->setCoords(freqMHzVal, magDb);
                textLabel->setText(QString("极大值 %1\n%2 MHz\n%3 dB")
                                   .arg(i+1)
                                   .arg(freqMHzVal, 0, 'f', 2)
                                   .arg(magDb, 0, 'f', 2));
                textLabel->setFont(QFont("Arial", 8));
                textLabel->setColor(Qt::darkGreen);
            }
        }

        mesFftPlot->replot();
    }
}



void SignalValueWindow::closeEvent(QCloseEvent *event)
{
    emit windowClosed();
    event->accept();
}
