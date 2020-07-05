#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QFile>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusClient>

#include "filter.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

const uint8_t READ_INPUT_REGS =     0x04;
const uint8_t SIGNAL_ADDR     =     0x05;
const uint8_t NOISE_ADDR      =     0x06;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    QSettings     *settings     = nullptr;
    QTimer         mbTimer;
    bool           process      = false;
    QFile         *outFile      = nullptr;
    QModbusReply  *lastRequest  = nullptr;
    QModbusClient *modbusDevice = nullptr;

    Filter filter;

    bool setupModbusDevice();

private slots:
    void readSettings();
    void saveSettings();
    void getOutputFileName();
    void onStartStopButtonClicked();
    void onMbTimerTimeout();
    void onReadyRead();
};
#endif // MAINWINDOW_H
