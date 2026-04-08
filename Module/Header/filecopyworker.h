#pragma once

#include <QObject>
#include <QString>
#include <QFileInfoList>
#include <QMap>

class FileCopyWorker : public QObject
{
    Q_OBJECT

public:
    FileCopyWorker(const QString &gunNumberStr, const QString &dataPath,
                   const QString &infPath, const QString &datFolderPath,
                   const QString &infFolderPath,
                   const QString &phaseDiffFileName = "",
                   const QString &fineFileName = "",
                   int workMode = 0);

public slots:
    void process();
    
public:
    // 手工处理指定炮号的合并文件
    void processSpecificGunNumber(const QString &gunNumber);

signals:
    void progressUpdated(int percentage);
    void logMessage(const QString &message);
    void finished();
    void updateFFT(const QString &filePath);
    void updateFFT1(const QString &filePath);

private:
    bool copyLargeFile(const QString &sourceFilePath, const QString &targetFilePath);
    void postProcessMergedFiles(const QFileInfoList &datFiles, const QFileInfoList &infFiles, const QString &gunNumber = "");
    QMap<int, bool> loadCardStorageSettings();
    bool parseAndFilterINFFile(const QString &sourceInfPath, const QString &targetInfPath, const QMap<int, bool> &cardSettings);
    bool parseAndFilterDATFile(const QString &sourceDatPath, const QString &targetDatPath, const QString &infPath, const QMap<int, bool> &cardSettings);

    QString m_gunNumberStr;
    QString m_dataPath;
    QString m_infPath;
    QString m_datFolderPath;
    QString m_infFolderPath;
    QString m_phaseDiffFileName; // 实时数据文件名
    QString m_fineFileName;      // 精细数据文件名
    int m_workMode;
};
