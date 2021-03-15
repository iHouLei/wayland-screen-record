#ifndef AVINPUTSTREAM_H
#define AVINPUTSTREAM_H

#include <string>
#include <assert.h>
#include <qimage.h>
#include <qqueue.h>
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
#include <libavutil/time.h>
}

using namespace std;

class AVInputStream
{
public:
    AVInputStream();
    ~AVInputStream(void);

public:
    void  setMicAudioRecord(bool bRecord);
    void  setSysAudioRecord(bool bRecord);
    bool  openInputStream();
    void  onsFinisheStream();
    bool  audioCapture();
    bool  GetVideoInputInfo(int & width, int & height, int & framerate, AVPixelFormat & pixFmt);
    bool  GetAudioInputInfo(AVSampleFormat & sample_fmt, int & sample_rate, int & channels,int &layout);
    bool  GetAudioSCardInputInfo(AVSampleFormat & sample_fmt, int & sample_rate, int & channels,int &layout);
    QString currentAudioChannel();

public:
    bool m_bMicAudio;
    bool m_bSysAudio;
    int     m_micAudioindex;
    int     m_sysAudioindex;
    int m_left;
    int m_top;
    int m_right;
    int m_bottom;
    int m_selectWidth;
    int m_selectHeight;
    AVPixelFormat m_ipix_fmt;
    int m_fps;
    int m_screenDW;
    int m_screenDH;
    AVFormatContext *m_pMicAudioFormatContext;
    AVFormatContext *m_pSysAudioFormatContext;
    AVInputFormat  *m_pAudioInputFormat;
    AVInputFormat  *m_pAudioCardInputFormat;
    AVPacket *dec_pkt;
    pthread_t  m_hMicAudioThread ,m_hSysAudioThread;
    pthread_t  m_hMixThread;
    int64_t     m_start_time;
    bool m_bMix;
    bool bWriteMix();
    void setbWriteAmix(bool bWriteMix);
    bool bRunThread() ;
    void setbRunThread(bool bRunThread);

protected:
    static void *captureMicAudioThreadFunc(void* param);
    static void *captureMicToMixAudioThreadFunc(void* param);
    static void *captureSysAudioThreadFunc(void* param);
    static void *captureSysToMixAudioThreadFunc(void* param);
    static void  *writeMixThreadFunc(void* param);
    void writMixAudio();
    int  readMicAudioPacket();
    int  readMicToMixAudioPacket();
    int  readSysAudioPacket();
    int  readSysToMixAudioPacket();
    void initScreenData();

private:
    bool m_bWriteMix;
    QMutex m_bWriteMixMutex;
    bool m_bRunThread;
    QMutex m_bRunThreadMutex;
};

#endif //AVINPUTSTREAM_H
