#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QDebug>

MainWindow *MainWindow::self = nullptr;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_waylandAdmin = new WaylandAdmin(this);
    m_dateAdmin = new DateAdmin(this);
    QRect screenRect = QApplication::desktop()->screen()->geometry();
    QStringList list;
    list << QString("%1").arg(RecordAdmin::videoType::MP4)
         << QString("%1").arg(screenRect.width())
         << QString("%1").arg(screenRect.height())
         << QString("0")
         << QString("0")
         << QString("24")
         << QString("./")
         << QString("%1").arg(RecordAdmin::audioType::SYS);
    qDebug() << list;
    m_recordAdmin = new RecordAdmin(list,this);
}

void MainWindow::init()
{
    MainWindow::self = this;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_start_clicked()
{
    if (!m_waylandAdmin->bInit()) {
        m_waylandAdmin->init();
    }
    m_dateAdmin->setBGetFrame(true);
}

void MainWindow::on_pushButton_stop_clicked()
{
    m_waylandAdmin->stop();
    m_recordAdmin->stop();
    QThread::msleep(500);
    QApplication::quit();
}

MainWindow *MainWindow::instance()
{
    return self;
}
