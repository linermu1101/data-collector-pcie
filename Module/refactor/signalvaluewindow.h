#ifndef SIGNALVALUEWINDOW_H
#define SIGNALVALUEWINDOW_H

#include <QWidget>
#include <QTabWidget>
#include <QList>
#include <QString>
#include "planargraph.h"
#include "SignalProcessing.h"

class SignalValueWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SignalValueWindow(const QList<PlanarGraph*>& plotList, QWidget *parent = nullptr);
    ~SignalValueWindow();

    // 更新信号数据 - 使用plotList的索引
    void updateSignalData(int plotIndex, const QString &filePath, int regionIndex, int numRegions, int frequencyMode = 0);

signals:
    void windowClosed();
    void sendExtremeIndices(int plotIndex, int index1, int index2, int index3);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void processRefSignalData(int plotIndex);
    void processMesSignalData(int plotIndex);
    void displayResults(int plotIndex,
                       const QVector<double>& aRegionData,
                       const QVector<double>& frequencies,
                       const QVector<double>& magnitudes,
                       const QVector<std::complex<double>>& fftOutput,
                       const QVector<Extremum>& topThreeMaxExtremes);
    void displayRefResults(int plotIndex,
                                         const QVector<double>& aRegionData,
                                         const QVector<double>& frequencies,
                                         const QVector<double>& magnitudes,
                                         const QVector<std::complex<double>>& /*fftOutput*/,
                                         const QVector<Extremum>& /*topThreeMaxExtremes*/);
    void displayMesResults(int plotIndex,
                                         const QVector<double>& aRegionData,
                                         const QVector<double>& frequencies,
                                         const QVector<double>& magnitudes,
                                         const QVector<std::complex<double>>& /*fftOutput*/,
                                         const QVector<Extremum>& /*topThreeMaxExtremes*/);

    QTabWidget *m_tabWidget;
    QList<PlanarGraph*> m_signalPlotList;  // 信号图表列表


    // 当前处理参数
    QString m_currentFilePath;
    int m_regionIndex;
    int m_numRegions;
    int m_frequencyMode;
};

#endif // SIGNALVALUEWINDOW_H
