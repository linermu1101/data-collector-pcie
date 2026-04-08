#include "signalwindow.h"
#include <QMessageBox>
#include <algorithm>

SignalWindow::SignalWindow(QWidget *parent)
    : QDialog(parent)
    , m_currentFilePath("") // 默认文件路径
    , m_regionIndex(1)
    , m_numRegions(2)
    , m_frequencyMode(0) // 默认设置为单频模式
{
    setupUI();

}

SignalWindow::~SignalWindow()
{
    // 清理操作
}

void SignalWindow::setupUI()
{
    // 设置窗口属性
    this->setWindowTitle("信号处理窗口");
    this->resize(800, 600);

    // 创建布局
    mainLayout = new QVBoxLayout(this);
    plotLayout = new QHBoxLayout();

    // 创建控件
    titleLabel = new QLabel("信号处理结果");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16pt; font-weight: bold;");

    resultTextEdit = new QTextEdit();
    resultTextEdit->setReadOnly(true);

    originalPlot = new QCustomPlot();
    originalPlot->setMinimumSize(380, 250);

    fftPlot = new QCustomPlot();
    fftPlot->setMinimumSize(380, 250);

    textLabel = new QCPItemText(fftPlot);

    closeButton = new QPushButton("关闭");

    // 添加到布局
    mainLayout->addWidget(titleLabel);

    plotLayout->addWidget(originalPlot);
    plotLayout->addWidget(fftPlot);
    mainLayout->addLayout(plotLayout);

    mainLayout->addWidget(resultTextEdit);
    mainLayout->addWidget(closeButton);

    // 连接信号和槽
    connect(closeButton, &QPushButton::clicked, this, &SignalWindow::onCloseButtonClicked);

    // 设置交互
    originalPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    fftPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void SignalWindow::onCloseButtonClicked()
{
    // 发送关闭信号
    emit windowClosed();

    // 关闭窗口
    this->close();
}

void SignalWindow::closeEvent(QCloseEvent *event)
{
    // 发送窗口关闭信号
    emit windowClosed();

    // 接受关闭事件
    event->accept();
}

void SignalWindow::updateSignalData(const QString &filePath, int regionIndex, int numRegions, int frequencyMode)
{

    // 更新数据源参数
    if (!filePath.isEmpty()) {
        m_currentFilePath = filePath;
    }

    if (regionIndex > 0) {
        m_regionIndex = regionIndex;
    }

    if (numRegions > 0) {
        m_numRegions = numRegions;
    }

    // 更新频率模式（0-单频，1-三频）
    m_frequencyMode = frequencyMode;

    // 清空之前的图表
    originalPlot->clearGraphs();
    fftPlot->clearGraphs();

    // 更新窗口标题，显示当前文件
    QFileInfo fileInfo(m_currentFilePath);
    setWindowTitle(QString("信号处理窗口 - %1").arg(fileInfo.fileName()));

    // 重新处理信号数据
    processSignalData();

    // 通知用户数据已更新
    titleLabel->setText(QString("信号处理结果 - %1 - %2")
                            .arg(fileInfo.fileName())
                            .arg(m_frequencyMode == 0 ? "单频模式" : "三频模式"));
}

void SignalWindow::processSignalData()
{
    // 参数准备
    int N = 256; // 样本点数
    double fs = 12.5e6; // 12.5 MHz

    try {
        // 解析DDR数据
        SignalProcessing signalProcessing;
        QVector<double> aRegionData = signalProcessing.readRegionData(m_currentFilePath, m_regionIndex, m_numRegions);

        // FFT变换
        QVector<double> frequencies;
        QVector<double> magnitudes;
        signalProcessing.fftTransform(N, fs, aRegionData, frequencies, magnitudes);

        // 获取FFT变换后的输出数据
        QVector<std::complex<double>> fftOutput = signalProcessing.getFFTOutput();

        // 根据频率模式进行处理
        QVector<Extremum> topThreeMaxExtremes;
        if (m_frequencyMode == 0) {
            // 单频模式 - 只找最大值
            Extremum maxExtreme = signalProcessing.findMaxExtreme(magnitudes, frequencies);
            topThreeMaxExtremes.append(maxExtreme);

            // 发送单频极值索引（发送3个相同的索引）
            emit sendExtremeIndices(maxExtreme.index, maxExtreme.index, maxExtreme.index);
        } else {
            // 三频模式 - 找三个最大的极大值
            topThreeMaxExtremes = signalProcessing.findTopThreeExtremes(magnitudes, frequencies);

            if(topThreeMaxExtremes.length()>=3)
            {
                // 发送三频极值索引
                emit sendExtremeIndices(
                    topThreeMaxExtremes[0].index,
                    topThreeMaxExtremes[1].index,
                    topThreeMaxExtremes[2].index
                    );
            }
        }

        // 去掉前两个点（确保有足够的数据点）
        if (aRegionData.size() > 2) {
            aRegionData.remove(0, 2);
        }
        if (frequencies.size() > 2) {
            frequencies.remove(0, 2);
        }
        if (magnitudes.size() > 2) {
            magnitudes.remove(0, 2);
        }
        if (fftOutput.size() > 2) {
            fftOutput.remove(0, 2);
        }

        // 调整极值点的索引（因为我们去掉了前两个点）
        for (Extremum& extremum : topThreeMaxExtremes) {
            if (extremum.index >= 2) {
                extremum.index -= 2;
            } else {
                // 如果索引小于2，可能需要特殊处理或设置为0
                extremum.index = 0;
            }
        }

        // 显示结果
        displayResults(aRegionData, frequencies, magnitudes, fftOutput, topThreeMaxExtremes);
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", QString("处理信号数据时发生错误: %1").arg(e.what()));
    }
}

void SignalWindow::displayResults(const QVector<double>& aRegionData,
                                  const QVector<double>& frequencies,
                                  const QVector<double>& magnitudes,
                                  const QVector<std::complex<double>>& fftOutput,
                                  const QVector<Extremum>& topThreeMaxExtremes)
{
    int N = aRegionData.size();

    // 显示原始数据文本
    QString inputResult;
    for (int i = 0; i < N; i++) {
        inputResult += QString("输入索引 %1: 实部 = %2\n")
                           .arg(i)
                           .arg(aRegionData[i]);
    }
    resultTextEdit->setText("原始数据：\n" + inputResult);

    // 显示原始信号图表
    QVector<double> sampleIndices(N);
    for (int i = 0; i < N; i++) {
        sampleIndices[i] = i;
    }

    // 原始数据图表
    originalPlot->addGraph();
    originalPlot->graph(0)->setData(sampleIndices, aRegionData);
    originalPlot->xAxis->setLabel("序号");
    originalPlot->yAxis->setLabel("信号值");
    originalPlot->xAxis->setRange(0, N - 1);
    originalPlot->yAxis->setRange(*std::min_element(aRegionData.begin(), aRegionData.end()),
                                  *std::max_element(aRegionData.begin(), aRegionData.end()));
    originalPlot->replot();

    // 显示频谱数据文本
    QString fftResult;
    for (int i = 0; i < N; i++) {
        fftResult += QString("频率索引 %1: 实部 = %2, 虚部 = %3\n")
                         .arg(i)
                         .arg(fftOutput[i].real())
                         .arg(fftOutput[i].imag());
    }
    resultTextEdit->append("\n转换后的频谱数据：\n" + fftResult);

    // 频谱图表
    fftPlot->addGraph();
    fftPlot->graph(0)->setData(frequencies, magnitudes);
    fftPlot->graph(0)->setPen(QPen(Qt::blue));
    fftPlot->graph(0)->setLineStyle(QCPGraph::lsLine);
    fftPlot->graph(0)->setScatterStyle(QCPScatterStyle::ssNone);

    fftPlot->xAxis->setLabel("频率 (Hz)");
    fftPlot->yAxis->setLabel("幅值");
    double fs = 12.5e6; // 12.5 MHz
    fftPlot->xAxis->setRange(0, fs / 2);
    fftPlot->yAxis->setRange(0, *std::max_element(magnitudes.begin(), magnitudes.end()) * 1.1);

    // 显示三个最大的极大值文本
    QString extremesResult;
    for (int i = 0; i < topThreeMaxExtremes.size(); i++) {
        extremesResult += QString("极大值 %1: 频率 = %2 Hz, 幅值 = %3\n")
                              .arg(i + 1)
                              .arg(topThreeMaxExtremes[i].frequency)
                              .arg(topThreeMaxExtremes[i].magnitude);
    }
    resultTextEdit->append("\n变换后的频谱图中的三个最大的极大值及其对应的频率：\n" + extremesResult);

    // 标注三个最大的极大值
    fftPlot->addGraph();
    fftPlot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 15));
    fftPlot->graph(1)->setPen(QPen(Qt::red));
    fftPlot->graph(1)->setLineStyle(QCPGraph::lsNone);
    fftPlot->graph(1)->setBrush(QBrush(Qt::red));

    // 根据实际的极大值数量进行绘制
    int extremeCount = topThreeMaxExtremes.size();
    for (int i = 0; i < extremeCount; i++) {
        double frequency = topThreeMaxExtremes[i].frequency;
        double magnitude = topThreeMaxExtremes[i].magnitude;
        int nindex = topThreeMaxExtremes[i].index;

        // 添加红色散点
        fftPlot->graph(1)->addData(frequency, magnitude);

        // 添加标签
        
        textLabel->setPositionAlignment(Qt::AlignTop);
        textLabel->position->setTypeX(QCPItemPosition::ptPlotCoords);
        textLabel->position->setTypeY(QCPItemPosition::ptPlotCoords);
        textLabel->position->setCoords(frequency, magnitude);
        textLabel->setText(QString("极大值 %1\n频率 = %2 Hz\n幅值 = %3 \n序列号 = %4")
                               .arg(i + 1)
                               .arg(frequency)
                               .arg(magnitude)
                               .arg(nindex));
        textLabel->setFont(QFont(font().family(), 8));
        textLabel->setColor(Qt::red);
    }

    fftPlot->replot();
}
