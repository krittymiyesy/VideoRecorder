
#ifndef SCREENRECORDER_H
#define SCREENRECORDER_H

#include <QThread>

#include "getvideothread.h"
#include "savevideofile.h"

class ScreenRecorder : public QObject
{
    Q_OBJECT

public:
    explicit ScreenRecorder();
    ~ScreenRecorder();

    void setFileName(QString filePath);

    ErroCode init(QString videoDevName,bool useVideo,QString audioDevName,bool useAudio);

    void startRecord();
    void pauseRecord();
    void restoreRecord();
    void stopRecord();

    void setPicRange(int x,int y,int w,int h); //����¼����Ļ������
    void setVideoFrameRate(int value);   //����¼������Ƶ֡��

    double getVideoPts();   //��ȡ��Ƶʱ���
    double getAudioPts();   //��ȡ��Ƶʱ���

private:
    SaveVideoFileThread * m_saveVideoFileThread; //�������Ƶ�ļ����߳�

    // ����Ƶ����Ƶ�ŵ�һ���ȡ
    // avformat_free_context�ͷŵ�ʱ��ᱼ��
    // ���Σ�ֻ�ܰ����Ƿŵ�2���߳���ִ��
    GetVideoThread *m_videoThread; //��ȡ��Ƶ���߳�
    GetVideoThread *m_audioThread; //��ȡ��Ƶ���߳�

    bool m_useVideo;
    bool m_useAudio;

};

#endif // SCREENRECORDER_H
