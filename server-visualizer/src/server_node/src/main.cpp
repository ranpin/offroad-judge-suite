/**
 * @file /src/main.cpp
 *
 * @brief Qt based gui.
 *
 * @date November 2010
 **/
/*****************************************************************************
** Includes
*****************************************************************************/

#include <QtGui>
#include <QtWidgets/QApplication>
#include "../include/server_node/cserver.h"
#include "../include/server_node/cscoreserver.h"
#include "../include/server_node/main_window.hpp"

/*****************************************************************************
** Main
*****************************************************************************/

int main(int argc, char **argv) {
    qRegisterMetaType<QVector<QString>>("QVector<int>");
    /*********************
    ** Qt
    **********************/
    QApplication app(argc, argv);
    server_node::MainWindow w(argc,argv);
    w.setWindowTitle("Off-Road Challenge - Server Visualizer");
    w.setAttribute(Qt::WA_QuitOnClose, true);

    w.show();
    // send "prepare competition" and receive "prepared" command
    // then send data tokens
    CServer cmdServer(argc, argv, w.ui);
    cmdServer.startServer();

    // Receive scores of players
    CScoreServer scoreServer(w.ui);
    scoreServer.startServer();

    int result = app.exec();

	return result;
}
