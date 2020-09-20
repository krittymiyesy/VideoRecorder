#include "savevideofile.h"
#include "AppConfig.h"

#include <QFileInfo>
#include <QDir>

/**
*  Add ADTS header at the beginning of each and every AAC packet.
*  This is needed as MediaCodec encoder generates a packet of raw
*  AAC data.
*
*  Note the packetLen must count in the ADTS header itself !!! .
*ע�⣬�����packetLen����Ϊraw aac Packet Len + 7; 7 bytes adts header
**/
void addADTStoPacket(uint8_t* packet, int packetLen)
{
   int profile = 2;  //AAC LC��MediaCodecInfo.CodecProfileLevel.AACObjectLC;
   int freqIdx = 4;  //32K, ������ע��avpriv_mpeg4audio_sample_rates��32000��Ӧ�������±꣬����ffmpegԴ��
   int chanCfg = 2;  //������ע��channel_configuration��Stero˫����������

   /*int avpriv_mpeg4audio_sample_rates[] = {
       96000, 88200, 64000, 48000, 44100, 32000,
               24000, 22050, 16000, 12000, 11025, 8000, 7350
   };
   channel_configuration: ��ʾ������chanCfg
   0: Defined in AOT Specifc Config
   1: 1 channel: front-center
   2: 2 channels: front-left, front-right
   3: 3 channels: front-center, front-left, front-right
   4: 4 channels: front-center, front-left, front-right, back-center
   5: 5 channels: front-center, front-left, front-right, back-left, back-right
   6: 6 channels: front-center, front-left, front-right, back-left, back-right, LFE-channel
   7: 8 channels: front-center, front-left, front-right, side-left, side-right, back-left, back-right, LFE-channel
   8-15: Reserved
   */

   // fill in ADTS data
   packet[0] = (uint8_t)0xFF;
   packet[1] = (uint8_t)0xF9;
   packet[2] = (uint8_t)(((profile-1)<<6) + (freqIdx<<2) +(chanCfg>>2));
   packet[3] = (uint8_t)(((chanCfg&3)<<6) + (packetLen>>11));
   packet[4] = (uint8_t)((packetLen&0x7FF) >> 3);
   packet[5] = (uint8_t)(((packetLen&7)<<5) + 0x1F);
   packet[6] = (uint8_t)0xFC;
}

SaveVideoFileThread::SaveVideoFileThread()
{
    isStop = false;

    m_containsVideo = true;
    m_containsAudio = true;

    videoDataQueneHead = NULL;
    videoDataQueneTail = NULL;

    videoBufferCount = 0;

    m_videoFrameRate = 15;

    lastVideoNode = NULL;

    mBitRate = 450000;

    audio_pts = 0;
    video_pts = 0;

    mONEFrameSize = 4096;
}

SaveVideoFileThread::~SaveVideoFileThread()
{

}

void SaveVideoFileThread::setContainsVideo(bool value)
{
    m_containsVideo = value;
}

void SaveVideoFileThread::setContainsAudio(bool value)
{
    m_containsAudio = value;
}

void SaveVideoFileThread::setVideoFrameRate(int value)
{
    m_videoFrameRate = value;
}

void SaveVideoFileThread::setFileName(QString filePath)
{
    mFilePath = filePath;
}

void SaveVideoFileThread::setQuantity(int value)
{
    mBitRate = 450000 + (value - 5) * 50000;
}

void SaveVideoFileThread::videoDataQuene_Input(uint8_t * buffer, int size, int64_t time)
{
//  qDebug()<<"void SaveVideoFileThread::videoDataQuene_Input(uint8_t * buffer,int size,long time)"<<time;
    BufferDataNode * node = (BufferDataNode*)malloc(sizeof(BufferDataNode));
    node->bufferSize = size;
    node->next = NULL;
    node->time = time;

    node->buffer = buffer;

    mVideoMutex.lock();

    if (videoDataQueneHead == NULL)
    {
        videoDataQueneHead = node;
    }
    else
    {
        videoDataQueneTail->next = node;
    }

    videoDataQueneTail = node;

    videoBufferCount++;

    mVideoMutex.unlock();
//  qDebug()<<__FUNCTION__<<videoBufferCount<<time;
    if (videoBufferCount >= 30)
    {
        QString logStr = QString("!!!!!!!!!! encode too slow! count=%1")
                    .arg(videoBufferCount);
        AppConfig::WriteLog(logStr);
    }

}

BufferDataNode *SaveVideoFileThread::videoDataQuene_get(int64_t time)
{
    BufferDataNode * node = NULL;

    mVideoMutex.lock();

    if (videoDataQueneHead != NULL)
    {
        node = videoDataQueneHead;

//  qDebug()<<"111:"<<time<<node->time<<videoBufferCount;
        if (time >= node->time)
        {
//            qDebug()<<"000";
            if (videoDataQueneHead->next != NULL)
            {
//                qDebug()<<"111";
                while(node != NULL)
                {
//                    qDebug()<<"222";
                    if (node->next == NULL)
                    {
                        videoDataQueneHead = node;
                        node = NULL;
                        break;
                    }
//  qDebug()<<"333"<<time << node->next->time;
                    if (time < node->next->time)
                    {
                        break;
                    }

                    BufferDataNode * tmp = node;
//  qDebug()<<"222:"<<node->time<<time;
                    node = node->next;
                    videoBufferCount--;
                    free(tmp->buffer);
                    free(tmp);
                }
            }
            else
            {
                node = NULL;
            }
        }
        else
        {
            node = lastVideoNode;
        }

        if (videoDataQueneTail == node)
        {
            videoDataQueneTail = NULL;
        }

        if (node != NULL && node != lastVideoNode)
        {
            videoDataQueneHead = node->next;
            videoBufferCount--;
        }

    }
//    qDebug()<<__FUNCTION__<<videoBufferCount<<node;
    mVideoMutex.unlock();

    return node;
}

void SaveVideoFileThread::audioDataQuene_Input(const uint8_t *buffer, const int &size)
{
    BufferDataNode  node;
    node.bufferSize = size;
    node.next = NULL;

    node.buffer = (uint8_t*)buffer;

    mAudioMutex.lock();
    mAudioDataList.append(node);
//  qDebug()<<__FUNCTION__<<audioBufferCount<<size;
    mAudioMutex.unlock();

}

bool SaveVideoFileThread::audioDataQuene_get(BufferDataNode &node)
{
    bool isSucceed = false;

    mAudioMutex.lock();

    if (!mAudioDataList.isEmpty())
    {
        node = mAudioDataList.takeFirst();

        isSucceed = true;

//    qDebug()<<__FUNCTION__<<mAudioDataList.size();
    }

    mAudioMutex.unlock();

    return isSucceed;
}


/*
 * add an audio output stream
 */
void SaveVideoFileThread::add_audio_stream(OutputStream *ost, AVFormatContext *oc,
                                                AVCodec **codec,
                                                enum AVCodecID codec_id)
{
    AVCodecContext *aCodecCtx;

    /* find the video encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    ost->st->id = oc->nb_streams-1;

    const AVCodec* aCodec = *codec;

    aCodecCtx = avcodec_alloc_context3(aCodec);
    if (!aCodecCtx)
    {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }

    //������仰�ҳ� aac������֧�ֵ� sample_fmt
    //�ҳ����� AV_SAMPLE_FMT_FLTP
    const enum AVSampleFormat *p = aCodec->sample_fmts;
    fprintf(stderr, "aac encoder sample format is: %s \n",av_get_sample_fmt_name(*p));

    ost->enc = aCodecCtx;

//    aCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    aCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    aCodecCtx->sample_rate= 44100;
    aCodecCtx->channels = 2;
    aCodecCtx->channel_layout=av_get_default_channel_layout(aCodecCtx->channels);

    ost->st->time_base.num = 1; // = (AVRational){ 1, c->sample_rate };
    ost->st->time_base.den = aCodecCtx->sample_rate;

}

void SaveVideoFileThread::open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost)
{
    Q_UNUSED(oc)
    AVCodecContext *aCodecCtx = ost->enc;

    /* open it */
    if (avcodec_open2(aCodecCtx, codec, NULL) < 0)
    {
        qDebug("could not open codec\n");
        exit(1);
    }

    mONEFrameSize = av_samples_get_buffer_size(NULL, aCodecCtx->channels, aCodecCtx->frame_size, aCodecCtx->sample_fmt, 1);
    qDebug()<<__FUNCTION__<<__LINE__<<"++++++++++++++++++++++++"<<"mONEFrameSize:"<<mONEFrameSize<<"aCodecCtx->channels: "<<aCodecCtx->channels<<"aCodecCtx->frame_size: "<<aCodecCtx->frame_size<<"aCodecCtx->sample_fmt:"<<aCodecCtx->sample_fmt;
    ost->frame           = av_frame_alloc();
    ost->frameBuffer     = (uint8_t *)av_malloc(mONEFrameSize);
    ost->frameBufferSize = mONEFrameSize;

    ///��仰����Ҫ(�������frame����Ĳ��������)
    int oneChannelBufferSize = mONEFrameSize / aCodecCtx->channels; //�����һ������������
    int nb_samplesize = oneChannelBufferSize / av_get_bytes_per_sample(aCodecCtx->sample_fmt); //��������������
    ost->frame->nb_samples = nb_samplesize;

    ///��2�ַ�ʽ������
//    avcodec_fill_audio_frame(ost->frame, aCodecCtx->channels, aCodecCtx->sample_fmt,(const uint8_t*)ost->frameBuffer, mONEFrameSize, 0);
    av_samples_fill_arrays(ost->frame->data, ost->frame->linesize, ost->frameBuffer, aCodecCtx->channels, ost->frame->nb_samples, aCodecCtx->sample_fmt, 0);

    ost->tmp_frame = nullptr;

    /* copy the stream parameters to the muxer */
    int ret = avcodec_parameters_from_context(ost->st->codecpar, aCodecCtx);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }

}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
//static int write_audio_frame(AVFormatContext *oc, OutputStream *ost)
bool SaveVideoFileThread::write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
    AVCodecContext *aCodecCtx = ost->enc;

    AVPacket pkt;
    av_init_packet(&pkt);

    AVPacket *packet = &pkt;

    AVFrame *aFrame;

    BufferDataNode node;

    if (audioDataQuene_get(node))
    {
        aFrame = ost->frame;

        memcpy(ost->frameBuffer, node.buffer, node.bufferSize);

        free(node.buffer);

        aFrame->pts = ost->next_pts;
        ost->next_pts  += aFrame->nb_samples;
    }
    else
    {
        return false;
    }

    if (aFrame)
    {
        AVRational rational;
        rational.num = 1;
        rational.den = aCodecCtx->sample_rate;
        aFrame->pts = av_rescale_q(ost->samples_count, rational, aCodecCtx->time_base);
        ost->samples_count += aFrame->nb_samples;
    }

    /* send the frame for encoding */
    int ret = avcodec_send_frame(aCodecCtx, aFrame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending the frame to the audio encoder\n");
        return false;
    }

    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(aCodecCtx, packet);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
        {
            char errstr[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_make_error_string(errstr, AV_ERROR_MAX_STRING_SIZE, ret);
            QString logStr = QString("!!!!!!!!!! Error encoding audio frame: %1 ret=%2")
                        .arg(QString(errstr))
                        .arg(ret);
            AppConfig::WriteLog(logStr);
            return false;
        }

        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(&pkt, aCodecCtx->time_base, ost->st->time_base);
        pkt.stream_index = ost->st->index;

        ///����MP4�ļ���ʱ�������1/1000���������ת�ɺ������ʽ��������ʾ�ͼ��㡣
        ///��Ptsת���ɺ������ʽ������pts����������ʾ�������޸�д���ļ���pts
        audio_pts = av_rescale_q(pkt.pts, ost->st->time_base, {1, 1000});

        /* Write the compressed frame to the media file. */
        ret = av_interleaved_write_frame(oc, &pkt);
        if (ret < 0)
        {
            char errstr[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_make_error_string(errstr, AV_ERROR_MAX_STRING_SIZE, ret);
            QString logStr = QString("!!!!!!!!!! Error while writing audio frame: %1 ret=%2")
                        .arg(QString(errstr))
                        .arg(ret);
            AppConfig::WriteLog(logStr);
        }

        av_packet_unref(packet);
        break;
    }

    return true;

}

void SaveVideoFileThread::close_audio(AVFormatContext *oc, OutputStream *ost)
{
    Q_UNUSED(oc)
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);

    if (ost->tmp_frame != nullptr)
    av_frame_free(&ost->tmp_frame);

    if (ost->frameBuffer != NULL)
    {
        av_free(ost->frameBuffer);
        ost->frameBuffer = NULL;
    }
}


/* add a video output stream */
void SaveVideoFileThread::add_video_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;

    /* find the video encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    ost->st->id = oc->nb_streams-1;

    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;

    c->codec_id = codec_id;

qDebug()<<__FUNCTION__<<c<<c->codec<<c->codec_id<<codec_id;

    /* resolution must be a multiple of two */
    c->width = WIDTH;
    c->height = HEIGHT;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->gop_size = m_videoFrameRate;
    c->pix_fmt = AV_PIX_FMT_YUV420P;

//    ��Ƶ���������õ����ʿ��Ʒ�ʽ����abr(ƽ������)��crf(�㶨����)��cqp(�㶨����)��
//    ffmpeg��AVCodecContext��ʾ�ṩ�����ʴ�С�Ŀ��Ʋ��������ǲ�û���ṩ�����Ŀ��Ʒ�ʽ��
//    ffmpeg�����ʿ��Ʒ�ʽ��Ϊ���¼��������
//    1.���������AVCodecContext��bit_rate�Ĵ�С�������abr�Ŀ��Ʒ�ʽ��
//    2.���û������AVCodecContext�е�bit_rate����Ĭ�ϰ���crf��ʽ���룬crfĬ�ϴ�СΪ23����ֵ������qpֵ��ͬ����ʾ��Ƶ��������
//    3.����û����Լ����ã�����Ҫ����av_opt_set��������AVCodecContext��priv_data����������������ֿ��Ʒ�ʽ��ʵ�ִ��룺

    c->bit_rate = mBitRate;

#if 0
    ///ƽ������
    //Ŀ������ʣ������������ʣ���Ȼ����������Խ����Ƶ��СԽ��
    c->bit_rate = mBitRate;

#elif 1
    ///�㶨����
//    ���������ķ�ΧΪ0~51������0Ϊ����ģʽ��23Ϊȱʡֵ��51���������ġ�������ԽС��ͼ������Խ�á��������Ͻ���18~28��һ������ķ�Χ��18��������Ϊ���Ӿ��Ͽ�������ģ����������Ƶ������������Ƶһģһ������˵����޼������Ӽ����ĽǶ�����������Ȼ������ѹ����
//    ��Crfֵ��6��������ʴ�ż���һ�룻��Crfֵ��6��������ʷ�����ͨ�����ڱ�֤�ɽ�����Ƶ������ǰ����ѡ��һ������Crfֵ����������Ƶ�����ܺã��Ǿͳ���һ�������ֵ��������������㣬�Ǿͳ���һ��Сһ��ֵ��
//    av_opt_set(c->priv_data, "crf", "31.000", AV_OPT_SEARCH_CHILDREN);

#else
    ///qp��ֵ��crfһ��
//    av_opt_set(c->priv_data, "qp", "31.000",AV_OPT_SEARCH_CHILDREN);
#endif

    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    ost->st->time_base = { 1, m_videoFrameRate };
    c->time_base       = ost->st->time_base;
//    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    c->gop_size = m_videoFrameRate * 2; ///I֡���

//    //�̶��������������ֵԽ����ƵԽС
//    c->bit_rate_tolerance = mBitRate;

//    //H264 ���������úܶ���� �����о���
//    pCodecCtx->me_range = 16;
//    pCodecCtx->max_qdiff = 1;
//    c->qcompress = 0.85;
    c->qmin = 16;
    c->qmax = 31;

//    //���ã�qmin/qmax�ı�ֵ���������ʣ�1��ʾ�ֲ����ô˷�����0��ʾȫ�֣�
//    c->rc_qsquish = 0;

//    //��Ϊ���ǵ�����ϵ��q����qmin��qmax֮�両���ģ�
//    //qblur��ʾ���ָ����仯�ı仯�̶ȣ�ȡֵ��Χ0.0��1.0��ȡ0��ʾ������
//    c->qblur = 1.0;

//std::cout<<"mBitRate"<<mBitRate<<m_videoFrameRate;

//    ///b_frame_strategy
//    ///���Ϊtrue�����Զ�����ʲôʱ����Ҫ����B֡����ߴﵽ���õ����B֡����
//    ///�������Ϊfalse,��ô����B֡����ʹ�á�
//    c->b_frame_strategy = 1;
//    c->max_b_frames = 5;

    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO){
        /* Needed to avoid using macroblocks in which some coeffs overflow.
           This does not happen with normal video, it just happens here as
           the motion of the chroma plane does not match the luma plane. */
        c->mb_decision=2;
    }

}

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }

    return picture;
}

void SaveVideoFileThread::open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost)
{
    Q_UNUSED(oc)
    AVCodecContext *c = ost->enc;

    // Set Option
    AVDictionary *param = 0;
    //H.264
    //av_dict_set(&param, "preset", "slow", 0);
    av_dict_set(&param, "preset", "superfast", 0);
    av_dict_set(&param, "tune", "zerolatency", 0);  //ʵ��ʵʱ����

    c->thread_count = 16;
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

qDebug()<<__FUNCTION__<<"333";
    int ret = 0;
    if (ret = avcodec_open2(c, codec,&param) < 0){
      qDebug()<<("Failed to open video encoder!\n")<<ret;
      exit(1);
    }
qDebug()<<__FUNCTION__<<"333"<<c->pix_fmt<<AV_PIX_FMT_YUV420P;

    /* allocate the encoded raw picture */
    {
        ost->frame = av_frame_alloc();

        ost->frame->format = c->pix_fmt;
        ost->frame->width  = c->width;
        ost->frame->height = c->height;

        int numBytes_yuv = avpicture_get_size(AV_PIX_FMT_YUV420P, c->width,c->height);

        uint8_t * out_buffer_yuv = (uint8_t *) av_malloc(numBytes_yuv * sizeof(uint8_t));

        avpicture_fill((AVPicture *) ost->frame, out_buffer_yuv, AV_PIX_FMT_YUV420P,
                c->width, c->height);

        ost->frameBuffer = out_buffer_yuv;
        ost->frameBufferSize = numBytes_yuv;
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }

}

bool SaveVideoFileThread::write_video_frame(AVFormatContext *oc, OutputStream *ost, double time)
{
    int out_size, ret = 0;
    AVCodecContext *c;
    int got_packet = 0;

    c = ost->enc;
//qDebug()<<__FUNCTION__<<"0000"<<time;
    BufferDataNode *node = videoDataQuene_get(time);

    if (node == NULL)
    {
//        qDebug()<<__FUNCTION__<<"0000 000"<<time;
        return false;
    }
    else
    {
        if (node != lastVideoNode)
        {
            if (lastVideoNode != NULL)
            {
                free(lastVideoNode->buffer);
                free(lastVideoNode);
            }

            lastVideoNode = node;
        }

        memcpy(ost->frameBuffer, node->buffer, node->bufferSize);
        ost->frame->pts = ost->next_pts++;

    }
//qDebug()<<__FUNCTION__<<"1111";

    AVPacket pkt = { 0 };

    /* encode the image */
    out_size = avcodec_encode_video2(c, &pkt, ost->frame, &got_packet);

    if (got_packet)
    {
//qDebug()<<__FUNCTION__<<"111"<<ost->frame->pts<<pkt.pts<<c->time_base.num<<c->time_base.den<<ost->st->time_base.den<<ost->st->time_base.num;
        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(&pkt, c->time_base, ost->st->time_base);
        pkt.stream_index = ost->st->index;
//qDebug()<<__FUNCTION__<<"222"<<ost->frame->pts<<pkt.pts<<time<<node->time;

        //����MP4�ļ���ʱ�������1/1000���������ת�ɺ������ʽ��������ʾ�ͼ��㡣
        //��Ptsת���ɺ������ʽ������pts����������ʾ�������޸�д���ļ���pts
        video_pts = av_rescale_q(pkt.pts, ost->st->time_base, {1, 1000});

        /* Write the compressed frame to the media file. */
        ret = av_interleaved_write_frame(oc, &pkt);
        if (ret < 0)
        {
            char errstr[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_make_error_string(errstr, AV_ERROR_MAX_STRING_SIZE, ret);
            QString logStr = QString("!!!!!!!!!! Error while writing video frame: %1 ret=%2")
                        .arg(QString(errstr))
                        .arg(ret);
            AppConfig::WriteLog(logStr);
        }

        av_packet_unref(&pkt);
    }

    return true;

}

void SaveVideoFileThread::close_video(AVFormatContext *oc, OutputStream *ost)
{
    Q_UNUSED(oc)
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);

    if (ost->tmp_frame != NULL)
        av_frame_free(&ost->tmp_frame);

    if (ost->frameBuffer != NULL)
    {
        av_free(ost->frameBuffer);
        ost->frameBuffer = NULL;
    }

}

int64_t SaveVideoFileThread::getVideoPts()
{
    return video_pts;
}

int64_t SaveVideoFileThread::getAudioPts()
{
    return audio_pts;
}

void SaveVideoFileThread::run()
{
    int writeFileIndex = 1;

while(1)
{
    if (isStop)
    {
        break;
    }

    if (WIDTH <= 0 || HEIGHT <= 0)
    {
        msleep(100);
        continue;
    }

    QString filePath;

    if (writeFileIndex > 1)
    {
        QFileInfo fileInfo(mFilePath);

        filePath = QString("%1/%2_%3.%4")
                .arg(fileInfo.absoluteDir().path())
                .arg(fileInfo.baseName())
                .arg(writeFileIndex)
                .arg(fileInfo.suffix());
    }
    else
    {
        filePath = mFilePath;
    }

    emit sig_StartWriteFile(filePath);

    char filename[1280] = {0};
    strcpy(filename, filePath.toLocal8Bit());
    writeFileIndex++;

    OutputStream video_st = { 0 }, audio_st = { 0 };
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *audio_codec, *video_codec;
    int have_video = 0, have_audio = 0;

    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc)
    {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }

    if (!oc)
    {
        fprintf(stderr,"Could not deduce output format from file extension: using MPEG.\n");

        QString logStr = QString("!!!!!!!!!! Could not deduce output format from file extension ... %1").arg(filePath);
        AppConfig::WriteLog(logStr);

        msleep(1000);

        continue;
    }

    fmt = oc->oformat;

    if (m_containsVideo)
    {
        qDebug()<<fmt->video_codec;
        if (fmt->video_codec != AV_CODEC_ID_NONE)
        {
            add_video_stream(&video_st, oc, &video_codec, AV_CODEC_ID_H264);
            have_video = 1;
        }
        qDebug()<<"333";
    }

    if (m_containsAudio)
    {
        if (fmt->audio_codec != AV_CODEC_ID_NONE)
        {
            add_audio_stream(&audio_st, oc, &audio_codec, AV_CODEC_ID_AAC);
            have_audio = 1;
        }
    }

    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_video){
        open_video(oc, video_codec, &video_st);
    }
    if (have_audio){
        open_audio(oc, audio_codec, &audio_st);
    }
    av_dump_format(oc, 0, filename, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        if (avio_open(&oc->pb, filename, AVIO_FLAG_WRITE) < 0)
        {
            qDebug()<<"Could not open "<<filename;
//            return;

            QString logStr = QString("!!!!!!!!!! Could not open %1").arg(filePath);
            AppConfig::WriteLog(logStr);

            msleep(1000);

            continue;

        }
    }

    /* write the stream header, if any */
    avformat_write_header(oc, NULL);

    video_pts = 0;
    audio_pts = 0;

    while(1)
    {
//        qDebug()<<__FUNCTION__<<video_st.next_pts<<audio_st.next_pts<<video_pts<<audio_pts;

        /* select the stream to encode */
        if (!have_audio || (av_compare_ts(video_st.next_pts, video_st.enc->time_base, audio_st.next_pts, audio_st.enc->time_base) <= 0))
        {
            if (!write_video_frame(oc, &video_st, video_pts))
            {
                if (isStop)
                {
                    break;
                }
                msleep(1);
            }
        }
        else
        {
            if (!write_audio_frame(oc, &audio_st))
            {
                if (isStop)
                {
                    break;
                }
                msleep(1);
            }
        }
    }

    QString logStr = QString("!!!!!!!!!! av_write_trailer ... %1").arg(filePath);
    AppConfig::WriteLog(logStr);

    av_write_trailer(oc);

    logStr = QString("!!!!!!!!!! av_write_trailer finised! %1").arg(filePath);
    AppConfig::WriteLog(logStr);

    emit sig_StopWriteFile(filePath);

    qDebug()<<"void RTMPPushThread::run() finished!";

    /* close each codec */
    if (have_video)
        close_video(oc, &video_st);
    if (have_audio)
        close_audio(oc, &audio_st);

    /* free the streams */
    for(unsigned int i = 0; i < oc->nb_streams; i++) {
        av_freep(&oc->streams[i]->codec);
        av_freep(&oc->streams[i]);
    }

    if (!(fmt->flags & AVFMT_NOFILE)) {
        /* close the output file */
        avio_close(oc->pb);
    }

    /* free the stream */
    avformat_free_context(oc);
}

    qDebug()<<"void RTMPPushThread::run() finished! 222";
}

void SaveVideoFileThread::setWidth(int width,int height)
{
    WIDTH = width;
    HEIGHT = height;
}

bool SaveVideoFileThread::startEncode()
{
    isStop = false;
    start();

    return true;
}

bool SaveVideoFileThread::stopEncode()
{
    isStop = true;

    return true;
}
