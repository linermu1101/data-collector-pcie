#ifndef SIGNALWINDOW_H
#define SIGNALWINDOW_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QCloseEvent>
#include "qcustomplot.h"
#include "SignalProcessing.h"

class SignalWindow : public QDialog
{
    Q_OBJECT

public:
    SignalWindow(QWidget *parent = nullptr);
    ~SignalWindow();

    // 更新数据的公共方法
    void updateSignalData(const QString &filePath, int regionIndex, int numRegions, int frequencyMode = 0);

signals:
    void windowClosed(); // 窗口关闭信号
    void sendExtremeIndices(int index1, int index2, int index3);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onCloseButtonClicked();

private:
    void processSignalData();
    void setupUI();
    void displayResults(const QVector<double>& aRegionData,
                        const QVector<double>& frequencies,
                        const QVector<double>& magnitudes,
                        const QVector<std::complex<double>>& fftOutput,
                        const QVector<Extremum>& topThreeMaxExtremes);

private:
    QVBoxLayout* mainLayout;
    QHBoxLayout* plotLayout;

    QLabel* titleLabel;
    QTextEdit* resultTextEdit;
    QCustomPlot* originalPlot;
    QCustomPlot* fftPlot;
    QPushButton* closeButton;
    QCPItemText* textLabel;

    // 当前文件路径
    QString m_currentFilePath;
    // 当前区域索引
    int m_regionIndex;
    // 当前区域数量
    int m_numRegions;
    int m_frequencyMode; // 0表示单频模式，1表示三频模式
};

#endif // SIGNALWINDOW_H
