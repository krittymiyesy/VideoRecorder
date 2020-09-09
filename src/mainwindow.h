
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QUdpSocket>
#include <QHostInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QDateTime>
#include <QNetworkInterface>
#include <QProcess>

#include <QMainWindow>
#include <QPropertyAnimation>

#include "widget/selectrect.h"
#include "video/screenrecorder.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    enum RecoderState
    {
        Recording,
        Pause,
        Stop
    };

    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void showOut();

protected:
    void closeEvent(QCloseEvent *);

protected:
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);

private:
    Ui::MainWindow *ui;

    bool isLeftBtnPressed;
    QPoint dragPosition;

    QPropertyAnimation *animation; //������ ����ʵ�ִ�����Ϸ���������

    QString mSaveFileDir; //��Ƶ����Ŀ¼
    QString mCurrentFilePath; //��ǰ¼�Ƶ���Ƶ�ļ����·��

    ScreenRecorder *m_screenRecorder;
    QRect deskRect; //���������С
    SelectRect *selectRectWidget;  //ѡ������Ŀؼ�
    QRect rect; //��ǰ¼�Ƶ�����
    float m_rate; //��Ļ��߱�

    QTimer * m_timer; //��ʱ�� ���ڻ�ȡʱ��

    RecoderState m_recordeState;

    void initDev(); //��ȡ¼���豸���б�

    void loadConfigFile(); //���������ļ�
    void saveConfigFile(); //д�������ļ�

    bool startRecord();
    bool pauseRecord();
    bool stopRecord();

private slots:
    void slotToolBtnToggled(bool);
    void slotBtnClicked();

    ///ѡ��¼��������� - Begin
    void slotSelectRectBtnClick();
    void slotEditRectBtnClick();
    void slotHideRectBtnClick();
    void slotSelectRectFinished(QRect);
    ///ѡ��¼��������� - End

    void slotTimerTimeOut();

    void slotCheckBoxClick(bool checked);
};

#endif // MAINWINDOW_H
