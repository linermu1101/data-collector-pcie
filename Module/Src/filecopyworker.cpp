/**
 * @file filecopyworker.cpp
 * @author z
 * @brief 文件分批存储
 * @version 0.1
 * @date 2025-04-09
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "filecopyworker.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QRegularExpression>
#include <algorithm>
#include <QtMath>

FileCopyWorker::FileCopyWorker(const QString &gunNumberStr, const QString &dataPath,
                              const QString &infPath, const QString &datFolderPath,
                              const QString &infFolderPath,
                              const QString &phaseDiffFileName,
                              const QString &fineFileName,
                              int workMode)
    : m_gunNumberStr(gunNumberStr)
    , m_dataPath(dataPath)
    , m_infPath(infPath)
    , m_datFolderPath(datFolderPath)
    , m_infFolderPath(infFolderPath)
    , m_phaseDiffFileName(phaseDiffFileName)
    , m_fineFileName(fineFileName)
    , m_workMode(workMode)
{
}

void FileCopyWorker::process()
{
    try {
        int count = 0;

        // 预先计算总文件数（初始化）
        QDir dataDir(m_dataPath);
        QDir infDir(m_infPath);
        QFileInfoList originalDatFiles = dataDir.entryInfoList(QStringList(m_gunNumberStr + "*.DAT"), QDir::Files);
        QFileInfoList infFiles = infDir.entryInfoList(QStringList(m_gunNumberStr + "*.INF"), QDir::Files);
        
        // 创建需要传输的文件列表
        QFileInfoList datFilesToTransfer;

        // 仅去解析主卡DDR数据
        QSettings settings(QDir::currentPath()+"/config.ini",QSettings::IniFormat);
        settings.beginGroup("BASIC_PARA");
        int hardTrigCard = settings.value("HardTrigCard").toInt();     
        settings.endGroup();

        // 合并REALTIME_DATA文件
        QStringList realtimeDatFiles;
        for (const QFileInfo &fileInfo : originalDatFiles) {
            if (fileInfo.fileName().contains("REALTIME_DATA")) {
                realtimeDatFiles.append(fileInfo.absoluteFilePath());
            }
        }

        // 如果有实时数据文件，则合并它们
        if (!realtimeDatFiles.isEmpty()) {
            // 使用phaseDiffFileName，如果为空则使用默认名
            QString filename = m_phaseDiffFileName.isEmpty() ?
                "REALTIME_DATA_MERGED" : m_phaseDiffFileName;
            QString mergedRealTimePath = QDir(m_dataPath).filePath(m_gunNumberStr + filename + ".DAT");

            try {
                emit logMessage("开始合并实时数据文件...");

                // 读取所有文件内容
                QByteArray allData;
                for (const QString &filePath : realtimeDatFiles) {
                    QFile sourceFile(filePath);
                    if (sourceFile.open(QIODevice::ReadOnly)) {
                        allData.append(sourceFile.readAll());
                        sourceFile.close();
                        emit logMessage("添加文件: " + filePath);
                    } else {
                        emit logMessage("无法打开文件: " + filePath);
                    }
                }

                // workMode=0时转换所有4字节数据为float
                if (m_workMode == 0) {
                    QByteArray convertedData;
                    for (int i = 0; i + 4 <= allData.size(); i += 4) {
                        // 读取32位有符号整数并转换为float
                        int32_t rawValue = *reinterpret_cast<const int32_t*>(allData.constData() + i);
                        float floatValue = (float)(
                            (double)rawValue / 8192.0 * 180.0 / M_PI
                            );
                        convertedData.append(reinterpret_cast<const char*>(&floatValue), sizeof(float));
                    }
                    allData = convertedData;
                    emit logMessage("workMode=0，实时数据已转换为float格式");
                }

                // workMode=1时重组数据
                if (m_workMode == 1) {
                    QByteArray ch1, ch2, ch3;
                    for (int i = 0; i + 8 <= allData.size(); i += 8) {
                        // ch1.append(allData.mid(i, 4));   // 32位通道1
                        // ch2.append(allData.mid(i+4, 2)); // 16位通道2
                        // ch3.append(allData.mid(i+6, 2)); // 16位通道3

                        // 读取32位有符号整数并转换为float
                        int32_t rawValue1 = *reinterpret_cast<const int32_t*>(allData.constData() + i);
                        float floatValue1 =  (float)(
                            (double)rawValue1 / 8192.0 * 180.0 / M_PI
                            );
                        ch1.append(reinterpret_cast<const char*>(&floatValue1), sizeof(float));

                        // 读取16位有符号整数并转换为float
                        int16_t rawValue2 = *reinterpret_cast<const int16_t*>(allData.constData() + i + 4);
                        float floatValue2 = (float)(
                            (double)rawValue2 / 8192.0 * 180.0 / M_PI
                            );
                        ch2.append(reinterpret_cast<const char*>(&floatValue2), sizeof(float));

                        // 读取16位有符号整数并转换为float
                        int16_t rawValue3 = *reinterpret_cast<const int16_t*>(allData.constData() + i + 6);
                        float floatValue3 = (float)(
                            (double)rawValue3 / 8192.0 * 180.0 / M_PI
                            );
                        ch3.append(reinterpret_cast<const char*>(&floatValue3), sizeof(float));
                    }
                    allData = ch1 + ch2 + ch3;
                    emit logMessage(QString("workMode=1，实时数据已重组 %1").arg(allData.size()));
                }

                QFile mergedFile(mergedRealTimePath);
                if (mergedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    mergedFile.write(allData);
                    mergedFile.close();
                    emit logMessage("实时数据文件合并完成: " + mergedRealTimePath);
                    QFileInfo mergedFileInfo(mergedRealTimePath);
                    datFilesToTransfer.append(mergedFileInfo);
                } else {
                    emit logMessage("无法创建合并文件: " + mergedRealTimePath);
                }
            } catch (const std::exception &e) {
                emit logMessage(QString("合并实时数据时发生错误: %1").arg(e.what()));
            }
        }

        

        // 合并DDR3_DATA文件
        QStringList ddrDatFiles;
        for (const QFileInfo &fileInfo : originalDatFiles) {
            if (fileInfo.fileName().contains("DDR3_DATA")) {
                ddrDatFiles.append(fileInfo.absoluteFilePath());
            }
        }

        // 如果有DDR3数据文件，则合并它们
        if (!ddrDatFiles.isEmpty()) {
            // 使用fineFileName，如果为空则使用默认名
            QString filename = m_fineFileName.isEmpty() ? 
                "DDR3_DATA_MERGED" : m_fineFileName;
            QString mergedDdrPath = QDir(m_dataPath).filePath(m_gunNumberStr + filename + ".DAT");
            
            // 重组DDR3数据：根据workMode决定数据组织方式
            try {
                // 根据workMode决定数据流数量
                int streamCount = (m_workMode == 0) ? 3 : 5;
                QString modeDesc = (m_workMode == 0) ? "单频模式，提取参考、测量、相位1" : "多频模式，提取参考、测量、相位1、相位2、相位3";
                emit logMessage(QString("开始重组DDR3数据（%1）...").arg(modeDesc));
                
                // 创建临时缓冲区存储数据
                QVector<QByteArray> dataStreams(streamCount);
                if (m_workMode == 0) {
                    // 单频模式：0-参考, 1-测量, 2-相位1
                } else {
                    // 多频模式：0-参考, 1-测量, 2-相位1, 3-相位2, 4-相位3
                }
                
                // 处理每个DDR3数据文件
                for (const QString &filePath : ddrDatFiles) {
                    QFile sourceFile(filePath);
                    if (!sourceFile.open(QIODevice::ReadOnly)) {
                        emit logMessage("无法打开文件: " + filePath);
                        continue;
                    }
                    
                    // 获取文件数据
                    QByteArray fileData = sourceFile.readAll();
                    sourceFile.close();
                    
                    // 每个数据块为256字节(128个2字节样本点)
                    const int blockSize = 256;
                    
                    if (m_workMode == 0) {
                        // 单频模式：文件仍然是5个数据流，但只提取前3个（参考、测量、相位1）
                        if (fileData.size() % (blockSize * 5) != 0) {
                            emit logMessage("警告: 文件 " + filePath + " 大小不是5个块的倍数，可能会导致数据错位");
                        }
                        
                        // 逐块提取，从每5个数据块中提取前3个
                        for (int offset = 0; offset < fileData.size(); offset += blockSize * 5) {
                            // 确保不会越界
                            if (offset + blockSize * 5 > fileData.size()) {
                                break;
                            }
                            
                            // 从5个数据块中只提取前3个（参考、测量、相位1）
                            for (int i = 0; i < 3; i++) {
                                int blockOffset = offset + i * blockSize;
                                dataStreams[i].append(fileData.mid(blockOffset, blockSize));
                            }
                        }
                    } else {
                        // 多频模式：确保文件大小是5个块的倍数
                        if (fileData.size() % (blockSize * 5) != 0) {
                            emit logMessage("警告: 文件 " + filePath + " 大小不是5个块的倍数，可能会导致数据错位");
                        }
                        
                        // 逐块提取5种类型的数据
                        for (int offset = 0; offset < fileData.size(); offset += blockSize * 5) {
                            // 确保不会越界
                            if (offset + blockSize * 5 > fileData.size()) {
                                break;
                            }
                            
                            // 提取5种类型的数据块
                            for (int i = 0; i < 5; i++) {
                                int blockOffset = offset + i * blockSize;
                                dataStreams[i].append(fileData.mid(blockOffset, blockSize));
                            }
                        }
                    }
                    
                    emit logMessage("已处理文件: " + filePath);
                }
                
                // 将重组后的数据写入合并文件
                QFile mergedFile(mergedDdrPath);
                if (mergedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    // 写入所有数据流
                    for (const QByteArray &dataStream : dataStreams) {
                        mergedFile.write(dataStream);
                    }
                    
                    mergedFile.close();
                    emit logMessage("DDR3数据重组完成，已保存到: " + mergedDdrPath);
                    
                    // 将合并后的文件添加到待复制列表
                    QFileInfo mergedFileInfo(mergedDdrPath);
                    datFilesToTransfer.append(mergedFileInfo);
                } else {
                    emit logMessage("无法创建合并文件: " + mergedDdrPath);
                }
            } catch (const std::exception &e) {
                emit logMessage(QString("重组DDR3数据时发生错误: %1").arg(e.what()));
            }
        }
        
        // 添加其他非REALTIME和非DDR3的文件（这些不需要合并）
        for (const QFileInfo &fileInfo : originalDatFiles) {
            if (!fileInfo.fileName().contains("REALTIME_DATA") && 
                !fileInfo.fileName().contains("DDR3_DATA")) {
                datFilesToTransfer.append(fileInfo);
            }
        }

        // 主卡FFT, 最后完成
        for (const QFileInfo &fileInfo : originalDatFiles) {
            if (fileInfo.fileName().contains(QString("DDR3_DATA_CARD%1").arg(hardTrigCard))) {
                emit updateFFT(fileInfo.absoluteFilePath());
            }
        }
        // 新增：对每个 DDR3_DATA_CARDx 文件都发射 FFT1 信号
        for (const QFileInfo &fileInfo : originalDatFiles) {
            if (fileInfo.fileName().contains("DDR3_DATA_CARD")) {
                emit updateFFT1(fileInfo.absoluteFilePath());
            }
        }
        // 计算总文件数
        int totalFiles = datFilesToTransfer.size() + infFiles.size();

        // 合并INF文件
        QStringList realtimeInfFiles;
        QStringList ddr3InfFiles;
        QFileInfoList infFilesToTransfer;

        // 分类INF文件
        for (const QFileInfo &fileInfo : infFiles) {
            if (fileInfo.fileName().contains("REALTIME_DATA")) {
                realtimeInfFiles.append(fileInfo.absoluteFilePath());
            } else if (fileInfo.fileName().contains("DDR3_DATA")) {
                ddr3InfFiles.append(fileInfo.absoluteFilePath());
            } else {
                infFilesToTransfer.append(fileInfo);
            }
        }

        // 合并REALTIME_DATA的INF文件
        if (!realtimeInfFiles.isEmpty()) {
            QString filename = m_phaseDiffFileName.isEmpty() ? 
                "REALTIME_DATA_MERGED" : m_phaseDiffFileName;
            QString mergedInfPath = QDir(m_infPath).filePath(m_gunNumberStr + filename + ".INF");
            
            try {
                emit logMessage("开始合并实时数据INF文件...");
                
                // 创建合并的INF文件
                QFile mergedFile(mergedInfPath);
                if (mergedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    // 使用QMap来存储已处理的内容，避免重复
                    QMap<QString, bool> processedContent;
                    
                    for (const QString &filePath : realtimeInfFiles) {
                        QFile sourceFile(filePath);
                        if (sourceFile.open(QIODevice::ReadOnly)) {
                            QByteArray infData = sourceFile.readAll();
                            QString infContent = QString::fromUtf8(infData);
                            
//                            // 如果内容未被处理过，则添加到合并文件
//                            if (!processedContent.contains(infContent)) {
                                mergedFile.write(infData);
//                                processedContent[infContent] = true;
//                                emit logMessage("添加INF文件: " + filePath);
//                            } else {
//                                emit logMessage("跳过重复INF文件内容: " + filePath);
//                            }
                            
                            sourceFile.close();
                        } else {
                            emit logMessage("无法打开INF文件: " + filePath);
                        }
                    }
                    
                    mergedFile.close();
                    emit logMessage("实时数据INF文件合并完成: " + mergedInfPath);
                    
                    // 将合并后的文件添加到待复制列表
                    QFileInfo mergedFileInfo(mergedInfPath);
                    infFilesToTransfer.append(mergedFileInfo);
                } else {
                    emit logMessage("无法创建合并INF文件: " + mergedInfPath);
                }
            } catch (const std::exception &e) {
                emit logMessage(QString("合并实时数据INF文件时发生错误: %1").arg(e.what()));
            }
        }
        
        // 合并DDR3_DATA的INF文件
        if (!ddr3InfFiles.isEmpty()) {
            QString filename = m_fineFileName.isEmpty() ? 
                "DDR3_DATA_MERGED" : m_fineFileName;
            QString mergedInfPath = QDir(m_infPath).filePath(m_gunNumberStr + filename + ".INF");
            
            try {
                QString modeDesc = (m_workMode == 0) ? "单频模式，处理前3个参数" : "多频模式，处理全部5个参数";
                emit logMessage(QString("开始合并DDR3 INF文件（%1）...").arg(modeDesc));
                
                // 创建合并的INF文件
                QFile mergedFile(mergedInfPath);
                if (mergedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    // 使用QMap来存储已处理的内容，避免重复
                    QMap<QString, bool> processedContent;
                    
                    for (const QString &filePath : ddr3InfFiles) {
                        QFile sourceFile(filePath);
                        if (sourceFile.open(QIODevice::ReadOnly)) {
                            QByteArray infData = sourceFile.readAll();
                            QString infContent = QString::fromUtf8(infData);
                            
                            // INF文件是二进制格式，直接写入原始内容
                            mergedFile.write(infData);
                            
                            sourceFile.close();
                        } else {
                            emit logMessage("无法打开INF文件: " + filePath);
                        }
                    }
                    
                    mergedFile.close();
                    emit logMessage("DDR3 INF文件合并完成: " + mergedInfPath);
                    
                    // 将合并后的文件添加到待复制列表
                    QFileInfo mergedFileInfo(mergedInfPath);
                    infFilesToTransfer.append(mergedFileInfo);
                } else {
                    emit logMessage("无法创建合并INF文件: " + mergedInfPath);
                }
            } catch (const std::exception &e) {
                emit logMessage(QString("合并DDR3 INF文件时发生错误: %1").arg(e.what()));
            }
        }

        // 多卡合并后续处理 重解析合并后的文件 并根据每个卡的配置开关进行重组合并 并放置在PostProcessed 目录下
        postProcessMergedFiles(datFilesToTransfer, infFilesToTransfer);
        
        // 从PostProcessed目录读取处理后的文件进行传输
        QString postProcessedDir = "C:/Data/FIRdata/90001/PostProcessed";
        QString postProcessedDataDir = QDir(postProcessedDir).filePath("DATA");
        QString postProcessedInfDir = QDir(postProcessedDir).filePath("INF");
        
        // 获取PostProcessed目录中与本炮号相关的DAT文件列表
        QDir datDir(postProcessedDataDir);
        QString datPattern = QString("%1*.DAT").arg(m_gunNumberStr);
        QFileInfoList postProcessedDatFiles = datDir.entryInfoList(QStringList() << datPattern, QDir::Files);
        
        // 获取PostProcessed目录中与本炮号相关的INF文件列表
        QDir postProcessedInfDirObj(postProcessedInfDir);
        QString infPattern = QString("%1*.INF").arg(m_gunNumberStr);
        QFileInfoList postProcessedInfFiles = postProcessedInfDirObj.entryInfoList(QStringList() << infPattern, QDir::Files);
        
        int totalPostProcessedFiles = postProcessedDatFiles.size() + postProcessedInfFiles.size();
        emit logMessage(QString("从PostProcessed目录传输炮号%1相关文件，DAT文件数量: %2，INF文件数量: %3")
                       .arg(m_gunNumberStr).arg(postProcessedDatFiles.size()).arg(postProcessedInfFiles.size()));

        // 复制PostProcessed目录中的DAT文件
        for (const QFileInfo &fileInfo : postProcessedDatFiles) {
            QString sourceFilePath = fileInfo.absoluteFilePath();
            QString targetFilePath = QDir(m_datFolderPath).filePath(fileInfo.fileName());

            if (copyLargeFile(sourceFilePath, targetFilePath)) {
                count++;
                emit logMessage("成功传输PostProcessed DAT文件: " + sourceFilePath);
            } else {
                emit logMessage("传输PostProcessed DAT文件失败: " + sourceFilePath);
            }
            emit progressUpdated(count * 100 / totalPostProcessedFiles);
        }

        // 复制PostProcessed目录中的INF文件
        for (const QFileInfo &fileInfo : postProcessedInfFiles) {
            QString sourceFilePath = fileInfo.absoluteFilePath();
            QString targetFilePath = QDir(m_infFolderPath).filePath(fileInfo.fileName());

            if (copyLargeFile(sourceFilePath, targetFilePath)) {
                count++;
                emit logMessage("成功传输PostProcessed INF文件: " + sourceFilePath);
            } else {
                emit logMessage("传输PostProcessed INF文件失败: " + sourceFilePath);
            }
            emit progressUpdated(count * 100 / totalPostProcessedFiles);
        }

        QString info = QString("本炮号全部传输完成! 成功传输文件数量：%1(PostProcessed处理后数据量)/%2(PostProcessed总数据量)").arg(count).arg(totalPostProcessedFiles);
        emit logMessage(info);
        emit progressUpdated(100);
        emit finished();

    } catch (const std::exception &e) {
        emit logMessage("文件传输过程发生错误: " + QString(e.what()));
        emit finished();
    }
}

bool FileCopyWorker::copyLargeFile(const QString &sourceFilePath, const QString &targetFilePath)
{
    QFile sourceFile(sourceFilePath);
    QFile targetFile(targetFilePath);

    try {
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            emit logMessage("无法打开源文件: " + sourceFilePath);
            return false;
        }

        if (!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit logMessage("无法打开目标文件: " + targetFilePath);
            sourceFile.close();
            return false;
        }

        const qint64 bufferSize = 1024 * 1024; // 1MB缓冲区
        QByteArray buffer;
        buffer.resize(bufferSize);

        while (!sourceFile.atEnd()) {
            qint64 bytesRead = sourceFile.read(buffer.data(), bufferSize);
            if (bytesRead <= 0) break;

            if (targetFile.write(buffer.data(), bytesRead) != bytesRead) {
                throw std::runtime_error("写入文件失败");
            }
        }

        sourceFile.close();
        targetFile.close();
        return true;

    } catch (const std::exception &e) {
        sourceFile.close();
        targetFile.close();
        emit logMessage(QString("复制文件时发生错误: %1").arg(e.what()));
        return false;
    }
}

void FileCopyWorker::postProcessMergedFiles(const QFileInfoList &datFiles, const QFileInfoList &infFiles, const QString &gunNumber)
{
    try {
        QString currentGunNumber = gunNumber.isEmpty() ? m_gunNumberStr : gunNumber;
        emit logMessage(QString("开始后处理合并文件，炮号: %1，根据卡开关配置过滤数据...").arg(currentGunNumber));
        
        // 加载卡存储配置
        QMap<int, bool> cardSettings = loadCardStorageSettings();
        
        // 创建PostProcessed目录结构 - 固定在采集路径下
        QString postProcessedDir = "C:/Data/FIRdata/90001/PostProcessed";
        QString postProcessedDataDir = QDir(postProcessedDir).filePath("DATA");
        QString postProcessedInfDir = QDir(postProcessedDir).filePath("INF");
        
        QDir().mkpath(postProcessedDataDir);
        QDir().mkpath(postProcessedInfDir);
        
        emit logMessage(QString("创建后处理目录: %1 (炮号: %2)").arg(postProcessedDir).arg(currentGunNumber));
        
        // 处理每个合并的文件对
        for (const QFileInfo &datFileInfo : datFiles) {
            QString datFileName = datFileInfo.fileName();
            QString baseName = datFileName;
            baseName.remove(".DAT");
            
            // 查找对应的INF文件
            QString infFileName = baseName + ".INF";
            QString sourceInfPath;
            for (const QFileInfo &infFileInfo : infFiles) {
                if (infFileInfo.fileName() == infFileName) {
                    sourceInfPath = infFileInfo.absoluteFilePath();
                    break;
                }
            }
            
            if (sourceInfPath.isEmpty()) {
                emit logMessage("警告: 未找到对应的INF文件: " + infFileName);
                continue;
            }
            
            // 目标文件路径
            QString targetDatPath = QDir(postProcessedDataDir).filePath(datFileName);
            QString targetInfPath = QDir(postProcessedInfDir).filePath(infFileName);
            
            emit logMessage(QString(">>> 处理文件对: %1 / %2 (炮号: %3)").arg(datFileName).arg(infFileName).arg(currentGunNumber));
            
            // 解析和过滤INF文件
            if (parseAndFilterINFFile(sourceInfPath, targetInfPath, cardSettings)) {
                // 根据INF文件路径构建对应的DAT文件路径
                QFileInfo infFileInfo(sourceInfPath);
                QString infDir = infFileInfo.absolutePath();
                QDir dir(infDir);
                dir.cdUp(); // 获取上级目录
                QString parentDir = dir.absolutePath();
                QString sourceDatPath = QDir(parentDir).filePath("DATA/" + baseName + ".DAT");
                
                emit logMessage(QString("构建的DAT数据来源文件路径: %1").arg(sourceDatPath));
                
                // 检查DAT文件是否存在
                if (!QFile::exists(sourceDatPath)) {
                    emit logMessage("警告: DAT文件不存在: " + sourceDatPath);
                    continue;
                }
                
                // 解析和过滤对应的DAT文件
                if (parseAndFilterDATFile(sourceDatPath, targetDatPath, targetInfPath, cardSettings)) {
                    emit logMessage(QString("成功处理文件对: %1 (炮号: %2)").arg(datFileName).arg(currentGunNumber));
                } else {
                    emit logMessage("处理DAT文件失败: " + datFileName);
                }
            } else {
                emit logMessage("处理INF文件失败: " + infFileName);
            }
        }
        
        emit logMessage(QString("后处理完成 (炮号: %1)").arg(currentGunNumber));
        
    } catch (const std::exception &e) {
        emit logMessage("后处理过程发生错误: " + QString(e.what()));
    }
}

void FileCopyWorker::processSpecificGunNumber(const QString &gunNumber)
{
    try {
        emit logMessage(QString("开始手工处理炮号: %1 的合并文件").arg(gunNumber));
        
        // 查找指定炮号的合并DAT文件 - 使用固定的采集文件路径
        QString collectDataPath = "C:/Data/FIRdata/90001/DATA";
        QDir dataDir(collectDataPath);
        QStringList datFilters;
        datFilters << gunNumber + "*" + m_phaseDiffFileName + "*.DAT";
        datFilters << gunNumber + "*" + m_fineFileName + "*.DAT";
        
        emit logMessage(QString("在路径 %1 中查找DAT文件").arg(collectDataPath));
        
        // 检查采集路径是否存在
        if (!dataDir.exists()) {
            emit logMessage(QString("错误：采集数据路径不存在: %1").arg(collectDataPath));
            return;
        }
        
        QFileInfoList datFilesToProcess;
        for (const QString &filter : datFilters) {
            QFileInfoList files = dataDir.entryInfoList(QStringList(filter), QDir::Files);
            datFilesToProcess.append(files);
        }
        
        // 查找对应的INF文件 - 使用固定的采集文件路径
        QString collectInfPath = "C:/Data/FIRdata/90001/INF";
        QDir infDir(collectInfPath);
        QStringList infFilters;
        infFilters << gunNumber + "*" + m_phaseDiffFileName + "*.INF";
        infFilters << gunNumber + "*" + m_fineFileName + "*.INF";
        
        emit logMessage(QString("在路径 %1 中查找INF文件").arg(collectInfPath));
        
        // 检查INF路径是否存在
        if (!infDir.exists()) {
            emit logMessage(QString("错误：INF文件路径不存在: %1").arg(collectInfPath));
            return;
        }
        
        QFileInfoList infFilesToProcess;
        for (const QString &filter : infFilters) {
            QFileInfoList files = infDir.entryInfoList(QStringList(filter), QDir::Files);
            infFilesToProcess.append(files);
        }
        
        if (datFilesToProcess.isEmpty()) {
            emit logMessage(QString("未找到炮号 %1 的合并DAT文件").arg(gunNumber));
            return;
        }
        
        if (infFilesToProcess.isEmpty()) {
            emit logMessage(QString("未找到炮号 %1 的合并INF文件").arg(gunNumber));
            return;
        }
        
        emit logMessage(QString("找到 %1 个DAT文件和 %2 个INF文件需要处理")
                       .arg(datFilesToProcess.size()).arg(infFilesToProcess.size()));
        
        // 调用后处理方法
        postProcessMergedFiles(datFilesToProcess, infFilesToProcess, gunNumber);
        
        emit logMessage(QString("炮号 %1 的文件处理完成").arg(gunNumber));
        
    } catch (const std::exception &e) {
        emit logMessage(QString("处理炮号 %1 时发生错误: %2").arg(gunNumber).arg(e.what()));
    }
}

QMap<int, bool> FileCopyWorker::loadCardStorageSettings()
{
    QMap<int, bool> cardSettings;
    
    try {
        QSettings settings(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
        settings.beginGroup("CHANNEL_PARA");
        
        // 读取所有卡的存储配置，默认为true
        for (int cardNum = 1; cardNum <= 20; cardNum++) {
            QString key = QString("Card%1_storage").arg(cardNum);
            bool storageEnabled = settings.value(key, true).toBool();
            cardSettings[cardNum] = storageEnabled;
            
            if (settings.contains(key)) {
                emit logMessage(QString("卡%1存储配置: %2").arg(cardNum).arg(storageEnabled ? "启用" : "禁用"));
            }
        }
        
        settings.endGroup();
        
    } catch (const std::exception &e) {
        emit logMessage("读取卡存储配置时发生错误: " + QString(e.what()));
    }
    
    return cardSettings;
}

bool FileCopyWorker::parseAndFilterINFFile(const QString &sourceInfPath, const QString &targetInfPath, const QMap<int, bool> &cardSettings)
{
    try {
        QFile sourceFile(sourceInfPath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            emit logMessage("无法打开源INF文件: " + sourceInfPath);
            return false;
        }
        
        QFile targetFile(targetInfPath);
        if (!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit logMessage("无法创建目标INF文件: " + targetInfPath);
            sourceFile.close();
            return false;
        }
        
        QByteArray sourceData = sourceFile.readAll();
        sourceFile.close();
        
        // INF文件是二进制格式，每个记录122字节
        const int recordSize = 122;
        int numRecords = sourceData.size() / recordSize;
        
        if (sourceData.size() % recordSize != 0) {
            emit logMessage("警告: INF文件大小不是记录大小的整数倍");
        }
        
        // 根据文件名判断数据类型
        QString fileName = QFileInfo(sourceInfPath).baseName();
        bool isRealTimeData = false;
        bool isDDRData = false;
        
        // 检查文件名是否包含实时数据标识（phaseDiffFileName）
        if (!m_phaseDiffFileName.isEmpty() && fileName.contains(m_phaseDiffFileName)) {
            isRealTimeData = true;
            emit logMessage("检测到实时数据采集文件");
        }
        // 检查文件名是否包含原始数据标识（fineFileName）
        else if (!m_fineFileName.isEmpty() && fileName.contains(m_fineFileName)) {
            isDDRData = true;
            emit logMessage("检测到DDR原始数据采集文件");
        }
        // 兼容旧的文件名格式
        else if (fileName.contains("REALTIME_DATA")) {
            isRealTimeData = true;
            emit logMessage("检测到实时数据采集文件（兼容格式）");
        }
        else if (fileName.contains("DDR3_DATA")) {
            isDDRData = true;
            emit logMessage("检测到DDR原始数据采集文件（兼容格式）");
        }
        
        // 读取信号模式来确定每张卡的通道数
        QSettings settings(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
        settings.beginGroup("RegisterParams");
        int signalMode = settings.value("singal_mode", 0).toInt();
        settings.endGroup();
        
        // 根据数据类型和信号模式确定每张卡的通道数
        int channelsPerCard;
        if (isRealTimeData) {
            // 实时数据采集：单频1通道，多频3通道（a1, a2, density1）
            channelsPerCard = (signalMode == 0) ? 1 : 3;
        } else if (isDDRData) {
            // DDR原始数据采集：单频3通道，多频5通道（a1, a2, density1, density2, density3）
            channelsPerCard = (signalMode == 0) ? 3 : 5;
        } else {
            // 默认按照工作模式设置
            channelsPerCard = (signalMode == 0) ? 3 : 5;
            emit logMessage("未能识别数据类型，使用默认通道配置");
        }
        
        emit logMessage(QString("数据类型: %1, 信号模式: %2, 每卡通道数: %3 当前文件记录数：%4")
                       .arg(isRealTimeData ? "实时数据" : (isDDRData ? "DDR原始数据" : "未知"))
                       .arg(signalMode == 0 ? "单频" : "多频").arg(channelsPerCard).arg(numRecords));
        
        QByteArray filteredData;
        int newAddr = 0;
        
        for (int i = 0; i < numRecords; i++) {
            int offset = i * recordSize;
            QByteArray record = sourceData.mid(offset, recordSize);
            
            // 解析记录结构（参考Python代码）
            // chnl_id在偏移12处，2字节
            if (record.size() < recordSize) break;
            
            // 提取各个字段进行调试输出
             QByteArray filetypeBytes = record.mid(0, 10);
             QString filetype = QString::fromUtf8(filetypeBytes).trimmed();
             
             qint16 chnl_id = *reinterpret_cast<const qint16*>(record.constData() + 10);
             
             QByteArray chnlBytes = record.mid(12, 12);
             QString chnl = QString::fromUtf8(chnlBytes).trimmed();
             
             qint32 originalAddr = *reinterpret_cast<const qint32*>(record.constData() + 24);
             float freq = *reinterpret_cast<const float*>(record.constData() + 28);
             qint32 len = *reinterpret_cast<const qint32*>(record.constData() + 32);
             qint32 post = *reinterpret_cast<const qint32*>(record.constData() + 36);
             quint16 maxDat = *reinterpret_cast<const quint16*>(record.constData() + 40);
             float lowRang = *reinterpret_cast<const float*>(record.constData() + 42);
             float highRang = *reinterpret_cast<const float*>(record.constData() + 46);
             float factor = *reinterpret_cast<const float*>(record.constData() + 50);
             float fieldOffset = *reinterpret_cast<const float*>(record.constData() + 54);
             QByteArray unitBytes = record.mid(58, 8);
             QString unit = QString::fromUtf8(unitBytes).trimmed();
             float dly = *reinterpret_cast<const float*>(record.constData() + 66);
             qint16 attribDt = *reinterpret_cast<const qint16*>(record.constData() + 70);
             qint16 datWth = *reinterpret_cast<const qint16*>(record.constData() + 72);
             qint32 sparI1 = *reinterpret_cast<const qint32*>(record.constData() + 74);
             qint32 sparI2 = *reinterpret_cast<const qint32*>(record.constData() + 78);
             qint32 sparI3 = *reinterpret_cast<const qint32*>(record.constData() + 82);
             float sparF1 = *reinterpret_cast<const float*>(record.constData() + 86);
             float sparF2 = *reinterpret_cast<const float*>(record.constData() + 90);
             QByteArray sparC1Bytes = record.mid(94, 8);
             QString sparC1 = QString::fromUtf8(sparC1Bytes).trimmed();
             QByteArray sparC2Bytes = record.mid(102, 8);
             QString sparC2 = QString::fromUtf8(sparC2Bytes).trimmed();
             QByteArray sparC3Bytes = record.mid(110, 12);
             QString sparC3 = QString::fromUtf8(sparC3Bytes).trimmed();
             
             emit logMessage(QString("记录%1解析结果: filetype=%2, chnl_id=%3, chnl=%4, addr=%5, freq=%6, len=%7, post=%8, maxDat=%9, lowRang=%10, highRang=%11, factor=%12, offset=%13, unit=%14, dly=%15, attribDt=%16, datWth=%17, sparI1=%18, sparI2=%19, sparI3=%20, sparF1=%21, sparF2=%22, sparC1=%23, sparC2=%24, sparC3=%25")
                             .arg(i).arg(filetype).arg(chnl_id).arg(chnl).arg(originalAddr).arg(freq).arg(len).arg(post).arg(maxDat).arg(lowRang).arg(highRang).arg(factor).arg(fieldOffset).arg(unit).arg(dly).arg(attribDt).arg(datWth).arg(sparI1).arg(sparI2).arg(sparI3).arg(sparF1).arg(sparF2).arg(sparC1).arg(sparC2).arg(sparC3));
             
             // 根据记录索引和数据类型计算卡号
             int cardNum;
             if (isRealTimeData) {
                 // 实时数据：根据每卡通道数计算卡号（多频模式下每卡3个通道）
                 cardNum = (i / channelsPerCard) + 1;
             } else {
                 // DDR数据：根据每卡通道数计算卡号
                 cardNum = (i / channelsPerCard) + 1;
             }
             
             // 检查该卡是否启用
              if (cardNum > 0 && cardSettings.value(cardNum, true)) {
                  // 获取len字段和datWth字段
                  // len偏移：filetype(10) + chnl_id(2) + chnl(12) + addr(4) + freq(4) = 32
                  // datWth偏移：filetype(10) + chnl_id(2) + chnl(12) + addr(4) + freq(4) + len(4) + post(4) + maxDat(2) + lowRang(4) + highRang(4) + factor(4) + offset(4) + unit(8) + dly(4) + attribDt(2) = 72
                  qint32 length = *reinterpret_cast<const qint32*>(record.constData() + 32);
                  qint16 datWth = *reinterpret_cast<const qint16*>(record.constData() + 72);
                  
                  // 记录当前分配的地址用于日志输出
                  qint32 currentAddr = newAddr;
                  
                  // 更新addr字段
                  // 偏移计算：filetype(10) + chnl_id(2) + chnl(12) = 24
                  QByteArray newRecord = record;
                  *reinterpret_cast<qint32*>(newRecord.data() + 24) = newAddr;
                
                filteredData.append(newRecord);
                
                // 计算下一个地址偏移
                newAddr += length * datWth;
                
                emit logMessage(QString("保留通道: %1 (卡%2), addr=%3, length=%4, datWth=%5")
                               .arg(chnl).arg(cardNum).arg(currentAddr).arg(length).arg(datWth));
            } else {
                emit logMessage(QString("过滤通道: %1 (卡%2)").arg(chnl).arg(cardNum));
            }
        }
        
        // 写入过滤后的二进制数据
        targetFile.write(filteredData);
        targetFile.close();
        
        // 输出过滤后的通道详细信息
        emit logMessage("=== 过滤后的通道信息汇总 ===");
        for (int i = 0; i < filteredData.size() / recordSize; i++) {
            int offset = i * recordSize;
            QByteArray record = filteredData.mid(offset, recordSize);
            
            // 提取通道名称
            QByteArray chnlBytes = record.mid(12, 12);
            QString chnl = QString::fromUtf8(chnlBytes).trimmed();
            
            // 提取通道ID
            qint16 chnl_id = *reinterpret_cast<const qint16*>(record.constData() + 10);
            
            // 提取新的addr
            qint32 addr = *reinterpret_cast<const qint32*>(record.constData() + 24);
            
            // 计算卡号
            int cardNum;
            if (isRealTimeData) {
                cardNum = (i / channelsPerCard) + 1;
            } else {
                cardNum = (i / channelsPerCard) + 1;
            }
            
            emit logMessage(QString("通道%1: 名称=%2, 卡号=%3, 新地址=%4")
                          .arg(i).arg(chnl).arg(cardNum).arg(addr));
        }
        
        emit logMessage(QString("INF文件过滤完成，从%1个记录过滤到%2个记录")
                       .arg(numRecords).arg(filteredData.size() / recordSize));
        return true;
        
    } catch (const std::exception &e) {
        emit logMessage("解析INF文件时发生错误: " + QString(e.what()));
        return false;
    }
}

bool FileCopyWorker::parseAndFilterDATFile(const QString &sourceDatPath, const QString &targetDatPath, const QString &infPath, const QMap<int, bool> &cardSettings)
{
    try {
        // 打印文件路径信息
        emit logMessage("=== DAT文件处理路径信息 ===");
        emit logMessage("原始DAT数据路径: " + sourceDatPath);
        // 读取原始INF文件来获取原始地址信息
        QString sourceInfPath = infPath;
        sourceInfPath.replace("/PostProcessed/INF/", "/INF/");
        sourceInfPath.replace("\\PostProcessed\\INF\\", "\\INF\\");

        emit logMessage("原始INF文件路径: " + sourceInfPath);

        emit logMessage("过滤后DAT文件路径: " + targetDatPath);
        emit logMessage("过滤后INF文件路径: " + infPath);
        
        QFile sourceFile(sourceDatPath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            emit logMessage("无法打开源DAT文件: " + sourceDatPath);
            return false;
        }
        
        QFile targetFile(targetDatPath);
        if (!targetFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit logMessage("无法创建目标DAT文件: " + targetDatPath);
            sourceFile.close();
            return false;
        }
        
        QFile sourceInfFile(sourceInfPath);
        if (!sourceInfFile.open(QIODevice::ReadOnly)) {
            emit logMessage("无法打开原始INF文件: " + sourceInfPath);
            sourceFile.close();
            targetFile.close();
            return false;
        }
        
        QByteArray sourceInfData = sourceInfFile.readAll();
        sourceInfFile.close();
        
        // 读取过滤后的INF文件来确定保留的通道
        QFile filteredInfFile(infPath);
        if (!filteredInfFile.open(QIODevice::ReadOnly)) {
            emit logMessage("无法打开过滤后的INF文件: " + infPath);
            sourceFile.close();
            targetFile.close();
            return false;
        }
        
        QByteArray filteredInfData = filteredInfFile.readAll();
        filteredInfFile.close();
        
        // 根据文件名判断数据类型（与parseAndFilterINFFile保持一致）
        QString fileName = QFileInfo(sourceDatPath).baseName();
        bool isRealTimeData = false;
        bool isDDRData = false;
        
        // 检查文件名是否包含实时数据标识（phaseDiffFileName）
        if (!m_phaseDiffFileName.isEmpty() && fileName.contains(m_phaseDiffFileName)) {
            isRealTimeData = true;
            emit logMessage("DAT文件检测到实时数据采集文件");
        }
        // 检查文件名是否包含原始数据标识（fineFileName）
        else if (!m_fineFileName.isEmpty() && fileName.contains(m_fineFileName)) {
            isDDRData = true;
            emit logMessage("DAT文件检测到DDR原始数据采集文件");
        }
        // 兼容旧的文件名格式
        else if (fileName.contains("REALTIME_DATA")) {
            isRealTimeData = true;
            emit logMessage("DAT文件检测到实时数据采集文件（兼容格式）");
        }
        else if (fileName.contains("DDR3_DATA")) {
            isDDRData = true;
            emit logMessage("DAT文件检测到DDR原始数据采集文件（兼容格式）");
        }
        
        // 读取信号模式来确定每张卡的通道数
        QSettings settings(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
        settings.beginGroup("RegisterParams");
        int signalMode = settings.value("singal_mode", 0).toInt();
        settings.endGroup();
        
        // 根据数据类型和信号模式确定每张卡的通道数（与parseAndFilterINFFile保持一致）
        int channelsPerCard;
        if (isRealTimeData) {
            // 实时数据采集：单频1通道，多频3通道（a1, a2, density1）
            channelsPerCard = (signalMode == 0) ? 1 : 3;
        } else if (isDDRData) {
            // DDR原始数据采集：单频3通道，多频5通道（a1, a2, density1, density2, density3）
            channelsPerCard = (signalMode == 0) ? 3 : 5;
        } else {
            // 默认按照工作模式设置
            channelsPerCard = (signalMode == 0) ? 3 : 5;
            emit logMessage("DAT文件未能识别数据类型，使用默认通道配置");
        }
        
        emit logMessage(QString("DAT文件数据类型: %1, 信号模式: %2, 每卡通道数: %3")
                       .arg(isRealTimeData ? "实时数据" : (isDDRData ? "DDR原始数据" : "未知"))
                       .arg(signalMode == 0 ? "单频" : "多频").arg(channelsPerCard));
        
        // 解析原始INF文件获取所有通道的原始地址信息
        const int recordSize = 122;
        int sourceNumRecords = sourceInfData.size() / recordSize;
        int filteredNumRecords = filteredInfData.size() / recordSize;
        
        // 获取过滤后的通道名称列表
        QStringList filteredChannels;
        for (int i = 0; i < filteredNumRecords; i++) {
            int offset = i * recordSize;
            if (offset + recordSize > filteredInfData.size()) break;
            
            QByteArray chnlBytes = filteredInfData.mid(offset + 12, 12);
            QString chnl = QString::fromUtf8(chnlBytes).trimmed();
            filteredChannels.append(chnl);
        }
        
        // 从原始INF文件中提取需要保留的通道的原始地址信息
        QList<QPair<int, int>> channelInfo; // 原始addr, length*width
        
        for (int i = 0; i < sourceNumRecords; i++) {
            int offset = i * recordSize;
            if (offset + recordSize > sourceInfData.size()) break;
            
            // 提取通道名称
            QByteArray chnlBytes = sourceInfData.mid(offset + 12, 12);
            QString chnl = QString::fromUtf8(chnlBytes).trimmed();
            
            // 检查该通道是否在过滤后的列表中
            if (filteredChannels.contains(chnl)) {
                // 提取原始addr字段（偏移24，4字节）
                qint32 originalAddr = *reinterpret_cast<const qint32*>(sourceInfData.constData() + offset + 24);
                
                // 提取length字段（偏移32，4字节）
                qint32 length = *reinterpret_cast<const qint32*>(sourceInfData.constData() + offset + 32);
                
                // 提取datWth字段（偏移72，2字节）
                qint16 datWth = *reinterpret_cast<const qint16*>(sourceInfData.constData() + offset + 72);
                
                channelInfo.append(qMakePair(originalAddr, length * datWth));
                
                emit logMessage(QString("通道%1: 原始地址=%2, 数据大小=%3字节")
                               .arg(chnl).arg(originalAddr).arg(length * datWth));
            }
        }
        
        // 读取源DAT文件数据
        QByteArray sourceData = sourceFile.readAll();
        sourceFile.close();
        
        // 根据过滤后的INF文件中的通道信息重新组织数据
        QByteArray filteredData;
        
        // 按照INF文件中记录的顺序和地址信息提取数据
        for (const QPair<int, int> &channelData : channelInfo) {
            int addr = channelData.first;
            int dataSize = channelData.second;
            
            // 检查地址和大小是否有效
            if (addr >= 0 && addr + dataSize <= sourceData.size()) {
                QByteArray channelBytes = sourceData.mid(addr, dataSize);
                filteredData.append(channelBytes);
                
                emit logMessage(QString("提取数据段: addr=%1, size=%2字节")
                               .arg(addr).arg(dataSize));
            } else {
                emit logMessage(QString("警告: 数据段超出范围: addr=%1, size=%2, 文件大小=%3")
                               .arg(addr).arg(dataSize).arg(sourceData.size()));
            }
        }
        
        // 如果没有提取到任何数据，记录警告
        if (filteredData.isEmpty()) {
            emit logMessage("警告: 未能提取到任何有效数据");
            return false;
        }
        
        // 写入过滤后的数据
        targetFile.write(filteredData);
        targetFile.close();
        
        emit logMessage(QString("DAT文件过滤完成，数据大小从%1字节减少到%2字节")
                       .arg(sourceData.size()).arg(filteredData.size()));
        
        return true;
        
    } catch (const std::exception &e) {
        emit logMessage("解析DAT文件时发生错误: " + QString(e.what()));
        return false;
    }
}
