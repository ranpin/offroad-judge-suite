#ifndef CTIMERMESSAGEBOX_H
#define CTIMERMESSAGEBOX_H

#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>
#include <iostream>


/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace server_node {

class cTimerMessageBox : public QWidget
{
    Q_OBJECT

public:
    cTimerMessageBox(QWidget* parent = 0);
    ~cTimerMessageBox();
    void start();
    bool playNext;
private:
  QTimer*      timer_countdown;
  QMessageBox* msgbox;
  QPushButton* okbtn;
  QPushButton* cancelbtn;
  double       msgbox_time_;

public slots:
  void count_time();
};

}


#endif // CTIMERMESSAGEBOX_H
