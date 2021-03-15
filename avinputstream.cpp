#include "avinputstream.h"
#include "recordadmin.h"
#include "mainwindow.h"

#include <unistd.h>
#include <stdio.h>
#include <qbuffer.h>
#include <QProcess>
#include <QTime>
#include <QDebug>
#include <QThread>
#include <QMutexLocker>

AVInputStream::AVInputStream()
{
    m_bMix = false;
    setbWriteAmix(true);
    m_hMicAudioThread = 0;
    setbRunThread(true);
    m_pMicAudioFormatContext = nullptr;
    m_pSysAudioFormatContext = nullptr;
    m_pAudioInputFormat = nullptr;
    dec_pkt = nullptr;
    m_bMicAudio = false;
    m_bSysAudio = false;
    m_micAudioindex = -1;
    m_sysAudioindex = -1;
    m_start_time = 0;
}

AVInputStream::~AVInputStream(void)
{
    printf("Desctruction Input!\n");
    setbWriteAmix(false);
    setbRunThread(false);
}

void AVInputStream::setMicAudioRecord(bool bRecord)
{
    m_bMicAudio = bRecord;
}

void AVInputStream::setSysAudioRecord(bool bRecord)
{
    m_bSysAudio = bRecord;
}

bool AVInputStream::openInputStream()
{
    AVDictionary *device_param = nullptr;
    int i;
    m_pAudioInputFormat = av_find_input_format("pulse"); //alsa
    assert(m_pAudioInputFormat != nullptr);
    if (m_pAudioInputFormat == nullptr) {
        printf("did not find this audio input devices\n");
    }
    if (m_bMicAudio) {
        string device_name = "default";
        if (avformat_open_input(&m_pMicAudioFormatContext, device_name.c_str(), m_pAudioInputFormat, &device_param) != 0) {
            return false;
        }
        if (avformat_find_stream_info(m_pMicAudioFormatContext, nullptr) < 0) {
            return false;
        }
        m_micAudioindex = -1;
        for (i = 0; i < static_cast<int>(m_pMicAudioFormatContext->nb_streams); i++) {
            if (m_pMicAudioFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                m_micAudioindex = i;
                break;
            }
        }
        if (m_micAudioindex == -1) {
            return false;
        }
        if (avcodec_open2(m_pMicAudioFormatContext->streams[m_micAudioindex]->codec, avcodec_find_decoder(m_pMicAudioFormatContext->streams[m_micAudioindex]->codec->codec_id), nullptr) < 0) {
            return false;
        }
        av_dump_format(m_pMicAudioFormatContext, 0, "default", 0);
    }
    m_pAudioCardInputFormat = av_find_input_format("pulse"); //alsa
    assert(m_pAudioCardInputFormat != nullptr);
    string device_name;
    if(m_bSysAudio)
    {
        QString device = currentAudioChannel();
        if(device.length()>0) {
            device_name = device.toLatin1().data()[0];
        }
        if (avformat_open_input(&m_pSysAudioFormatContext, device_name.c_str(), m_pAudioCardInputFormat, &device_param) != 0) {
            return false;
        }
        if (avformat_find_stream_info(m_pSysAudioFormatContext, nullptr) < 0) {
            return false;
        }
        fflush(stdout);
        m_sysAudioindex = -1;
        for (i = 0; i < static_cast<int>(m_pSysAudioFormatContext->nb_streams); i++) {
            if (m_pSysAudioFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
                m_sysAudioindex = i;
                break;
            }
        }
        if (m_sysAudioindex == -1) {
            return false;
        }
        if (avcodec_open2(m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec, avcodec_find_decoder(m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec->codec_id), nullptr) < 0) {
            return false;
        }
        av_dump_format(m_pSysAudioFormatContext, 0, device_name.c_str(), 0);
    } else {
        device_name = "default";
    }
    if (m_bSysAudio && m_bMicAudio) {
        m_bMix = true;
    }
    return true;
}

bool AVInputStream::audioCapture()
{
    m_start_time = av_gettime();
    if (m_bMix) {
        pthread_create(&m_hMicAudioThread, nullptr, captureMicToMixAudioThreadFunc,static_cast<void*>(this));
        pthread_create(&m_hSysAudioThread, nullptr, captureSysToMixAudioThreadFunc,static_cast<void*>(this));
        pthread_create(&m_hMixThread, nullptr, writeMixThreadFunc,static_cast<void*>(this));
    } else {
        if (m_bMicAudio) {
            pthread_create(&m_hMicAudioThread, nullptr, captureMicAudioThreadFunc,static_cast<void*>(this));
        }
        if (m_bSysAudio) {
            pthread_create(&m_hSysAudioThread, nullptr, captureSysAudioThreadFunc,static_cast<void*>(this));
        }
    }
    return true;
}

void AVInputStream::initScreenData()
{
}

bool AVInputStream::bWriteMix()
{
    QMutexLocker locker(&m_bWriteMixMutex);
    return m_bWriteMix;
}

void AVInputStream::setbWriteAmix(bool bWriteMix)
{
    QMutexLocker locker(&m_bWriteMixMutex);
    m_bWriteMix = bWriteMix;
}

bool AVInputStream::bRunThread()
{
    QMutexLocker locker(&m_bRunThreadMutex);
    return m_bRunThread;
}

void AVInputStream::setbRunThread(bool bRunThread)
{
    QMutexLocker locker(&m_bRunThreadMutex);
    m_bRunThread = bRunThread;
}

void  AVInputStream::onsFinisheStream()
{
    if (m_hMixThread) {
        m_hMixThread = 0;
    }
    if (m_hMicAudioThread) {
        m_hMicAudioThread = 0;
    }
    if (m_hSysAudioThread) {
        m_hSysAudioThread = 0;
    }
}

void *AVInputStream::writeMixThreadFunc(void* param)
{
    AVInputStream *inputStream = static_cast<AVInputStream*>(param);
    inputStream->writMixAudio();
    return nullptr;
}

void AVInputStream::writMixAudio()
{
    while(bWriteMix() && m_bMix)
    {
        qContext->m_recordAdmin->m_pOutputStream->writeMixAudio();
    }
}

void* AVInputStream::captureMicAudioThreadFunc(void* param)
{
    AVInputStream *inputStream = static_cast<AVInputStream*>(param);
    inputStream->readMicAudioPacket();
    return nullptr;
}

void *AVInputStream::captureMicToMixAudioThreadFunc(void *param)
{
    AVInputStream *inputStream = static_cast<AVInputStream*>(param);
    inputStream->readMicToMixAudioPacket();
    return nullptr;
}

int AVInputStream::readMicAudioPacket()
{
    int ret;
    int got_frame_ptr = 0;
    while (bRunThread())
    {
        AVFrame *inputFrame = av_frame_alloc();
        if (!inputFrame)
            return AVERROR(ENOMEM);
        AVPacket inputPacket;
        av_init_packet(&inputPacket);
        inputPacket.data = nullptr;
        inputPacket.size = 0;
        if ((ret = av_read_frame(m_pMicAudioFormatContext, &inputPacket)) < 0)
        {
            if (ret == AVERROR_EOF)
            {
                setbRunThread(false);
            }
            else
            {
                continue;
            }
        }
        if ((ret = avcodec_decode_audio4(m_pMicAudioFormatContext->streams[m_micAudioindex]->codec, inputFrame, &got_frame_ptr, &inputPacket)) < 0)
        {
            printf("Could not decode audio frame\n");
            return ret;
        }
        av_packet_unref(&inputPacket);
        if (got_frame_ptr)
        {
            qContext->m_recordAdmin->m_pOutputStream->writeMicAudioFrame(m_pMicAudioFormatContext->streams[m_micAudioindex], inputFrame, av_gettime() - m_start_time);
        }
        av_frame_free(&inputFrame);
        fflush(stdout);
    }
    if (nullptr != m_pMicAudioFormatContext)
    {
        avformat_close_input(&m_pMicAudioFormatContext);
    }
    if(nullptr != m_pMicAudioFormatContext)
    {
        avformat_free_context(m_pMicAudioFormatContext);
        m_pMicAudioFormatContext = nullptr;
    }
    m_micAudioindex = -1;
    return 0;
}

int AVInputStream::readMicToMixAudioPacket()
{
    int ret;
    int got_frame_ptr = 0;
    while (bRunThread())
    {
        AVFrame *inputFrame = av_frame_alloc();
        if (!inputFrame)
            return AVERROR(ENOMEM);
        AVPacket inputPacket;
        av_init_packet(&inputPacket);
        inputPacket.data = nullptr;
        inputPacket.size = 0;
        if ((ret = av_read_frame(m_pMicAudioFormatContext, &inputPacket)) < 0)
        {
            if (ret == AVERROR_EOF)
            {
                setbRunThread(false);
            }
            else
            {
                continue;
            }
        }
        if ((ret = avcodec_decode_audio4(m_pMicAudioFormatContext->streams[m_micAudioindex]->codec, inputFrame, &got_frame_ptr, &inputPacket)) < 0)
        {
            printf("Could not decode audio frame\n");
            return ret;
        }
        av_packet_unref(&inputPacket);
        if (got_frame_ptr)
        {
            qContext->m_recordAdmin->m_pOutputStream->writeMicToMixAudioFrame(m_pMicAudioFormatContext->streams[m_micAudioindex], inputFrame, av_gettime() - m_start_time);
        }
        av_frame_free(&inputFrame);
        fflush(stdout);
    }
    if (nullptr != m_pMicAudioFormatContext)
    {
        avformat_close_input(&m_pMicAudioFormatContext);
    }
    if(nullptr != m_pMicAudioFormatContext)
    {
        avformat_free_context(m_pMicAudioFormatContext);
        m_pMicAudioFormatContext = nullptr;
    }
    m_micAudioindex = -1;
    return 0;
}

void* AVInputStream::captureSysAudioThreadFunc(void* param)
{
    AVInputStream *inputStream = static_cast<AVInputStream*>(param);
    inputStream->readSysAudioPacket();
    return nullptr;
}

void *AVInputStream::captureSysToMixAudioThreadFunc(void *param)
{
    AVInputStream *inputStream = static_cast<AVInputStream*>(param);
    inputStream->readSysToMixAudioPacket();
    return nullptr;
}

int AVInputStream::readSysAudioPacket()
{
    if(!m_bSysAudio)
        return 1;
    int ret;
    int got_frame_ptr = 0;
    while (bRunThread())
    {
        AVFrame *input_frame = av_frame_alloc();
        if (!input_frame)
        {
            return AVERROR(ENOMEM);
        }
        AVPacket inputPacket;
        av_init_packet(&inputPacket);
        inputPacket.data = nullptr;
        inputPacket.size = 0;
        if ((ret = av_read_frame(m_pSysAudioFormatContext, &inputPacket)) < 0)
        {
            if (ret == AVERROR_EOF)
            {
                setbRunThread(false);
            }
            else
            {
                continue;
            }
        }
        if ((ret = avcodec_decode_audio4(m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec, input_frame, &got_frame_ptr, &inputPacket)) < 0)
        {
            printf("Could not decode audio frame\n");
            return ret;
        }
        av_packet_unref(&inputPacket);
        if (got_frame_ptr)
        {
            qContext->m_recordAdmin->m_pOutputStream->writeSysAudioFrame(m_pSysAudioFormatContext->streams[m_sysAudioindex], input_frame, av_gettime() - m_start_time);
        }
        av_frame_free(&input_frame);
    }
    if (nullptr != m_pSysAudioFormatContext)
    {
        avformat_close_input(&m_pSysAudioFormatContext);
    }
    if(nullptr != m_pSysAudioFormatContext)
    {
        avformat_free_context(m_pSysAudioFormatContext);
        m_pSysAudioFormatContext = nullptr;
    }
    m_sysAudioindex = -1;
    return 0;
}

int AVInputStream::readSysToMixAudioPacket()
{
    int ret;
    int got_frame_ptr = 0;
    while (bRunThread())
    {
        AVFrame *inputFrame = av_frame_alloc();
        if (!inputFrame)
        {
            return AVERROR(ENOMEM);
        }
        AVPacket inputPacket;
        av_init_packet(&inputPacket);
        inputPacket.data = nullptr;
        inputPacket.size = 0;
        if ((ret = av_read_frame(m_pSysAudioFormatContext, &inputPacket)) < 0)
        {
            if (ret == AVERROR_EOF)
            {
                setbRunThread(false);
            }
            else
            {
                continue;
            }
        }
        if ((ret = avcodec_decode_audio4(m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec, inputFrame, &got_frame_ptr, &inputPacket)) < 0)
        {
            printf("Could not decode audio frame\n");
            return ret;
        }
        av_packet_unref(&inputPacket);
        if (got_frame_ptr)
        {
            qContext->m_recordAdmin->m_pOutputStream->writeSysToMixAudioFrame(m_pSysAudioFormatContext->streams[m_sysAudioindex], inputFrame, av_gettime() - m_start_time);
        }
        av_frame_free(&inputFrame);
    }
    if (nullptr != m_pSysAudioFormatContext)
    {
        avformat_close_input(&m_pSysAudioFormatContext);
    }
    if(nullptr != m_pSysAudioFormatContext)
    {
        avformat_free_context(m_pSysAudioFormatContext);
        m_pSysAudioFormatContext = nullptr;
    }
    m_sysAudioindex = -1;
    return 0;
}

bool AVInputStream::GetVideoInputInfo(int & width, int & height, int & frame_rate, AVPixelFormat & pixFmt)
{
    width =m_screenDW-m_left-m_right;
    height = m_screenDH-m_top-m_bottom;
    pixFmt = m_ipix_fmt;
    frame_rate = m_fps;
    return true;
}

bool  AVInputStream::GetAudioInputInfo(AVSampleFormat & sample_fmt, int & sample_rate, int & channels,int &layout)
{
    if(m_micAudioindex != -1)
    {
        sample_fmt = m_pMicAudioFormatContext->streams[m_micAudioindex]->codec->sample_fmt;
        sample_rate = m_pMicAudioFormatContext->streams[m_micAudioindex]->codec->sample_rate;
        channels = m_pMicAudioFormatContext->streams[m_micAudioindex]->codec->channels;
        layout = static_cast<int>(m_pMicAudioFormatContext->streams[m_micAudioindex]->codec->channel_layout);
        return true;
    }
    return false;
}

bool  AVInputStream::GetAudioSCardInputInfo(AVSampleFormat & sample_fmt, int & sample_rate, int & channels,int& layout)
{
    if(m_sysAudioindex != -1)
    {
        sample_fmt = m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec->sample_fmt;
        sample_rate = m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec->sample_rate;
        channels =m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec->channels;
        layout = static_cast<int>(m_pSysAudioFormatContext->streams[m_sysAudioindex]->codec->channel_layout);
        return true;
    }
    return false;
}

QString AVInputStream::currentAudioChannel()
{
    QStringList options;
    options << QString(QStringLiteral("-c"));
    options << QString(QStringLiteral("pacmd list-sources | grep -PB 1 'analog.*monitor>' | head -n 1 | perl -pe 's/.* //g'"));
    QProcess process;
    process.start(QString(QStringLiteral("bash")), options);
    process.waitForFinished();
    process.waitForReadyRead();
    QByteArray tempArray =  process.readAllStandardOutput();
    char * charTemp = tempArray.data();
    QString str_output = QString(QLatin1String(charTemp));
    process.close();
    return str_output;
}
