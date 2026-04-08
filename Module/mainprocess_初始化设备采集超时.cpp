#include "mainprocess.h"
#include "ui_mainprocess.h"
#include "QsLog.h"
#include <QRegularExpression>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

int g_cnt=0;  //防止多卡多次触发停止



MainProcess::MainProcess(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainProcess)
{
    ui->setupUi(this);
    ui->lab_GunInf->setText("等待采集");
    qRegisterMetaType<PlanarGraph*>("PlanarGraph*");
    qRegisterMetaType<SettingPara>("SettingPara");
    qRegisterMetaType<QCPRange>("QCPRange");
    qRegisterMetaType<int16_t>("int16_t");
    qRegisterMetaType<int16_t>("int32_t");
    qRegisterMetaType<QTextCursor>("QTextCursor");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVector<double>>("QVector<QVector<double>>");

    QFile file(":/new/prefix1/qrc/style.css");
    file.open(QIODevice::ReadOnly);
    QString styleStr = file.readAll();
    this->setStyleSheet(styleStr);
    gunTime = 0;

    // 初始化设备连接状态(默认开始时断联，要初始化处于连接状态将次改成true)
    m_deviceConnected = false;

    QList<QColor> colorList;
    colorList<<QColor(0,120,215)<<QColor(0,215,120)<<QColor(215,0,120)<<QColor(120,0,215)<<QColor(0,0,0);

    /***********************反射内存卡*************************/
    pRFM2g = new RFM2G_CARD(); // NULL;

    // 初始化反射内存卡
    if (pRFM2g->openRFM2g() == 0) {
        Log("反射内存卡初始化成功 单机最大支持15张板卡");
        pRFM2g->setMaxCardCount(15);  // 设置最大支持15张卡

        // 连接错误信号
        connect(pRFM2g, &RFM2G_CARD::errorOccurred, this, [this](const QString &error) {
            Log("反射内存卡错误:" + error);
        });
    } else {
        Log("反射内存卡初始化失败");
    }

    /***********************采集卡初始化（延迟到按钮点击时执行）*************************/
    cardNumbers = 0;

    // 读取配置文件中的调试参数
    readIniFile();

    // 初始化工具栏控件（包含初始化/断联按钮）
    initToolBarCtl();

    // 设置初始布局（此时还没有图表，但先设置好布局结构）
    switchLayout(Layout_4x2);

    updateTimer = new QTimer(this);
    QLabel *tmLab = new QLabel(this);

    connect(updateTimer,&QTimer::timeout,this,[=](){
        tmLab->setText(QDateTime::currentDateTime().toString("yyyy年MM月dd日hh:mm"));
        ui->statusbar->addPermanentWidget(tmLab);
    });
    updateTimer->start(2000);

    referenceTimer  = new QTimer(this);
    connect(referenceTimer,&QTimer::timeout,this,[=](){
        for(int i=0 ;i<cardNumbers; i++){
            unsigned char *buf;
            buf = (unsigned char*)malloc(4);
        }
    });

    measureTimer  = new QTimer(this);
    connect(measureTimer,&QTimer::timeout,this,[=](){
        for(int i=0 ;i<cardNumbers; i++){
            unsigned char *buf;
            buf = (unsigned char*)malloc(4);
        }
    });

    checkTimer = new QTimer(this);
    connect(checkTimer,&QTimer::timeout,this,&MainProcess::checkElapsedTime);

    ui->tabSettingWidget->clear();
    ui->tabSettingWidget->addTab(ui->basicParaWidget,"参数设置");
    ui->tabSettingWidget->addTab(ui->channelParaWidget,"通道名称");
    udpHandle = new QUdpSocket(this);

    QList<QComboBox*> list = QObject::findChildren<QComboBox*>();
    foreach(QComboBox *cb, list)
        cb->setView(new QListView());

    QList<QSpinBox*> splist = QObject::findChildren<QSpinBox*>();
    foreach(QSpinBox *sp, splist)
        sp->setMaximum(2147483647);

    ctlStatus(Init);
    initReflectFileInfWidget();

    changeWorkingMode(0);
    realTimePhasePickUp(4);
    phaseFilp_50ms(0);

    // 连接按钮信号
    connect(btnStart,&QPushButton::clicked,this,&MainProcess::startCollection);

    //新增：示波器功能
    // 初始化自动采集选择框
    autoCheckBox = new QCheckBox("自动采集", this);
    autoCheckBox->setChecked(false);

    // 将选择框添加到工具栏或其他合适位置
    ui->toolBar->addWidget(autoCheckBox);

    // 连接选择框状态改变信号
    connect(autoCheckBox, &QCheckBox::stateChanged, this, &MainProcess::onAutoCollectionStateChanged);

    // 修改btnStart的点击连接
    connect(btnStart, &QPushButton::clicked, this, &MainProcess::onStartButtonClicked);

    connect(btnStop,&QPushButton::clicked,this,&MainProcess::stopCollection);
    connect(trigMode,SIGNAL(currentIndexChanged(int)),this,SLOT(changeTrigMode(int)));
    connect(paraConfig,&QPushButton::clicked,this,&MainProcess::switchParaWidget);
    connect(ui->Mode,SIGNAL(currentIndexChanged(int)),this,SLOT(changeWorkingMode(int)));
    connect(ui->PickUp,SIGNAL(currentIndexChanged(int)),this,SLOT(realTimePhasePickUp(int)));
    connect(ui->phaseFilp,SIGNAL(currentIndexChanged(int)),this,SLOT(phaseFilp_50ms(int)));
    connect(ui->set2MainCard,&QPushButton::clicked,this,&MainProcess::setTrigMainCard);
    // 保存按钮
    connect(ui->saveConfig,&QPushButton::clicked,this,&MainProcess::newsaveIniFile);
    // 调试参数控件变化时自动保存配置
    connect(ui->txtSimulatedCardCount, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainProcess::saveIniFile);
    connect(ui->cboDebugModel, &QCheckBox::toggled, this, &MainProcess::saveIniFile);

    // 连接参数源选择信号
    connect(ui->cboParameterSource, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainProcess::onParameterSourceChanged);

    // 连接人工参数编辑信号
    connect(ui->txtnf0_r_manual, &QLineEdit::editingFinished, this, &MainProcess::onManualParamEdited);
    connect(ui->txtnf1_r_manual, &QLineEdit::editingFinished, this, &MainProcess::onManualParamEdited);
    connect(ui->txtnf2_r_manual, &QLineEdit::editingFinished, this, &MainProcess::onManualParamEdited);
    connect(ui->txtnf0_s_manual, &QLineEdit::editingFinished, this, &MainProcess::onManualParamEdited);
    connect(ui->txtnf1_s_manual, &QLineEdit::editingFinished, this, &MainProcess::onManualParamEdited);
    connect(ui->txtnf2_s_manual, &QLineEdit::editingFinished, this, &MainProcess::onManualParamEdited);

    readIniFile();
    setTrigMainCard();
    this->setWindowTitle("红外激光干涉仪数据采集系统  V251015");
    ui->label_10->setHidden(true);
    ui->phaseFilp->setHidden(true);
    ui->PickUp->setItemText(0,"实时挑点");
    ui->PickUp->setItemData(0, QVariant(false), Qt::UserRole - 1); // 设置选项为禁用状态
    ui->maintainSync->setChecked(true); // 默认保持同步

    QStringList hiddenWidgets = {
        "txtdensity_ratio_b",
        "txtchange_rt",
        "txtsymb_on",
        "txtsymbol",
        "txtsym_time",
        "txtsingal_mode",
        "label_24",
        "label_36",
        "label_38",
        "label_39",
        "label_50",
        "label_51",
    };

    for (const QString &widgetName : hiddenWidgets) {
        QLineEdit *widget = findChild<QLineEdit*>(widgetName);
        QLabel *label = findChild<QLabel*>(widgetName);
        if (widget) {
            widget->setVisible(false);
            widget->setStyleSheet("visibility: hidden;");
        }else{
            label->setVisible(false);
            label->setStyleSheet("visibility: hidden;");
        }
    }

    prePareDir();
}

void MainProcess::busSortBycfg(QStringList &plist, QStringList &pnewlist)
{
    // 清空pnewlist以确保没有残留数据
    pnewlist.clear();

    // 如果开启了调试模拟板卡数量功能，就不从pcie.txt加载了，直接使用模拟的板卡名称
    if(debugMode && simulatedCardCount > 0) {
        pnewlist = plist; // 直接使用模拟板卡列表
        Log("调试模式：使用模拟板卡，跳过pcie.txt配置文件加载");
        QLOG_INFO() << "调试模式 card list: " << pnewlist;
        return;
    }

    QMap<int, QString> map; // 使用行号作为键，确保按行号排序
    QFile file(QCoreApplication::applicationDirPath() + "/pcie.txt");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Log("未找到配置文件 " + QCoreApplication::applicationDirPath() + "/pcie.txt");
        return;
    }

    QTextStream in(&file);
    int lineNumber = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.isEmpty()) { // 忽略空行
            map.insert(lineNumber, line); // 行号作为键，确保按行号排序
            QString msg = QString("加载pcie.txt第%1行 %2").arg(lineNumber).arg(line);
            Log(msg);
        }
        ++lineNumber;
    }

    file.close();

    // 遍历map，按照行号（键）排序的顺序处理
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QString& pciPath = it.value();
        if (plist.contains(pciPath)) {
            pnewlist.append(pciPath);
        }
    }

    QLOG_INFO() << "原始 card list plist: " << plist;
    QLOG_INFO() << "排序后 card list pnewlist: " << pnewlist;
}


//卡槽排序
void MainProcess::busSort(QStringList &plist,QList<int> &bList)
{
//    QMap<QString,int> map;
//	map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#8&3a162b92&0&004800080008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",1);
//        map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#6&19eed8c&0&00400008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",1);
//        map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#6&2e44ce28&0&00800008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",2);
//        map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#6&d6a556e&0&00580008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",3);
//        map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#6&112d9bac&0&00500008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",4);
//        map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#6&2fb13337&0&00200008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",5);
//        map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#6&2a8187ea&0&00280008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",6);
//        map.insert("\\\\?\\pci#ven_10ee&dev_7024&subsys_000710ee&rev_00#6&defc094&0&00300008#{74c7e4a9-6d5d-4a70-bc0d-20691dff9e9d}",7);

    QMap<QString, int> map;
    QFile file(QCoreApplication::applicationDirPath() + "/pcie.txt");

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Log("未找到配置文件 " + QCoreApplication::applicationDirPath() + "/pcie.txt");
        return;
    }

    QTextStream in(&file);
    int lineNumber = 1;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.isEmpty()) { // 忽略空行
            map.insert(line, lineNumber);
            QString msg=QString("加载pcie.txt第%1行 %2").arg(lineNumber).arg(line);
            Log(msg);
        }
        ++lineNumber;
    }

    file.close();

    QString str;

    for(int i=0; i<plist.size(); i++)
    {
        int a = map.value(plist[i]);
        if (a != 0) {
            // 处理找不到键的情况
            bList.append(a);
        }

    }

    // int size = bList.size();
    // int temp;

    // for(int i=0; i<size; i++)
    // {
    //     for(int j=0; j<(size-i-1); j++)
    //     {
    //         if(bList[j]>bList[j+1]){
    //             temp = bList[j];
    //             bList[j] = bList[j+1];
    //             bList[j+1] = temp;

    //             str = plist[j];
    //             plist[j] = plist[j+1];
    //             plist[j+1] = str;
    //         }
    //     }
    // }

    QLOG_INFO() << "card list plist "  << plist;
    QLOG_INFO() << "card list bList "  << bList;
}

//清除布局
void MainProcess::clearLayout(QWidget *wg)
{
    QGridLayout *gly  = wg->findChild<QGridLayout*>();
    delete  gly;
    QVBoxLayout *vly  = wg->findChild<QVBoxLayout*>();
    delete  vly;
}

//切换曲线布局
void MainProcess::switchLayout(int style)
{
    clearLayout(ui->plotWidget);
    clearLayout(ui->plotWidget2);
    bool even = (cardNumbers%2==0);  //判断卡是否为偶数卡
    bool nFlag = (cardNumbers>4);    //判断卡是否大于4张
    QVBoxLayout *mainLayout = new QVBoxLayout;
    QVBoxLayout *mainLayout2 = new QVBoxLayout;
    QHBoxLayout *vlayout = new QHBoxLayout;
    QGridLayout *glayout = new QGridLayout;
    QGridLayout *glayout2 = new QGridLayout;

    int cols_three = 3; // 定义三列布局的列数

    switch (style) {
    case Layout_4x2:
        for(int i=0; i<cardNumbers; i++)
        {
            // 只添加有图表的卡
            if (plotList[i] == nullptr) {
                continue;
            }

            QVBoxLayout *t_vlayout = new QVBoxLayout;
            // rulerList可能也需要相应处理，这里假设rulerList与plotList对应
//            if (i < rulerList.size() && rulerList[i] != nullptr) {
//                t_vlayout->addWidget(rulerList[i],1);
//            }
            t_vlayout->addWidget(plotList[i],10);

            if(!nFlag){
                if(cardNumbers<3){
                    mainLayout->addLayout(t_vlayout);
                }else{
                    glayout->addLayout(t_vlayout,i/2,i%2);
                }
            }else{
                if(i<4){
                    glayout->addLayout(t_vlayout,i/2,i%2);
                }else{
                    if(!even && i == (cardNumbers-1)){
                        vlayout->addLayout(t_vlayout);
                    }else{
                        glayout2->addLayout(t_vlayout,i/2,i%2);
                    }
                }
            }
        }

        if(cardNumbers>3){
             mainLayout->addLayout(glayout);
            if(cardNumbers>4){
                if(even){
                    mainLayout2->addLayout(glayout2);
                }else{
                    mainLayout2->addLayout(glayout2);
                    mainLayout2->addLayout(vlayout);
                }
            }
        }

        ui->plotWidget->setLayout(mainLayout);
        ui->plotWidget2->setLayout(mainLayout2);
        break;
    case Layout_8x1:
        for(int i=0; i<cardNumbers; i++)
        {
            // 只添加有图表的卡
            if (plotList[i] == nullptr) {
                continue;
            }

            QVBoxLayout *t_vlayout = new QVBoxLayout;
//            if (i < rulerList.size() && rulerList[i] != nullptr) {
//                t_vlayout->addWidget(rulerList[i],1);
//            }
            t_vlayout->addWidget(plotList[i],10);

            if(cardNumbers<=3){
                mainLayout->addLayout(t_vlayout);
            }else{
                if(!even && i == (cardNumbers-1)){
                    vlayout->addLayout(t_vlayout);
                }else{
                    glayout->addLayout(t_vlayout,i / cols_three,i % cols_three);
                }
            }
        }
        if(cardNumbers>3){
            if(even){
                mainLayout->addLayout(glayout);
            }else{
                mainLayout->addLayout(glayout, cardNumbers / cols_three);
                mainLayout->addLayout(vlayout, 1);
            }
        }
        ui->plotWidget->setLayout(mainLayout);
        break;
    default:
        break;
    }
}

//初始化工具栏控件
void MainProcess::initToolBarCtl()
{
    QStringList list;
    list<<"软件触发"<<"硬件触发";
    trigMode = new QComboBox;
    trigMode->addItems(list);

    QWidget *tpWg = new QWidget();
    tpWg->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);

    // 添加初始化/断联设备按钮
    btnInitDevice = new QPushButton("初始化设备");
    btnInitDevice->setIcon(QIcon(":/new/prefix1/qrc/connect.png"));
    btnInitDevice->setCheckable(true); // 设置为可切换状态

    btnStart = new QPushButton("启动");
    btnStart->setIcon(QIcon(":/new/prefix1/qrc/play.png"));
    btnStop = new QPushButton("停止");
    btnStop->setIcon(QIcon(":/new/prefix1/qrc/stop.png"));

    QLabel *labTime = new QLabel("时间（毫秒）");
    sp_gunTime = new QSpinBox;
    sp_gunTime->setMaximum(2147483647);
    sp_gunTime->setValue(200);
    QLabel *labGun = new QLabel("炮号");
    sp_gunNumber = new QSpinBox;
    sp_gunNumber->setMaximum(2147483647);
    sp_gunNumber->setValue(90001);

    QLabel *lab = new QLabel("运行状态:");
    labStatus = new QLabel;
    labStatus->setFixedSize(30,30);
    labStatus->setStyleSheet("QLabel{border-radius:14px;"
                          "background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5,"
                          "stop:0 rgb(0, 255, 0),"
                          "stop:0.79 rgb(0, 230, 0),"
                          "stop:0.8 rgb(100, 100, 100),"
                          "stop:0.9 rgb(255, 255, 255),"
                          "stop:1 rgb(0, 0, 0));}");
    QLabel *lab2 = new QLabel("溢出:");

    labOverflow = new QLabel();
    labOverflow->setFixedSize(30,30);
    labOverflow->setStyleSheet("QLabel{border-radius:14px;"
                          "background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5,"
                          "stop:0 rgb(0, 80, 0),"
                          "stop:0.79 rgb(0, 80, 0),"
                          "stop:0.8 rgb(100, 100, 100),"
                          "stop:0.9 rgb(255, 255, 255),"
                          "stop:1 rgb(0, 0, 0));}");

    paraConfig = new QPushButton("高级选项");
    paraConfig->setIcon(QIcon(":/new/prefix1/qrc/automation.png"));
    echo = new QPushButton("回显");
    QPushButton *l8 = new QPushButton(this);
    QPushButton *l4 = new QPushButton(this);

    connect(l8,&QPushButton::clicked,this,[=](){
        switchLayout(Layout_8x1);
        btnSwitchPlot->setEnabled(false);
    });

    connect(l4,&QPushButton::clicked,this,[=](){
        switchLayout(Layout_4x2);
        btnSwitchPlot->setEnabled(true);
    });

    l8->setIcon(QIcon(":/new/prefix1/qrc/8x1.png"));
    l4->setIcon(QIcon(":/new/prefix1/qrc/4x2.png"));
    l8->setIconSize(QSize(20,20));
    l4->setIconSize(QSize(20,20));

    btnSwitchPlot = new QPushButton("切换");

    connect(btnSwitchPlot,&QPushButton::clicked,this,[=](){
        if(cardNumbers>4)
        {
            int index = ui->stackedWidget_2->currentIndex();
            ui->stackedWidget_2->setCurrentIndex(!index);
        }
    });

    btnRealPlotView  = new QPushButton("实时文件显示");

    connect(btnRealPlotView,&QPushButton::clicked,this,[=](){
        for(int c = 0; c< cardNumbers; c++)
        {
            showReal(c);
        }
    });

    btnHistoryPlotView  = new QPushButton("DDR文件显示");

    connect(btnHistoryPlotView,&QPushButton::clicked,this,[=](){
        for(int c = 0; c< cardNumbers; c++)
        {
            showDDr(c);
        }
    });

    labOverflow->setHidden(true);
    echo->setHidden(true);
    lab2->setHidden(true);

    ui->toolBar->addWidget(createTmpLabel(10));

    // 添加初始化/断联设备按钮到工具栏
    ui->toolBar->addWidget(btnInitDevice);
    ui->toolBar->addWidget(createTmpLabel(10));

    ui->toolBar->addWidget(trigMode);
    ui->toolBar->addWidget(btnStart);
    ui->toolBar->addWidget(btnStop);

    ui->toolBar->addWidget(createTmpLabel(20));
    ui->toolBar->addWidget(labTime);
    ui->toolBar->addWidget(createTmpLabel(5));
    ui->toolBar->addWidget(sp_gunTime);
    ui->toolBar->addWidget(createTmpLabel(10));
    ui->toolBar->addWidget(labGun);
    ui->toolBar->addWidget(createTmpLabel(5));
    ui->toolBar->addWidget(sp_gunNumber);

    ui->toolBar->addWidget(createTmpLabel(20));
    ui->toolBar->addWidget(paraConfig);
    ui->toolBar->addWidget(createTmpLabel(20));
    ui->toolBar->addWidget(echo);
    ui->toolBar->addWidget(createTmpLabel(20));
    ui->toolBar->addWidget(l8);
    ui->toolBar->addWidget(l4);
    ui->toolBar->addWidget(btnSwitchPlot);

    ui->toolBar->addWidget(tpWg);
    ui->toolBar->addWidget(lab);
    ui->toolBar->addWidget(createTmpLabel(5));
    ui->toolBar->addWidget(labStatus);
    ui->toolBar->addWidget(createTmpLabel(10));
    ui->toolBar->addWidget(lab2);
    ui->toolBar->addWidget(createTmpLabel(5));
    ui->toolBar->addWidget(labOverflow);
    ui->toolBar->addWidget(createTmpLabel(10));

    // 连接初始化/断联按钮信号
    connect(btnInitDevice, &QPushButton::clicked, this, &MainProcess::onInitDeviceClicked);

    // 初始状态：开始和停止按钮禁用
    btnStart->setEnabled(false);
    btnStop->setEnabled(false);
}

// 初始化/断联设备按钮点击处理
void MainProcess::onInitDeviceClicked(bool checked)
{
    if (checked) {
        // 点击初始化设备
        btnInitDevice->setText("断联设备");
        btnInitDevice->setIcon(QIcon(":/new/prefix1/qrc/disconnect.png"));
        initializePcieDevices();
    } else {
        // 点击断联设备
        btnInitDevice->setText("初始化设备");
        btnInitDevice->setIcon(QIcon(":/new/prefix1/qrc/connect.png"));
        disconnectPcieDevices();
    }
}

// 初始化PCIe设备
void MainProcess::initializePcieDevices()
{
    if (m_deviceConnected) {
        Log("设备已连接，无需重复初始化");
        return;
    }

    Log("开始初始化PCIe设备...");

    // 扫描PCIe设备
    QStringList piceList = PcieProtocol::pcieScan();

    // 调试模式：模拟指定数量的板卡
    if(debugMode && simulatedCardCount > 0) {
        piceList.clear();
        for(int i = 0; i < simulatedCardCount; i++) {
            piceList << QString("SimulatedCard_%1").arg(i+1);
        }
        Log(QString("调试模式：模拟%1张板卡").arg(simulatedCardCount));
    }

    cardNumbers = piceList.length();
    if(cardNumbers <= 0){
        Log("板卡未识别");
        btnInitDevice->setChecked(false);
        btnInitDevice->setText("初始化设备");
        return;
    }
    else{
        if(debugMode && simulatedCardCount > 0) {
            Log("模拟板卡识别数量"+QString::number(cardNumbers));
        } else {
            Log("板卡识别数量"+QString::number(cardNumbers));
        }
        // 打印所有板卡的信息
        for (const auto& item : piceList) {
            Log(item);
        }
    }

    // 注册并排序卡槽
    QStringList piceListBycfg;
    busSortBycfg(piceList, piceListBycfg);

    cardNumbers = piceListBycfg.size();
    Log("板卡（根据配置排序）识别数量"+QString::number(cardNumbers));

    // 清空之前的设备列表
    if (!pcieRealList.isEmpty()) {
        disconnectPcieDevices();
    }

    QList<QColor> colorList;
    colorList<<QColor(0,120,215)<<QColor(0,215,120)<<QColor(215,0,120)<<QColor(120,0,215)<<QColor(0,0,0);

    // 先读取配置文件的卡存储状态
    QSettings pConfig(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
    pConfig.beginGroup("CHANNEL_PARA");
    QVector<bool> cardStorageStates;
    for (int i = 0; i < cardNumbers; i++) {
        bool storageState = pConfig.value("Card" + QString::number(i + 1) + "_storage", true).toBool();
        cardStorageStates.append(storageState);
    }
    pConfig.endGroup();

    // 初始化PCIe设备
    for(int k = 0; k < cardNumbers; k++)
    {
        QString pciePathItem = piceListBycfg[k];
        qDebug() << " PCI path(byCfg):" << pciePathItem;

        QString info = QString("初始化第%1张卡 %2").arg(k+1).arg(pciePathItem);
        Log(info);

        QThread *threadDataLoader = new QThread();
        QThread *threadRealDataLoader = new QThread();
        QThread *threadReal = new QThread();

        DataLoader *dataLoader = new DataLoader(nullptr, k);
        RealDataLoader *realdataLoader = new RealDataLoader(nullptr, k);
        PcieProtocol *pcieReal = new PcieProtocol(nullptr, pciePathItem, k);

        pcieRealList.append(pcieReal);
        dataLoaderList.append(dataLoader);
        realdataLoaderList.append(realdataLoader);
        threadRealList.append(threadReal);
        threadDataLoaderList.append(threadDataLoader);
        threadRealDataLoaderList.append(threadRealDataLoader);

        // 根据卡存储开关状态创建图表
        if (cardStorageStates[k]) {
            // 卡存储开关打开，创建图表
            PlanarGraph *m_plot = new PlanarGraph(QString::number(k));
            m_plot->plot_2D_Init(Qt::white, 3, colorList);
            plotList.append(m_plot);
            connect(m_plot, SIGNAL(windowFullorRestoreSize(int,bool)), this, SLOT(maximizingSingleWindow(int,bool)));
            Log(QString("卡%1图表已创建").arg(k+1));
        } else {
            // 卡存储开关关闭，图表为空
            plotList.append(nullptr);
            Log(QString("卡%1存储关闭，图表未创建").arg(k+1));
        }

        // 初始化反射内存卡地址映射
        if (pRFM2g && pRFM2g->isOpened()) {
            if (pRFM2g->initializeCardMemory(k)) {
                Log(QString("卡%1反射内存地址映射初始化成功").arg(k));
            } else {
                Log(QString("卡%1反射内存地址映射初始化失败").arg(k));
            }
        }
    }

    // 更新布局
    switchLayout(Layout_4x2);

    // 重新初始化通道信息窗口，应用保存的通道参数
    initReflectFileInfWidget();

    // 初始化硬件触发
    initHardwareTrigger();

    // 启用开始按钮
    btnStart->setEnabled(true);
    m_deviceConnected = true;

    Log(QString("PCIe设备初始化完成，共识别%1张卡捏").arg(cardNumbers));
}

// 初始化硬件触发
void MainProcess::initHardwareTrigger()
{
    // 设置硬件触发主卡
    setTrigMainCard();

    // 初始化UDP socket用于硬件触发
    if (trigMode->currentIndex() == 1) { // 硬件触发模式
        quint16 port = ui->localPort->text().toUInt();
        if(udpHandle->bind(port)){
            connect(udpHandle,&QUdpSocket::readyRead,this,&MainProcess::udpReceive);
            btnStart->setEnabled(false);
            sp_gunTime->setReadOnly(true);
            sp_gunTime->setStyleSheet("QSpinBox { background-color: lightgray; color: gray;}");
            sp_gunTime->setValue(5000);
            sp_gunNumber->setReadOnly(true);
            sp_gunNumber->setStyleSheet("QSpinBox { background-color: lightgray; color: gray;}");
            Log("硬件触发模式初始化完成，可以接收远程指令控制...");
        } else {
            Log("硬件触发UDP端口绑定失败");
        }
    }
}

// 断联PCIe设备
void MainProcess::disconnectPcieDevices()
{
    if (!m_deviceConnected) {
        Log("设备未连接，无需断联");
        return;
    }

    Log("开始断联PCIe设备...");

    // 保存当前的通道参数
    saveIniFile();

    // 停止所有采集
    stopCollection();

    // 清理硬件触发
    if (trigMode->currentIndex() == 1) {
        udpHandle->abort();
        disconnect(udpHandle, &QUdpSocket::readyRead, this, &MainProcess::udpReceive);
        Log("硬件触发已关闭");
    }

    // ... 其他清理代码（停止线程、删除对象等）...

    Log("PCIe设备断联完成，通道参数已保存");
}

//初始化通道信息窗口
void MainProcess::initReflectFileInfWidget()
{
    QStringList hList,vList;
    hList<<"参考"<<"测量"<<"相位1"<<"相位2"<<"相位3"<<"卡存储开关";
    for(int i=0;i<cardNumbers;i++)
    {
        vList.append(QString::number(i+1));
    }
    ui->tableWidget->setColumnCount(6);
    ui->tableWidget->setRowCount(cardNumbers);
    ui->tableWidget->setHorizontalHeaderLabels(hList);
    ui->tableWidget->setVerticalHeaderLabels(vList);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget->verticalHeader()->setMinimumSectionSize(150); // 大幅增加行高
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    // 设置行高
    for (int i = 0; i < cardNumbers; i++) {
        ui->tableWidget->setRowHeight(i, 150); // 设置具体行高
    }

    // 读取配置文件中的通道参数
    QSettings pConfig(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
    pConfig.beginGroup("CHANNEL_PARA");

    for(int i=0; i<cardNumbers; i++)
    {
        // 为前5列创建通道信息控件
        for(int j=0; j<5; j++)
        {
            QWidget *paraWidget = new QWidget;
            QHBoxLayout *hlayout = new QHBoxLayout;
            QVBoxLayout *vlayout = new QVBoxLayout;
            QVBoxLayout *vlayout2 = new QVBoxLayout;
            hlayout->setAlignment(Qt::AlignCenter);
            vlayout->setAlignment(Qt::AlignCenter);
            vlayout2->setAlignment(Qt::AlignCenter);

            // 增加布局间距
            hlayout->setSpacing(20);
            vlayout->setSpacing(10);
            vlayout2->setSpacing(10);

            QLabel *namelabel = new QLabel("通道名称");
            namelabel->setAlignment(Qt::AlignCenter);
            namelabel->setMinimumHeight(25); // 增加标签高度
            QLineEdit *nameEdit = new QLineEdit;
            nameEdit->setMinimumHeight(35); // 大幅增加输入框高度

            // 从配置文件读取通道名称
            QString part;
            switch (j) {
            case 0: part = "_a1"; break;
            case 1: part = "_a2"; break;
            case 2: part = "_density1"; break;
            case 3: part = "_density2"; break;
            case 4: part = "_density3"; break;
            }
            QString channelName = pConfig.value("Card" + QString::number(i + 1) + part + "_ini", "").toString();
            nameEdit->setText(channelName);

            vlayout->addWidget(namelabel);
            vlayout->addWidget(nameEdit);
            hlayout->addLayout(vlayout);

            QLabel *idlabel = new QLabel("通道ID");
            idlabel->setAlignment(Qt::AlignCenter);
            idlabel->setMinimumHeight(25); // 增加标签高度
            QLineEdit *idEdit = new QLineEdit;
            idEdit->setMinimumHeight(35); // 大幅增加输入框高度

            // 从配置文件读取通道ID
            QString channelId = pConfig.value("Card" + QString::number(i + 1) + part + "_id", QString::number(j+1)).toString();
            idEdit->setText(channelId);

            vlayout2->addWidget(idlabel);
            vlayout2->addWidget(idEdit);
            hlayout->addLayout(vlayout2);

            paraWidget->setLayout(hlayout);
            ui->tableWidget->setCellWidget(i, j, paraWidget);
        }

        // 为第6列创建卡存储开关
        QWidget *storageWidget = new QWidget;
        QVBoxLayout *storageLayout = new QVBoxLayout;
        storageLayout->setAlignment(Qt::AlignCenter);
        storageLayout->setSpacing(15); // 增加开关布局间距

        QLabel *storageBtnLabel = new QLabel(QString("卡%1存储").arg(i+1));
        storageBtnLabel->setAlignment(Qt::AlignCenter);
        storageBtnLabel->setMinimumHeight(25); // 增加标签高度
        QPushButton *storageBtn = new QPushButton;
        storageBtn->setObjectName(QString("cardStorageBtn_%1").arg(i+1));
        storageBtn->setFixedSize(40, 40); // 增大开关按钮
        storageBtn->setStyleSheet("QPushButton{min-width:40px;min-height:40px;border-radius:20px;"
                                  "border:3px solid gray;background-color:rgb(0,60,0);}"
                                  "QPushButton::checked{background-color:rgb(0,255,0)}"
                                  "QPushButton::disabled{background-color:rgb(230,230,230)}");
        storageBtn->setCheckable(true);

        // 从配置文件读取存储开关状态
        bool storageState = pConfig.value("Card" + QString::number(i + 1) + "_storage", true).toBool();
        storageBtn->setChecked(storageState);

        storageLayout->addWidget(storageBtnLabel);
        storageLayout->addWidget(storageBtn);
        storageWidget->setLayout(storageLayout);
        ui->tableWidget->setCellWidget(i, 5, storageWidget);
    }

    QStringList realList;
    realList<<"相位1"<<"相位2"<<"相位3";
    ui->tableWidget_2->setColumnCount(3);
    ui->tableWidget_2->setRowCount(cardNumbers);
    ui->tableWidget_2->setHorizontalHeaderLabels(realList);
    ui->tableWidget_2->setVerticalHeaderLabels(vList);
    ui->tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableWidget_2->verticalHeader()->setMinimumSectionSize(150); // 大幅增加行高
    ui->tableWidget_2->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->tableWidget_2->setSelectionMode(QAbstractItemView::SingleSelection);

    // 设置行高
    for (int i = 0; i < cardNumbers; i++) {
        ui->tableWidget_2->setRowHeight(i, 150); // 设置具体行高
    }

    for(int i=0; i<cardNumbers; i++)
    {
        for(int j=0; j<3; j++)
        {
            QWidget *paraWidget = new QWidget;
            QHBoxLayout *hlayout = new QHBoxLayout;
            QVBoxLayout *vlayout = new QVBoxLayout;
            QVBoxLayout *vlayout2 = new QVBoxLayout;
            hlayout->setAlignment(Qt::AlignCenter);
            vlayout->setAlignment(Qt::AlignCenter);
            vlayout2->setAlignment(Qt::AlignCenter);

            // 增加布局间距
            hlayout->setSpacing(20);
            vlayout->setSpacing(10);
            vlayout2->setSpacing(10);

            QLabel *namelabel = new QLabel("通道名称");
            namelabel->setAlignment(Qt::AlignCenter);
            namelabel->setMinimumHeight(25); // 增加标签高度
            QLineEdit *nameEdit = new QLineEdit;
            nameEdit->setMinimumHeight(35); // 大幅增加输入框高度

            // 从配置文件读取实时数据通道名称
            QString part;
            switch (j) {
            case 0: part = "_RT_a1"; break;
            case 1: part = "_RT_a2"; break;
            case 2: part = "_RT_density1"; break;
            }
            QString realChannelName = pConfig.value("Card" + QString::number(i + 1) + part + "_ini", "").toString();
            nameEdit->setText(realChannelName);

            vlayout->addWidget(namelabel);
            vlayout->addWidget(nameEdit);
            hlayout->addLayout(vlayout);

            QLabel *idlabel = new QLabel("通道ID");
            idlabel->setAlignment(Qt::AlignCenter);
            idlabel->setMinimumHeight(25); // 增加标签高度
            QLineEdit *idEdit = new QLineEdit;
            idEdit->setMinimumHeight(35); // 大幅增加输入框高度

            // 从配置文件读取实时数据通道ID
            QString realChannelId = pConfig.value("Card" + QString::number(i + 1) + part + "_id", QString::number(j+1)).toString();
            idEdit->setText(realChannelId);

            vlayout2->addWidget(idlabel);
            vlayout2->addWidget(idEdit);
            hlayout->addLayout(vlayout2);

            paraWidget->setLayout(hlayout);
            ui->tableWidget_2->setCellWidget(i, j, paraWidget);
        }
    }

    pConfig.endGroup();
}

//创建布局窗口
QWidget* MainProcess::createParaWidget(bool realTime,IniFileInf *inf)
{
    QWidget *paraWidget = new QWidget;
    QHBoxLayout *hlayout = new QHBoxLayout;
    QVBoxLayout *vlayout = new QVBoxLayout;
    QVBoxLayout *vlayout1 = new QVBoxLayout;
    QVBoxLayout *vlayout2 = new QVBoxLayout;
    hlayout->setAlignment(Qt::AlignCenter);
    vlayout->setAlignment(Qt::AlignCenter);
    vlayout1->setAlignment(Qt::AlignCenter);
    vlayout2->setAlignment(Qt::AlignCenter);

    QLabel *namelabel = new QLabel("inf通道名称");
    namelabel->setAlignment(Qt::AlignCenter);
    QLineEdit *nameEdit = new QLineEdit;
    if(realTime)
        inf->realTime_ini_List.append(nameEdit->text());
    else
        inf->ini_List.append(nameEdit->text());
    vlayout->addWidget(namelabel);
    vlayout->addWidget(nameEdit);
    hlayout->addLayout(vlayout);

    if(!realTime)
    {
        QLabel *btnlabel = new QLabel("存储");
        btnlabel->setAlignment(Qt::AlignCenter);
        QPushButton *btn = new QPushButton;
        btn->setStyleSheet("QPushButton{min-width:20px;min-height:20px;border-radius:12px;"
                           "border:2px solid gray;background-color:rgb(0,60,0);}"
                           "QPushButton::checked{background-color:rgb(0,255,0)}"
                           "QPushButton::disabled{background-color:rgb(230,230,230)}");
        btn->setCheckable(true);
        inf->save_List.append(btn->isChecked());
        vlayout1->addWidget(btnlabel);
        vlayout1->addWidget(btn);
        hlayout->addLayout(vlayout1);
    }

    QLabel *idlabel = new QLabel("通道ID");
    idlabel->setAlignment(Qt::AlignCenter);
    QLineEdit *idEdit = new QLineEdit;
    if(realTime)
        inf->realTime_id_List.append(nameEdit->text());
    else
        inf->id_List.append(nameEdit->text());
    vlayout2->addWidget(idlabel);
    vlayout2->addWidget(idEdit);
    hlayout->addLayout(vlayout2);


    paraWidget->setLayout(hlayout);
    return paraWidget;

}

//创建布局窗口2
QWidget* MainProcess::createParaWidget2()
{
    QWidget *paraWidget = new QWidget;
    QHBoxLayout *hlayout = new QHBoxLayout;
    QVBoxLayout *vlayout = new QVBoxLayout;
    QVBoxLayout *vlayout1 = new QVBoxLayout;
    QVBoxLayout *vlayout2 = new QVBoxLayout;
    hlayout->setAlignment(Qt::AlignCenter);
    vlayout->setAlignment(Qt::AlignCenter);
    vlayout1->setAlignment(Qt::AlignCenter);
    vlayout2->setAlignment(Qt::AlignCenter);


    QPushButton *btn = new QPushButton;
    btn->setStyleSheet("QPushButton{min-width:20px;min-height:20px;border-radius:12px;"
                       "border:2px solid gray;background-color:rgb(0,60,0);}"
                       "QPushButton::checked{background-color:rgb(0,255,0)}"
                       "QPushButton::disabled{background-color:rgb(230,230,230)}");
    btn->setCheckable(true);

    Fliplist.append(btn->isChecked());
    vlayout1->addWidget(btn);
    hlayout->addLayout(vlayout1);

    paraWidget->setLayout(hlayout);
    return paraWidget;

}

//创建临时窗口
QWidget* MainProcess::createTmpWidget()
{
    QWidget *tmp = new QWidget();
    tmp->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    return tmp;
}

//创建临时标签
QLabel* MainProcess::createTmpLabel(int size)
{
    QLabel *tmp = new QLabel();
    tmp->setFixedWidth(size);
    return tmp;
}

//初始化pcie卡
// int MainProcess::pcieInit()
// {
//     pathList.clear();
//     DWORD numDevice;
//     numDevice = scanPcieDevice(GUID_DEVINTERFACE_XDMA,&pathList,261);
//     return numDevice;
// }

//扫描设备
int MainProcess::scanPcieDevice(GUID guid, QStringList *dev_path, DWORD32 len_devpath)
{
    SP_DEVICE_INTERFACE_DATA device_interface;
    PSP_DEVICE_INTERFACE_DETAIL_DATA dev_detail;
    HDEVINFO device_info;
    DWORD index = 0;
    wchar_t tmp[MAX_PATH_LENGEH+1];
    char devpath[MAX_PATH_LENGEH+1];

    device_info = SetupDiGetClassDevs((LPGUID)&guid,NULL,NULL,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(device_info == INVALID_HANDLE_VALUE){
        return 0;
    }

    device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (index = 0; SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, index, &device_interface); ++index) {
        ULONG detailLength = 0;
        if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &detailLength, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            break;
        }

        dev_detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, detailLength);
        if (!dev_detail) {
            break;
        }
        dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, dev_detail, detailLength, NULL, NULL)) {
            HeapFree(GetProcessHeap(), 0, dev_detail);
            break;
        }

        StringCchCopy(tmp, len_devpath, dev_detail->DevicePath);
        wcstombs(devpath, tmp, 256);
        dev_path->append(devpath);
        HeapFree(GetProcessHeap(), 0, dev_detail);
    }
    SetupDiDestroyDeviceInfoList(device_info);
    return index;
}


void MainProcess::showReal(int cardIndex)
{
    // if(threadRealDataLoader->isRunning()){
    //     qWarning() << "重复启动";
    //     return;
    // }

    // xdata[0].clear();
    // ydata[0].clear();
    // xdata[1].clear();
    // ydata[1].clear();

    QString gunNumber = getGunNumber();

    bool isLittleEndian = true;// ui->cboByteOrderReal->isChecked();
    QDataStream::ByteOrder byteOrder = isLittleEndian ? QDataStream::LittleEndian : QDataStream::BigEndian;

    disconnect(threadRealDataLoaderList[cardIndex], &QThread::started, nullptr, nullptr);
    disconnect(realdataLoaderList[cardIndex], &RealDataLoader::dataLoaded, nullptr, nullptr);
    disconnect(realdataLoaderList[cardIndex], &RealDataLoader::loadingFinished, nullptr, nullptr);

    realdataLoaderList[cardIndex]->moveToThread(threadRealDataLoaderList[cardIndex]);


    connect(realdataLoaderList[cardIndex], &RealDataLoader::dataLoaded, this, &MainProcess::onShowReal);
    connect(realdataLoaderList[cardIndex], &RealDataLoader::loadingFinished, this, &MainProcess::onLoadingFinished);
    connect(threadRealDataLoaderList[cardIndex], &QThread::started, [this, byteOrder, cardIndex, gunNumber]() {
        QVector<QCPGraphData> vecdata1,vecdata2,vecdata3,vecdata4,vecdata5,pNullVec;
        QCPGraphData gp_data1,gp_data2,gp_data3,gp_data4,gp_data5;
        QSharedPointer<QCPGraphDataContainer> ch1_graph,ch2_graph,ch3_graph,ch4_graph,ch5_graph;

        ch1_graph = plotList[cardIndex]->plot->graph(0)->data();
        ch1_graph->clear();
        ch2_graph = plotList[cardIndex]->plot->graph(1)->data();
        ch2_graph->clear();
        ch3_graph = plotList[cardIndex]->plot->graph(2)->data();
        ch3_graph->clear();

        qDebug() << " showReal " << cardIndex << gunNumber;

        QString fileRealDatPath = QString("%1/%2REALTIME_DATA_CARD%3.DAT").arg(m_dataPath).arg(gunNumber).arg(cardIndex+1);
        QString fileRealInfPath = QString("%1/%2REALTIME_DATA_CARD%3.INF").arg(m_infPath).arg(gunNumber).arg(cardIndex+1);


        int mode = ui->txtsingal_mode->text().toInt();

        if (mode == 0) {
            this->realdataLoaderList[cardIndex]->loadFile(cardIndex, fileRealDatPath, mapXdata, mapYdata, byteOrder);
            // （这个加点画图废除了）
            // 单频
            for (int i = 0; i < mapXdata[cardIndex][0].size(); ++i) {
                QCPGraphData dataPoint(mapXdata[cardIndex][0][i], mapYdata[cardIndex][0][i]);
                ch1_graph->add(dataPoint);
            }
        } else {
            this->realdataLoaderList[cardIndex]->loadThreeFile(cardIndex, fileRealDatPath, mapXdata, mapYdata, byteOrder);
            // 三频
            // 第一条曲线 - 前32位数据
            for (int i = 0; i < mapXdata[cardIndex][0].size(); ++i) {
                QCPGraphData dataPoint1(mapXdata[cardIndex][3][i], mapYdata[cardIndex][3][i]);
                ch1_graph->add(dataPoint1);
            }

            // 第二条曲线 - 中间16位数据
            for (int i = 0; i < mapXdata[cardIndex][2].size(); ++i) {
                QCPGraphData dataPoint2(mapXdata[cardIndex][2][i], mapYdata[cardIndex][2][i]);
                ch2_graph->add(dataPoint2);
            }

            // 第三条曲线 - 最后16位数据
            for (int i = 0; i < mapXdata[cardIndex][3].size(); ++i) {
                QCPGraphData dataPoint3(mapXdata[cardIndex][0][i], mapYdata[cardIndex][0][i]);
                ch3_graph->add(dataPoint3);
            }
        }


        replotChart(cardIndex);
    });
    connect(realdataLoaderList[cardIndex], &RealDataLoader::loadingFinished, threadRealDataLoaderList[cardIndex], &QThread::quit);

    threadRealDataLoaderList[cardIndex]->start();
}

void MainProcess::onShowReal(int cardIndex, const QVector<QVector<double>> &xdata, const QVector<QVector<double>> &ydata,double xmax, double ymax)
{
    QString msg = QString("卡%1准备绘制.. 点数%2").arg(cardIndex+1).arg(xdata[0].size());
    Log(msg);
}

void MainProcess::onLoadingFinished(int cardIndex)
{
    QString msg = QString("卡%1绘制完成").arg(cardIndex+1);
    Log(msg);
}


void MainProcess::showDDr(int cardIndex )
{
    // xdata.resize(2);
    // ydata.resize(2);

    // if(threadDataLoader->isRunning()){
    //     qWarning() << "重复启动";
    //     return;
    // }

    QString gunNumber = getGunNumber();

    bool isLittleEndian = true;// ui->cboByteOrder->isChecked();
    QDataStream::ByteOrder byteOrder = isLittleEndian ? QDataStream::LittleEndian : QDataStream::BigEndian;

    disconnect(threadDataLoaderList[cardIndex], &QThread::started, nullptr, nullptr);
    disconnect(dataLoaderList[cardIndex], &DataLoader::dataLoaded, nullptr, nullptr);
    disconnect(dataLoaderList[cardIndex], &DataLoader::loadingFinished, nullptr, nullptr);

    dataLoaderList[cardIndex]->moveToThread(threadDataLoaderList[cardIndex]);

    // connect(dataLoaderList[cardIndex], &DataLoader::dataLoaded, this, &MainWindow::plotData);
    // connect(dataLoaderList[cardIndex], &DataLoader::loadingFinished, this, &MainWindow::onLoadingFinished);
    connect(threadDataLoaderList[cardIndex], &QThread::started, [this, byteOrder, cardIndex, gunNumber ]() {


        QVector<QCPGraphData> vecdata1,vecdata2,vecdata3,vecdata4,vecdata5,pNullVec;
        QCPGraphData gp_data1,gp_data2,gp_data3,gp_data4,gp_data5;

        // 通道1的5个图
        QSharedPointer<QCPGraphDataContainer> ch1_graph,ch2_graph,ch3_graph,ch4_graph,ch5_graph;
        ch1_graph = plotList[cardIndex]->plot->graph(0)->data();
        ch2_graph = plotList[cardIndex]->plot->graph(1)->data();
        ch3_graph = plotList[cardIndex]->plot->graph(2)->data();
        ch4_graph = plotList[cardIndex]->plot->graph(3)->data();
        ch5_graph = plotList[cardIndex]->plot->graph(4)->data();
        ch2_graph->clear();
        ch3_graph->clear();

        Log("准备绘制DDR数据 数据量较大请耐心等待...");
        for(int t=0;t<2;t++)
        {
            QString fileDDrDatPath = QString("%1/%2DDR3_DATA_CARD%3_CH%4.DAT").arg(m_dataPath).arg(gunNumber).arg(cardIndex+1).arg(t+1);
            QString fileDDrInfPath = QString("%1/%2DDR3_DATA_CARD%3_CH%4.INF").arg(m_infPath).arg(gunNumber).arg(cardIndex+1).arg(t+1);



            // 清空已有数据
            if(t+1 == 1)
            {
                QVector<double> xdata1, ydata1;
                this->dataLoaderList[cardIndex]->loadFile(t+1, fileDDrDatPath, xdata1, ydata1, byteOrder);

                for (int i = 0; i < xdata1.size() ; ++i) {
                    QCPGraphData dataPoint(xdata1[i], ydata1[i]);
                    ch2_graph->add(dataPoint);
                }

                Log("绘制DDR数据 通道一完成...");
            }
            if(t+1 == 2)
            {
                QVector<double> xdata1, ydata1;
                this->dataLoaderList[cardIndex]->loadFile(t+1, fileDDrDatPath, xdata1, ydata1, byteOrder);

                for (int i = 0; i < xdata1.size() ; ++i) {
                    QCPGraphData dataPoint(xdata1[i], ydata1[i]);
                    ch3_graph->add(dataPoint);
                }

                Log("绘制DDR数据 通道二完成...");
            }
        }

        replotChart(cardIndex);
    });
    connect(dataLoaderList[cardIndex], &DataLoader::loadingFinished, threadDataLoaderList[cardIndex], &QThread::quit);

    threadDataLoaderList[cardIndex]->start();
}

QString MainProcess::getGunNumber()
{
    QString gunNumber = QString::number(sp_gunNumber->value()); // 将整数转换为字符串
    // 确保字符串长度为5位，不足则前面补0
    gunNumber = gunNumber.rightJustified(5, '0');

    return gunNumber;
}

void MainProcess::startReal()
{
    if (threadRealList.length() == 0) {
        Log("未识别任何板卡");
        ctlStatus(Stop);
        return;
    }

    if (threadRealList[0]->isRunning()) {
        Log("重复启动");
        ctlStatus(Stop);
        return;
    }

    m_finishedCards.store(0);//重置计数

    // 清空三频模式第一相位值缓存
    m_triplePhase1Values.clear();

    float collectTime = (sp_gunTime->value()+ui->extraSaveTime->value())* 1000*1024/990; //us
    QString gunNumber = getGunNumber();


    ui->textLog->setText("");


    bool isLittleEndian = true;// ui->cboByteOrderReal->isChecked();
    QDataStream::ByteOrder byteOrder = isLittleEndian ? QDataStream::LittleEndian : QDataStream::BigEndian;

    int triggerMode = trigMode->currentIndex();  //ui->cboRealSoftorHard->isChecked();
    int signalMode = ui->txtsingal_mode->text().toInt();

    // 软件触发校准
    if (trigBeforeHard) {
        triggerMode = 0;
        collectTime = 200000;
        trigBeforeHard = false;
    }

    qDebug() << "on_btnStartReal_clicked main线程地址: " << QThread::currentThread() <<" collectTime" <<collectTime << " triggerMode "<< triggerMode<< " gunNumber "<< gunNumber;

    QString msg = QString("开始实时采集... 采集时间%1us 触发模式%2(0软件1硬件) 炮号%3 单频or三频%4").arg(collectTime).arg(triggerMode).arg(gunNumber).arg(signalMode);
    Log(msg);

    // 初始化反射内存卡参数数据
    if (pRFM2g && pRFM2g->isOpened()) {
        for(int k = 0; k < cardNumbers; k++) {
            RFM2G_ParamData paramData;
            paramData.cardIndex = k;
            paramData.triggerMode = triggerMode;
            paramData.workMode = signalMode;
            paramData.sampleInterval = 500;  // 500us采样间隔
            paramData.startTime = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;
            paramData.reserved[0] = collectTime;
            paramData.reserved[1] = 0;
            paramData.reserved[2] = 0;

            pRFM2g->writeParamData(k, paramData);
        }
    }


    for(int k = 0; k < cardNumbers; k ++)
    {
        if (plotList[k] != nullptr) {
        disconnect(threadRealList[k], &QThread::started, nullptr, nullptr);
        disconnect(pcieRealList[k], &PcieProtocol::realdataLoaded, nullptr, nullptr);
        disconnect(pcieRealList[k], &PcieProtocol::report, nullptr, nullptr);
        //disconnect(pcieList[k], &PcieProtocol::finished, nullptr, nullptr);

        pcieRealList[k]->moveToThread(threadRealList[k]);

        connect(threadRealList[k], &QThread::started, pcieRealList[k], [this, k, collectTime, gunNumber, byteOrder, triggerMode, signalMode](){

            bool ok;
            int vFlip = 0;// this->ui->txtFlip->text().toInt(&ok);
            int vPosition = 3;//this->ui->txtPosition->text().toInt(&ok);
            int vFiterWidth = 10;//this->ui->txtFiterWidth->text().toInt(&ok);
            int sampleInterval = 500; // 1000;//this->ui->txtsampleInterval->text().toInt(&ok);
            int singleSampleTime = 500; // 1000;//this->ui->txtsingleSampleTime->text().toInt(&ok);

            pcieRealList[k]->m_regParams.singal_mode = signalMode; // UI统一控制单/三频模式
            pcieRealList[k]->cmdSetDefaultParams();

            QString fileRealDatPath = QString("%1/%2REALTIME_DATA_CARD%3.DAT").arg(m_dataPath).arg(gunNumber).arg(k+1);
            QString fileRealInfPath = QString("%1/%2REALTIME_DATA_CARD%3.INF").arg(m_infPath).arg(gunNumber).arg(k+1);
            if(signalMode == 1)
            {
                //pcieRealList[k]->collectRealTimeThreeData(triggerMode, collectTime, singleSampleTime, sampleInterval, fileRealDatPath, fileRealInfPath, byteOrder);
                pcieRealList[k]->collectRealTimeData(triggerMode, collectTime, singleSampleTime, sampleInterval, fileRealDatPath, fileRealInfPath, byteOrder);
            }
            else{
                pcieRealList[k]->collectRealTimeData(triggerMode, collectTime, singleSampleTime, sampleInterval, fileRealDatPath, fileRealInfPath, byteOrder);
            }
        });

        connect(pcieRealList[k], &PcieProtocol::offlineProgress, this, &MainProcess::handleOfflineProgress, Qt::UniqueConnection);
        connect(pcieRealList[k], &PcieProtocol::offlineFinished, this, &MainProcess::handleOfflineFinished, Qt::UniqueConnection);
        connect(pcieRealList[k], &PcieProtocol::offlineFinished, threadRealList[k], &QThread::quit, Qt::UniqueConnection);
        connect(pcieRealList[k], &PcieProtocol::realdataLoaded, this, &MainProcess::handleRealProgress, Qt::QueuedConnection);
        connect(pcieRealList[k], &PcieProtocol::report, this, [this](QString msg) {
                this->Log(msg);
            }, Qt::UniqueConnection);
        connect(pcieRealList[k], &PcieProtocol::realFinished, this, &MainProcess::handleRealFinished, Qt::UniqueConnection);
        //connect(pcieRealList[k], &PcieProtocol::realFinished, threadRealList[k], &QThread::quit, Qt::UniqueConnection);
        connect(this, &MainProcess::beginDDR, pcieRealList[k], &PcieProtocol::collectOfflineAlldata, Qt::UniqueConnection);

        // 增加FIFO溢出警告处理
        connect(pcieRealList[k], &PcieProtocol::fifoOverflowWarning, this, [this](int cardindex, double fillRatio) {
            this->handleFifoOverflow(cardindex, fillRatio);
        }, Qt::QueuedConnection);

        // 设置较高的线程优先级，确保采集线程获得足够的CPU时间
        threadRealList[k]->setPriority(QThread::HighPriority);

        threadRealList[k]->start();
    }

    }
    timerRealCollect.restart();
}


void MainProcess::stopReal()
{
    // 检查是否有正在运行的线程
    if (threadRealList.isEmpty()) {
        Log("没有正在运行的采集任务");
        ctlStatus(Stop);
        return;
    }

    // 更新UI状态
    ctlStatus(Stop);

    // 发送停止信号给所有卡
    for(int k = 0; k < cardNumbers; ++k) {
        if (threadRealList[k]->isRunning()) {
            // 调用 PcieProtocol 的停止方法
            //pcieRealList[k]->cmdStopCollect(); // 假设有一个这样的命令可以停止采集
            // 或者直接断开某些连接，防止进一步处理
            disconnect(pcieRealList[k], &PcieProtocol::realdataLoaded, nullptr, nullptr);
            disconnect(pcieRealList[k], &PcieProtocol::report, nullptr, nullptr);

            // 请求线程退出
            threadRealList[k]->requestInterruption();
            // 可选：尝试优雅地结束线程
            threadRealList[k]->quit();
            threadRealList[k]->wait(); // 等待线程完成退出
        }
    }

    // 重置计时器
    timerRealCollect.invalidate();

    // 更新反射内存卡状态数据 - 标记停止状态
    if (pRFM2g && pRFM2g->isOpened()) {
        for(int k = 0; k < cardNumbers; k++) {
            RFM2G_StatusData statusData;
            statusData.cardIndex = k;
            statusData.isActive = 0;  // 停止状态
            statusData.totalSamples = 0;
            statusData.currentFifo = 0;
            statusData.lastUpdateTime = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;
            statusData.errorCount = 0;
            statusData.reserved[0] = 0;
            statusData.reserved[1] = 0;

            pRFM2g->writeStatusData(k, statusData);
        }
    }

    Log("实时采集已停止");
}

void MainProcess::handleOfflineProgress(int cardindex, int value) {
    QString msg = QString("卡%1 DDR采集中... 通道 %2").arg(cardindex+1).arg(value)  ;
    Log(msg);
}

void MainProcess::handleOfflineFinished(int cardindex) {
    QString msg = QString("卡%1 DDR 全部通道采集结束").arg(cardindex+1)  ;
    Log(msg);



    // 最后的文件整理 （旧）
    // 增加已完成的卡数
    int finishedCount = m_finishedCards.fetchAndAddRelaxed(1) + 1;
    msg = QString("已完成%1张卡").arg(finishedCount)  ;
    Log(msg);
    // 检查是否所有卡都已经完成
    if (finishedCount == getValidPlotCount()) {
        msg = QString("本次炮号所有卡的采集均已经完成") ;
        Log(msg);
        // consolidatedFineDocuments();

        on_btnFileSync_clicked();
    }



    // 当操作完成时，更新状态显示
    labStatus->setStyleSheet("QLabel{border-radius:14px;"
                             "background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5,"
                             "stop:0 rgb(0, 255, 0),"
                             "stop:0.79 rgb(0, 255, 0),"
                             "stop:0.8 rgb(100, 100, 100),"
                             "stop:0.9 rgb(255, 255, 255),"
                             "stop:1 rgb(0, 0, 0));}");
    // 重置计数和状态，准备下一次操作
    cnt = 0;
    firstTrig = true;
    ctlStatus(Stop);
    emit collectionFinished();

}

int MainProcess::getValidPlotCount() const
{
    int count = 0;
    for (PlanarGraph* plot : plotList) {
        if (plot != nullptr) {
            count++;
        }
    }
    return count;
}

//数据绘图在这里
void MainProcess::handleRealProgress(int cardindex, int index, int fifoLen, int32_t valueAll)
{
    // 定义显示频率参数 越大显示的越少 占用的系统资源越不频繁
    const int log_show_span = 100;
    static int cnt = 0;
    if(index % 1 == 0)//就问你什么整数对1取余不是0？
    {
        int us = timerRealCollect.nsecsElapsed()/1000;
        int mode = ui->txtsingal_mode->text().toInt();

        QLOG_INFO() << " handleRealProgress " << index << " " << valueAll;//index是第x个点，也可以说handleRealProgress执行的次数

        double allValuef = (double)(valueAll)/8192*180/M_PI;  //
        cnt++ ;

        // 图表显示
        QSharedPointer<QCPGraphDataContainer> ch1_graph = plotList[cardindex]->plot->graph(0)->data();
        QSharedPointer<QCPGraphDataContainer> ch2_graph = plotList[cardindex]->plot->graph(1)->data();
        QSharedPointer<QCPGraphDataContainer> ch3_graph = plotList[cardindex]->plot->graph(2)->data();
        if(index == 0)
        {
            ch1_graph->clear();
            ch2_graph->clear();
            ch3_graph->clear();
        }

        // 原来代码移入单频模式中
        if(mode == 0)
        {
            QCPGraphData dataPoint(index/2, allValuef);
            ch1_graph->add(dataPoint);

            // 写入反射内存卡 - 单频模式
            if (pRFM2g && pRFM2g->isOpened()) {
                RFM2G_RealtimeData rfmData;
                rfmData.cardIndex = cardindex;
                rfmData.sampleIndex = index/2;
                rfmData.mode = 0;  // 单频模式
                rfmData.timestamp = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;
                rfmData.data.single.singleValue = allValuef;
                rfmData.data.single.reserved[0] = fifoLen;
                rfmData.data.single.reserved[1] = us;

                pRFM2g->writeRealtimeData(cardindex, rfmData);
            }

            if(index % log_show_span == 0)
            {
                QString msg = QString("卡%1实时采集中... 标识%2 当前FIFO长度%3 本轮消耗%4微秒 采集值%5").arg(cardindex+1).arg(index).arg(fifoLen).arg(us).arg(allValuef) ;
                Log(msg);
            }
        }
        else if(mode == 1)
        {
            QString msg;
            if(index % 2 == 0)
            {
                QCPGraphData dataPoint(index/2, allValuef);
                ch1_graph->add(dataPoint);

                // 保存第一相位值供下次使用
                m_triplePhase1Values[cardindex] = allValuef;

                msg = QString("卡%1实时采集中... 标识%2 当前FIFO长度%3 本轮消耗%4微秒 采集值%5").arg(cardindex+1).arg(index).arg(fifoLen).arg(us).arg(allValuef) ;
            }

            if(index % 2 == 1)
            {
                int16_t valueMid = static_cast<int16_t>((valueAll >> 16) & 0xFFFF);
                int16_t valueLow = static_cast<int16_t>(valueAll & 0xFFFF);

                double valueMidf = (double)(valueMid)/256*180/M_PI;// 原/8192
                double valueLowf = (double)(valueLow)/256*180/M_PI;// 原/8192
                QCPGraphData dataPoint2(index/2, valueLowf);
                ch2_graph->add(dataPoint2);
                // 中16放到作为第三个图表 相位三
                QCPGraphData dataPoint3(index/2, valueMidf);
                ch3_graph->add(dataPoint3);

                // 写入反射内存卡 - 三频模式（完整采样点）
                if (pRFM2g && pRFM2g->isOpened()) {
                    RFM2G_RealtimeData rfmData;
                    rfmData.cardIndex = cardindex;
                    rfmData.sampleIndex = index / 2;  // 采样点序号
                    rfmData.mode = 1;  // 三频模式
                    rfmData.timestamp = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;
                    rfmData.data.triple.phase1 = m_triplePhase1Values.value(cardindex, 0.0);  // 第一相位（来自前一次index%2==0）
                    rfmData.data.triple.phase2 = valueLowf;    // 第二相位（低16位）
                    rfmData.data.triple.phase3 = valueMidf;    // 第三相位（高16位）

                    pRFM2g->writeRealtimeData(cardindex, rfmData);
                }

                msg = QString("卡%1实时采集中... 标识%2 当前FIFO长度%3 本轮消耗%4微秒 高16采集值%5 低16采集值%6").arg(cardindex+1).arg(index).arg(fifoLen).arg(us).arg(valueMid).arg(valueLow) ;
            }

            if(index % log_show_span == 0)
            {
                Log(msg);
            }

        }

        // 每1000个采样点更新一次状态数据
        if (index % 1000 == 0 && pRFM2g && pRFM2g->isOpened()) {
            RFM2G_StatusData statusData;
            statusData.cardIndex = cardindex;
            statusData.isActive = 1;  // 活跃状态
            statusData.totalSamples = index;
            statusData.currentFifo = fifoLen;
            statusData.lastUpdateTime = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;
            statusData.errorCount = 0;  // 暂时设为0
            statusData.reserved[0] = mode;
            statusData.reserved[1] = us;

            pRFM2g->writeStatusData(cardindex, statusData);
        }
    }

    if(index % 100 ==0)
    {
        replotChart(cardindex);
    }

    timerRealCollect.restart();
    QString totalPoint = QString("一共绘制了%1个点").arg(cnt);
    qDebug() << totalPoint;
}

void MainProcess::handleRealFinished(int cardindex) {
    QString gunNumber = getGunNumber();
    QString fileRealDatPath = QString("%1/%2REALTIME_DATA_CARD%3.DAT").arg(m_dataPath).arg(gunNumber).arg(cardindex+1);
    QString info = QString("卡%1实时采集结束 保存路径为./%2").arg(cardindex+1).arg(fileRealDatPath);
    Log(info);

    // 启动DDR采集
    int triggerMode = trigMode->currentIndex();
    int samplingRate = 12500000;    //20M
    float collectTime = (sp_gunTime->value()+ui->extraSaveTime->value())* 1000*1024/990;

    emit beginDDR(cardindex , 1, triggerMode, samplingRate, collectTime, m_dataPath, m_infPath, gunNumber);

    info = QString("卡%1开启DDR取数...").arg(cardindex+1);
    Log(info);

    // 整理一次文件 （旧）
    // consolidatedPhaseDiffDocuments();
}


void MainProcess::Log(QString log)
{
    ui->textLog->append(log);
}

void MainProcess::prePareDir()
{
    // 定义基础路径并创建目录
    //QString basePath = "D:/";
    QString basePath = "C:/";
    QDir baseDir(basePath);

    if(!baseDir.exists("Data")) {
        baseDir.mkdir("Data");
    }


    QString path = basePath + "Data";
    QDir dir(path+"/FIRdata");
    dir.mkdir(path+"/FIRdata");
    QString dirPath = path+"/FIRdata/"+QString::number(sp_gunNumber->value());
    dir.setPath(dirPath);
    dir.mkdir(dirPath);
    m_dataPath = dirPath+"/DATA";
    m_infPath = dirPath+"/INF";
    dir.setPath(m_dataPath);
    dir.mkdir(m_dataPath);
    dir.setPath(m_infPath);
    dir.mkdir(m_infPath);
}

// 开始采集
void MainProcess::startCollection()
{
//    // 检查炮号锁定状态
//        if (m_gunNumberLocked) {
//            // 在自动采集模式下，确保使用当前锁定的炮号
//            gunNumber = QString::number(sp_gunNumber->value());
//        }

    // 检查是否需要重新开始收集数据
    if(firstTrig || hardTrig)
    {
        // 重置计数器
        cnt=0;
        // 获取当前模式索引
        modeIndex = ui->Mode->currentIndex();
        // 获取炮号并转换为字符串
        gunNumber = QString::number(sp_gunNumber->value());

        // 如果硬件触发，则更新炮击信息标签
        if(!trigMode->currentIndex()){
            QString curTime = QDateTime::currentDateTime().toString("yyyy年MM月dd日hh:mm");
            QString inf = curTime+"     炮号："+gunNumber+"    时间："+ QString::number(sp_gunTime->value())+"毫秒";
            ui->lab_GunInf->setText(inf);
        }

        // 启动定时器
        // checkTimer->start(50);
        // 更新状态标签样式
        labStatus->setStyleSheet("QLabel{border-radius:14px;"
                                 "background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5,"
                                 "stop:0 rgb(255, 0, 0),"
                                 "stop:0.79 rgb(255, 0, 0),"
                                 "stop:0.8 rgb(100, 100, 100),"
                                 "stop:0.9 rgb(255, 255, 255),"
                                 "stop:1 rgb(0, 0, 0));}");

        // 清除所有图表数据
        for(int i=0;i<cardNumbers;i++)
        {
            if (plotList[i] != nullptr) {

            plotList[i]->plot->graph(0)->data().data()->clear();
            // plotList[i]->plot->graph(1)->data().data()->clear();
            // plotList[i]->plot->graph(2)->data().data()->clear();
            // plotList[i]->plot->graph(3)->data().data()->clear();
            // plotList[i]->plot->graph(4)->data().data()->clear();
            //hDevList[i]->ddrcnt = 0;

            QSharedPointer<QCPGraphDataContainer> ch1_graph;
            ch1_graph = plotList[i]->plot->graph(0)->data();
            ch1_graph->clear();

            replotChart(i);
            }
        }

        // 重置触发标志
       firstTrig = false;
        hardTrig = false;
        // 调用控制状态方法
        ctlStatus(Start);
    }

    // 清除时间测量列表
    tmList.clear();

    // 重置全局计数器
    g_cnt = 0;
    int address;

    startReal();
    return;



     // 如果是第二次收集数据，则执行数据整合操作
//    if(cnt == 1)
//    {
//      consolidatedPhaseDiffDocuments();
//    }

    // 如果是初始收集数据，则进行设备触发操作
    // if (cnt == 0) {
    //     unsigned char value;
    //     for(int i=0;i < cardNumber;i++)
    //     {
    //         // value = 2;
    //         // hDevList[i]->user_write(Trigger_Addr,4,&value,i);
    //         // value = 0;
    //         // hDevList[i]->user_write(Trigger_Addr,4,&value,i);

    //         // 这里进行复位动作 以及参数设置
    //         value = 1;
    //         //hDevList[i]->user_write(V2_SINGEL_RESET_Addr,4,&value,i);
    //     }
    // }



    QString fileName,infName;
    int index,c_length;

    // 根据不同的收集数据阶段，设置不同的地址和文件名 新版长度用不着
    switch (cnt) {
    case 0:
        address = RealTime_Phase_Addr;
        fileName = "readlTime_Data";
        index = ui->PickUp->currentIndex();
        switch (index) {
        case 0:
            c_length = 1000000;
            break;
        case 1:
            c_length = 100000;
            break;
        case 2:
            c_length = 50000;
            break;
        case 3:
            c_length = 20000;
            break;
        case 4:
            c_length = 10000;
            break;
        }

        timerRealCollect.restart();
        break;
    case 1:
        Log("第二轮DDR采集...");
        address = DDR3_Reference_Addr;
        fileName = "DDR3_Data";
        c_length = 1000*1000*10;
        break;
    case 2:
        address = DDR3_Measure_Addr;
        fileName = "DDR3_Data";
        c_length = 1000*1000*10;
        break;
    case 3:
        address = DDR3_Phase1_Addr;
        fileName = "DDR3_Data";
        c_length = 1000*1000*10;
        break;
    case 4:
        address = DDR3_Phase2_Addr;
        fileName = "DDR3_Data";
        c_length = 1000*1000*10;
        break;
    case 5:
        address = DDR3_Phase3_Addr;
        fileName = "DDR3_Data";
        c_length = 1000*1000*10;
        break;
    };

    // 设置采样率
    samplingRate = c_length;
     // 设置卡片参数
    cardPara.c_time = sp_gunTime->value()+ui->extraSaveTime->value();//单位为ms
    cardPara.singleLength = c_length * 2; //单帧字节数
    cardPara.totalLength = c_length * 2 * cardPara.c_time / 1000; //字节总数
    cardPara.mode = modeIndex;
    cardPara.address = address;
    cardPara.saveCnt = cnt;
    cardPara.coefficient = ui->coefficient->text().toInt();
    cardPara.pickUpMode = ui->PickUp->currentIndex();
    cardPara.triggerMode = trigMode->currentIndex();

    QLOG_INFO() << " 开始采集 时间ms "<< cardPara.c_time << " 触发模式 "<< cardPara.triggerMode << " address " << address;

    // 对每张卡片进行数据保存操作
    // for(int i=0;i<cardNumber;i++)
    // {
    //     // 如果不是第一次收集数据，则根据保存列表进行数据保存
    //     if(cnt)
    //     {
    //         if(hDevList[i]->saveFileInf->save_List[cnt-1])
    //         {
    //             hDevList[i]->ddrcnt++;
    //             unsigned char *infBuf;
    //             infBuf = (unsigned char*)malloc(122);
    //             InfPara infpara;
    //             strcpy(infpara.filetype,"swip_das");
    //             QLineEdit *id_edit = static_cast<QLineEdit*>(ui->tableWidget->cellWidget(i,cnt-1)->layout()->itemAt(2)->layout()->itemAt(1)->widget());
    //             infpara.channelId = id_edit->text().toInt();
    //             QLineEdit *name_edit = static_cast<QLineEdit*>(ui->tableWidget->cellWidget(i,cnt-1)->layout()->itemAt(0)->layout()->itemAt(1)->widget());
    //             strcpy(infpara.channelName,name_edit->text().toLocal8Bit().data());
    //             infpara.freq = c_length;
    //             int count=0;
    //             for(int m=0; m<i+1; m++)
    //             {
    //                 if(m==0) continue;

    //                 count += hDevList[m-1]->ddrcount;
    //             }

    //             if(i>0)
    //                 infpara.addr = cardPara.totalLength*count+(hDevList[i]->ddrcnt-1)*cardPara.totalLength;
    //             else
    //                 infpara.addr =(hDevList[i]->ddrcnt-1)*cardPara.totalLength;
    //             infpara.len = cardPara.totalLength/2;
    //             infpara.post = cardPara.totalLength/2;
    //             infpara.maxDat = 0;
    //             infpara.lowRang = 0;
    //             infpara.highRang = 0;
    //             infpara.factor = 1;//
    //             infpara.Offset = 0;
    //             strcpy(infpara.unit,"DEG");
    //             infpara.Dly = 0;
    //             infpara.attribDt = 2;//3
    //             infpara.datWth = 2;//
    //             memcpy(infBuf,&infpara,122);
    //             hDevList[i]->dataHandle = fopen((dataPath+"/"+fileName+"_card"+QString::number(busList[i])+".DAT").toLocal8Bit().data(),"ab+");
    //             hDevList[i]->infHandle = fopen((infPath+"/"+fileName+"_card"+QString::number(busList[i])+".INF").toLocal8Bit().data(),"ab+");
    //             fwrite(infBuf,122,1,hDevList[i]->infHandle);
    //             fclose(hDevList[i]->infHandle);
    //             free(infBuf);
    //         }
    //         hDevList[i]->isStop = false;
    //     }
    //     else
    //     {
    //         // 如果是第一次收集数据，则根据模式进行inf保存
    //         // 硬件触发
    //         if(ui->Mode->currentIndex() == 1)
    //         {
    //             hDevList[i]->infHandle = fopen((infPath+"/"+fileName+"_card"+QString::number(busList[i])+".INF").toLocal8Bit().data(),"ab+");
    //             for(int j=0; j<5; j++)
    //             {
    //                 unsigned char *infBuf;
    //                 infBuf = (unsigned char*)malloc(122);
    //                 InfPara infpara;
    //                 strcpy(infpara.filetype,"swip_das");
    //                 QLineEdit *id_edit = static_cast<QLineEdit*>(ui->tableWidget->cellWidget(i,j)->layout()->itemAt(2)->layout()->itemAt(1)->widget());
    //                 infpara.channelId = id_edit->text().toInt();
    //                 QLineEdit *name_edit = static_cast<QLineEdit*>(ui->tableWidget->cellWidget(i,j)->layout()->itemAt(0)->layout()->itemAt(1)->widget());
    //                 strcpy(infpara.channelName,name_edit->text().toLocal8Bit().data());
    //                 infpara.freq = c_length;
    //                 infpara.addr = i*cardPara.totalLength*5+j*cardPara.totalLength;
    //                 infpara.len = cardPara.totalLength/2;
    //                 infpara.post = cardPara.totalLength/2;
    //                 infpara.maxDat = 0;
    //                 infpara.lowRang = 0;
    //                 infpara.highRang = 0;
    //                 infpara.factor = 1;//
    //                 infpara.Offset = 0;
    //                 strcpy(infpara.unit,"DEG");
    //                 infpara.Dly = 0;
    //                 infpara.attribDt = 2;//3
    //                 infpara.datWth = 2;//
    //                 memcpy(infBuf,&infpara,122);

    //                 if(cnt)
    //                 {
    //                     if(hDevList[i]->saveFileInf->save_List[cnt-1])
    //                         fwrite(infBuf,122,1,hDevList[i]->infHandle);
    //                 }else{
    //                      fwrite(infBuf,122,1,hDevList[i]->infHandle);
    //                 }
    //                 free(infBuf);
    //             }
    //             hDevList[i]->dataHandle = fopen((dataPath+"/"+fileName+"_card"+QString::number(busList[i])+".dat").toLocal8Bit().data(),"ab+");
    //             fclose(hDevList[i]->infHandle);
    //             hDevList[i]->isStop = false;
    //         }
    //         else
    //         {
    //             // 第一次 软件触发
    //             unsigned char *infBuf;
    //             infBuf = (unsigned char*)malloc(122);
    //             InfPara infpara;
    //             strcpy(infpara.filetype,"swip_das");
    //             QLineEdit *id_edit = static_cast<QLineEdit*>(ui->tableWidget_2->cellWidget(i,2)->layout()->itemAt(1)->layout()->itemAt(1)->widget());
    //             infpara.channelId = id_edit->text().toInt();
    //             QLineEdit *name_edit = static_cast<QLineEdit*>(ui->tableWidget_2->cellWidget(i,2)->layout()->itemAt(0)->layout()->itemAt(1)->widget());
    //             strcpy(infpara.channelName,name_edit->text().toLocal8Bit().data());
    //             infpara.freq = c_length;
    //             infpara.addr = i*cardPara.totalLength;
    //             infpara.len = cardPara.totalLength/2;
    //             infpara.post = cardPara.totalLength/2;
    //             infpara.maxDat = 0;
    //             infpara.lowRang = 0;
    //             infpara.highRang = 0;
    //             infpara.factor = 1;//
    //             infpara.Offset = 0;
    //             strcpy(infpara.unit,"DEG");
    //             infpara.Dly = 0;
    //             infpara.attribDt = 2;//3
    //             infpara.datWth = 2;//
    //             memcpy(infBuf,&infpara,122);
    //             temp.push(busList[i]);
    //             hDevList[i]->dataHandle = fopen((dataPath+"/"+fileName+"_card"+QString::number(busList[i])+".DAT").toLocal8Bit().data(),"ab+");
    //             hDevList[i]->infHandle = fopen((infPath+"/"+fileName+"_card"+QString::number(busList[i])+".INF").toLocal8Bit().data(),"ab+");
    //             if(cnt)
    //             {
    //                 if(hDevList[i]->saveFileInf->save_List[cnt-1])
    //                     fwrite(infBuf,122,1,hDevList[i]->infHandle);
    //             }else{
    //                  fwrite(infBuf,122,1,hDevList[i]->infHandle);

    //             }
    //             fclose(hDevList[i]->infHandle);
    //             free(infBuf);

    //             hDevList[i]->isStop = false;
    //         }
    //     }
    //     cardPara.cardIndex = i;
    //     // 开始取数
    //     emit readData(plotList[i],cardPara);
    // }
    cnt++;
}



//停止采集
void MainProcess::stopCollection()
{
//     stopReal();
    labStatus->setStyleSheet("QLabel{border-radius:14px;"
                          "background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5,"
                          "stop:0 rgb(0, 255, 0),"
                          "stop:0.79 rgb(0, 230, 0),"
                          "stop:0.8 rgb(100, 100, 100),"
                          "stop:0.9 rgb(255, 255, 255),"
                          "stop:1 rgb(0, 0, 0));}");
    // for(int i=0; i<cardNumbers; i++){
    //     hDevList[i]->isStop = true;
    // }
    delay_ms(200);
    if(sender() == btnStop)
    {
        firstTrig = true;
        checkTimer->stop();

        ctlStatus(Stop);
        return;
    }
}

//读取ddr的开始结束过程
void MainProcess::ddrProcess(int index)
{
    // 如果g_cnt不为0，则直接返回，避免重复处理
    if(g_cnt) return;
    g_cnt++;

    // 根据模式指数初始化操作次数
    int count=2; //0;  //改成两轮 一次实时采集 一次ddr采集
    // if(modeIndex == Single_Frequency)
    //     count = 4;
    // else if(modeIndex == Three_Frequency)
    //     count = 6;

    // 根据当前计数与所需操作次数的比较，决定下一步操作
    if(cnt < count){
       QTimer::singleShot(500,this,&MainProcess::startCollection);
    }else{
        QTimer::singleShot(400,this,&MainProcess::consolidatedFineDocuments);
        // 当操作完成时，更新状态显示
        labStatus->setStyleSheet("QLabel{border-radius:14px;"
"background-color: qradialgradient(spread:pad, cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5,"
"stop:0 rgb(0, 255, 0),"
"stop:0.79 rgb(0, 255, 0),"
"stop:0.8 rgb(100, 100, 100),"
"stop:0.9 rgb(255, 255, 255),"
"stop:1 rgb(0, 0, 0));}");
        // 重置计数和状态，准备下一次操作
        cnt = 0;
        firstTrig = true;
        ctlStatus(Stop);
    }
}
//udp接收数据
void MainProcess::udpReceive()
{
    QString rev,flagStr,flag;
    while(udpHandle->hasPendingDatagrams())
    {
        QByteArray array;
        array.resize(udpHandle->pendingDatagramSize());
        udpHandle->readDatagram(array.data(),array.size());
        rev = array.data();

        Log("收到总控指令：" + rev);

//        // 检查炮号锁定状态(udp采集出现问题把这个if删掉)
//               if (m_gunNumberLocked && rev.contains("+PLS_") && !rev.contains("+PLS_000000")) {
//                   Log("自动采集中，忽略炮号修改指令：" + rev);
//                   continue; // 跳过炮号修改指令的处理
//               }

        // 硬件触发前进行软件触发校准
        if(rev.contains("+TDOWN_19") && rev.length() == 9)
        {
            QString curTime = QDateTime::currentDateTime().toString("yyyy年MM月dd日hh:mm");
            gunTime = sp_gunTime->value();
            QString inf = curTime+"     炮号："+gunNumber+"    时间："+QString::number(gunTime)+"毫秒";
            ui->lab_GunInf->setText(inf);
            ui->lab_GunInf->repaint();
            //emit readIrq(ui->hardtrigIndex->currentIndex(),ui->extraSaveTime->value());
            trigBeforeHard = true;  // 标志位置1
            emit hardCollectionTrigger();
            Log("软件触发校准中...");
        }
        // 总控指令 触发硬件采集,FPGA配置需要时间，需要提前配入进行准备采集
        if(rev.contains("+TDOWN_9") && rev.length() == 8)
        {
            QString curTime = QDateTime::currentDateTime().toString("yyyy年MM月dd日hh:mm");
            gunTime = sp_gunTime->value();
            QString inf = curTime+"     炮号："+gunNumber+"    时间："+QString::number(gunTime)+"毫秒";
            ui->lab_GunInf->setText(inf);
            ui->lab_GunInf->repaint();
            //emit readIrq(ui->hardtrigIndex->currentIndex(),ui->extraSaveTime->value());
            emit hardCollectionTrigger();
            Log("已按总控指令 开始硬件触发..");
        }

        if(rev.contains("+PLS_")&&!rev.contains("+PLS_000000")){
            gunNumber = rev.split("_").at(1);
            sp_gunNumber->setValue(gunNumber.toInt());
            Log("已按总控指令 设置炮号..");
        }
    }
}

//~析构
MainProcess::~MainProcess()
{
    referenceTimer->stop();
    measureTimer->stop();
    unsigned char value;
    value = 0;
    // for(int i=0; i<cardNumbers; i++)
    // {
    //     hDevList[i]->user_write(Phase_Filp_Addr,4,&value,i);
    //     hDevList[i]->trigEnd = true;
    // }
    //emit closeDev();
    //delay_ms(100);

    // for(int i=0; i<cardNumbers; i++)
    // {
    //     threadList[i]->quit();
    //     threadList[i]->wait();
    // }
    // if(cardNumbers > 0)
    // {
    //     unsigned char val;
    //     val = 0;

    //     // for(int i=0; i<cardNumbers; i++)
    //     // {
    //     //     if(busList[i] == mainCard){
    //     //          hDevList[i]->user_write(0x1C,4,&val,mainCard);
    //     //     }
    //     // }

    // }
    delete ui;
}

//最大化单个窗口
void MainProcess::maximizingSingleWindow(int index, bool full)
{

    for (int i = 0; i < busLists.length(); i++) {
        if(index == busLists[i]){
            index = i;
            break;
        }
    }

    for(int i=0; i<cardNumbers; i++){
        if (plotList[i] != nullptr) {
        if(full){
            if(i!=index)
            {
                plotList[i]->setHidden(true);
             //   rulerList[i]->setHidden(true);
            }
        }else{
            if(i!=index)
            {
                plotList[i]->setHidden(false);
              //  rulerList[i]->setHidden(false);
            }
        }
          delay_ms(200);
        plotList[i]->plot->replot(QCustomPlot::rpImmediateRefresh);
    }
    }
}

//超时函数
void MainProcess::hardTrigTimeout()
{
    ctlStatus(Stop);
}

//切换窗口
void MainProcess::switchParaWidget()
{
    ui->stackedWidget->setCurrentIndex(1);
}

//关闭配置界面
void MainProcess::on_cancleConfig_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

//触发方式更改：硬件 or 软件
void MainProcess::changeTrigMode(int index)
{
    if (!m_deviceConnected) {
        Log("设备未连接，无法设置触发模式");
        trigMode->setCurrentIndex(0); // 强制设为软件触发
        return;
    }

    if(index){
        quint16 port = ui->localPort->text().toUInt();
        if(udpHandle->bind(port)){
            connect(udpHandle,&QUdpSocket::readyRead,this,&MainProcess::udpReceive);
            btnStart->setEnabled(false);
            sp_gunTime->setReadOnly(true);
            sp_gunTime->setStyleSheet("QSpinBox { background-color: lightgray; color: gray;}");
            sp_gunTime->setValue(5000);
            sp_gunNumber->setReadOnly(true);
            sp_gunNumber->setStyleSheet("QSpinBox { background-color: lightgray; color: gray;}");
            Log("当前为硬件触发模式..可以接收远程指令控制...");
        }
    }else{
        udpHandle->abort();
        btnStart->setEnabled(true);
        sp_gunTime->setReadOnly(false);
        sp_gunTime->setStyleSheet("");
        sp_gunNumber->setReadOnly(false);
        sp_gunNumber->setStyleSheet("");
        Log("当前为软件触发模式..");
    }
}


void MainProcess::changeWorkingModeParams(int mode)
{
    UINT32 value;
    switch (mode) {
    case Single_Frequency:
        ui->txtchange_rt->setText("6250");
        ui->txtsingal_mode->setText("0");
        break;
    case Three_Frequency:
        ui->txtchange_rt->setText("12500");
        ui->txtsingal_mode->setText("1");
        break;
    }
}

//更改工作模式 0为单频 1为仨频
void MainProcess::changeWorkingMode(int mode)
{
    changeWorkingModeParams(mode);

    UINT32 value;
    switch (mode) {
    case Single_Frequency:
        for(int i=0; i<cardNumbers; i++)
        {
        if (plotList[i] != nullptr) {
            value = Single_Frequency;
            unsigned char buf[4];
            buf[0] = value;
            buf[1] = value>>8;
            buf[2] = value>>16;
            buf[3] = value>>24;
           // hDevList[i]->user_write(Working_Mode_Addr,4,buf,i);

            // ui->tableWidget->cellWidget(i,3)->setEnabled(false);
            // ui->tableWidget->cellWidget(i,4)->setEnabled(false);
            // ui->tableWidget_2->cellWidget(i,0)->setEnabled(false);
            // ui->tableWidget_2->cellWidget(i,1)->setEnabled(false);
            // ui->tableWidget_2->cellWidget(i,3)->setEnabled(false);
            // ui->tableWidget_2->cellWidget(i,4)->setEnabled(false);

            // plotList[i]->plot->graph(1)->setVisible(false);
            // plotList[i]->plot->graph(2)->setVisible(false);
            // plotList[i]->plot->graph(3)->setVisible(false);
            // plotList[i]->plot->graph(4)->setVisible(false);
            // plotList[i]->plot->legend->setVisible(false);
            plotList[i]->plot->replot(QCustomPlot::rpImmediateRefresh);
        }
        }
        break;
    case Three_Frequency:
        for(int i=0; i<cardNumbers; i++)
        {
            if (plotList[i] != nullptr) {
            value = Three_Frequency;
            unsigned char buf[4];
            buf[0] = value;
            buf[1] = value>>8;
            buf[2] = value>>16;
            buf[3] = value>>24;
            //hDevList[i]->user_write(Working_Mode_Addr,4,buf,i);

            // ui->tableWidget->cellWidget(i,3)->setEnabled(true);
            // ui->tableWidget->cellWidget(i,4)->setEnabled(true);
            // ui->tableWidget_2->cellWidget(i,0)->setEnabled(true);
            // ui->tableWidget_2->cellWidget(i,1)->setEnabled(true);
            // ui->tableWidget_2->cellWidget(i,3)->setEnabled(true);
            // ui->tableWidget_2->cellWidget(i,4)->setEnabled(true);
            // plotList[i]->plot->graph(1)->setVisible(true);
            // plotList[i]->plot->graph(2)->setVisible(true);
            // plotList[i]->plot->graph(3)->setVisible(true);
            // plotList[i]->plot->graph(4)->setVisible(true);
            plotList[i]->plot->legend->setVisible(true);
            plotList[i]->plot->replot(QCustomPlot::rpImmediateRefresh);
        }
        }
        break;
    }
}

//实现相位挑点设置
void MainProcess::realTimePhasePickUp(int index)
{
    UINT32 value;
    switch (index) {
    case 0:
        for(int i=0; i<cardNumbers; i++)
        {
            value = PickUp_1us;
            unsigned char buf[4];
            buf[0] = value;
            buf[1] = value>>8;
            buf[2] = value>>16;
            buf[3] = value>>24;
          //  hDevList[i]->user_write(Phase_PickUp_Addr,4,buf,i);
        }
        break;
    case 1:
        for(int i=0; i<cardNumbers; i++)
        {
            value = PickUp_10us;
            unsigned char buf[4];
            buf[0] = value;
            buf[1] = value>>8;
            buf[2] = value>>16;
            buf[3] = value>>24;
          //  hDevList[i]->user_write(Phase_PickUp_Addr,4,buf,i);
        }
        break;
    case 2:
        for(int i=0; i<cardNumbers; i++)
        {
            value = PickUp_20us;
            unsigned char buf[4];
            buf[0] = value;
            buf[1] = value>>8;
            buf[2] = value>>16;
            buf[3] = value>>24;
          //  hDevList[i]->user_write(Phase_PickUp_Addr,4,buf,i);
        }
        break;
    case 3:
        for(int i=0; i<cardNumbers; i++)
        {
            value = PickUp_50us;
            unsigned char buf[4];
            buf[0] = value;
            buf[1] = value>>8;
            buf[2] = value>>16;
            buf[3] = value>>24;
           // hDevList[i]->user_write(Phase_PickUp_Addr,4,buf,i);
        }
        break;
    case 4:
        for(int i=0; i<cardNumbers; i++)
        {
            value = PickUp_100us;
            unsigned char buf[4];
            buf[0] = value;
            buf[1] = value>>8;
            buf[2] = value>>16;
            buf[3] = value>>24;
           // hDevList[i]->user_write(Phase_PickUp_Addr,4,buf,i);
        }
        break;
    }
}

//50ms相位翻转设置
void MainProcess::phaseFilp_50ms(int index)
{
}

//设置硬件触发主卡
void MainProcess::setTrigMainCard()
{
    unsigned char value;
    int hardtrig_index = ui->hardtrigIndex->currentIndex();
    if( hardtrig_index == -1)
    {
        QLOG_INFO()  << " hardtrig_index -1  busList：" << busLists.length();
        return;
    }

    mainCard = hardtrig_index; //busLists[hardtrig_index];

    // 保存至配置文件
    saveIniFile();

    // for(int i=0; i<cardNumbers; i++)
    // {
    //     if(busLists[i] == mainCard){
    //         value = 1;
    //     }else{
    //         value = 0;
    //     }
      //  hDevList[i]->user_write(28,4,&value,i);
//    }
}

//接收硬件触发开始
void MainProcess::revHardTrigStart()
{
    hardTrig = true;
    startCollection();
}

//延时
void MainProcess::delay_ms(int mes)
{
    QTime dieTime = QTime::currentTime().addMSecs(mes);
    while(dieTime>QTime::currentTime()){
        QCoreApplication::processEvents(QEventLoop::AllEvents,100);
    }
}

//控件状态
void MainProcess::ctlStatus(int action)
{
    switch(action){
    case Init:
        btnStart->setEnabled(true);
        btnStop->setEnabled(false);
        break;
    case Start:
        //btnStart->setEnabled(false);
        btnStop->setEnabled(true);
        sp_gunTime->setReadOnly(true);
        sp_gunNumber->setReadOnly(true);
        paraConfig->setEnabled(false);
        echo->setEnabled(false);
        trigMode->setEnabled(false);
        break;
    case Stop:
        if(trigMode->currentIndex()){
            btnStart->setEnabled(true);//这里硬件触发允许手工点击
            btnStop->setEnabled(false);
            for(int c = 0; c< cardNumbers; c++)
            {
               // showReal(c);
            }
        }else
        {
            btnStart->setEnabled(true);
            btnStop->setEnabled(false);
            for(int c = 0; c< cardNumbers; c++)
            {
               // showReal(c);
            }
        }
        sp_gunTime->setReadOnly(false);
        sp_gunNumber->setReadOnly(false);
        paraConfig->setEnabled(true);
        echo->setEnabled(true);
        trigMode->setEnabled(true);
        break;
    default:
        break;
    }
}

//检测触发事件是否到达
void MainProcess::checkElapsedTime()
{

}

//更新图表
void MainProcess::replotChart(int index)
{
    plotList[index]->plot->yAxis->rescale();
    plotList[index]->plot->xAxis->rescale();
    plotList[index]->plot->replot(QCustomPlot::rpQueuedRefresh);
}

//整合数据文件
void MainProcess::consolidatedDocuments(QString dirType,QString fileFilter,QString fileType,QString suffix,QString  serverReflectionPath)
{
    QString dataPath,path,copyPath,renamePath;
    QStringList nameFilter,list;
    QDir dir;
    FILE *handle;
    unsigned char *buf;
    QFile file;
    long totalSize;
    //QString path2 = "D:\Data";
    QString path2 = "C:\Data";
    nameFilter<<"*.DAT"<<"*.INF";
     dataPath = path2+"/FIRdata/"+gunNumber+"/"+dirType;

    dir.setPath(dataPath);
    list = dir.entryList(nameFilter,QDir::Files,QDir::Name);

    if(list.isEmpty()) return;

    path = dataPath+"/"+gunNumber+fileType+suffix;

    int gunNumber_int = gunNumber.toInt()-gunNumber.toInt()%200;
    QString gunNumber_string = QString("%1").arg(gunNumber_int,5,10,QLatin1Char('0'));
    QString gunNumber_string2 = QString("%1").arg(gunNumber.toInt(),5,10,QLatin1Char('0'));
    if(dirType == "DATA")
        copyPath = serverReflectionPath+"\\"+gunNumber_string+"\\"+dirType+"\\"+gunNumber_string2+fileType+".DTMP";
    else
         copyPath = serverReflectionPath+"\\"+gunNumber_string+"\\"+dirType+"\\"+gunNumber_string2+fileType+".ITMP";
    renamePath =  serverReflectionPath+"\\"+gunNumber_string+"\\"+dirType+"\\"+gunNumber_string2+fileType+suffix;
    if(trigMode->currentIndex() == 1){
    QFile remoteFile(renamePath);
        if(remoteFile.exists()){
                remoteFile.remove();
        }
    }

    handle = fopen(path.toLocal8Bit().data(),"wb");
    file.setFileName(path);

    foreach(QString str,list)
    {
        if(str.mid(0,1) == fileFilter)
        {
            FILE *fp;
            QString filePath = dataPath +"/"+str;
            fp = fopen(filePath.toLocal8Bit().data(),"rb+");
            fseek(fp,0,SEEK_END);
            totalSize = ftell(fp);
            buf = (unsigned char*)malloc(totalSize);
            fseek(fp,0,SEEK_SET);
            int size = fread(buf,totalSize,1,fp);
            fwrite(buf,totalSize,1,handle);
            fclose(fp);
            free(buf);
            remove(filePath.toLocal8Bit().data());
        }
    }
    fclose(handle);
    if(trigMode->currentIndex() == 1){
    bool flag = file.copy(path,copyPath);
    if(flag){
        file.setFileName(copyPath);
        file.rename(renamePath);
    };
    };
}

//整合相位差文件
void MainProcess::consolidatedPhaseDiffDocuments()
{
    consolidatedDocuments("DATA","r",ui->phaseDiffFileName->text(),".DAT",ui->serverReflectionPath->text());
    consolidatedDocuments("INF","r",ui->phaseDiffFileName->text(),".INF",ui->serverReflectionPath->text());
}

//整合精细文件
void MainProcess::consolidatedFineDocuments()
{
    consolidatedDocuments("DATA","D",ui->fineFileName->text(),".DAT",ui->serverReflectionPath->text());
    consolidatedDocuments("INF","D",ui->fineFileName->text(),".INF",ui->serverReflectionPath->text());
}

void MainProcess::readIniFile()
{
    QSettings pConfig(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
    pConfig.setIniCodec(QTextCodec::codecForName("utf-8"));
    QString str;

    // 基础参数
    pConfig.beginGroup("BASIC_PARA");
    // mode
    str = pConfig.value("WorkMode", "单频").toString();
    if (str == "单频") {
        ui->Mode->setCurrentIndex(0);
        changeWorkingModeParams(0);
    } else if (str == "三频") {
        ui->Mode->setCurrentIndex(1);
        changeWorkingModeParams(1);
    }
    // pickup
    str = pConfig.value("Pahse_PickUp", "100us").toString();
    if (str == "1us")
        ui->PickUp->setCurrentIndex(0);
    else if (str == "10us")
        ui->PickUp->setCurrentIndex(1);
    else if (str == "20us")
        ui->PickUp->setCurrentIndex(2);
    else if (str == "50us")
        ui->PickUp->setCurrentIndex(3);
    else if (str == "100us")
        ui->PickUp->setCurrentIndex(4);
    // filp
    str = pConfig.value("Phase_Filp", "自动判断").toString();
    if (str == "自动判断")
        ui->phaseFilp->setCurrentIndex(0);
    else if (str == "取负值")
        ui->phaseFilp->setCurrentIndex(1);
    // hardware
    int index = pConfig.value("HardTrigCard", 1).toInt();
    ui->hardtrigIndex->setCurrentIndex(index - 1);

    // coefficient
    int coefficient = pConfig.value("Coefficient", 1).toInt();
    ui->coefficient->setText(QString::number(coefficient));

    // 调试参数
    debugMode = pConfig.value("DebugMode", false).toBool();
    simulatedCardCount = pConfig.value("SimulatedCardCount", 0).toInt();

    // 将配置值设置到界面控件
    ui->cboDebugModel->setChecked(debugMode);
    ui->txtSimulatedCardCount->setValue(simulatedCardCount);
    pConfig.endGroup();

    // 网络参数
    pConfig.beginGroup("NET_PARA");
    ui->localPort->setValue(pConfig.value("LocalPort", 8080).toInt());
    ui->remotePort->setValue(pConfig.value("remotePort").toInt());
    ui->serverReflectionPath->setText(pConfig.value("serverReflectionPath").toString());
    ui->phaseDiffFileName->setText(pConfig.value("phaseDiffFileName").toString());
    ui->fineFileName->setText(pConfig.value("fineFileName").toString());
    ui->trigTimeout->setValue(pConfig.value("trigTimeout").toInt());
    ui->extraSaveTime->setValue(pConfig.value("extraSaveTime").toInt());
    pConfig.endGroup();

    // 通道参数 - 先读取所有配置但不创建图表
    QString iniName, idName;
    QString part;
    pConfig.beginGroup("CHANNEL_PARA");

    // 先收集卡存储状态
    QVector<bool> cardStorageStates;
    for (int i = 0; i < cardNumbers; i++) {
        // 读取原始数据通道信息（前5列）
        for (int j = 0; j < 5; j++) {
            switch (j) {
            case 0:
                part = "_a1";
                break;
            case 1:
                part = "_a2";
                break;
            case 2:
                part = "_density1";
                break;
            case 3:
                part = "_density2";
                break;
            case 4:
                part = "_density3";
                break;
            }

            if (ui->tableWidget->cellWidget(i, j)) {
                QWidget *widget = ui->tableWidget->cellWidget(i, j);

                // 读取通道名称
                iniName = pConfig.value("Card" + QString::number(i + 1) + part + "_ini", "").toString();
                QList<QLineEdit*> nameEdits = widget->findChildren<QLineEdit*>();
                if (!nameEdits.isEmpty()) {
                    nameEdits.first()->setText(iniName);
                }

                // 读取通道ID
                idName = pConfig.value("Card" + QString::number(i + 1) + part + "_id", "").toString();
                if (nameEdits.size() > 1) {
                    nameEdits.at(1)->setText(idName.isEmpty() ? QString::number(j + 1) : idName);
                }
            }
        }

        // 读取卡存储开关状态（第6列）
        bool cardStorageState = true;
        if (ui->tableWidget->cellWidget(i, 5)) {
            QWidget *storageWidget = ui->tableWidget->cellWidget(i, 5);
            cardStorageState = pConfig.value("Card" + QString::number(i + 1) + "_storage", true).toBool();
            QPushButton *cardStorageBtn = storageWidget->findChild<QPushButton*>();
            if (cardStorageBtn) {
                cardStorageBtn->setChecked(cardStorageState);
            }
        }
        cardStorageStates.append(cardStorageState);

        // 读取实时数据通道信息
        for (int j = 0; j < 3; j++) {
            switch (j) {
            case 0:
                part = "_RT_a1";
                break;
            case 1:
                part = "_RT_a2";
                break;
            case 2:
                part = "_RT_density1";
                break;
            }

            if (ui->tableWidget_2->cellWidget(i, j)) {
                QWidget *widget = ui->tableWidget_2->cellWidget(i, j);

                // 读取通道名称
                iniName = pConfig.value("Card" + QString::number(i + 1) + part + "_ini", "").toString();
                QList<QLineEdit*> nameEdits = widget->findChildren<QLineEdit*>();
                if (!nameEdits.isEmpty()) {
                    nameEdits.first()->setText(iniName);
                }

                // 读取通道ID
                idName = pConfig.value("Card" + QString::number(i + 1) + part + "_id", "").toString();
                if (nameEdits.size() > 1) {
                    nameEdits.at(1)->setText(idName.isEmpty() ? QString::number(j + 1) : idName);
                }
            }
        }
    }
    pConfig.endGroup();

    // 寄存器参数
    pConfig.beginGroup("RegisterParams");
    ui->txtnf0_r->setText(pConfig.value("nf0_r").toString());
    ui->txtnf1_r->setText(pConfig.value("nf1_r").toString());
    ui->txtnf2_r->setText(pConfig.value("nf2_r").toString());
    ui->txtnf0_s->setText(pConfig.value("nf0_s").toString());
    ui->txtnf1_s->setText(pConfig.value("nf1_s").toString());
    ui->txtnf2_s->setText(pConfig.value("nf2_s").toString());

    // 读取人工参数
    ui->txtnf0_r_manual->setText(pConfig.value("nf0_r_manual", "").toString());
    ui->txtnf1_r_manual->setText(pConfig.value("nf1_r_manual", "").toString());
    ui->txtnf2_r_manual->setText(pConfig.value("nf2_r_manual", "").toString());
    ui->txtnf0_s_manual->setText(pConfig.value("nf0_s_manual", "").toString());
    ui->txtnf1_s_manual->setText(pConfig.value("nf1_s_manual", "").toString());
    ui->txtnf2_s_manual->setText(pConfig.value("nf2_s_manual", "").toString());

    // 读取参数源选择
    int paramSource = pConfig.value("parameter_source", 0).toInt();
    ui->cboParameterSource->setCurrentIndex(paramSource);

    ui->txtdensity_ratio_b->setText(pConfig.value("density_ratio_b").toString());
    ui->txtchange_rt->setText(pConfig.value("change_rt").toString());
    ui->txtsymb_on->setText(pConfig.value("symb_on").toString());
    ui->txtsymbol->setText(pConfig.value("symbol").toString());
    ui->txtsym_time->setText(pConfig.value("sym_time").toString());
    ui->txtsingal_mode->setText(pConfig.value("singal_mode").toString());
    pConfig.endGroup();
    pConfig.sync();
}

void MainProcess::saveIniFile()
{
    QSettings pConfig(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
    pConfig.setIniCodec(QTextCodec::codecForName("utf-8"));
    int m_mode = ui->Mode->currentIndex();
    int m_pickUp = ui->PickUp->currentIndex();
    int m_filp = ui->phaseFilp->currentIndex();
    int m_maincard = ui->hardtrigIndex->currentIndex();
    int m_coefficient = ui->coefficient->text().toInt();
    QString str;

    pConfig.beginGroup("BASIC_PARA");
    switch (m_mode) {
    case 0:
        str = "单频";
        break;
    case 1:
        str = "三频";
        break;
    }
    pConfig.setValue("WorkMode", str);
    switch (m_pickUp) {
    case 0:
        str = "1us";
        break;
    case 1:
        str = "10us";
        break;
    case 2:
        str = "20us";
        break;
    case 3:
        str = "50us";
        break;
    case 4:
        str = "100us";
        break;
    }
    pConfig.setValue("Pahse_PickUp", str);
    switch (m_filp) {
    case 0:
        str = "自动判断";
        break;
    case 1:
        str = "取负值";
        break;
    }
    pConfig.setValue("Phase_Filp", str);
    pConfig.setValue("HardTrigCard", m_maincard + 1);
    pConfig.setValue("Coefficient", m_coefficient);

    // 调试参数
    debugMode = ui->cboDebugModel->isChecked();
    simulatedCardCount = ui->txtSimulatedCardCount->value();
    pConfig.setValue("DebugMode", debugMode);
    pConfig.setValue("SimulatedCardCount", simulatedCardCount);
    pConfig.endGroup();

    pConfig.beginGroup("NET_PARA");
    pConfig.setValue("LocalPort", ui->localPort->value());
    pConfig.setValue("remotePort", ui->remotePort->value());
    pConfig.setValue("serverReflectionPath", ui->serverReflectionPath->text());
    pConfig.setValue("phaseDiffFileName", ui->phaseDiffFileName->text());
    pConfig.setValue("fineFileName", ui->fineFileName->text());
    pConfig.setValue("trigTimeout", ui->trigTimeout->value());
    pConfig.setValue("extraSaveTime", ui->extraSaveTime->value());
    pConfig.endGroup();

    pConfig.beginGroup("CHANNEL_PARA");
    QString iniName, idName;
    QString part;
    for (int i = 0; i < cardNumbers; i++) {
        // 保存原始数据通道信息（前5列）
        for (int j = 0; j < 5; j++) {
            switch (j) {
            case 0:
                part = "_a1";
                break;
            case 1:
                part = "_a2";
                break;
            case 2:
                part = "_density1";
                break;
            case 3:
                part = "_density2";
                break;
            case 4:
                part = "_density3";
                break;
            }

            if (ui->tableWidget->cellWidget(i, j)) {
                QWidget *widget = ui->tableWidget->cellWidget(i, j);
                QLineEdit *nameEdit = widget->findChild<QLineEdit*>();
                if (nameEdit) {
                    iniName = nameEdit->text();
                    pConfig.setValue("Card" + QString::number(i + 1) + part + "_ini", iniName);
                }

                QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();
                if (lineEdits.size() > 1) {
                    idName = lineEdits.at(1)->text();
                    pConfig.setValue("Card" + QString::number(i + 1) + part + "_id", idName);
                }
            }
        }

        // 保存卡存储开关状态（第6列）
        if (ui->tableWidget->cellWidget(i, 5)) {
            QWidget *storageWidget = ui->tableWidget->cellWidget(i, 5);
            QPushButton *cardStorageBtn = storageWidget->findChild<QPushButton*>();
            if (cardStorageBtn) {
                bool isChecked = cardStorageBtn->isChecked();
                pConfig.setValue("Card" + QString::number(i + 1) + "_storage", isChecked);
            }
        }

        // 保存实时数据通道信息
        for (int j = 0; j < 3; j++) {
            switch (j) {
            case 0:
                part = "_RT_a1";
                break;
            case 1:
                part = "_RT_a2";
                break;
            case 2:
                part = "_RT_density1";
                break;
            }

            if (ui->tableWidget_2->cellWidget(i, j)) {
                QWidget *widget = ui->tableWidget_2->cellWidget(i, j);
                QLineEdit *nameEdit = widget->findChild<QLineEdit*>();
                if (nameEdit) {
                    iniName = nameEdit->text();
                    pConfig.setValue("Card" + QString::number(i + 1) + part + "_ini", iniName);
                }

                QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();
                if (lineEdits.size() > 1) {
                    idName = lineEdits.at(1)->text();
                    pConfig.setValue("Card" + QString::number(i + 1) + part + "_id", idName);
                }
            }
        }
    }
    pConfig.endGroup();

    pConfig.beginGroup("RegisterParams");
    // 计算参数
    pConfig.setValue("nf0_r", ui->txtnf0_r->text().trimmed());
    pConfig.setValue("nf1_r", ui->txtnf1_r->text().trimmed());
    pConfig.setValue("nf2_r", ui->txtnf2_r->text().trimmed());
    pConfig.setValue("nf0_s", ui->txtnf0_s->text().trimmed());
    pConfig.setValue("nf1_s", ui->txtnf1_s->text().trimmed());
    pConfig.setValue("nf2_s", ui->txtnf2_s->text().trimmed());

    // 人工参数
    pConfig.setValue("nf0_r_manual", ui->txtnf0_r_manual->text().trimmed());
    pConfig.setValue("nf1_r_manual", ui->txtnf1_r_manual->text().trimmed());
    pConfig.setValue("nf2_r_manual", ui->txtnf2_r_manual->text().trimmed());
    pConfig.setValue("nf0_s_manual", ui->txtnf0_s_manual->text().trimmed());
    pConfig.setValue("nf1_s_manual", ui->txtnf1_s_manual->text().trimmed());
    pConfig.setValue("nf2_s_manual", ui->txtnf2_s_manual->text().trimmed());

    // 参数源选择
    pConfig.setValue("parameter_source", ui->cboParameterSource->currentIndex());

    pConfig.setValue("density_ratio_b", ui->txtdensity_ratio_b->text().trimmed());
    pConfig.setValue("change_rt", ui->txtchange_rt->text().trimmed());
    pConfig.setValue("symb_on", ui->txtsymb_on->text().trimmed());
    pConfig.setValue("symbol", ui->txtsymbol->text().trimmed());
    pConfig.setValue("sym_time", ui->txtsym_time->text().trimmed());
    pConfig.setValue("singal_mode", ui->txtsingal_mode->text().trimmed());
    pConfig.endGroup();
    pConfig.sync();

    ui->stackedWidget->setCurrentIndex(0);
}

void MainProcess::newsaveIniFile()
{
    saveIniFile();
    // 不再重启，直接应用新的存储开关状态
    // restartApplication();
    applyCardStorageChanges();

}

// 应用卡存储开关变化
void MainProcess::applyCardStorageChanges()
{
    if (!m_deviceConnected) {
        Log("设备未连接，无法应用存储设置");
        return;
    }

    Log("正在应用存储设置并重新初始化图像...");

    // 读取最新的配置
    QSettings pConfig(QDir::currentPath() + "/config.ini", QSettings::IniFormat);
    pConfig.beginGroup("CHANNEL_PARA");

    // 先停止所有可能正在进行的采集
    stopCollection();

    // 清理现有的图表
    for (int i = 0; i < plotList.size(); i++) {
        if (plotList[i] != nullptr) {
            plotList[i]->deleteLater();
            plotList[i] = nullptr;
        }
    }
    plotList.clear();

    QList<QColor> colorList;
    colorList<<QColor(0,120,215)<<QColor(0,215,120)<<QColor(215,0,120)<<QColor(120,0,215)<<QColor(0,0,0);

    // 根据新的存储设置重新创建图表
    for (int i = 0; i < cardNumbers; i++) {
        bool storageState = pConfig.value("Card" + QString::number(i + 1) + "_storage", true).toBool();

        if (storageState) {
            // 创建新的图表
            PlanarGraph *m_plot = new PlanarGraph(QString::number(i));
            m_plot->plot_2D_Init(Qt::white, 3, colorList);
            plotList.append(m_plot);
            connect(m_plot, SIGNAL(windowFullorRestoreSize(int,bool)), this, SLOT(maximizingSingleWindow(int,bool)));
            Log(QString("卡%1图表已重新初始化").arg(i+1));
        } else {
            // 存储关闭，图表为空
            plotList.append(nullptr);
            Log(QString("卡%1存储关闭，无图表").arg(i+1));
        }
    }

    pConfig.endGroup();

    // 完全重新布局
    switchLayout(Layout_4x2);

    // 重置采集状态
    ctlStatus(Stop);

    Log("存储设置应用完成，图像已重新初始化");
}

void MainProcess::on_btnFileSync_clicked()
{
    int gunNumber = sp_gunNumber->value();
    // 获取目标文件夹路径
    QString serverReflectionPath = ui->serverReflectionPath->text().trimmed();

    // 检查目标文件夹是否存在
    if (!QDir(serverReflectionPath).exists()) {
        QLOG_INFO() << "目标路径不存在 请设置映射路径";
        Log("目标路径不存在，请先设置映射路径");
        return;
    }

    QString gunNumberStr = getGunNumber();

    // 检查炮号锁定状态 未锁定（不是自动采集模式）时软件触发可以自动加一
        if (!m_gunNumberLocked) {
           sp_gunNumber->setValue(gunNumber + 1);
        }


    if (!ui->maintainSync->isChecked()) {
        return;
    }

    // 计算200炮倍数文件夹名
    int folderNumber = (gunNumber / 200) * 200; // 向下取整到最接近的200的倍数

    // 构建200炮倍数文件夹路径
    QString gunFolderPath = QDir(serverReflectionPath).filePath(QString::number(folderNumber));

    // 检查或创建200炮倍数文件夹
    QDir().mkpath(gunFolderPath);

    // 构建inf和dat文件夹路径
    QString infFolderPath = QDir(gunFolderPath).filePath("INF");
    QString datFolderPath = QDir(gunFolderPath).filePath("DATA");

    // 检查或创建inf和dat文件夹
    QDir().mkpath(infFolderPath);
    QDir().mkpath(datFolderPath);

    // 现在你可以使用 infFolderPath 和 datFolderPath 来进行进一步的操作
    QLOG_INFO() << "INF 文件夹路径:" << infFolderPath;
    QLOG_INFO() << "DATA 文件夹路径:" << datFolderPath;

    Log("目标 DAT 文件夹路径" + datFolderPath);


    // 把当前炮号的所有DAT INF文件都发送到目标文件夹

    // 获取文件名称配置
    QString phaseDiffFileName = ui->phaseDiffFileName->text();
    QString fineFileName = ui->fineFileName->text();

    // 异步执行文件复制任务
//    QtConcurrent::run([this, gunNumberStr, datFolderPath, infFolderPath](){
        copyFiles(gunNumberStr, m_dataPath, m_infPath, datFolderPath, infFolderPath,
                 phaseDiffFileName, fineFileName);
//    });


}

void MainProcess::copyFiles(const QString &gunNumberStr, const QString &m_dataPath, const QString &m_infPath,
                          const QString &datFolderPath, const QString &infFolderPath,
                          const QString &phaseDiffFileName, const QString &fineFileName)
{
    Log("本炮号数据即将传输...请耐心等待完成...");

    int workMode = ui->txtsingal_mode->text().toInt();

    // 创建工作线程
    QThread* workerThread = new QThread(this);

    // 创建文件复制工作对象
    FileCopyWorker* worker = new FileCopyWorker(gunNumberStr, m_dataPath, m_infPath,
                                                   datFolderPath, infFolderPath,
                                                   phaseDiffFileName, fineFileName, workMode);
    worker->moveToThread(workerThread);

    // 连接信号和槽
    connect(workerThread, &QThread::started, worker, &FileCopyWorker::process);
    connect(worker, &FileCopyWorker::finished, workerThread, &QThread::quit);
    connect(worker, &FileCopyWorker::finished, worker, &QObject::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QObject::deleteLater);

    // 转发进度和日志信号
    connect(worker, &FileCopyWorker::progressUpdated, this, &MainProcess::copyProgress);
    connect(worker, &FileCopyWorker::logMessage, this, &MainProcess::Log);

    // 连接DAT文件传输信号到更新信号窗口的方法
    connect(worker, &FileCopyWorker::updateFFT, this, &MainProcess::updateSignalWindow);
    connect(worker, &FileCopyWorker::updateFFT1, this, &MainProcess::updateSignalWindow1);

    // 启动线程
    workerThread->start();
}

void MainProcess::updateSignalWindow1(const QString& datFilePath)
{
    showSignalValueWindow(datFilePath);
}
// 在 MainProcess.cpp 中实现这些函数
void MainProcess::updateSignalWindow(const QString& datFilePath)
{
    Log("准备更新信号窗口，文件: " + datFilePath);

    // 确保信号窗口存在
    if (!m_signalWindow) {
        m_signalWindow = new SignalWindow();
        connect(m_signalWindow, &SignalWindow::windowClosed,
                this, &MainProcess::onSignalWindowClosed);
        connect(m_signalWindow, &SignalWindow::sendExtremeIndices,
                this, &MainProcess::handleExtremeIndices);
        m_signalWindow->setAttribute(Qt::WA_DeleteOnClose);
    }

    int mode = ui->txtsingal_mode->text().toInt();
    // 更新信号窗口数据
    m_signalWindow->updateSignalData(datFilePath, 2, 2, mode);

    // 显示窗口并确保它在前台
//    m_signalWindow->show();
//    m_signalWindow->raise();
//    m_signalWindow->activateWindow();

    Log("信号窗口已更新: " + datFilePath);
}

void MainProcess::onSignalWindowClosed()
{
    // 窗口关闭时，置空指针
    m_signalWindow = nullptr;
    Log("信号窗口已关闭");
}


void MainProcess::handleExtremeIndices(int index1, int index2, int index3)
{
    // 处理接收到的极值索引（此处索引始终为计算参数）
    qDebug() << "接收到计算的极值索引:" << index1 << index2 << index3;

    // 更新UI显示
    ui->txtnf0_r->setText(QString::number(index1));
    ui->txtnf1_r->setText(QString::number(index2));
    ui->txtnf2_r->setText(QString::number(index3));
    ui->txtnf0_s->setText(QString::number(index1));
    ui->txtnf1_s->setText(QString::number(index2));
    ui->txtnf2_s->setText(QString::number(index3));

    saveIniFile();

}

// 在适当位置（如 handleRealProgress 方法后）添加
void MainProcess::handleFifoOverflow(int cardindex, double fillRatio)
{
    // 记录警告
    QString warningMsg = QString("FIFO溢出警告：卡%1填充率达到%2%，可能导致数据丢失!")
                             .arg(cardindex+1)
                             .arg(fillRatio * 100, 0, 'f', 2);
    Log(warningMsg);

    // 当FIFO溢出严重时（填充率>95%），可以考虑自动调整采集策略
    if (fillRatio > 0.95) {
        // 这里可以实现紧急措施，如降低采样率，暂停其他卡的采集等
        // 例如：给用户一个严重警告，建议减少同时采集的卡数
        QString criticalMsg = QString("严重警告：卡%1 FIFO接近完全溢出! 建议减少同时采集的卡数量!")
                              .arg(cardindex+1);
        Log(criticalMsg);

        // 可选：将警告显示到UI的某个特定区域，使其更明显
        // 例如：设置状态栏、闪烁警告灯等
    }
}



void MainProcess::on_btnAync_clicked()
{
    on_btnFileSync_clicked();
}


void MainProcess::on_btnProcessSpecific_clicked()
{
    // 获取用户输入的炮号
    int gunNumber = sp_gunNumber->value();
    QString gunNumberStr = QString::number(gunNumber);

    Log(QString("开始手工处理炮号 %1 的合并文件...").arg(gunNumberStr));

    // 获取文件名称配置
    QString phaseDiffFileName = ui->phaseDiffFileName->text();
    QString fineFileName = ui->fineFileName->text();

    // 获取服务器映射路径
    QString serverReflectionPath = ui->serverReflectionPath->text().trimmed();

    // 检查目标文件夹是否存在
    if (serverReflectionPath.isEmpty() || !QDir(serverReflectionPath).exists()) {
        Log("错误：目标路径不存在，请先设置映射路径");
        return;
    }

    try {
        // 计算200炮倍数文件夹名
        int folderNumber = (gunNumber / 200) * 200; // 向下取整到最接近的200的倍数
        // 构建200炮倍数文件夹路径
        QString gunFolderPath = QDir(serverReflectionPath).filePath(QString::number(folderNumber));
        QString datFolderPath = QDir(gunFolderPath).filePath("DATA");
        QString infFolderPath = QDir(gunFolderPath).filePath("INF");

        // 确保目标文件夹存在
        QDir().mkpath(datFolderPath);
        QDir().mkpath(infFolderPath);

        // 创建 FileCopyWorker 实例 - 参数顺序：炮号、源DATA路径、源INF路径、目标DATA路径、目标INF路径、文件名配置
        FileCopyWorker* worker = new FileCopyWorker(
            gunNumberStr,
            "C:/Data/FIRdata/90001/DATA",  // 源DATA路径（固定采集路径）
            "C:/Data/FIRdata/90001/INF",   // 源INF路径（固定采集路径）
            datFolderPath,                   // 目标DATA路径
            infFolderPath,                   // 目标INF路径
            phaseDiffFileName,
            fineFileName
        );

        // 连接信号槽以接收处理结果
        connect(worker, &FileCopyWorker::finished, this, [this, gunNumberStr]() {
            Log(QString("炮号 %1 的文件处理完成").arg(gunNumberStr));
        });

        connect(worker, &FileCopyWorker::logMessage, this, [this](const QString& message) {
            Log(message);
        });

        connect(worker, &FileCopyWorker::finished, worker, &QObject::deleteLater);

        // 调用处理特定炮号的方法
        worker->processSpecificGunNumber(gunNumberStr);

    } catch (const std::exception& e) {
        Log(QString("创建文件处理器失败: %1").arg(e.what()));
    } catch (...) {
        Log("创建文件处理器时发生未知错误");
    }
}

void MainProcess::restartApplication()
{
    QString program = QCoreApplication::applicationFilePath();
    QStringList arguments = QCoreApplication::arguments();
    QString workingDirectory = QCoreApplication::applicationDirPath();

    // 启动新实例
    QProcess::startDetached(program, arguments, workingDirectory);

    // 退出当前实例
    QCoreApplication::exit(0);
}

int MainProcess::extractCardIndex(const QString& filePath) {
    QRegularExpression re("DDR3_DATA_CARD(\\d+)");
    QRegularExpressionMatch match = re.match(filePath);
    if (match.hasMatch()) {
        return match.captured(1).toInt(); // 文件里是1基，程序里用0基
    }
    return -1;
}

void MainProcess::showSignalValueWindow(const QString& datFilePath)
{
    // 提取卡号
    int cardNumber = extractCardIndex(datFilePath);
    if (cardNumber < 1) {
        qWarning() << "无效的卡号:" << cardNumber;
        return;
    }

    // 卡号转换为plotList索引（卡号-1）
    int plotIndex = cardNumber - 1;

    // 检查索引有效性
    if (plotIndex < 0 || plotIndex >= plotList.size()) {
        qWarning() << "无效的图表索引:" << plotIndex << "卡号:" << cardNumber;
        return;
    }

    if (plotList[plotIndex] == nullptr) {
        qWarning() << "卡" << cardNumber << "的图表为空";
        return;
    }

    // 创建或显示窗口
    if (!m_signalValueWindow) {
        m_signalValueWindow = new SignalValueWindow(plotList, this);
        connect(m_signalValueWindow, &SignalValueWindow::windowClosed,
                this, &MainProcess::onSignalWindowClosed);
    }

    // 更新数据 - 传递plotList索引
    int mode = ui->txtsingal_mode->text().toInt();//sp_gunTime
    float collectTime = sp_gunTime->text().toInt();
    m_signalValueWindow->updateSignalData(plotIndex, datFilePath, 2, collectTime*100, mode);

    m_signalValueWindow->show();
    m_signalValueWindow->raise();
}


// 自动采集选择框状态改变处理
void MainProcess::onAutoCollectionStateChanged(int state)
{
    if (state == Qt::Checked) {
        sp_gunTime->setValue(100);

        // 锁定炮号：设置为只读并改变样式
        sp_gunNumber->setReadOnly(true);
        sp_gunNumber->setStyleSheet("QSpinBox { background-color: lightgray; color: gray; }");

        // 禁用炮号相关的网络指令处理
        m_gunNumberLocked = true;

        // 修改按钮文本为"暂停"
        btnStart->setText("暂停");

        // 连接采集完成信号到重新开始采集的槽函数
        connect(this, &MainProcess::collectionFinished, this, &MainProcess::onCollectionFinished, Qt::UniqueConnection);

        // 如果是暂停状态转为自动采集，立即开始采集
        if (btnStart->text() == "开始") {
            startCollection();
        }
    } else {
        // 取消勾选：恢复采样时间和炮号可编辑
        sp_gunNumber->setReadOnly(false);
        sp_gunNumber->setStyleSheet(""); // 恢复默认样式

        // 启用炮号相关的网络指令处理
        m_gunNumberLocked = false;

        // 修改按钮文本为"开始"
        btnStart->setText("开始");

        // 断开自动重新采集的连接
        disconnect(this, &MainProcess::collectionFinished, this, &MainProcess::onCollectionFinished);
    }
}

// 开始按钮点击处理
void MainProcess::onStartButtonClicked()
{
    if (autoCheckBox->isChecked()) {
        // 自动采集模式下
        if (btnStart->text() == "暂停") {
            // 点击暂停：停止自动循环
            btnStart->setText("开始");
            disconnect(this, &MainProcess::collectionFinished, this, &MainProcess::onCollectionFinished);

            // 停止当前采集（如果有）
            stopCollection();
        } else {
            // 点击开始：恢复自动循环
            btnStart->setText("暂停");
            connect(this, &MainProcess::collectionFinished, this, &MainProcess::onCollectionFinished, Qt::UniqueConnection);

            // 开始采集
            startCollection();
        }
    } else {
        // 非自动采集模式：正常单次采集
        startCollection();
    }
}

// 采集完成处理（自动模式下重新开始）
void MainProcess::onCollectionFinished()
{
    if (autoCheckBox->isChecked() && btnStart->text() == "暂停") {
        // 延迟一段时间后重新开始采集，避免连续采集过快
        QTimer::singleShot(100, this, [this]() {
            if (autoCheckBox->isChecked() && btnStart->text() == "暂停") {
                startCollection();
            }
        });
    }
}

// 参数源选择改变
void MainProcess::onParameterSourceChanged(int index)
{
    // 保存配置
    saveIniFile();

    QString sourceName = (index == 0) ? "计算参数" : "人工参数";
    Log(QString("参数源已切换为: %1").arg(sourceName));

    // 通知所有PcieProtocol实例重新加载参数
    for (PcieProtocol* pcie : pcieRealList) {
        if (pcie) {
            // 这里可以添加一个重新加载参数的方法，或者在下一次采集时自动重新加载
            // pcie->reloadParameters(); // 如果需要立即生效，可以添加这个方法
        }
    }
}

// 更新人工参数显示
void MainProcess::updateManualParamsDisplay()
{
    ui->txtnf0_r_manual->setText(m_manualParams.nf0_r);
    ui->txtnf1_r_manual->setText(m_manualParams.nf1_r);
    ui->txtnf2_r_manual->setText(m_manualParams.nf2_r);
    ui->txtnf0_s_manual->setText(m_manualParams.nf0_s);
    ui->txtnf1_s_manual->setText(m_manualParams.nf1_s);
    ui->txtnf2_s_manual->setText(m_manualParams.nf2_s);
}

// 人工参数编辑完成
void MainProcess::onManualParamEdited()
{
    // 保存配置
   // saveIniFile();

    QLineEdit* editor = qobject_cast<QLineEdit*>(sender());
    if (editor) {
        QString paramName = editor->objectName();
        QString value = editor->text();
        Log(QString("人工参数 %1 已更新为: %2").arg(paramName).arg(value));
    }
}

// 卡存储开关状态改变
void MainProcess::onCardStorageChanged(int cardIndex, bool checked)
{
    if (!m_deviceConnected) {
        Log("设备未连接，无法修改存储状态");
        return;
    }

    if (checked) {
        // 开关打开，创建图表
        if (cardIndex < plotList.size() && plotList[cardIndex] == nullptr) {
            QList<QColor> colorList;
            colorList<<QColor(0,120,215)<<QColor(0,215,120)<<QColor(215,0,120)<<QColor(120,0,215)<<QColor(0,0,0);

            PlanarGraph *m_plot = new PlanarGraph(QString::number(cardIndex));
            m_plot->plot_2D_Init(Qt::white, 3, colorList);
            plotList[cardIndex] = m_plot;
            connect(m_plot, SIGNAL(windowFullorRestoreSize(int,bool)), this, SLOT(maximizingSingleWindow(int,bool)));
            Log(QString("卡%1存储已开启，图表已创建").arg(cardIndex+1));
        }
    } else {
        // 开关关闭，删除图表
        if (cardIndex < plotList.size() && plotList[cardIndex] != nullptr) {
            plotList[cardIndex]->deleteLater();
            plotList[cardIndex] = nullptr;
            Log(QString("卡%1存储已关闭，图表已删除").arg(cardIndex+1));
        }
    }

    // 重新布局
    switchLayout(Layout_4x2);

    // 保存配置
    saveIniFile();
}
