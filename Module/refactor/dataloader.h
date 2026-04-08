#ifndef DATALOADER_H
#define DATALOADER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QDataStream>

class DataLoader : public QObject
{
    Q_OBJECT
public:
    explicit DataLoader(QObject *parent = nullptr);
    DataLoader(QObject *parent, int index);

    void loadFile(int channel, const QString &filename, QVector<double> &xdata, QVector<double> &ydata, QDataStream::ByteOrder byteOrder = QDataStream::BigEndian);

signals:
    void dataLoaded(int channel, const QVector<double> &xdata, const QVector<double> &ydata,double xmax, double ymax);
    void loadingFinished();

private:
    QMutex mutex;
    int m_index;// 线程编号
};

#endif // DATALOADER_H
