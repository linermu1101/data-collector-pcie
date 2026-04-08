#ifndef PLANARGRAPH_H
#define PLANARGRAPH_H

#include <QObject>
#include <QElapsedTimer>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "qcustomplot.h"

#define ADDRESS 4000
#define NX 1025
#define NY 1001

//2D平面图表
class PlanarGraph : public QWidget
{
    Q_OBJECT
public:

    explicit PlanarGraph(QString devName,QWidget *parent = nullptr);
    ~PlanarGraph();
    QCustomPlot *plot;
    QCPColorMap *colorMap;
    QVector<double> ch_xData,ch_yData;
    QCPTextElement *devText;

    QVector<double> xNumber,dataNumber;
    QSharedPointer<QCPGraphDataContainer> ch_graph;
    QCustomPlot* getCustomPlot() const { return plot; }
    QCustomPlot* getRefTimePlot() const { return m_refTimePlot; }
    QCustomPlot* getRefFftPlot()  const { return m_refFftPlot; }
    QCustomPlot* getMesTimePlot() const { return m_mesTimePlot; }
    QCustomPlot* getMesFftPlot()  const { return m_mesFftPlot; }

    void replot() { if (plot) plot->replot(); }
    // 控制按钮
    QPushButton *dragButton;
    QPushButton *zoomButton;
    bool isDragEnabled;
    bool isZoomEnabled;

    void plot_2D_Init(QColor bkColor,uint8_t num,QList<QColor> &pColor);
    void depth_Plot_Init(QColor bkColor,QColor axisColor);
    void update_bkColor(QColor bkColor);
    void updateGraphData(bool first_flag);
    void changeGraphName(int index, QString name);
    QSharedPointer<QCPGraphDataContainer> resetCurve(QVector<double> &x,QVector<double> &y,uint8_t index);
    QSharedPointer<QCPGraphDataContainer> resetCurve(QVector<double> &x,QVector<double> &y,bool m_flag);
    // 修改getter方法，返回const引用
        const QList<QColor>& getColorList() const { return colorList; }

        // 修改setter方法，接受const引用
        void setColorList(const QList<QColor> &colors) { colorList = colors; }

        int getLineWidth() const { return lineWidth; }
        void setLineWidth(int width) { lineWidth = width; }

        const QColor& getBackgroundColor() const { return backgroundColor; }
        void setBackgroundColor(const QColor &color) { backgroundColor = color; }

        bool isMaximized() const { return windowState() & Qt::WindowMaximized; }
        // 添加数据访问和设置方法
            void setGraphData(int graphIndex, const QVector<double> &xData, const QVector<double> &yData);
            QVector<double> getXData(int graphIndex) const;
            QVector<double> getYData(int graphIndex) const;

            void setXAxisRange(double lower, double upper);
            void setYAxisRange(double lower, double upper);
            void setXAxisLabel(const QString &label);
            void setYAxisLabel(const QString &label);

            QCPRange getXAxisRange() const;
            QCPRange getYAxisRange() const;
            QString getXAxisLabel() const;
            QString getYAxisLabel() const;

            int graphCount() const;
private slots:
    void selectState();
    void updateAxisRange(QCPRange range);
    void dealMousePressEvent(QMouseEvent *event);
    void addCurve(QColor color,int i);
    void legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item);

    void ContextMenu(const QPoint &pos);

    void resetPlotRange();
    void resetPlotRange1();
    void resetPlotRange2();
    void resetPlotRange3();
    void resetPlotRange4();

    void maxPlotWidget();
    void maxPlotWidget1();
    void maxPlotWidget2();
    void maxPlotWidget3();
    void maxPlotWidget4();

    void savePlot2Png();
    void savePlot2Png1();
    void savePlot2Png2();
    void savePlot2Png3();
    void savePlot2Png4();

    // 按钮控制槽函数
    void onDragButtonClicked();
    void onZoomButtonClicked();


signals:
    void windowFullorRestoreSize(int devIndex,bool full);

private:
    QList<QColor> cList;

    QMenu *contextMenu;
    QMenu *contextMenu_refTime;
    QMenu *contextMenu_refFft;
    QMenu *contextMenu_mesTime;
    QMenu *contextMenu_mesFft;

    QAction *resetRange;
    QAction *maxWidget;
    QAction *savePng;

    QAction *resetRange1;
    QAction *maxWidget1;
    QAction *savePng1;

    QAction *resetRange2;
    QAction *maxWidget2;
    QAction *savePng2;

    QAction *resetRange3;
    QAction *maxWidget3;
    QAction *savePng3;

    QAction *resetRange4;
    QAction *maxWidget4;
    QAction *savePng4;

    QList<QColor> colorList;
    int lineWidth;
    QColor backgroundColor;

    void plotAddText(QString m_devName);
    void addTestData();
    void applyInteractions(QCustomPlot* p, bool dragEnabled, bool zoomEnabled);
    void toggleMaximize(QCustomPlot* targetPlot, QAction* action);
    // 添加按钮相关的成员变量
    QHBoxLayout *buttonLayout; // 按钮布局
    QHBoxLayout *checkboxLayout; // 复选框布局
    QVector<QCheckBox*> visibilityCheckboxes; // 可见性复选框

    QCustomPlot* m_refTimePlot;
    QCustomPlot* m_refFftPlot;
    QCustomPlot* m_mesTimePlot;
    QCustomPlot* m_mesFftPlot;

//    void Equipment(QString Equipment_number);
};

#endif // PLANARGRAPH_H
