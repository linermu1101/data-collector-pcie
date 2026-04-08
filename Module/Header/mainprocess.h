#ifndef MAINPROCESS_H
#define MAINPROCESS_H

#include <QStack>
#include <QMainWindow>
#include "pciefunction.h"
#include "planargraph.h"
//#include "sliderruler.h"
#include <cstring>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QUdpSocket>
#include <QToolBar>
#include <QSpinBox>
#include <QSettings>
#include <QMetaType>
#include <QAtomicInteger>

#include "header.h"
#include "pcieprotocol.h"
#include "dataloader.h"
#include "realdataloader.h"
#include "filecopyworker.h"
#include "signalwindow.h"
#include "rfm2g_card.h"
#include "signalvaluewindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainProcess; }
QT_END_NAMESPACE



class MainProcess : public QMainWindow
{
    Q_OBJECT

public:
    MainProcess(QWidget *parent = nullptr);
    ~MainProcess();
    void Log(QString log);
    struct SettingPara cardPara;


public slots:
    void maximizingSingleWindow(int index,bool full);
    void hardTrigTimeout();
    void handleExtremeIndices(int index1, int index2, int index3);

signals:
    void openDev(QString devPath,int index);
    void closeDev();
    void readData(PlanarGraph *plot,SettingPara cardPara);
    void writeData(UINT32 address,UINT32 size,unsigned char *buffer,int index);
    void readCfg(UINT32 address,UINT32 size,unsigned char *buffer,int index);
    void writeCfg(UINT32 address,UINT32 size,unsigned char *buffer,int index);
    void readIrq(int index, int timeout);

    void beginDDR(int cardIndex, int channels, int triggerMode, int samplingRate, int collectTime,  QString datPathPre, QString infPathPre, QString gunNumber);
    void hardCollectionTrigger();
    void copyProgress(int percentage);
    void collectionFinished();


private slots:
//    bool copyLargeFile(const QString &sourceFilePath, const QString &targetFilePath);
    void copyFiles(const QString &gunNumberStr, const QString &m_dataPath, const QString &m_infPath, 
                  const QString &datFolderPath, const QString &infFolderPath,
                  const QString &phaseDiffFileName = "", const QString &fineFileName = "");
    void prePareDir();
    void on_cancleConfig_clicked();
    void initDevice();
    void disconnectDevice();
    void onDeviceToggleButtonClicked();
    void onShowReal(int cardIndex, const QVector<QVector<double>> &xdata, const QVector<QVector<double>> &ydata,double xmax, double ymax);
    void onLoadingFinished(int cardIndex);
    void showDDr(int cardIndex);
    void showReal(int cardIndex);
    QString getGunNumber();
    void startReal();
    void stopReal();
    void handleOfflineFinished(int cardindex);
    void handleOfflineProgress(int cardindex, int value);
    void handleRealProgress(int cardindex, int index, int fifoLen, int32_t valueAll);
    void handleRealFinished(int cardindex);
    void handleFifoOverflow(int cardindex, double fillRatio);

    void startCollection();
    void stopCollection();
    void switchParaWidget();
    void changeWorkingModeParams(int mode);
    void changeWorkingMode(int mode);
    void realTimePhasePickUp(int index);
    void phaseFilp_50ms(int index);
    void setTrigMainCard();
    void changeTrigMode(int mode);
    void udpReceive();
    void checkElapsedTime();
    void replotChart(int index);
    void ddrProcess(int index);
    void revHardTrigStart();
    void readIniFile();
    void saveIniFile();
    void newsaveIniFile();
    void consolidatedPhaseDiffDocuments();
    void consolidatedFineDocuments();
//翻转系列
//    void Cardfilp1();
//    void Cardfilp2();
//    void Cardfilp3();
//    void Cardfilp4();
//    void Cardfilp5();
//    void Cardfilp6();
//    void Cardfilp7();
    void on_btnFileSync_clicked();
    //FFT相关
    void updateSignalWindow(const QString& datFilePath);
    void updateSignalWindow1(const QString& datFilePath);
    void onSignalWindowClosed();

    void on_btnAync_clicked();

    void on_btnProcessSpecific_clicked();

private:
    Ui::MainProcess *ui;

private:
    // 新增
    QAtomicInteger<int> m_finishedCards;  // 已完成的卡数，线程安全
    QString m_dataPath,m_infPath; //存储路径前缀
    QList<int> busLists;
    //QStringList pciPaths;
    uint8_t cardNumbers = 0;
    QElapsedTimer timerRealCollect;
    //QVector<QVector<double>> xdata;
    //QVector<QVector<double>> ydata;
    QMap<int, QVector<QVector<double>>> mapXdata;
    QMap<int, QVector<QVector<double>>> mapYdata;
    bool loadingFinished;
    QCheckBox* autoCheckBox;
    // QThread *threadPcie;
    // QThread *threadReal;
    // QThread *threadDataLoader;
    // QThread *threadRealDataLoader;
    // PcieProtocol* pcie;
    // PcieProtocol* pcieReal;
    // DataLoader *dataLoader;
    // RealDataLoader *realdataLoader;

    QList<PcieProtocol*> pcieList;
    QList<PcieProtocol*> pcieRealList;
    QList<DataLoader*> dataLoaderList;
    QList<RealDataLoader*> realdataLoaderList;
    QList<QThread*> threadPcieList;
    QList<QThread*> threadRealList;
    QList<QThread*> threadDataLoaderList;
    QList<QThread*> threadRealDataLoaderList;

    //反射内存卡
    RFM2G_CARD *pRFM2g;
    
    // 三频模式第一相位值保存（每张卡一个值）
    QMap<int, double> m_triplePhase1Values;
    // 新增END


    //uint8_t cardNumber= 0;
    // 当前选择卡
    uint8_t mainCard = 0;
    uint8_t cnt=0;
    uint8_t modeIndex=0;
    bool firstTrig = true;
    bool hardTrig = false;
    bool m_gunNumberLocked = false; // 炮号锁定状态
    int samplingRate = 0;
    // 极值参数源枚举
        enum ParameterSource {
            CALCULATED_PARAMS = 0,  // 计算参数
            MANUAL_PARAMS = 1       // 人工参数
        };
        ParameterSource m_currentParamSource = CALCULATED_PARAMS; // 当前参数源
        // 人工输入极值的参数结构体
            struct ManualParams {
                QString nf0_r;
                QString nf1_r;
                QString nf2_r;
                QString nf0_s;
                QString nf1_s;
                QString nf2_s;
            };

            ManualParams m_manualParams; // 存储人工参数
    
    // 调试参数
    bool debugMode = false;
    int simulatedCardCount = 0;

    //QList<int> busList;
    QList<int> Fliplist;

    //线程列表
    // QList<PcieFunction*> hDevList;
    // QList<QThread*> threadList;
    QList<PlanarGraph*> plotList;
    //QStringList pathList;
    QList<int> tmList; //用来判断是否所有卡都停止了
    QList<QString> Equipment_Number;//用于判断设备号

    QUdpSocket *udpHandle;
    QString gunNumber;
    int gunTime;
    QStack<int> temp;

    // 分批文件存储计数器
    int m_currentFileCount;
    int m_totalFiles;

    QTimer *updateTimer;        //定时器
    QTimer *referenceTimer;
    QTimer *measureTimer;
    QTimer *checkTimer;
    QElapsedTimer *elTimer;

    //界面控件
    QComboBox *trigMode;
    QPushButton *btnStart;
    QPushButton *btnStop;
    QSpinBox *sp_gunTime;
    QSpinBox *sp_gunNumber;
    //QList<SliderRuler*> rulerList;
    QWidget *createParaWidget2();

    QLabel *labStatus;          //采集状态灯
    QLabel *labOverflow;        //溢出状态灯
    QPushButton *paraConfig;
    QPushButton *echo;
    QPushButton *btnDeviceToggle;
    QPushButton *btnSwitchPlot;
    QPushButton *btnRealPlotView; //实时数据查看
    QPushButton *btnHistoryPlotView; //历史数据查看

    SignalWindow* m_signalWindow = nullptr;  // 信号窗口指针
    SignalValueWindow *m_signalValueWindow = nullptr;
    bool trigBeforeHard = false;    // 硬件触发前需要进行一次软件触发进行校准，此标志位由UDP信号控制

    //int pcieInit();
    int scanPcieDevice(GUID guid, QStringList *dev_path, DWORD32 len_devpath);

    void dealRealTimeInf();
    void realDataCut();
    void ddrDataCut();
    void realDataDeal();
    void ddrDataDeal();
    void switchLayout(int style);
    void initToolBarCtl();
    void initReflectFileInfWidget();
    QWidget* createParaWidget(bool realTime,IniFileInf *inf);
    void clearLayout(QWidget *wg);
    QWidget* createTmpWidget();
    QLabel* createTmpLabel(int size);
    void delay_ms(int mes);
    void ctlStatus(int action);
    void time_Update();
    void consolidatedDocuments(QString dirtype,QString fileFilter,QString fileType,QString suffix,QString serverReflectionPath);
    void Equipment_manage();//用于管理设备号
    void busSort(QStringList &plist,QList<int> &bList);//卡槽排序
    void busSortBycfg(QStringList &plist,QStringList &pnewlist);
    void ProcessFlipList();
    int getValidPlotCount() const;
    void restartApplication();
    void showSignalValueWindow(const QString& datFilePath);
    int extractCardIndex(const QString& filePath);
    void onAutoCollectionStateChanged(int state);
    void onStartButtonClicked();
    void onCollectionFinished(); // 采集完成信号的处理函数
    void onParameterSourceChanged(int index);
    void updateManualParamsDisplay();
    void onManualParamEdited();

};
#endif // MAINPROCESS_H

