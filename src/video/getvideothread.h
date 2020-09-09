
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

    AVFormatContext	*pFormatCtx;
    int				i, videoindex ,audioindex;

    ///��Ƶ���
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame	*pFrame,*pFrameYUV;
    uint8_t *out_buffer;

    ///��Ƶ���
    AVCodecContext *aCodecCtx;
    AVCodec *aCodec;
    AVFrame *aFrame;

    ///���±���������Ƶ�ز���
    /// �����°�ffmpeg����aacֻ֧��AV_SAMPLE_FMT_FLTP����˲ɼ�����Ƶ���ݺ�ֱ�ӽ����ز���
    /// �ز�����44100��32 bits ˫��������(AV_SAMPLE_FMT_FLTP)
    AVFrame *aFrame_ReSample;
    SwrContext *swrCtx;

    enum AVSampleFormat in_sample_fmt; //����Ĳ�����ʽ
    enum AVSampleFormat out_sample_fmt;//����Ĳ�����ʽ 16bit PCM
    int in_sample_rate;//����Ĳ�����
    int out_sample_rate;//����Ĳ�����
    int audio_tgt_channels; ///av_get_channel_layout_nb_channels(out_ch_layout);
    int out_ch_layout;

    int pic_x;
    int pic_y;
    int pic_w;
    int pic_h;

    bool m_isRun;
    bool m_pause;

    SaveVideoFileThread * m_saveVideoFileThread;

    ///���ڴ洢��ȡ������Ƶ����
    /// ����ƽ��ģʽ��pcm�洢��ʽΪ��LLLLLLLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRRRRRRR�����������Ҫ�������������ݷֿ����
    DECLARE_ALIGNED(16, uint8_t, audio_buf_L) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    unsigned int audio_buf_size_L;
    DECLARE_ALIGNED(16, uint8_t, audio_buf_R) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    unsigned int audio_buf_size_R;

};

#endif // GetVideoThread_H
