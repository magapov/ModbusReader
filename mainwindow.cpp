#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QModbusRtuSerialMaster>

#ifdef _WIN32
#include <winsock2.h>
#elif defined(__unix__)
#include <netinet/in.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->actionOpenSettings, SIGNAL(triggered()), this, SLOT(readSettings()));
    connect(ui->actionSaveSettings, SIGNAL(triggered()), this, SLOT(saveSettings()));
    connect(ui->openFilePB, SIGNAL(clicked()), this, SLOT(getOutputFileName()));
    connect(ui->startStopPB, SIGNAL(clicked()), this, SLOT(onStartStopButtonClicked()));
    connect(ui->scanRateSpinBox, qOverload<int>(&QSpinBox::valueChanged), [this](int val){
        mbTimer.setInterval(val);
    });
    connect(&mbTimer, SIGNAL(timeout()), this, SLOT(onMbTimerTimeout()));

    ui->functionComboBox->addItem("04 Read Input Registers (3x)", READ_INPUT_REGS);
    //Add port info to combo box
    for (const auto &port : QSerialPortInfo::availablePorts())
        ui->portComboBox->addItem(port.portName(), port.portName());
    //Add baudrates to combobox
    ui->baudrateComboBox->addItem("1200",   QSerialPort::Baud1200);
    ui->baudrateComboBox->addItem("2400",   QSerialPort::Baud2400);
    ui->baudrateComboBox->addItem("4800",   QSerialPort::Baud4800);
    ui->baudrateComboBox->addItem("9600",   QSerialPort::Baud9600);
    ui->baudrateComboBox->addItem("19200",  QSerialPort::Baud19200);
    ui->baudrateComboBox->addItem("38400",  QSerialPort::Baud38400);
    ui->baudrateComboBox->addItem("57600",  QSerialPort::Baud57600);
    ui->baudrateComboBox->addItem("115200", QSerialPort::Baud115200);
    ui->baudrateComboBox->setCurrentIndex(3);
    //Add data bits to combobox
    ui->dataBitsComboBox->addItem("5", QSerialPort::Data5);
    ui->dataBitsComboBox->addItem("6", QSerialPort::Data6);
    ui->dataBitsComboBox->addItem("7", QSerialPort::Data7);
    ui->dataBitsComboBox->addItem("8", QSerialPort::Data8);
    ui->dataBitsComboBox->setCurrentIndex(3);
    //Add parity to combobox
    ui->parityComboBox->addItem("No Parity", QSerialPort::NoParity);
    ui->parityComboBox->addItem("Even",      QSerialPort::EvenParity);
    ui->parityComboBox->addItem("Odd",       QSerialPort::OddParity);
    ui->parityComboBox->addItem("Space",     QSerialPort::SpaceParity);
    ui->parityComboBox->addItem("Mark",      QSerialPort::MarkParity);
    //Add stopbits to combobox
    ui->stopBitsComboBox->addItem("1",   QSerialPort::OneStop);
    ui->stopBitsComboBox->addItem("1.5", QSerialPort::OneAndHalfStop);
    ui->stopBitsComboBox->addItem("2",   QSerialPort::TwoStop);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::setupModbusDevice()
{
    if (modbusDevice) {
        modbusDevice->disconnectDevice();
        delete modbusDevice;
        modbusDevice = nullptr;
        return true;
    }

    modbusDevice = new QModbusRtuSerialMaster(this);
    if (!modbusDevice)
        return false;
    connect(modbusDevice, &QModbusClient::errorOccurred, [this](QModbusDevice::Error) {
        statusBar()->showMessage(modbusDevice->errorString(), 5000);
    });

    if (modbusDevice->state() != QModbusDevice::ConnectedState) {
        modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter,
                                             ui->portComboBox->currentData());
        modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter,
                                             ui->parityComboBox->currentData());
        modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter,
                                             ui->baudrateComboBox->currentData());
        modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter,
                                             ui->dataBitsComboBox->currentData());
        modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter,
                                             ui->stopBitsComboBox->currentData());
        modbusDevice->setTimeout(ui->timeoutSpinBox->value());
        if (!modbusDevice->connectDevice()) {
            statusBar()->showMessage(tr("Connect failed: ") + modbusDevice->errorString(), 5000);
            delete modbusDevice;
            modbusDevice = nullptr;
            return false;
        }
    }
    return true;
}

void MainWindow::readSettings()
{
    QString settingsFile = QFileDialog::getOpenFileName(this, "Open settings");
    if (settingsFile.isEmpty())
        return;

    settings = new QSettings(settingsFile, QSettings::IniFormat, this);
    {
        bool ok = false;
        auto slaveId = settings->value("modbus/slaveid").toInt(&ok);
        if (ok)
            ui->slaveIdSpinBox->setValue(slaveId);
    }
    {
        bool ok = false;
        auto address = settings->value("modbus/address").toInt(&ok);
        if (ok)
            ui->addressSpinBox->setValue(address);
    }
    {
        bool ok = false;
        auto quantity = settings->value("modbus/quantity").toInt(&ok);
        if (ok)
            ui->quantitySpinBox->setValue(quantity);
    }
    {
        bool ok = false;
        auto rate = settings->value("modbus/rate").toInt(&ok);
        if (ok)
            ui->scanRateSpinBox->setValue(rate);
    }
    {
        auto port = settings->value("serial/port").toString();
        auto ind = ui->portComboBox->findData(port);
        if (ind != -1)
            ui->portComboBox->setCurrentIndex(ind);
    }
    {
        bool ok = false;
        auto baudrate = settings->value("serial/baudrate").toInt(&ok);
        auto ind = ok ? ui->baudrateComboBox->findData(baudrate) : -1;
        if (ind != -1)
            ui->baudrateComboBox->setCurrentIndex(ind);
    }
    {
        bool ok = false;
        auto parity = settings->value("serial/parity").toInt(&ok);
        auto ind = ok ? ui->parityComboBox->findData(parity) : -1;
        if (ind != -1)
            ui->parityComboBox->setCurrentIndex(ind);
    }
    {
        bool ok = false;
        auto stopBits = settings->value("serial/stopbits").toInt(&ok);
        auto ind = ok ? ui->stopBitsComboBox->findData(stopBits) : -1;
        if (ind != -1)
            ui->stopBitsComboBox->setCurrentIndex(ind);
    }
    {
        bool ok = false;
        auto timeout = settings->value("serial/timeout").toInt(&ok);
        if (ok)
            ui->timeoutSpinBox->setValue(timeout);
    }
    delete settings;
    settings = nullptr;
}

void MainWindow::saveSettings()
{
    QString settingsFile = QFileDialog::getSaveFileName(this, "Save settings");
    settings = new QSettings(settingsFile, QSettings::IniFormat, this);
    settings->beginGroup("modbus");
    settings->setValue("slaveid", ui->slaveIdSpinBox->value());
    settings->setValue("address", ui->addressSpinBox->value());
    settings->setValue("quantity", ui->quantitySpinBox->value());
    settings->setValue("rate", ui->scanRateSpinBox->value());
    settings->endGroup();
    settings->beginGroup("serial");
    settings->setValue("port", ui->portComboBox->currentData().toString());
    settings->setValue("baudrate", ui->baudrateComboBox->currentData().toInt());
    settings->setValue("parity", ui->parityComboBox->currentData().toInt());
    settings->setValue("stopbits", ui->stopBitsComboBox->currentData().toInt());
    settings->setValue("timeout", ui->timeoutSpinBox->value());
    settings->endGroup();
    settings->sync();
    delete settings;
    settings = nullptr;
}

void MainWindow::getOutputFileName()
{
    QString csvFile = QFileDialog::getSaveFileName(this, "Save settings", "output.csv", "*.csv");
    ui->fileLineEdit->setText(csvFile);
}

void MainWindow::onStartStopButtonClicked()
{
    if (process) {
        ui->startStopPB->setText("Start");
        process = false;
        mbTimer.stop();
        outFile->close();
        delete outFile;
        outFile = nullptr;
        if (!setupModbusDevice()) {
            QMessageBox::critical(this, "Error", "Can't disconnect");
            this->close();
        }
        qDebug() << "Stopped";
    } else {
        if (ui->fileLineEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Warning", "Output file name is empty");
            return;
        }
        if (!setupModbusDevice()) {
            QMessageBox::critical(this, "Error", "Connection error");
            return;
        }
        outFile = new QFile(ui->fileLineEdit->text(), this);
        outFile->open(QIODevice::WriteOnly);
        //Write header
        outFile->write("Signal;Noise;Filtered\n");
        ui->startStopPB->setText("Stop");
        process = true;
        mbTimer.start(ui->scanRateSpinBox->value());
    }
}

void MainWindow::onMbTimerTimeout()
{
    if (!modbusDevice)
        return;
    const auto table = QModbusDataUnit::InputRegisters;
    const auto startAddress = ui->addressSpinBox->value();
    Q_ASSERT(startAddress >= 0 && startAddress < 10);

    // do not go beyond 10 entries
    int numberOfEntries = qMin(ui->quantitySpinBox->value() + 1, 10 - startAddress);
    auto unit = QModbusDataUnit(table, startAddress, numberOfEntries);

    if (auto reply = modbusDevice->sendReadRequest(unit, ui->slaveIdSpinBox->value())) {
        if (!reply->isFinished())
            connect(reply, &QModbusReply::finished, this, &MainWindow::onReadyRead);
        else
            delete reply;
    } else {
        statusBar()->showMessage(tr("Read error: ") + modbusDevice->errorString(), 5000);
    }
}

void MainWindow::onReadyRead()
{
    auto reply = qobject_cast<QModbusReply *>(sender());
    if (!reply) {
        QMessageBox::critical(this, "Error", "Bad reply");
        this->close();
    }

    if (reply->error() == QModbusDevice::NoError) {
        const auto unit = reply->result();
        int16_t signal = unit.value(SIGNAL_ADDR - unit.startAddress());
        float noise;
        uint32_t noiseI = htons(unit.value(NOISE_ADDR - unit.startAddress()));
        noiseI += htons(unit.value(NOISE_ADDR - unit.startAddress() + 1)) << 16;
        memcpy(&noise, &noiseI, sizeof(uint32_t));
        auto filtered = filter.process(noise + signal);
        outFile->write(tr("%1;%2;%3\n").arg(signal).arg(noise).arg(filtered).toUtf8());
    }
    reply->deleteLater();
}

