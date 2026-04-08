#include "planargraph.h"

PlanarGraph::PlanarGraph(QString devName, QWidget *parent)
    : QWidget(parent),
      isDragEnabled(true),
      isZoomEnabled(false),
      checkboxLayout(nullptr)
{
    this->setWindowFlags((this->windowFlags() & ~Qt::WindowCloseButtonHint) & ~Qt::WindowContextHelpButtonHint);

    // 原来的主 plot（保持兼容）
    plot = new QCustomPlot(this);
    plot->setNoAntialiasingOnDrag(true);
    plot->xAxis->setAntialiased(false);
    plot->yAxis->setAntialiased(false);


    // 新增左右两个子图（参考/测量）
    m_refTimePlot = new QCustomPlot(this);
    m_refTimePlot->setMinimumSize(400, 300);
    m_refTimePlot->setVisible(false); // 默认隐藏，按需显示

    m_refFftPlot = new QCustomPlot(this);
    m_refFftPlot->setMinimumSize(400, 300);
    m_refFftPlot->setVisible(false); // 默认隐藏，按需显示

    m_mesTimePlot = new QCustomPlot(this);
    m_mesTimePlot->setMinimumSize(400, 300);
    m_mesTimePlot->setVisible(false); // 默认隐藏，按需显示

    m_mesFftPlot = new QCustomPlot(this);
    m_mesFftPlot->setMinimumSize(400, 300);
    m_mesFftPlot->setVisible(false); // 默认隐藏，按需显示

    // 创建控制按钮
    dragButton = new QPushButton(this);
    zoomButton = new QPushButton(this);

    QIcon dragOnIcon(":/new/prefix1/icons/drag_on.png");
    QIcon zoomOffIcon(":/new/prefix1/icons/zoom_off.png");

    if (dragOnIcon.isNull()) {
        dragButton->setText("拖");
        dragButton->setStyleSheet(dragButton->styleSheet() + " font-weight: bold;");
    } else {
        dragButton->setIcon(dragOnIcon);
    }

    if (zoomOffIcon.isNull()) {
        zoomButton->setText("选");
        zoomButton->setStyleSheet(zoomButton->styleSheet() + " font-weight: bold;");
    } else {
        zoomButton->setIcon(zoomOffIcon);
    }

    dragButton->setFixedSize(40, 40);
    zoomButton->setFixedSize(40, 40);
    dragButton->setFlat(true);
    zoomButton->setFlat(true);
    dragButton->setToolTip("拖动模式");
    zoomButton->setToolTip("框选放大模式");

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(dragButton);
    buttonLayout->addWidget(zoomButton);
    buttonLayout->addStretch();
    buttonLayout->setContentsMargins(5, 5, 5, 0);
    buttonLayout->setSpacing(5);

    // --- 这里关键：创建一个容器放置 plot 和左右两个子图（水平排列）
    QWidget *plotsContainer = new QWidget(this);
    QGridLayout *plotsLayout = new QGridLayout(plotsContainer);
    plotsLayout->setContentsMargins(0,0,0,0);
    plotsLayout->setSpacing(4);
    plotsLayout->addWidget(plot, 0, 0, 1, 2); // 占两列
    // 第一行
    plotsLayout->addWidget(m_refTimePlot, 0, 0);
    plotsLayout->addWidget(m_refFftPlot, 1, 0);

    // 第二行
    plotsLayout->addWidget(m_mesTimePlot, 0, 1);
    plotsLayout->addWidget(m_mesFftPlot, 1, 1);


    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(plotsContainer);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    // plot 的交互与属性（保持你原来设置）
    plot->setAutoFillBackground(true);
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes | QCP::iSelectPlottables | QCP::iSelectItems);
    plot->setNoAntialiasingOnDrag(true);

    // 标识/文字
    this->setProperty("设备:", devName.toUInt());
    QString strName = QString::number(devName.toUInt() + 1);
    plotAddText(strName);




    connect(dragButton, &QPushButton::clicked, this, &PlanarGraph::onDragButtonClicked);
    connect(zoomButton, &QPushButton::clicked, this, &PlanarGraph::onZoomButtonClicked);



    plot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(plot, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ContextMenu(const QPoint &)));
    contextMenu = new QMenu(plot);
    resetRange = new QAction("初始范围", plot);
    maxWidget = new QAction("窗口最大化", plot);
    savePng = new QAction("保存为图片", plot);
    contextMenu->addAction(resetRange);
    contextMenu->addAction(maxWidget);
    contextMenu->addAction(savePng);
    connect(resetRange, &QAction::triggered, this, &PlanarGraph::resetPlotRange);
    connect(maxWidget, &QAction::triggered, this, &PlanarGraph::maxPlotWidget);
    connect(savePng, &QAction::triggered, this, &PlanarGraph::savePlot2Png);


    m_refTimePlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_refTimePlot, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ContextMenu(const QPoint &)));
    contextMenu_refTime = new QMenu(m_refTimePlot);
    resetRange1 = new QAction("初始范围", m_refTimePlot);
    maxWidget1 = new QAction("窗口最大化", m_refTimePlot);
    savePng1 = new QAction("保存为图片", m_refTimePlot);
    contextMenu_refTime->addAction(resetRange1);
    contextMenu_refTime->addAction(maxWidget1);
    contextMenu_refTime->addAction(savePng1);
    connect(resetRange1, &QAction::triggered, this, &PlanarGraph::resetPlotRange1);
    connect(maxWidget1, &QAction::triggered, this, &PlanarGraph::maxPlotWidget1);
    connect(savePng1, &QAction::triggered, this, &PlanarGraph::savePlot2Png1);


    m_refFftPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_refFftPlot, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ContextMenu(const QPoint &)));
    contextMenu_refFft = new QMenu(m_refFftPlot);
    resetRange2 = new QAction("初始范围", m_refFftPlot);
    maxWidget2 = new QAction("窗口最大化", m_refFftPlot);
    savePng2 = new QAction("保存为图片", m_refFftPlot);
    contextMenu_refFft->addAction(resetRange2);
    contextMenu_refFft->addAction(maxWidget2);
    contextMenu_refFft->addAction(savePng2);
    connect(resetRange2, &QAction::triggered, this, &PlanarGraph::resetPlotRange2);
    connect(maxWidget2, &QAction::triggered, this, &PlanarGraph::maxPlotWidget2);
    connect(savePng2, &QAction::triggered, this, &PlanarGraph::savePlot2Png2);


    m_mesTimePlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_mesTimePlot, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ContextMenu(const QPoint &)));
    contextMenu_mesTime = new QMenu(m_mesTimePlot);
    resetRange3 = new QAction("初始范围", m_mesTimePlot);
    maxWidget3 = new QAction("窗口最大化", m_mesTimePlot);
    savePng3 = new QAction("保存为图片", m_mesTimePlot);
    contextMenu_mesTime->addAction(resetRange3);
    contextMenu_mesTime->addAction(maxWidget3);
    contextMenu_mesTime->addAction(savePng3);
    connect(resetRange3, &QAction::triggered, this, &PlanarGraph::resetPlotRange3);
    connect(maxWidget3, &QAction::triggered, this, &PlanarGraph::maxPlotWidget3);
    connect(savePng3, &QAction::triggered, this, &PlanarGraph::savePlot2Png3);


    m_mesFftPlot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_mesFftPlot, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(ContextMenu(const QPoint &)));
    contextMenu_mesFft = new QMenu(m_mesFftPlot);
    resetRange4 = new QAction("初始范围", m_mesFftPlot);
    maxWidget4 = new QAction("窗口最大化", m_mesFftPlot);
    savePng4 = new QAction("保存为图片", m_mesFftPlot);
    contextMenu_mesFft->addAction(resetRange4);
    contextMenu_mesFft->addAction(maxWidget4);
    contextMenu_mesFft->addAction(savePng4);
    connect(resetRange4, &QAction::triggered, this, &PlanarGraph::resetPlotRange4);
    connect(maxWidget4, &QAction::triggered, this, &PlanarGraph::maxPlotWidget4);
    connect(savePng4, &QAction::triggered, this, &PlanarGraph::savePlot2Png4);

    auto connectPlot = [this](QCustomPlot* p){
        connect(p, SIGNAL(selectionChangedByUser()), this, SLOT(selectState()));
        connect(p, SIGNAL(mousePress(QMouseEvent*)), this, SLOT(dealMousePressEvent(QMouseEvent*)));
        connect(p->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(updateAxisRange(QCPRange)));
        connect(p->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(updateAxisRange(QCPRange)));
    };

    connectPlot(plot);
    connectPlot(m_refTimePlot);
    connectPlot(m_refFftPlot);
    connectPlot(m_mesTimePlot);
    connectPlot(m_mesFftPlot);

}

//曲线的绘制 默认num为5
void PlanarGraph::plot_2D_Init(QColor bkColor,uint8_t num,QList<QColor> &pColor)
{
    plot->setAutoFillBackground(true);
    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom|QCP::iSelectAxes|QCP::iSelectPlottables|QCP::iSelectLegend);
    plot->setNoAntialiasingOnDrag(true);//关闭拖动期间的抗锯齿
    plot->setBackground(bkColor);
    plot->setBackgroundScaled(true);
    plot->setBackgroundScaledMode(Qt::IgnoreAspectRatio);

    cList = pColor;

    QFont labelfont;
//    labelfont.setBold(true);
    labelfont.setFamily("黑体");
    labelfont.setPixelSize(14);

    QPen axisPen,tickerPen,gridPen;
    axisPen.setColor(QColor(0,0,0));
    axisPen.setWidthF(1.5);
    tickerPen.setColor(QColor(100,100,100));
    tickerPen.setWidthF(1.5);
    gridPen.setColor(QColor(150,150,150));
    gridPen.setWidthF(0.8);
    gridPen.setStyle(Qt::DashLine);
    //x轴
    plot->xAxis->setLabelColor(Qt::black);
    plot->xAxis->setTickLabelFont(font());
    plot->xAxis->setLabelFont(labelfont);
    plot->xAxis->setTickLabelColor(Qt::black);
    plot->xAxis->setLabel("时刻（ms）");
    plot->xAxis->setRange(0,4000);
    plot->xAxis->setTicks(true);
    plot->xAxis->setTickLabels(true);
    plot->xAxis->setSubTicks(true);
    plot->xAxis->setBasePen(axisPen);
    plot->xAxis->ticker()->setTickCount(8);
    plot->xAxis->setTickPen(tickerPen);
    plot->xAxis->setSubTickPen(tickerPen);
    plot->xAxis->grid()->setVisible(true);
    plot->xAxis->grid()->setPen(gridPen);
    plot->xAxis->grid()->setSubGridPen(gridPen);
    //y轴
    plot->yAxis->setLabelColor(Qt::black);
    plot->yAxis->setTickLabelColor(Qt::black);
    plot->yAxis->setTickLabelFont(font());
    plot->yAxis->setLabelFont(labelfont);
    plot->yAxis->setLabel("相位差(°)");//差/°
    plot->yAxis->setRange(0,100);
    plot->yAxis->setTicks(true);
    plot->yAxis->setTickLabels(true);
    plot->yAxis->setSubTicks(true);
    plot->yAxis->setBasePen(axisPen);
    plot->yAxis->ticker()->setTickCount(5);
    plot->yAxis->setTickPen(tickerPen);
    plot->yAxis->setSubTickPen(tickerPen);
    plot->yAxis->grid()->setVisible(true);
    plot->yAxis->grid()->setPen(gridPen);
    plot->yAxis->grid()->setSubGridPen(gridPen);

    //添加曲线
    for(int i=0; i<num; i++){
        addCurve(pColor[i],i);
     }

    labelfont.setPixelSize(12);
    if(num > 1){
        plot->legend->setVisible(true);
        plot->legend->setFont(labelfont);
        plot->legend->setSelectedParts(QCPLegend::spNone);
        plot->legend->setBorderPen(QPen(Qt::black));
        plot->legend->setTextColor(Qt::black);
        plot->legend->setBrush(QBrush(QColor(230,230,230,150)));
        connect(plot, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)),
                this, SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*)));
    }

    plot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::addCurve(QColor color, int i)
{
    plot->addGraph();

    QPen pen;
    if(plot->openGl())
        pen.setWidth(2);
    else
        pen.setWidth(1);

    pen.setColor(color);
    plot->graph()->setPen(pen);

    QString curveName;
    switch (i) {
    case 0:
        curveName = "相位一";
        break;
    case 1:
        curveName = "相位二";
        break;
    case 2:
        curveName = "相位三";
        break;
    case 3:
        curveName = "密度2";
        break;
    case 4:
        curveName = "密度3";
        break;
    default:
        curveName = "曲线" + QString::number(i);
        break;
    }

    plot->graph()->setName(curveName);

    // 创建复选框控制可见性
    if (!checkboxLayout) {
        // 创建复选框布局
        checkboxLayout = new QHBoxLayout();
        QWidget *checkboxContainer = new QWidget(this);
        checkboxContainer->setLayout(checkboxLayout);
        checkboxContainer->setFixedHeight(30); // 设置固定高度

        // 获取主布局并插入复选框容器
        QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
        if (mainLayout) {
            // 找到按钮布局的位置，在其后插入复选框布局
            int buttonLayoutIndex = -1;
            for (int j = 0; j < mainLayout->count(); j++) {
                QLayoutItem *item = mainLayout->itemAt(j);
                if (item && item->layout() == buttonLayout) {
                    buttonLayoutIndex = j;
                    break;
                }
            }

            if (buttonLayoutIndex != -1) {
                mainLayout->insertWidget(buttonLayoutIndex + 1, checkboxContainer);
            } else {
                mainLayout->insertWidget(1, checkboxContainer); // 默认在按钮布局后
            }
        }
    }

    // 确保复选框向量足够大
    if (i >= visibilityCheckboxes.size()) {
        visibilityCheckboxes.resize(i + 1);
    }

    QCheckBox *visibilityCheckbox = new QCheckBox(curveName, this);
    visibilityCheckbox->setChecked(true); // 默认显示

    // 设置样式 - 使用曲线颜色
    QString style = QString(
        "QCheckBox {"
        "    spacing: 5px;"
        "    color: black;"
        "    font-size: 10px;"
        "}"
        "QCheckBox::indicator {"
        "    width: 12px;"
        "    height: 12px;"
        "    border: 1px solid gray;"
        "    border-radius: 2px;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: %1;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "    background-color: lightgray;"
        "}"
        "QCheckBox::indicator:checked:hover {"
        "    background-color: %2;"
        "}"
        "QCheckBox::indicator:unchecked:hover {"
        "    background-color: #d0d0d0;"
        "}"
    ).arg(color.name(), color.lighter(120).name());

    visibilityCheckbox->setStyleSheet(style);

    // 连接信号槽
    connect(visibilityCheckbox, &QCheckBox::toggled, [this, i, color](bool checked) {
        if (i < plot->graphCount()) {
            plot->graph(i)->setVisible(checked);

            // 更新曲线颜色（隐藏时变灰色，显示时恢复原色）
            QPen pen = plot->graph(i)->pen();
            if (checked) {
                pen.setColor(color);
            } else {
                pen.setColor(Qt::gray);
            }
            plot->graph(i)->setPen(pen);

            plot->replot();
        }
    });

    checkboxLayout->addWidget(visibilityCheckbox);
    visibilityCheckboxes[i] = visibilityCheckbox;

    plot->graph()->setSelectable(QCP::stNone);
    plot->replot(QCustomPlot::rpQueuedRefresh);
}

void PlanarGraph::legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
    Q_UNUSED(legend);

    for(int i = 0;i<plot->graphCount();i++)
    {
        if(plot->legend->itemWithPlottable(plot->graph(i))==item)
        {
            QPen pen;
            if(plot->graph(i)->visible())
            {
                pen.setColor(QColor("black"));
                plot->graph(i)->setVisible(false);
            }
            else
            {
                pen.setColor(cList[i]);
                plot->graph(i)->setVisible(true);
            }
            plot->graph(i)->setPen(pen);
            plot->replot(QCustomPlot::rpQueuedRefresh);
            return;
        }
    }
}

QSharedPointer<QCPGraphDataContainer> PlanarGraph::resetCurve(QVector<double> &x,QVector<double> &y,uint8_t index)
{
    plot->graph(index)->setData(x,y);
    return plot->graph(index)->data();
}

QSharedPointer<QCPGraphDataContainer> PlanarGraph::resetCurve(QVector<double> &x,QVector<double> &y,bool m_flag)
{
    if(m_flag)
        plot->graph()->setData(x,y);
    return plot->graph()->data();
}

void PlanarGraph::selectState()
{
    QCustomPlot* senderPlot = qobject_cast<QCustomPlot*>(sender());
    if (!senderPlot) return;

    if(senderPlot->xAxis->selectedParts() == QCPAxis::spAxis || senderPlot->xAxis->selectedParts() == QCPAxis::spTickLabels){
        senderPlot->xAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
        QList<QCPAxis*> axis;
        axis << senderPlot->xAxis;
        senderPlot->axisRect()->setRangeZoomAxes(axis);
    } else if(senderPlot->yAxis->selectedParts() == QCPAxis::spAxis || senderPlot->yAxis->selectedParts() == QCPAxis::spTickLabels){
        senderPlot->yAxis->setSelectedParts(QCPAxis::spAxis|QCPAxis::spTickLabels);
        QList<QCPAxis*> axis;
        axis << senderPlot->yAxis;
        senderPlot->axisRect()->setRangeZoomAxes(axis);
    } else {
        QList<QCPAxis*> axis;
        axis << senderPlot->xAxis << senderPlot->yAxis;
        senderPlot->axisRect()->setRangeZoomAxes(axis);
    }
}


void PlanarGraph::updateAxisRange(const QCPRange range)
{
    QCustomPlot* senderPlot = qobject_cast<QCustomPlot*>(sender());
    if (!senderPlot) return;

    if(senderPlot->xAxis->range().lower < 0){
        senderPlot->xAxis->setRangeLower(0);
    }
}


void PlanarGraph::dealMousePressEvent(QMouseEvent *event)
{
    QCustomPlot* senderPlot = qobject_cast<QCustomPlot*>(sender());
    if (!senderPlot) return;

    if(event->button() == Qt::RightButton){
         senderPlot->setSelectionRectMode(QCP::srmNone);
    } else if(event->button() == Qt::MiddleButton){
        senderPlot->selectionRect()->setBrush(QColor(200,200,200,50));
        senderPlot->setSelectionRectMode(QCP::SelectionRectMode::srmZoom);
        senderPlot->axisRect()->setRangeZoomFactor(0.5);
        senderPlot->axisRect()->setRangeZoomAxes(senderPlot->xAxis,senderPlot->yAxis);
    } else if(event->button() == Qt::LeftButton){
        if(isZoomEnabled){
            senderPlot->setSelectionRectMode(QCP::SelectionRectMode::srmZoom);
        } else {
            senderPlot->setSelectionRectMode(QCP::SelectionRectMode::srmNone);
        }
    }
}

void PlanarGraph::update_bkColor(QColor bkColor)
{
    plot->setBackground(bkColor);
    plot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::updateGraphData(bool first_flag)
{
    xNumber.resize(ADDRESS);
    for(int i=0; i<ADDRESS; i++){
        xNumber[i] = i;
    }
    dataNumber.resize(ADDRESS);
    ch_graph = resetCurve(xNumber,dataNumber,first_flag);
}

void PlanarGraph::changeGraphName(int index, QString name)
{
    plot->graph(index)->setName(name);
}

void PlanarGraph::plotAddText(QString m_devName)
{
    plot->plotLayout()->insertRow(0);
    devText   = new QCPTextElement(plot);
//    devText->setMarginGroup();

    devText->setText("卡:"+m_devName);

    devText->setTextFlags(Qt::AlignHCenter|Qt::AlignVCenter);

    devText->setFont(QFont("黑体",10));

    QCPLayoutGrid *m_layout = new QCPLayoutGrid;
    m_layout->addElement(0,0,new QCPTextElement(plot,""));
    m_layout->addElement(0,1,devText);
    m_layout->addElement(0,2,new QCPTextElement(plot,""));
    m_layout->setMargins(QMargins(0,0,0,0));

    plot->plotLayout()->addElement(0,0,m_layout);
    plot->plotLayout()->setRowSpacing(0);
}

void PlanarGraph::ContextMenu(const QPoint &pos)
{
    QObject* s = sender();
    if (s == plot) {
        contextMenu->exec(plot->mapToGlobal(pos));
    } else if (s == m_refTimePlot) {
        contextMenu_refTime->exec(m_refTimePlot->mapToGlobal(pos));
    } else if (s == m_refFftPlot) {
        contextMenu_refFft->exec(m_refFftPlot->mapToGlobal(pos));
    } else if (s == m_mesTimePlot) {
        contextMenu_mesTime->exec(m_mesTimePlot->mapToGlobal(pos));
    } else if (s == m_mesFftPlot) {
        contextMenu_mesFft->exec(m_mesFftPlot->mapToGlobal(pos));
    }
}


//初始化范围
void PlanarGraph::resetPlotRange()
{
    plot->rescaleAxes();
    plot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::resetPlotRange1()
{
    m_refTimePlot->rescaleAxes();
    m_refTimePlot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::resetPlotRange2()
{
    m_refFftPlot->rescaleAxes();
    m_refFftPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::resetPlotRange3()
{
    m_mesTimePlot->rescaleAxes();
    m_mesTimePlot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::resetPlotRange4()
{
    m_mesFftPlot->rescaleAxes();
    m_mesFftPlot->replot(QCustomPlot::rpImmediateRefresh);
}


void PlanarGraph::maxPlotWidget()
{
    UINT8 index = this->property("设备:").toUInt();

    if(maxWidget->text() == "窗口最大化")
    {
        maxWidget->setText("窗口大小还原");
        plot->yAxis->ticker()->setTickCount(10);
        emit windowFullorRestoreSize(index,true);
    }
    else
    {
        maxWidget->setText("窗口最大化");
        plot->yAxis->ticker()->setTickCount(5);
        emit windowFullorRestoreSize(index,false);
    }
    plot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::maxPlotWidget1()
{
    toggleMaximize(m_refTimePlot, maxWidget1);
}

void PlanarGraph::maxPlotWidget2()
{
    toggleMaximize(m_refFftPlot, maxWidget2);
}

void PlanarGraph::maxPlotWidget3()
{
    toggleMaximize(m_mesTimePlot, maxWidget3);
}

void PlanarGraph::maxPlotWidget4()
{
    toggleMaximize(m_mesFftPlot, maxWidget4);
}

void PlanarGraph::toggleMaximize(QCustomPlot* targetPlot, QAction* action)
{
    if (!targetPlot || !action) return;

    bool isMax = (action->text() == "窗口最大化");

    if (isMax)
    {
        action->setText("窗口大小还原");
        targetPlot->yAxis->ticker()->setTickCount(10);

        // 隐藏所有子图，只保留当前 targetPlot
        QList<QCustomPlot*> allPlots = {plot, m_refTimePlot, m_refFftPlot, m_mesTimePlot, m_mesFftPlot};
        for (auto p : allPlots)
        {
            if (p && p != targetPlot)
                p->setHidden(true);
        }
    }
    else
    {
        action->setText("窗口最大化");
        targetPlot->yAxis->ticker()->setTickCount(5);

        // 恢复显示所有子图
        QList<QCustomPlot*> allPlots = {plot, m_refTimePlot, m_refFftPlot, m_mesTimePlot, m_mesFftPlot};
        for (auto p : allPlots)
        {
            if (p)
                p->setHidden(false);
        }
    }

    targetPlot->replot(QCustomPlot::rpImmediateRefresh);
}

void PlanarGraph::savePlot2Png()
{
    QString path =QFileDialog::getSaveFileName(this,"保存图片","../","PNG(*.png);;JPG(*.jpg)");
    plot->savePng(path,800,600);
}

void PlanarGraph::savePlot2Png1()
{
    QString path =QFileDialog::getSaveFileName(this,"保存图片","../","PNG(*.png);;JPG(*.jpg)");
    m_refTimePlot->savePng(path,800,600);
}

void PlanarGraph::savePlot2Png2()
{
    QString path =QFileDialog::getSaveFileName(this,"保存图片","../","PNG(*.png);;JPG(*.jpg)");
     m_refFftPlot->savePng(path,800,600);
}

void PlanarGraph::savePlot2Png3()
{
    QString path =QFileDialog::getSaveFileName(this,"保存图片","../","PNG(*.png);;JPG(*.jpg)");
    m_mesTimePlot->savePng(path,800,600);
}

void PlanarGraph::savePlot2Png4()
{
    QString path =QFileDialog::getSaveFileName(this,"保存图片","../","PNG(*.png);;JPG(*.jpg)");
     m_mesFftPlot->savePng(path,800,600);
}
/*********************深度图***********************/
void PlanarGraph::depth_Plot_Init(QColor bkColor,QColor axisColor)
{
    plot->setAutoFillBackground(true);
    plot->setNoAntialiasingOnDrag(true);//关闭拖动期间的抗锯齿
    plot->setBackground(bkColor);
    plot->setBackgroundScaled(true);
    plot->setBackgroundScaledMode(Qt::IgnoreAspectRatio);

    QFont font("Times New Roman",11);

    plot->setInteractions(QCP::iRangeDrag|QCP::iRangeZoom);
    plot->setOpenGl(false);
    plot->axisRect()->setupFullAxesBox(true);
    plot->xAxis->setLabel("ADC Channel");
    plot->yAxis->setLabel("PSD Ratio");
    plot->xAxis->setLabelFont(font);
    plot->yAxis->setLabelFont(font);

    plot->xAxis->ticker()->setTickCount(10);
    plot->yAxis->ticker()->setTickCount(10);

    QPen axisPen,tickerPen,gridPen;
    axisPen.setColor(axisColor);
    axisPen.setWidthF(1.5);
    tickerPen.setColor(axisColor);
    tickerPen.setWidthF(1.5);
    gridPen.setColor(axisColor);
    gridPen.setWidthF(1);
    gridPen.setStyle(Qt::DashLine);

    //x轴
    plot->xAxis->setLabelColor(axisColor);
    plot->xAxis->setTickLabelColor(axisColor);
    plot->xAxis->setTicks(true);
    plot->xAxis->setTickLabels(true);
    plot->xAxis->setSubTicks(true);
    plot->xAxis->setBasePen(axisPen);
    plot->xAxis->setTickPen(tickerPen);
    plot->xAxis->setSubTickPen(tickerPen);
    plot->xAxis->grid()->setVisible(true);
    plot->xAxis->grid()->setPen(gridPen);
    plot->xAxis->grid()->setSubGridPen(gridPen);

    plot->xAxis2->setBasePen(axisPen);
    plot->xAxis2->setTickPen(tickerPen);
    plot->xAxis2->setSubTickPen(tickerPen);
    plot->xAxis2->grid()->setPen(gridPen);
    plot->xAxis2->grid()->setSubGridPen(gridPen);

    //y轴
    plot->yAxis->setLabelColor(axisColor);
    plot->yAxis->setTickLabelColor(axisColor);
    plot->yAxis->setTicks(true);
    plot->yAxis->setTickLabels(true);
    plot->yAxis->setSubTicks(true);
    plot->yAxis->setBasePen(axisPen);
    plot->yAxis->setTickPen(tickerPen);
    plot->yAxis->setSubTickPen(tickerPen);
    plot->yAxis->grid()->setVisible(true);
    plot->yAxis->grid()->setPen(gridPen);
    plot->yAxis->grid()->setSubGridPen(gridPen);

    plot->yAxis2->setBasePen(axisPen);
    plot->yAxis2->setTickPen(tickerPen);
    plot->yAxis2->setSubTickPen(tickerPen);
    plot->yAxis2->grid()->setPen(gridPen);
    plot->yAxis2->grid()->setSubGridPen(gridPen);


    //初始化画布,设置点为1024*1000
    colorMap = new QCPColorMap(plot->xAxis,plot->yAxis);
    colorMap->data()->setSize(NX,NY);
    colorMap->data()->setRange(QCPRange(0,1024),QCPRange(0,1000));

    //初始化色级
    QCPColorScale *scale = new QCPColorScale(plot);
    plot->plotLayout()->addElement(1,1,scale);
    scale->setType(QCPAxis::atRight);
    scale->setDataRange(QCPRange(0,350));
    scale->setRangeDrag(false);
    scale->setRangeZoom(false);
    scale->axis()->ticker()->setTickCount(6);
    scale->axis()->setTickLabelColor(axisColor);
    colorMap->setColorScale(scale);

    //初始化色梯
    QCPColorGradient *gradient = new QCPColorGradient();
    gradient->setColorStopAt(0.15,QColor(0,100,255));
    gradient->setColorStopAt(0.3,QColor(0,255,255));
    gradient->setColorStopAt(0.45,QColor(0,255,120));
    gradient->setColorStopAt(0.6,QColor(255,255,0));
    gradient->setColorStopAt(0.75,QColor(255,140,0));
    gradient->setColorStopAt(0.9,QColor(200,20,0));
    gradient->setColorStopAt(1,QColor(130,13,0));
    colorMap->setGradient(*gradient);
    colorMap->rescaleDataRange(true);

    //设置色级显示的高度大小
    QCPMarginGroup *marginGroup = new QCPMarginGroup(plot);
    plot->axisRect()->setMarginGroup(QCP::msBottom|QCP::msTop, marginGroup);
    scale->setMarginGroup(QCP::msBottom|QCP::msTop, marginGroup);
    plot->rescaleAxes();

    //初始化曲线界面
//    QVector<QVector<DWORD32>> Psd_data;
//    Psd_data.resize(NX);
//    for(int i=0; i<NX; i++){
//        Psd_data[i].resize(NY);
//    }
    for(int i=0;i<NX;i++)
    {
        for(int j=0; j<NY; j++)
        {
//              Psd_data[i][j] = 0;
              colorMap->data()->setAlpha(i,j,0);
        }
    }
    plot->replot(QCustomPlot::rpQueuedRefresh);
}

// 按钮点击事件的槽函数实现
void PlanarGraph::onDragButtonClicked()
{
    isDragEnabled = true;
    isZoomEnabled = false;

    // 更新按钮图标（使用内嵌资源）
    QIcon dragOnIcon(":/new/prefix1/icons/drag_on.png");
    QIcon zoomOffIcon(":/new/prefix1/icons/zoom_off.png");

    if (!dragOnIcon.isNull()) {
        dragButton->setIcon(dragOnIcon);
    } else {
        dragButton->setText("拖");
        dragButton->setStyleSheet("QPushButton { background-color: #d0f0d0; border: 1px solid #ccc; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #c0e0c0; }");
    }

    if (!zoomOffIcon.isNull()) {
        zoomButton->setIcon(zoomOffIcon);
    } else {
        zoomButton->setText("选");
        zoomButton->setStyleSheet("QPushButton { background-color: #f0f0f0; border: 1px solid #ccc; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #e0e0e0; }");
    }

    // 同步所有图
        applyInteractions(plot, true, false);
        applyInteractions(m_refTimePlot, true, false);
        applyInteractions(m_refFftPlot, true, false);
        applyInteractions(m_mesTimePlot, true, false);
        applyInteractions(m_mesFftPlot, true, false);

}

void PlanarGraph::onZoomButtonClicked()
{
    isDragEnabled = false;
    isZoomEnabled = true;

    // 更新按钮图标（使用内嵌资源）
    QIcon dragOffIcon(":/new/prefix1/icons/drag_off.png");
    QIcon zoomOnIcon(":/new/prefix1/icons/zoom_on.png");

    if (!dragOffIcon.isNull()) {
        dragButton->setIcon(dragOffIcon);
    } else {
        dragButton->setText("拖");
        dragButton->setStyleSheet("QPushButton { background-color: #f0f0f0; border: 1px solid #ccc; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #e0e0e0; }");
    }

    if (!zoomOnIcon.isNull()) {
        zoomButton->setIcon(zoomOnIcon);
    } else {
        zoomButton->setText("选");
        zoomButton->setStyleSheet("QPushButton { background-color: #d0f0d0; border: 1px solid #ccc; border-radius: 3px; font-weight: bold; } QPushButton:hover { background-color: #c0e0c0; }");
    }

    // 设置框选放大模式
    plot->selectionRect()->setPen(QPen(Qt::black, 1, Qt::DashLine));//设置选框的样式：虚线
    plot->selectionRect()->setBrush(QBrush(QColor(0, 0, 100, 50)));//设置选框的样式：半透明浅蓝
    // 设置选框样式（你原来的逻辑，只要一次即可）
        plot->selectionRect()->setPen(QPen(Qt::black, 1, Qt::DashLine));
        plot->selectionRect()->setBrush(QBrush(QColor(0, 0, 100, 50)));

        // 同步所有图
        applyInteractions(plot, false, true);
        applyInteractions(m_refTimePlot, false, true);
        applyInteractions(m_refFftPlot, false, true);
        applyInteractions(m_mesTimePlot, false, true);
        applyInteractions(m_mesFftPlot, false, true);
}

void PlanarGraph::addTestData()
{
    // 创建测试颜色列表
    QList<QColor> testColors;
    testColors << QColor(Qt::red) << QColor(Qt::blue) << QColor(Qt::green);
    
    // 初始化2D图表，创建3条曲线
    plot_2D_Init(QColor(Qt::white), 3, testColors);
    
    // 生成测试数据
    QVector<double> x, y1, y2, y3;
    for (int i = 0; i < 1000; ++i) {
        double time = i * 4.0; // 时间范围0-4000ms
        x.append(time);
        
        // 第一条曲线：正弦波
        y1.append(50 + 30 * qSin(time * 0.01) + 10 * qSin(time * 0.03));
        
        // 第二条曲线：余弦波
        y2.append(40 + 25 * qCos(time * 0.008) + 15 * qSin(time * 0.02));
        
        // 第三条曲线：复合波形
        y3.append(60 + 20 * qSin(time * 0.005) + 10 * qCos(time * 0.015));
    }
    
    // 设置测试数据到各条曲线
    if (plot->graphCount() >= 3) {
        plot->graph(0)->setData(x, y1);
        plot->graph(1)->setData(x, y2);
        plot->graph(2)->setData(x, y3);
        
        // 重新绘制图表
        plot->replot();
    }
    
    // 重新应用当前按钮状态的交互设置
    if (isDragEnabled) {
        onDragButtonClicked();
    } else if (isZoomEnabled) {
        onZoomButtonClicked();
    }
}

void PlanarGraph::applyInteractions(QCustomPlot* p, bool dragEnabled, bool zoomEnabled)
{
    if (!p) return;
    p->setSelectionRectMode(zoomEnabled ? QCP::srmZoom : QCP::srmNone);
    p->setInteraction(QCP::iRangeDrag, dragEnabled);
    p->setInteraction(QCP::iRangeZoom, zoomEnabled);
    p->setInteraction(QCP::iSelectAxes, true);
    p->setInteraction(QCP::iSelectLegend, true);
}

// planargraph.cpp 中可以移除或简化一些方法，因为我们直接使用QCustomPlot

// 保留这些简单的方法
void PlanarGraph::setGraphData(int graphIndex, const QVector<double> &xData, const QVector<double> &yData)
{
    if (plot && graphIndex >= 0 && graphIndex < plot->graphCount()) {
        plot->graph(graphIndex)->setData(xData, yData);
        plot->replot();
    }
}

void PlanarGraph::setXAxisRange(double lower, double upper)
{
    if (plot) {
        plot->xAxis->setRange(lower, upper);
        plot->replot();
    }
}

void PlanarGraph::setYAxisRange(double lower, double upper)
{
    if (plot) {
        plot->yAxis->setRange(lower, upper);
        plot->replot();
    }
}
PlanarGraph::~PlanarGraph()
{
    // 清理复选框
    for (QCheckBox *checkbox : visibilityCheckboxes) {
        if (checkbox) {
            delete checkbox;
        }
    }
    visibilityCheckboxes.clear();
}
