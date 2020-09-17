
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

    AVFormatContext	*pFormatCtx;        //AVFormatContext是一个贯穿始终的结构体，很多函数都要用到它作为参数。它是FFMPEG解封装（flv，mp4，rmvb，avi）功能的结构体
    int				i, videoindex ,audioindex;

    //视频相关
    AVCodecContext *pCodecCtx;          //视频结构体
    AVCodec *pCodec;                    //视频解码器
    AVFrame	*pFrame,*pFrameYUV;         //视频流结构体，视频流转YUV图像格式结构体
    uint8_t *out_buffer;                //输出视频数据缓存

    //音频相关
    AVCodecContext *aCodecCtx;          //音频结构体
    AVCodec *aCodec;                    //音频解码器
    AVFrame *aFrame;                    //音视频流结构体，此处为音频

    //以下变量用于音频重采样
    // 由于新版ffmpeg编码aac只支持AV_SAMPLE_FMT_FLTP，因此采集到音频数据后直接进行重采样
    // 重采样成44100的32 bits 双声道数据(AV_SAMPLE_FMT_FLTP)
    AVFrame *aFrame_ReSample;           //重采样音频结构体
    SwrContext *swrCtx;                 //重采样结构体

    enum AVSampleFormat in_sample_fmt; //输入的采样格式
    enum AVSampleFormat out_sample_fmt;//输出的采样格式 16bit PCM
    int in_sample_rate;//输入的采样率
    int out_sample_rate;//输出的采样率
    int audio_tgt_channels; //音频声道数
    int out_ch_layout;      //输出的声道格式

    int pic_x;
    int pic_y;
    int pic_w;
    int pic_h;

    bool m_isRun;
    bool m_pause;

    SaveVideoFileThread * m_saveVideoFileThread;    //保存音视频线程

    //用于存储读取到的音频数据
    // 由于平面模式的pcm存储方式为：LLLLLLLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRRRRRRR，因此这里需要将左右声道数据分开存放
    DECLARE_ALIGNED(16, uint8_t, audio_buf_L) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];   //左声道存放
    unsigned int audio_buf_size_L;                                                  //左声道缓存数据大小
    DECLARE_ALIGNED(16, uint8_t, audio_buf_R) [AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];   //右声道存放
    unsigned int audio_buf_size_R;                                                  //右声道缓存数据大小

};

#endif // GetVideoThread_H
