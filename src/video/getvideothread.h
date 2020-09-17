
#ifndef GetVideoThread_H
#define GetVideoThread_H

#include <QThread>

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libavdevice/avdevice.h"
}

#include "savevideofile.h"

enum ErroCode
{
    AudioOpenFailed = 0,
    VideoOpenFailed,
    AudioDecoderOpenFailed,
    VideoDecoderOpenFailed,
    SUCCEED
};

class GetVideoThread : public QThread
{
    Q_OBJECT

public:
    explicit GetVideoThread();
    ~GetVideoThread();

    ErroCode init(QString videoDevName,bool useVideo,QString audioDevName,bool useAudio);
    void deInit();

    void startRecord();
    void pauseRecord();
    void restoreRecord();
    void stopRecord();

    void setPicRange(int x,int y,int w,int h);

    void setSaveVideoFileThread(SaveVideoFileThread * p);

protected:
    void run();

private:

    AVFormatContext	*pFormatCtx;        //AVFormatContext��һ���ᴩʼ�յĽṹ�壬�ܶຯ����Ҫ�õ�����Ϊ����������FFMPEG���װ��flv��mp4��rmvb��avi�����ܵĽṹ��
    int				i, videoindex ,audioindex;

    //��Ƶ���
    AVCodecContext *pCodecCtx;          //��Ƶ�ṹ��
    AVCodec *pCodec;                    //��Ƶ������
    AVFrame	*pFrame,*pFrameYUV;         //��Ƶ���ṹ�壬��Ƶ��תYUVͼ���ʽ�ṹ��
    uint8_t *out_buffer;                //�����Ƶ���ݻ���

    //��Ƶ���
    AVCodecContext *aCodecCtx;          //��Ƶ�ṹ��
    AVCodec *aCodec;                    //��Ƶ������
    AVFrame *aFrame;                    //����Ƶ���ṹ�壬�˴�Ϊ��Ƶ

    //���±���������Ƶ�ز���
    // �����°�ffmpeg����aacֻ֧��AV_SAMPLE_FMT_FLTP����˲ɼ�����Ƶ���ݺ�ֱ�ӽ����ز���
    // �ز�����44100��32 bits ˫��������(AV_SAMPLE_FMT_FLTP)
    AVFrame *aFrame_ReSample;           //�ز�����Ƶ�ṹ��
    SwrContext *swrCtx;                 //�ز����ṹ��

    enum AVSampleFormat in_sample_fmt; //����Ĳ�����ʽ
    enum AVSampleFormat out_sample_fmt;//����Ĳ�����ʽ 16bit PCM
    int in_sample_rate;//����Ĳ�����
    int out_sample_rate;//����Ĳ�����
    int audio_tgt_channels; //��Ƶ������
    int out_ch_layout;      //�����������ʽ

    int pic_x;
    int pic_y;
    int pic_w;
    int pic_h;

    bool m_isRun;
    bool m_pause;

    SaveVideoFileThread * m_saveVideoFileThread;    //��������Ƶ�߳�

    //���ڴ洢��ȡ������Ƶ����
    // ����ƽ��ģʽ��pcm�洢��ʽΪ��LLLLLLLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRRRRRRR�����������Ҫ�������������ݷֿ����
    DECLARE_ALIGNED(16, uint8_t, audio_buf_L) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];   //���������
    unsigned int audio_buf_size_L;                                                  //�������������ݴ�С
    DECLARE_ALIGNED(16, uint8_t, audio_buf_R) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];   //���������
    unsigned int audio_buf_size_R;                                                  //�������������ݴ�С

};

#endif // GetVideoThread_H
