#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTime>
#include <QTimer>
#include <lsl_cpp.h>
#include <qcustomplot.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(int n_channel, int s_rate,std::vector<std::vector<int>> layout,QWidget *parent = nullptr);
    ~MainWindow();

    void addCustomPlot(int row, int col);
private slots:
    void setupPlot();
    void realtimeDataSlot();
private:
    Ui::MainWindow *ui;
    QTimer dataTimer;
    std::unique_ptr<lsl::stream_inlet> inlet; // Pointer for the LSL inlet
    std::unique_ptr<lsl::stream_inlet> spike_inlet;
    int n_channel, s_rate;
    std::vector<QCustomPlot*> plots;
    std::vector<QCustomPlot*> spike_plots;
    std::unordered_map<QCustomPlot*, int> plotChannelMap;
    std::unordered_map<QCustomPlot*, int> spike_plotChannelMap;

    std::vector<std::vector<int>> layout;
};
#endif // MAINWINDOW_H
