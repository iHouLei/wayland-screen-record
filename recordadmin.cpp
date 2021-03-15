#include "recordadmin.h"
#include "mainwindow.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <cstring>
#include <math.h>
#include <unistd.h>
#include <qtimer.h>
#include <QDebug>

RecordAdmin::RecordAdmin(QStringList list, QObject *parent): QObject(parent),
    m_pInputStream(nullptr),
    m_pOutputStream(nullptr),
    m_writeFrameThread(nullptr)
{
    if(list.size() > 7)
    {
        m_videoType = list[0].toInt();
        m_selectWidth = list[1].toInt()/2*2;
        m_selectHeight = list[2].toInt()/2*2;
        m_x = list[3].toInt();
        m_y = list[4].toInt();
        m_fps = list[5].toInt();
        m_filePath = list[6];
        m_audioType = list[7].toInt();
    }
    m_pInputStream  = new AVInputStream();
    m_pOutputStream = new AVOutputStream();
    avcodec_register_all();
    av_register_all();
    avdevice_register_all();
    m_writeFrameThread = new WriteFrameThread();
}

RecordAdmin::~RecordAdmin()
{
    if(nullptr != m_writeFrameThread)
    {
        delete m_writeFrameThread;
        m_writeFrameThread = nullptr;
    }
    if(nullptr != m_pOutputStream)
    {
        delete m_pOutputStream;
        m_pOutputStream = nullptr;
    }
    if(nullptr != m_pInputStream)
    {
        delete m_pInputStream;
        m_pInputStream = nullptr;
    }
}

void RecordAdmin::setRecordAudioType(int audioType)
{
    switch (audioType)
    {
    case audioType::MIC:
        setMicAudioRecord(true);
        setSysAudioRecord(false);
        break;
    case audioType::SYS:
        setMicAudioRecord(false);
        setSysAudioRecord(true);
        break;
    case audioType::MIC_SYS:
        setMicAudioRecord(true);
        setSysAudioRecord(true);
        break;
    case audioType::NOS:
        setMicAudioRecord(false);
        setSysAudioRecord(false);
        break;
    default:{
        setMicAudioRecord(false);
        setSysAudioRecord(false);
    }
    }
}

void  RecordAdmin::setMicAudioRecord(bool bRecord)
{
    m_pInputStream->setMicAudioRecord(bRecord);
}

void  RecordAdmin::setSysAudioRecord(bool bRecord)
{
    m_pInputStream->setSysAudioRecord(bRecord);
}

void RecordAdmin::init(int screenWidth, int screenHeight)
{
    m_pInputStream->m_screenDW = screenWidth;
    m_pInputStream->m_screenDH = screenHeight;
    m_pInputStream->m_ipix_fmt = AV_PIX_FMT_RGB32;
    m_pInputStream->m_fps = m_fps;
    m_pOutputStream->m_left = m_pInputStream->m_left = m_x;
    m_pOutputStream->m_top = m_pInputStream->m_top = m_y;
    m_pOutputStream->m_right = m_pInputStream->m_right = screenWidth-m_x-m_selectWidth;
    m_pOutputStream->m_bottom = m_pInputStream->m_bottom = screenHeight-m_y-m_selectHeight;
    if(m_pInputStream->m_right<0){
        m_selectWidth += m_pInputStream->m_right;
        m_pInputStream->m_selectWidth = m_selectWidth;
        m_pInputStream->m_right = 0;
    }
    if(m_pInputStream->m_bottom<0){
        m_selectHeight += m_pInputStream->m_bottom;
        m_pInputStream->m_selectHeight = m_selectHeight;
        m_pInputStream->m_bottom = 0;
    }
    setRecordAudioType(m_audioType);
    pthread_create(&m_mainThread, nullptr, stream,static_cast<void*>(this));
    pthread_detach(m_mainThread);
}

int RecordAdmin::startStream()
{
    bool bRet;
    bRet = m_pInputStream->openInputStream();
    if(!bRet)
    {
        return 1;
    }
    int cx, cy, fps;
    AVPixelFormat pixel_fmt;
    if(m_pInputStream->GetVideoInputInfo(cx, cy, fps, pixel_fmt))
    {
        m_pOutputStream->SetVideoCodecProp(AV_CODEC_ID_H264, fps, 500000, 30, cx, cy);
    }
    int sample_rate = 0, channels = 0,layout;
    AVSampleFormat  sample_fmt;
    if(m_pInputStream->GetAudioInputInfo(sample_fmt, sample_rate, channels,layout))
    {
        m_pOutputStream->SetAudioCodecProp(AV_CODEC_ID_MP3, sample_rate, channels,layout, 32000);
    }
    if(m_pInputStream->GetAudioSCardInputInfo(sample_fmt, sample_rate, channels,layout))
    {
        m_pOutputStream->SetAudioCardCodecProp(AV_CODEC_ID_MP3, sample_rate, channels,layout, 32000);
    }
    bRet = m_pOutputStream->open(m_filePath);
    if(!bRet)
    {
        return 1;
    }
    m_writeFrameThread->setBWriteFrame(true);
    m_writeFrameThread->start();
    m_pInputStream->audioCapture();
    return 0;
}

void* RecordAdmin::stream(void* param)
{
    RecordAdmin *recordAdmin = static_cast<RecordAdmin*>(param);
    recordAdmin->startStream();
    return nullptr;
}

void RecordAdmin::stop()
{
    qContext->m_dateAdmin->setBGetFrame(false);
    m_writeFrameThread->setBWriteFrame(false);
    m_pInputStream->setbWriteAmix(false);
    m_pOutputStream->setIsWriteFrame(false);
    m_pInputStream->setbRunThread(false);
    m_pOutputStream->close();
}

