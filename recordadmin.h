#ifndef RECORDADMIN_H
#define RECORDADMIN_H
#include "avinputstream.h"
#include "avoutputstream.h"
#include "writeframethread.h"
#include <sys/time.h>
#include <map>
#include <qimage.h>
#include "writeframethread.h"
#define AUDIO_INPUT_DEVICE    "hw:0,0"
#define VIDEO_INPUT_DEVICE    "/dev/video0"
#include <QThread>

extern "C"
{
#include <libavdevice/avdevice.h>
}

using namespace std;
class RecordAdmin :public QObject
{
    Q_OBJECT

public:

    enum audioType
    {
        MIC = 2,
        SYS,
        MIC_SYS,
        NOS
    };

    enum videoType
    {
        GIF = 1,
        MP4,
        MKV
    };

    RecordAdmin(QStringList list,QObject *parent = nullptr);
    virtual ~RecordAdmin();

public:

    void init(int screenWidth, int screenHeight);
    void stop();

protected:
    void  setRecordAudioType(int audioType);
    void  setMicAudioRecord(bool bRecord);
    void  setSysAudioRecord(bool bRecord);
    int   startStream();
    static void* stream(void* param);

public:
    AVInputStream                           *m_pInputStream;
    AVOutputStream                          *m_pOutputStream;
    WriteFrameThread                         *m_writeFrameThread;

private:
    QList<QString> argvList;
    QString m_filePath;
    int m_fps;
    int m_audioType;
    int  m_videoType;
    int m_x;
    int m_y;
    int m_selectWidth;
    int m_selectHeight;
    pthread_t  m_mainThread;
    int m_delay;
    QMutex m_oldFrameMutex;
    int m_gifBuffersize;
};

#endif // RECORDADMIN_H
