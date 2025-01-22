#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <lsl_cpp.h>
#include <iostream>
#include <QThread>

MainWindow::MainWindow(int n_channel,int s_rate,std::vector<std::vector<int>> layout,QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , n_channel(n_channel)
    , s_rate(s_rate)
    , layout(layout)

{
    ui->setupUi(this);

    // resolve stream of filtered Data
    std::vector<lsl::stream_info> results = lsl::resolve_stream("name", "BioSemi_filtered");
    if (!results.empty()) {
        inlet = std::make_unique<lsl::stream_inlet>(results[0]);
        this->n_channel =results[0].channel_count();
        this->s_rate = results[0].nominal_srate();
        std::cout << this->n_channel << " " << s_rate << std::endl;
    } else {
            qWarning("No LSL stream named 'BioSemiFiltered' found. Ensure stream is running.");
    }

    // resolve stream of spike values
    std::vector<lsl::stream_info> spike_results = lsl::resolve_stream("name", "spikes");
    if(!spike_results.empty()){
        spike_inlet = std::make_unique<lsl::stream_inlet>(spike_results[0]);
        std::cout << "connected to stream of spike waveforms!" << std::endl;
    } else {
        qWarning("No LSL stream named 'spikes' found. Ensure stream is running.");
    }

    MainWindow::setupPlot();

    connect(&dataTimer, &QTimer::timeout, this, &MainWindow::realtimeSpikeDataSlot);
    connect(&dataTimer, &QTimer::timeout, this, &MainWindow::realtimeDataSlot);
    dataTimer.start(0);
}

MainWindow::~MainWindow() {
    delete ui;
}
void MainWindow::setupPlot()
{
    QGridLayout* gridLayout = qobject_cast<QGridLayout*>(ui->customPlot->layout());
    if (!gridLayout) {
        gridLayout = new QGridLayout(ui->customPlot);
        ui->customPlot->setLayout(gridLayout);
    }

    QGridLayout* spike_gridLayout = qobject_cast<QGridLayout*>(ui->spike_waves->layout());
    if (!spike_gridLayout) {
        spike_gridLayout = new QGridLayout(ui->spike_waves);
        ui->spike_waves->setLayout(spike_gridLayout);
    }

    spike_gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    spike_gridLayout->setSpacing(0);
    gridLayout->setSpacing(0);

    int rows = layout.size();
    int cols = layout[0].size();

    // setup raw and filtered data plots
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int channel = layout[row][col];
            if (channel <= 0 || channel > n_channel/2) {
                continue;
            }

            // Create the plot for this channel
            QCustomPlot* plot = new QCustomPlot(ui->customPlot);
            plot->addGraph(); // Raw data
            plot->addGraph(); // Filtered data

            // Configure graph appearance
            plot->graph(0)->setPen(QPen(QColor(40, 110, 255))); // Blue for raw
            plot->graph(1)->setPen(QPen(QColor(255, 110, 40))); // Red for filtered

            // Configure axes
            QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
            timeTicker->setTimeFormat("%h:%m:%s");
            plot->xAxis->setTicker(timeTicker);
            //plot->axisRect()->setupFullAxesBox();
            plot->yAxis->setRange(-750, 750);

            plot->xAxis->setLabel("");
            plot->yAxis->setLabel("");

            plot->xAxis->setTickLabels(false);
            plot->yAxis->setTickLabels(false);

            QCPMarginGroup* marginGroup = new QCPMarginGroup(plot);
            plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
            plot->axisRect()->setMinimumMargins(QMargins(0, 0, 0, 0));
            plot->axisRect()->setMarginGroup(QCP::msAll, marginGroup);

            gridLayout->addWidget(plot, row, col);

            // Add a text label with the channel index
            QCPItemText* textLabel = new QCPItemText(plot);
            textLabel->setText(QString("%1").arg(channel));
            textLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
            textLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
            textLabel->position->setCoords(0.08, 0.02); // Position in the top-left corner
            textLabel->setFont(QFont("Arial", 10, QFont::Bold));
            textLabel->setColor(QColor(50, 50, 50));

            plots.push_back(plot);
            plotChannelMap[plot] = channel - 1;
        }
    }
    std::cout << "Raw and Filter Data Plot layer is ready.." << std::endl;

    // setup Waveform Data plots:
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            int channel = layout[row][col];
            if (channel <= 0 || channel > n_channel/2) {
                continue;
            }

            // Create the plot for this channel
            QCustomPlot* plot = new QCustomPlot(ui->spike_waves);
            plot->addGraph();

            // Configure graph appearance
            plot->graph(0)->setPen(QPen(QColor(0, 0, 0)));


            plot->xAxis->setRange(0, 32);  // MAGIC NUMBER!!
            //plot->axisRect()->setupFullAxesBox();
            plot->yAxis->setRange(-500, 300);

            plot->xAxis->setLabel("");
            plot->yAxis->setLabel("");

            plot->xAxis->setTickLabels(false);
            plot->yAxis->setTickLabels(false);

            QCPMarginGroup* marginGroup = new QCPMarginGroup(plot);
            plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
            plot->axisRect()->setMinimumMargins(QMargins(0, 0, 0, 0));
            plot->axisRect()->setMarginGroup(QCP::msAll, marginGroup);

            spike_gridLayout->addWidget(plot, row, col);

            //QCPItemText* textLabel = new QCPItemText(plot);
            //textLabel->setText(QString("%1").arg(channel));
            //textLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
            //textLabel->position->setType(QCPItemPosition::ptAxisRectRatio);
            //textLabel->position->setCoords(0.08, 0.02); // Position in the top-left corner
            //textLabel->setFont(QFont("Arial", 10, QFont::Bold));
            //textLabel->setColor(QColor(50, 50, 50));

            // create color for this channel:
            QColor* color = new QColor(0,0,0);
            spike_colors.push_back(color);

            spike_plots.push_back(plot);
            spike_plotChannelMap[plot] = channel - 1;
        }
    }
}
void MainWindow::realtimeSpikeDataSlot(){
    std::vector<double> timestamps;
    std::vector<std::vector<double>> spike_samples;
    bool spikes_available = false;

    spikes_available = spike_inlet->pull_chunk(spike_samples, timestamps);


    if(spikes_available){
         for(auto& spike : spike_samples){
            for(auto& [spike_plot, channel] : spike_plotChannelMap){
                if(spike[0] == channel){
                    spike_plot->graph(0)->data()->clear();
                    for(int i=1; i< spike.size(); i++){
                        spike_plot->graph(0)->addData(i,spike[i]);
                    }
                    spike_plot->replot();
                }else{

                }
            }
        }
    }

}

const std::unordered_map<QCustomPlot *, int> &MainWindow::getSpike_plotChannelMap() const
{
    return spike_plotChannelMap;
}

lsl::stream_inlet*MainWindow::getSpikeInlet() const
{
    return spike_inlet.get();
}

lsl::stream_inlet* MainWindow::getInlet() const
{
    return inlet.get();
}
void MainWindow::realtimeDataSlot()
{
    std::vector<std::vector<int>> samples;
    std::vector<double> timestamps;
    inlet->pull_chunk(samples, timestamps);

    float minVal = std::numeric_limits<double>::max();
    float maxVal = std::numeric_limits<double>::lowest();
    int step_size = 1;
    if(s_rate > 10000){
        step_size = 3;
    }
    for (size_t i = 0; i < samples.size(); i+=step_size) {
        samples_received+=step_size;
        for (auto& [plot, channel] : plotChannelMap) {
            plot->graph(0)->addData(timestamps[i], samples[i][2*channel]);
            plot->graph(1)->addData(timestamps[i],samples[i][2*channel+1]);
            if (samples_received % s_rate == 0) {  // every second
                double sampleValue = samples[i][2*channel];
                if (sampleValue < minVal) minVal = sampleValue;  // update plot limits
                if (sampleValue > maxVal) maxVal = sampleValue;
                plot->graph(0)->data()->removeBefore(timestamps[i] - 1);  // remove old datapoints
                plot->graph(1)->data()->removeBefore(timestamps[i] - 1);

            }
        }
    }

    if (!timestamps.empty()) {
        // Update the range of all plots
        for (auto& plot : plots) {
            plot->xAxis->setRange(timestamps.back(), 1, Qt::AlignRight);

            if (samples_received % s_rate == 0) {
                double margin = (maxVal - minVal) * 0.1; // 10% margin
                if (maxVal - margin > 1000 && minVal + margin < -1000) {
                    plot->yAxis->setRange(minVal - margin, maxVal + margin);
                }
            }
            plot->replot();
        }
        ui->statusbar->showMessage(QString("Seconds passed: %1").arg(samples_received/s_rate), 0);
    }
}
