#ifndef REALDATALOADER_H
#define REALDATALOADER_H
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QDataStream>

class RealDataLoader: public QObject
{
    Q_OBJECT
public:
    explicit RealDataLoader(QObject *parent = nullptr);
    RealDataLoader(QObject *parent, int index);

    void loadFile(int cardIndex, const QString &filename,  QMap<int, QVector<QVector<double>>> &mapXdata, QMap<int, QVector<QVector<double>>> &mapYdata, QDataStream::ByteOrder byteOrder = QDataStream::BigEndian);
    void loadThreeFile(int cardIndex, const QString &filename, QMap<int, QVector<QVector<double>>> &mapXdata, QMap<int, QVector<QVector<double>>> &mapYdata, QDataStream::ByteOrder byteOrder);
    uint32_t swapEndian(uint32_t value);

signals:
    void dataLoaded(int cardIndex, const QVector<QVector<double>> &xdata, const QVector<QVector<double>> &ydata,double xmax, double ymax);
    void loadingFinished(int cardIndex);

private:
    QMutex mutex;
    int m_index;// 线程编号
};

#endif // REALDATALOADER_H
