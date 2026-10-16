#include "../include/server_node/ctimermessagebox.h"

#define TOTAL_TIME 8.0
#define INTERVAL 1

namespace server_node {

cTimerMessageBox::cTimerMessageBox(QWidget* parent)
  : QWidget(parent)
{
  msgbox = new QMessageBox(this);
  msgbox->setGeometry(QRect(800, 800, 400, 300));
  okbtn     = new QPushButton("确定");
  cancelbtn = new QPushButton("取消");
  msgbox->addButton(cancelbtn, QMessageBox::RejectRole);
  msgbox->addButton(okbtn, QMessageBox::AcceptRole);
  msgbox->setText(tr("自动开始下一套考题？"));
  msgbox->setStyleSheet("QPushButton {min-width: 5em; min-height: 2em;}");

  timer_countdown = new QTimer(this);
  connect(timer_countdown, SIGNAL(timeout()), this, SLOT(count_time()));
  this->playNext = true;
}

cTimerMessageBox::~cTimerMessageBox()
{
}

void cTimerMessageBox::count_time()
{
  double letf_time = TOTAL_TIME - msgbox_time_;
  if( letf_time <= 0 )
  {
    okbtn->click();
    return;
  }
  else
  {
    QString output;
    output.sprintf("%2.1f秒后 开始下一套考题 ", letf_time);
    msgbox->setText(output);
  }
  msgbox_time_ += INTERVAL;
}

void cTimerMessageBox::start()
{
  timer_countdown->start(INTERVAL*1000);
  msgbox_time_ = 0.0;
  msgbox->exec();
  if (msgbox->clickedButton() == okbtn)
  {
    qDebug()<<"确定播放下一套考题";
    this->playNext = true;
    timer_countdown->stop();
  }
  else
  {
    qDebug()<<"取消播放下一套考题";
    this->playNext = false;
    timer_countdown->stop();
  }
}

}
