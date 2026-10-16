/**
 * @file /src/main_window.cpp
 *
 * @brief Implementation for the qt gui.
 *
 * @date February 2011
 **/
/*****************************************************************************
** Includes
*****************************************************************************/
#ifndef Q_MOC_RUN
#include <ros/ros.h>
#endif
#include <QtGui>
#include <QMessageBox>
#include <iostream>
#include "../include/server_node/main_window.hpp"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

using namespace Qt;

/*****************************************************************************
** Implementation [MainWindow]
*****************************************************************************/

MainWindow::MainWindow(int argc, char** argv, QWidget *parent)
  : QMainWindow(parent),
    init_argc(argc),
    init_argv(argv),
//    qnode(argc,argv),
    ui(new Ui::MainWindowDesign)
{
    ui->setupUi(this);
    // qApp is a global variable for the application
    setWindowIcon(QIcon(":/images/icon.png"));
    QObject::connect(ui->actionAbout_Qt, SIGNAL(triggered(bool)), qApp, SLOT(aboutQt()));
    ReadSettings();
    // ensure the first tab is showing - qt-designer should have this already hardwired, but often loses it (settings?).
    ui->tabWidget->setCurrentIndex(0);


    /*********************
    ** Auto Start: initialize the connection to ros master
    **********************/

    // 可视化模块
    if (!ros::isInitialized()) {
        ros::init(argc, argv, "server_node", ros::init_options::AnonymousName);
        if (!ros::master::check()) {
            qDebug() << "No ROS Master!";
            showNoMasterMessage();
        }
    }
    rviz = new QProcess();
    rviz->setProcessChannelMode(QProcess::MergedChannels);
    rviz->start(QString("rviz"));
    connect(rviz, SIGNAL(finished(int)), rviz, SLOT(deleteLater()));
    rviz->waitForStarted();

}

MainWindow::~MainWindow() {
    delete ui;
}

/*****************************************************************************
** Implementation [Slots]
*****************************************************************************/

void MainWindow::showNoMasterMessage() {
	QMessageBox msgBox;
  msgBox.setText("Couldn't find the ROS master.");
  msgBox.exec();
}

/*****************************************************************************
** Implemenation [Slots][manually connected]
*****************************************************************************/

void MainWindow::slotSaveLog()
{
    QString fileName = QFileDialog::getSaveFileName(this, "保存日志",
                                                    QDir::cleanPath(QDir::currentPath()+QDir::separator()+"server.log"));

    if (fileName.isEmpty()) {
        QMessageBox::warning(this, "警告", "不合法的文件路径");
    }
    QFile fileSave(fileName);
    if (fileSave.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString logStr = ui->textBrowser->toPlainText();
        QTextStream stream(&fileSave);
        stream << logStr;
        fileSave.close();
        QMessageBox::information(this, "保存成功", "保存成功");
    }
    else {
        QMessageBox::warning(this, "警告", "打开文件失败");
    }
}
/*****************************************************************************
** Implementation [Menu]
*****************************************************************************/

void MainWindow::on_actionAbout_triggered() {
    QMessageBox::about(this, tr("About"),
                       tr("<h2>Server Visualizer for Off-Road Challenge</h2><p>Multi-Sensor Autonomous Judging Suite</p>"));
}

/*****************************************************************************
** Implementation [Configuration]
*****************************************************************************/

void MainWindow::ReadSettings() {
    QSettings settings("Qt-Ros Package", "server_node");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::WriteSettings() {
    QSettings settings("Qt-Ros Package", "server_node");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  WriteSettings();
  // 停止rviz
  QProcess* kill_process = new QProcess;
  QString str = "kill -SIGINT " + QString::number(this->rviz->processId());
  qDebug() << str;
  kill_process->start(str);
  connect(kill_process, SIGNAL(finished(int)), kill_process, SLOT(deleteLater()));
	QMainWindow::closeEvent(event);
}

}  // namespace server_node

