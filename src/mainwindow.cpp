
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QUrl>
#include <QTimer>
#include <QProcess>
#include <QMessageBox>
#include <QDesktopWidget>
#include <QDesktopServices>

#include <QFileDialog>

#include "AppConfig.h"

///mingwʹ��QStringLiteral ��������
/// û��_MSC_VER����� ���Ǿ���Ϊ���õ���mingw������

#ifndef _MSC_VER
#define MINGW
#endif

#if defined(WIN32) && !defined(MINGW)

#else
    #define QStringLiteral QString
#endif


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    av_register_all();
    avformat_network_init();
    avdevice_register_all();

    setWindowFlags(Qt::WindowStaysOnTopHint|Qt::FramelessWindowHint);  //ʹ���ڵı���������
    setAttribute(Qt::WA_TranslucentBackground, true);

    mSaveFileDir = AppConfig::AppFilePath_Video;
    ui->lineEdit_filepath->setText(mSaveFileDir);

    connect(ui->startButton,SIGNAL(clicked()),this,SLOT(slotBtnClicked()));
    connect(ui->pauseButton,SIGNAL(clicked()),this,SLOT(slotBtnClicked()));
    connect(ui->stopButton,SIGNAL(clicked()),this,SLOT(slotBtnClicked()));
    connect(ui->pushButton_playBack,SIGNAL(clicked()),this,SLOT(slotBtnClicked()));

    connect(ui->selectRectButton,SIGNAL(clicked()),this,SLOT(slotSelectRectBtnClick()));
    connect(ui->editRectButton,SIGNAL(clicked()),this,SLOT(slotEditRectBtnClick()));
    connect(ui->hideRectButton,SIGNAL(clicked()),this,SLOT(slotHideRectBtnClick()));

    m_screenRecorder = NULL;
    isLeftBtnPressed = false;
    m_recordeState = Stop;

    rect = QRect(0,0,0,0);

    selectRectWidget = new SelectRect(NULL,SelectRect::RecordGif);
    QDesktopWidget* desktopWidget = QApplication::desktop();
    deskRect = desktopWidget->screenGeometry();//��ȡ���������С
    m_rate = deskRect.height() * 1.0 / deskRect.width();
    selectRectWidget->setRate(m_rate);

    connect(selectRectWidget,SIGNAL(finished(QRect)),this,SLOT(slotSelectRectFinished(QRect)));
    connect(selectRectWidget,SIGNAL(rectChanged(QRect)),this,SLOT(slotSelectRectFinished(QRect)));

    initDev();
    loadConfigFile();

    connect(ui->toolButton_video,SIGNAL(clicked(bool)),this,SLOT(slotToolBtnToggled(bool)));
    connect(ui->toolButton_audio,SIGNAL(clicked(bool)),this,SLOT(slotToolBtnToggled(bool)));
    connect(ui->toolButton_file,SIGNAL(clicked(bool)),this,SLOT(slotToolBtnToggled(bool)));

    ///������ ����ʵ�ִ�����Ϸ���������
    animation = new QPropertyAnimation(this, "geometry");
    animation->setDuration(1000);

    ui->toolButton_setting->setChecked(false);
    ui->widget_extern->hide();
    QTimer::singleShot(50,this,SLOT(showOut()));

    m_timer = new QTimer;
    connect(m_timer,SIGNAL(timeout()),this,SLOT(slotTimerTimeOut()));
    m_timer->setInterval(500);

    connect(ui->checkBox,SIGNAL(clicked(bool)),this,SLOT(slotCheckBoxClick(bool)));

    if (ui->toolButton_video->isChecked())
    {
        selectRectWidget->show();
        selectRectWidget->setPointHide();
        ui->hideRectButton->setText(QStringLiteral("����"));
    }
}

MainWindow::~MainWindow()
{
    delete ui;
    delete selectRectWidget;
}

void MainWindow::showOut()
{
    show();
    move(deskRect.width() / 2 - width() / 2,0-height());
    animation->setStartValue(QRect(deskRect.width() / 2 - width() / 2,0-height(),width(),height()));
    animation->setEndValue(QRect(deskRect.width() / 2 - width() / 2,0,width(),height()));
    animation->start();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_screenRecorder)
    {
        m_screenRecorder->stopRecord();
    }

    selectRectWidget->close();
}

void MainWindow::mousePressEvent(QMouseEvent * event)
{
    if (event->button() == Qt::LeftButton)
    {
        isLeftBtnPressed = true;
         dragPosition=event->globalPos()-frameGeometry().topLeft();
         event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent * event)
{  //ʵ������ƶ�����
    if (event->buttons() & Qt::LeftButton)
    {
        if (isLeftBtnPressed)
        {
            move(event->globalPos() - dragPosition);
            event->accept();
        }
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent * event)
{
    isLeftBtnPressed = false;
    event->accept();
}

void MainWindow::loadConfigFile()
{

    QFile file(AppConfig::AppFilePath_EtcFile);
    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream fileOut(&file);
        fileOut.setCodec("GBK");  //unicode UTF-8  ANSI
        while (!fileOut.atEnd())
        {
            QString str = fileOut.readLine();
//            qDebug()<<str;

            str = str.remove(" ");

            if (str.isEmpty())
            {
                continue;
            }

            if (str.contains("area="))
            {
                str = str.remove("area=");

                QStringList strList = str.split(",");

                if (strList.size() == 4)
                {
                    rect = QRect(strList.at(0).toInt(),strList.at(1).toInt(),strList.at(2).toInt(),strList.at(3).toInt());
                }
            }
            else if (str.contains("mode="))
            {
                str = str.remove("mode=");
                if (str == "video")
                {
                    ui->toolButton_video->setChecked(true);
                }
                else
                {
                    ui->toolButton_audio->setChecked(true);
                    ui->startButton->setEnabled(true);
                }
            }
            else if (str.contains("framerate="))
            {
                str = str.remove("framerate=");

                int rate = str.toInt();

                if (rate > 0 && rate <= 25)
                {
                    ui->comboBox_framerate->setCurrentIndex(rate-1);
                }
            }
            else if (str.contains("savedir="))
            {
                str = str.remove("savedir=");
                str.replace("/","\\\\");
                mSaveFileDir = str;

                ui->lineEdit_filepath->setText(mSaveFileDir);

                AppConfig::MakeDir(mSaveFileDir);

            }
        }
        file.close();
    }
    else
    {
        ui->toolButton_video->setChecked(true);
    }

    if (rect.width() <= 0 || rect.height() <= 0 || !deskRect.contains(rect))
    {
        rect = deskRect;
    }

    selectRectWidget->setRect(rect);
    ui->startButton->setEnabled(true);
    selectRectWidget->setVisible(false);
    ui->hideRectButton->setText(QStringLiteral("��ʾ"));
    ui->startButton->setEnabled(true);
}

void MainWindow::saveConfigFile()
{
    QFile file(AppConfig::AppFilePath_EtcFile);
    if (file.open(QIODevice::WriteOnly))
    {
        QTextStream fileOut(&file);
        fileOut.setCodec("GBK");  //unicode UTF-8  ANSI

        if (!ui->comboBox_audio->currentText().isEmpty())
        {
            fileOut<<QString("audio=%1").arg(ui->comboBox_audio->currentText());
            fileOut<<"\n";
        }

        QString areaStr = QString("area=%1,%2,%3,%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
        QString modeStr = "mode=audio";
        if (ui->toolButton_video->isChecked())
        {
            modeStr = "mode=video";
        }

        QString fileStr = QString("savedir=%1").arg(mSaveFileDir);
        QString rateStr = QString("framerate=%1").arg(ui->comboBox_framerate->currentText().toInt());

        fileOut<<areaStr;
        fileOut<<"\n";
        fileOut<<modeStr;
        fileOut<<"\n";
        fileOut<<rateStr;
        fileOut<<"\n";
        fileOut<<fileStr;
        fileOut<<"\n";

        file.close();
    }
}

void MainWindow::initDev()
{
    QString videoDevName;
    QString audioDevName;
    QFile devFile(AppConfig::AppFilePath_EtcFile);
    if (devFile.open(QIODevice::ReadOnly))
    {
        QTextStream fileOut(&devFile);
        fileOut.setCodec("GBK");  //unicode UTF-8  ANSI

        while (!fileOut.atEnd())
        {
            QString str = fileOut.readLine();
            str = str.remove("\r");
            str = str.remove("\n");
            if (str.contains("video="))
            {
                videoDevName = str.remove("video=");
            }
            else if (str.contains("audio="))
            {
                audioDevName = str.remove("audio=");
            }
        }

        devFile.close();
    }

    /// ִ��ffmpeg������ ��ȡ����Ƶ�豸
    /// �뽫ffmpeg.exe�ͳ���ŵ�ͬһ��Ŀ¼��

    QString ffmpegPath = QCoreApplication::applicationDirPath() + "/ffmpeg.exe";
    ffmpegPath.replace("/","\\\\");
    ffmpegPath = QString("\"%1\" -list_devices true -f dshow -i dummy 2>a.txt \n").arg(ffmpegPath);

     QProcess p(0);
     p.start("cmd");
     p.waitForStarted();
     p.write(ffmpegPath.toLocal8Bit());
     p.closeWriteChannel();
     p.waitForFinished();


    QFile file("a.txt");
    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream fileOut(&file);
        fileOut.setCodec("UTF-8");  //unicode UTF-8  ANSI

        bool isVideoBegin = false;
        bool isAudioBegin = false;

        while (!fileOut.atEnd())
        {
            QString str = fileOut.readLine();

            if (str.contains("DirectShow video devices") && str.contains("[dshow @"))
            {
                isVideoBegin = true;
                isAudioBegin = false;
                continue;
            }

            if (str.contains("DirectShow audio devices") && str.contains("[dshow @"))
            {
                isAudioBegin = true;
                isVideoBegin = false;
                continue;
            }

            if (str.contains("[dshow @") && (!str.contains("Alternative name")) )
            {
                int index = str.indexOf("\"");
                str = str.remove(0,index);
                str = str.remove("\"");
                str = str.remove("\n");
                str = str.remove("\r");

                if (isVideoBegin)
                {
//                    if ("screen-capture-recorder" != str)
//                    ui->comboBox_camera->addItem(str);
                }
                else if (isAudioBegin)
                {
                    if ("virtual-audio-capturer" != str)
                        ui->comboBox_audio->addItem(str);
                }
            }

        }

        file.close();
    }
    else
    {
        qDebug()<<"open a.txt failed!";
    }

    QFile::remove("a.txt");

    for (int i=0;i<ui->comboBox_audio->count();i++)
    {
        if (ui->comboBox_audio->itemText(i) == audioDevName)
        {
            ui->comboBox_audio->setCurrentIndex(i);
            break;
        }
    }

}

void MainWindow::slotBtnClicked()
{
    if (QObject::sender() == ui->startButton)
    {
        startRecord();
    }
    else if (QObject::sender() == ui->pauseButton)
    {
        pauseRecord();
    }
    else if (QObject::sender() == ui->stopButton)
    {
        stopRecord();
    }
    else if (QObject::sender() == ui->pushButton_playBack)
    {
        QString path = mCurrentFilePath;
        path.replace("\\\\","/");
        path="file:///" + path;
        qDebug()<<QUrl(path)<<path;
        QDesktopServices::openUrl(QUrl(path));
    }
}

void MainWindow::slotToolBtnToggled(bool isChecked)
{
    if (QObject::sender() == ui->toolButton_video)
    {
        if (isChecked)
        {
            ui->toolButton_audio->setChecked(!isChecked);
            selectRectWidget->show();
        }
        else
        {
            ui->toolButton_video->setChecked(!isChecked);
        }
    }
    else if (QObject::sender() == ui->toolButton_audio)
    {
        if (isChecked)
        {
            ui->toolButton_video->setChecked(!isChecked);
            selectRectWidget->hide();
        }
        else
        {
            ui->toolButton_audio->setChecked(!isChecked);
        }
    }
    else if (QObject::sender() == ui->toolButton_file)
    {
        QString s = QFileDialog::getExistingDirectory(
                     NULL, "ѡ�񱣴��ļ���·��",
                     mSaveFileDir);

         if (!s.isEmpty())
         {
//             mSaveFileDir = s.replace("/","\\\\");
             ui->lineEdit_filepath->setText(mSaveFileDir);
         }
    }
}

void MainWindow::slotSelectRectFinished(QRect re)
{
    /// 1.����ffmpeg�����ͼ���߱�����ż����
    /// 2.ͼ��ü�����ʼλ�úͽ���λ��Ҳ������ż��
    /// ���ֶ�ѡ���������п��ܻ��������������Ҫ����һ�� ����Ū��ż��
    /// ����ķ����ܼ����ʵ������ǰ����������һ������
    /// һ�����صĴ�С���ۻ���Ҳ��������ɶ����

    int x = re.x();
    int y = re.y();
    int w = re.width();
    int h = re.height();

    if (x % 2 != 0)
    {
        x--;
        w++;
    }

    if (y % 2 != 0)
    {
        y--;
        h++;
    }

    if (w % 2 != 0)
    {
        w++;
    }

    if (h % 2 != 0)
    {
        h++;
    }

    rect = QRect(x,y,w,h);

    QString str = QStringLiteral("==��ǰ����==\n\n���(%1,%2)\n\n��С(%3 x %4)")
            .arg(rect.left()).arg(rect.left()).arg(rect.width()).arg(rect.height());

    ui->showRectInfoLabel->setText(str);

    ui->startButton->setEnabled(true);
    ui->editRectButton->setEnabled(true);
    ui->hideRectButton->setEnabled(true);
    ui->hideRectButton->setText(QStringLiteral("����"));

    saveConfigFile();

}

bool MainWindow::startRecord()
{
    int ret = 0;
    QString msg = "ok";
    if (m_recordeState == Recording)
    {
        ret = -1;
        msg = "is already start!";
    }
    else if (m_recordeState == Pause)
    {
        ui->startButton->setEnabled(false);
        ui->pauseButton->setEnabled(true);
        ui->stopButton->setEnabled(true);

        m_screenRecorder->restoreRecord();

        m_recordeState = Recording;
        m_timer->start();
    }
    else if (m_recordeState == Stop)
    {

        if (rect.width() <= 0 || rect.height() <= 0 || !deskRect.contains(rect))
        {
            rect = deskRect;
        }

        ///���������ļ�
        saveConfigFile();

        QString audioDevName = ui->comboBox_audio->currentText();

        if (audioDevName.isEmpty())
        {
            QMessageBox::critical(this, QStringLiteral("��ʾ"), QStringLiteral("������,��Ƶ����Ƶ�豸δ�����������޷����У�"));

            ret = -3;
            msg = "audio device not set";
            goto end;
        }

        if (m_screenRecorder)
            delete m_screenRecorder;

        QDateTime dateTime = QDateTime::currentDateTime();
        QString fileName = QString("video_%1.mp4").arg(dateTime.toString("yyyy-MM-dd hhmmss"));
        mCurrentFilePath = QString("%1/%2").arg(mSaveFileDir).arg(fileName);


        m_screenRecorder = new ScreenRecorder;
        m_screenRecorder->setFileName(mCurrentFilePath);
        m_screenRecorder->setVideoFrameRate(ui->comboBox_framerate->currentText().toInt());

        if (ui->toolButton_video->isChecked())
        {
            if (m_screenRecorder->init("screen-capture-recorder",true,audioDevName,true) == SUCCEED)
            {
                m_screenRecorder->setPicRange(rect.x(),rect.y(),rect.width(),rect.height());
                m_screenRecorder->startRecord();
            }
            else
            {
                QMessageBox::critical(this, QStringLiteral("��ʾ"), QStringLiteral("������,��ʼ��¼���豸ʧ�ܣ�"));

                ret = -4;
                msg = "init screen device failed!";
                goto end;

            }
        }
        else
        {
            if (m_screenRecorder->init("",false,audioDevName,true) == SUCCEED)
            {
//                    qDebug()<<rect;
                m_screenRecorder->setPicRange(rect.x(),rect.y(),rect.width(),rect.height());
                m_screenRecorder->startRecord();
            }
            else
            {
                QMessageBox::critical(this, QStringLiteral("��ʾ"), QStringLiteral("������,��ʼ����Ƶ�豸ʧ�ܣ�"));
                ret = -5;
                msg = "init audio device failed!";
                goto end;
            }
        }

        ui->startButton->setEnabled(false);
        ui->pauseButton->setEnabled(true);
        ui->stopButton->setEnabled(true);

        ui->selectRectButton->setEnabled(false);
        ui->editRectButton->setEnabled(false);
        //ui->hideRectButton->setEnabled(false);

        ui->comboBox_audio->setEnabled(false);
    //    ui->comboBox_recordeMode->setEnabled(false);

        ui->toolButton_video->setEnabled(false);
        ui->toolButton_audio->setEnabled(false);

        m_recordeState = Recording;
        m_timer->start();


        ui->pushButton_playBack->setEnabled(false);
    }

end:

//    m_erroMsg = QString("\"ret\":%1,\"msg\":\"%2\"").arg(ret).arg(msg);
    return ret == 0;
}

bool MainWindow::pauseRecord()
{
    int ret = 0;
    QString msg = "ok";
    if (m_recordeState == Recording)
    {
        m_timer->stop();

        ui->startButton->setEnabled(true);
        ui->pauseButton->setEnabled(false);

        ui->selectRectButton->setEnabled(false);
        ui->editRectButton->setEnabled(false);
        //ui->hideRectButton->setEnabled(false);

        m_screenRecorder->pauseRecord();

        m_recordeState = Pause;
    }
    else
    {
        ret = -6;
        msg = "is not started!";
    }

//    m_erroMsg = QString("\"ret\":%1,\"msg\":\"%2\"").arg(ret).arg(msg);
    return ret == 0;
}

bool MainWindow::stopRecord()
{
    int ret = 0;
    QString msg = "ok";
    if (m_recordeState != Stop)
    {
        m_timer->stop();
        m_recordeState = Stop;
        ui->label_time->setText("00:00:00");
        m_screenRecorder->stopRecord();

        ui->startButton->setEnabled(true);
        ui->pauseButton->setEnabled(false);
        ui->stopButton->setEnabled(false);

        ui->selectRectButton->setEnabled(true);
        ui->editRectButton->setEnabled(true);

        ui->comboBox_audio->setEnabled(true);
        ui->toolButton_video->setEnabled(true);
        ui->toolButton_audio->setEnabled(true);

        ui->pushButton_playBack->setEnabled(true);

    }
    else
    {
        ret = -7;
        msg = "not started!";
    }

//    m_erroMsg = QString("\"ret\":%1,\"msg\":\"%2\"").arg(ret).arg(msg);

    return ret == 0;
}

void MainWindow::slotSelectRectBtnClick()
{
    selectRectWidget->getReadyToSelect();
}

void MainWindow::slotEditRectBtnClick()
{
    selectRectWidget->showFullScreen();
    selectRectWidget->editRect();
}

void MainWindow::slotHideRectBtnClick()
{
    if (selectRectWidget->isVisible())
    {
       selectRectWidget->setVisible(false);
       ui->hideRectButton->setText(QStringLiteral("��ʾ"));
    }
    else
    {
       selectRectWidget->setVisible(true);
       ui->hideRectButton->setText(QStringLiteral("����"));
    }
}

void MainWindow::slotTimerTimeOut()
{
    qint64 audioPts = 0;
    if (m_screenRecorder)
    {
        audioPts = m_screenRecorder->getAudioPts();

        audioPts /= 1000; //�������
    }

    QString hStr = QString("00%1").arg(audioPts/3600);
    QString mStr = QString("00%1").arg(audioPts%3600/60);
    QString sStr = QString("00%1").arg(audioPts%60);

    QString str = QString("%1:%2:%3").arg(hStr.right(2)).arg(mStr.right(2)).arg(sStr.right(2));
    ui->label_time->setText(str);

}

void MainWindow::slotCheckBoxClick(bool checked)
{
    if (checked)
    {
        selectRectWidget->setRate(m_rate);
    }
    else
    {
        selectRectWidget->setRate(-1);
    }
}
