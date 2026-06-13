#include "mainwindow.h"
#include "capturethread.h"
#include <QVBoxLayout>
#include <QtCharts/QValueAxis>
#include <QDebug>
#include <QTimer>
#include <iostream>
MainWindow::MainWindow(const QString &iface, QWidget *parent)
    : QMainWindow(parent), m_button(new QPushButton("Сбросить график", this)),
      m_chartView(new QChartView(this)), m_series2(new QLineSeries())
{
    m_series = new QLineSeries();
    auto pen = m_series->pen();
    pen.setWidth(10);
    pen.setBrush(QBrush("red"));
    m_series->setPen(pen);

    m_series2 = new QLineSeries();
    auto pen2 = m_series2->pen();
    pen2.setWidth(10);
    pen2.setBrush(QBrush("blue"));
    m_series2->setPen(pen2);
    setupUI();
    initGraph();
    // Запускаем поток захвата с указанным интерфейсом
    m_captureThread = new CaptureThread(iface, this);
    connect(m_captureThread, &CaptureThread::packetCountPair, this, &MainWindow::onPacketCountReceived);
    m_captureThread->start();

    m_graphTimer = new QTimer(this);
    connect(m_graphTimer, &QTimer::timeout, this, &MainWindow::updateGraph);
    m_graphTimer->start(100);
}

MainWindow::~MainWindow()
{
    if (m_captureThread) {
        m_captureThread->stop();
        m_captureThread->wait();
        delete m_captureThread;
    }
}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->addWidget(m_chartView);
    layout->addWidget(m_button);
    setCentralWidget(central);
    resize(1600, 1000);
    setWindowTitle("Счётчик фотонов");

    connect(m_button, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
}

void MainWindow::initGraph()
{
    QChart *chart = new QChart();
    chart->setTitle("Счётчик фотонов");
    chart->addSeries(m_series);
    chart->addSeries(m_series2);
    
    m_series->setName("Канал 0");
    m_series2->setName("Канал 1");   
    chart->legend()->setVisible(true);
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Время");
    axisX->setRange(0, 100);
    axisX->setLabelFormat("%.0f");
    axisX->setTickCount(0.1);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Число фотонов");
    axisY->setRange(0, 100); // начальный диапазон, будет автоматически расширяться

    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    m_series->attachAxis(axisX);
    m_series->attachAxis(axisY);
    m_series2->attachAxis(axisX);
    m_series2->attachAxis(axisY);

    m_chartView->setChart(chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_points.clear();
    /*for (int i = 0; i < 10; ++i) {
        m_points.append(QPointF(i, 0.0));
    }*/
    // Таймер для обновления графика (не чаще 100 мс)
    m_graphTimer = new QTimer(this);
    connect(m_graphTimer, &QTimer::timeout, this, &MainWindow::updateGraph);
    m_graphTimer->start(100);
}

void MainWindow::onButtonClicked()
{
    // Сброс графика
    m_points.clear();
    m_series->clear();
    m_points2.clear();
    m_series2->clear();
    QChart *chart = m_chartView->chart();
    QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
    if (axisY) axisY->setRange(0, 100);
}

void MainWindow::onPacketCountReceived(int first, int second)
{
    // Добавляем новую точку (индекс = количество уже сохранённых точек)
    qreal x = m_points.size();          // 0,1,2,... до 99
    m_points.append(QPointF(x, first));
    m_points2.append(QPointF(x, second));

    // Оставляем только последние 100 точек
    while (m_points.size() > 100) {
        m_points.removeFirst();
        m_points2.removeFirst();

        // Сдвигаем X-координаты оставшихся точек влево
        for (int i = 0; i < m_points.size(); ++i) {
            m_points[i].setX(i);
            m_points2[i].setX(i);

        }
    }
}

void MainWindow::updateGraph()
{
    m_series->clear();
    m_series2->clear();
    for (const QPointF &p : m_points) {
        m_series->append(p);
    }
    for (const QPointF &p : m_points2) {
        m_series2->append(p);
    }
    // Автоматическая настройка оси Y (с запасом)
    if (!m_points.isEmpty()) {
        double maxY = 0;
        for (const QPointF &p : m_points)
            if (p.y() > maxY) maxY = p.y();
        for (const QPointF &p : m_points2)
            if (p.y() > maxY) maxY = p.y();
        maxY = qMax(10.0, maxY * 1.1);
        QChart *chart = m_chartView->chart();
        QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
        if (axisY) axisY->setRange(0, maxY);
    }
}