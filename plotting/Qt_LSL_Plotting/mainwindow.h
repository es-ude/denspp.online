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

    lsl::stream_inlet*getInlet() const;

    lsl::stream_inlet*getSpikeInlet() const;

    const std::unordered_map<QCustomPlot *, int> &getSpike_plotChannelMap() const;

private slots:
    void setupPlot();
public slots:
    void realtimeDataSlot();
    void realtimeSpikeDataSlot();

private:
    Ui::MainWindow *ui;
    QTimer dataTimer;
    std::unique_ptr<lsl::stream_inlet> inlet; // Pointer for the LSL inlet
    std::unique_ptr<lsl::stream_inlet> spike_inlet;
    int n_channel, s_rate;
    long samples_received = 0;
    std::vector<QCustomPlot*> plots;
    std::vector<QCustomPlot*> spike_plots;
    std::vector<QColor*> spike_colors;
    std::unordered_map<QCustomPlot*, int> plotChannelMap;
    std::unordered_map<QCustomPlot*, int> spike_plotChannelMap;
    std::vector<std::vector<int>> layout;
};
#endif // MAINWINDOW_H
