#ifndef AVOUTPUTSTREAM_H
#define AVOUTPUTSTREAM_H

#include "dateadmin.h"

#include <string>
#include <assert.h>
#include <QMutex>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
#include "libavdevice/avdevice.h"
#include "libavfilter/buffersink.h"
#include "libavutil/avutil.h"
#include "libavutil/fifo.h"
}

using namespace std;

class DateAdmin;

class AVOutputStream
{
public:
    AVOutputStream();
    ~AVOutputStream(void);

    void SetVideoCodecProp(AVCodecID codec_id, int framerate, int bitrate, int gopsize, int width, int height);
    void SetAudioCodecProp(AVCodecID codec_id, int samplerate, int channels, int layout,int bitrate);
    void SetAudioCardCodecProp(AVCodecID codec_id, int samplerate, int channels,int layout, int bitrate);
    bool open(QString path);
    void close();
    int writeVideoFrame(DateAdmin::waylandFrame &frame);
    int writeMicAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp);
    int writeMicToMixAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp);
    int writeSysAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp);
    int writeSysToMixAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp);
    int write_filter_audio_frame(AVStream *&outst,AVCodecContext* &codecCtx_audio,AVFrame *&outframe);
    int init_filters();
    int init_context_amix(int channel, uint64_t channel_layout,int sample_rate,int64_t bit_rate);
    void writeMixAudio();
    void setIsOverWrite(bool isCOntinue);
    int audioRead(AVAudioFifo *af, void **data, int nb_samples);
    int audioWrite(AVAudioFifo *af, void **data, int nb_samples);
    int writeFrame(AVFormatContext *s, AVPacket *pkt);
    int writeTrailer(AVFormatContext *s);
    int audioFifoSpace(AVAudioFifo *af);
    int audioFifoSize(AVAudioFifo *af);
    int audioFifoRealloc(AVAudioFifo *af, int nb_samples);
    AVAudioFifo *audioFifoAlloc(enum AVSampleFormat sample_fmt, int channels,int nb_samples);
    void audioFifoFree(AVAudioFifo *af);
    bool isWriteFrame();
    void setIsWriteFrame(bool isWriteFrame);

public:
    int m_left;
    int m_top;
    int m_right;
    int m_bottom;

private:
    char *m_path;
    QMutex m_audioReadWriteMutex;
    QMutex m_writeFrameMutex;
    bool m_isWriteFrame;
    QMutex m_isWriteFrameMutex;
    AVStream* m_videoStream;
    AVStream* m_micAudioStream;
    AVStream* m_sysAudioStream;
    AVStream* audio_amix_st;
    AVFormatContext *m_videoFormatContext;
    AVCodecContext* pCodecCtx;
    AVCodecContext* m_pMicCodecContext;
    AVCodecContext* m_pSysCodecContext;
    AVCodecContext* pCodecCtx_amix;
    AVCodec* pCodec;
    AVCodec* pCodec_a;
    AVCodec* pCodec_aCard;
    AVCodec* pCodec_amix;
    AVFrame *pFrameYUV;
    struct SwsContext *m_pVideoSwsContext;
    struct SwrContext *m_pMicAudioSwrContext;
    struct SwrContext *m_pSysAudioSwrContext;
    AVAudioFifo * m_micAudioFifo;
    AVAudioFifo * m_sysAudioFifo;
    int is_fifo_scardinit;
    int  m_micAudioFrame;
    int  m_sysAudioFrame;
    int  m_nb_samples;
    int64_t m_start_mix_time;
    int64_t m_next_vid_time;
    int64_t m_next_aud_time;
    int64_t  m_nLastAudioPresentationTime;
    int64_t  m_nLastAudioCardPresentationTime;
    int64_t  m_nLastAudioMixPresentationTime;
    int64_t  m_mixCount;
    uint8_t ** m_convertedMicSamples;
    uint8_t ** m_convertedSysSamples;
    uint8_t * m_out_buffer;
    AVFilterGraph *filter_graph;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx1;
    AVFilterContext *buffersrc_ctx2;
    int tmpFifoFailed;
    bool m_isOverWrite;
    AVCodecID  m_videoCodecID;
    AVCodecID  m_micAudioCodecID;
    AVCodecID  m_sysAudioCodecID;
    bool m_bMix;
    AVFrame * mMic_frame;
    AVFrame * mSpeaker_frame;
    int m_width, m_height;
    int m_framerate;
    int m_video_bitrate;
    int m_gopsize;
    int m_samplerate;
    int m_channels;
    int m_channels_layout;
    int m_audio_bitrate;
    int m_samplerate_card;
    int m_channels_card;
    int m_channels_card_layout;
    int m_audio_bitrate_card;
};

#endif //AVOUTPUTSTREAM_H
