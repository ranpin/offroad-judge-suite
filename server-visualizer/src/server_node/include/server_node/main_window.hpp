/**
 * @file /include/server_node/main_window.hpp
 *
 * @brief Qt based gui for server_node.
 *
 * @date November 2010
 **/
#ifndef server_node_MAIN_WINDOW_H
#define server_node_MAIN_WINDOW_H

/*****************************************************************************
** Includes
*****************************************************************************/

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include "ui_main_window.h"

/*****************************************************************************
** Namespace
*****************************************************************************/

namespace server_node {

/*****************************************************************************
** Interface [MainWindow]
*****************************************************************************/
/**
 * @brief Qt central, all operations relating to the view part here.
 */
class MainWindow : public QMainWindow {
Q_OBJECT

public:
	MainWindow(int argc, char** argv, QWidget *parent = 0);
	~MainWindow();

	void ReadSettings(); // Load up qt program settings at startup
	void WriteSettings(); // Save qt program settings when closing

	void closeEvent(QCloseEvent *event); // Overloaded function
	void showNoMasterMessage();

public Q_SLOTS:
	/******************************************
	** Auto-connections (connectSlotsByName())
	*******************************************/
  void on_actionAbout_triggered();

private slots:
    void slotSaveLog();

public:
    Ui::MainWindowDesign *ui;
    int init_argc;
    char** init_argv;
    QProcess *rviz;
};

}  // namespace server_node

#endif // server_node_MAIN_WINDOW_H
