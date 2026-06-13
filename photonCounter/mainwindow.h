#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QPushButton>
#include <QVector>
#include <QTimer>
QT_CHARTS_USE_NAMESPACE

class CaptureThread;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(const QString &iface, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onButtonClicked();
    void onPacketCountReceived(int first, int second);
    void updateGraph();

private:
    QPushButton *m_button;
    QChartView *m_chartView;
    QLineSeries *m_series;
    QLineSeries *m_series2;
    QVector<QPointF> m_points;
    QVector<QPointF> m_points2;
    CaptureThread *m_captureThread;
    QTimer *m_graphTimer;

    void setupUI();
    void initGraph();
};

#endif