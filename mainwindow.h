#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "waylandadmin.h"
#include "dateadmin.h"
#include "recordadmin.h"

#include <QMainWindow>

#define qContext (static_cast<MainWindow *>(MainWindow::instance()))

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void init();
    static MainWindow *instance();
    ~MainWindow();

private slots:
    void on_pushButton_start_clicked();
    void on_pushButton_stop_clicked();

public:
    WaylandAdmin *m_waylandAdmin;
    DateAdmin *m_dateAdmin;
    RecordAdmin *m_recordAdmin;

private:
    Ui::MainWindow *ui;
    static MainWindow *self;
};

#endif // MAINWINDOW_H
