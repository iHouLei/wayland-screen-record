#ifndef DATEADMIN_H
#define DATEADMIN_H

#include <QMutexLocker>
#include <QObject>

class DateAdmin : public QObject
{
    Q_OBJECT
public:

    struct waylandFrame {
        int64_t _time;
        int _index;
        int _width;
        int _height;
        int _stride;
        unsigned char *_frame;
    };

    explicit DateAdmin(QObject *parent = nullptr);
    void appendBuffer(unsigned char *frame, int width, int height, int stride, int64_t time);
    bool getFrame(waylandFrame &frame);
    bool bGetFrame();
    void setBGetFrame(bool bGetFrame);

signals:

public slots:

private:
    bool m_bInit;
    int m_width;
    int m_height;
    int m_stride;
    unsigned char *m_ffmFrame;
    int m_bufferSize;
    QList<unsigned char *> m_freeList;
    QMutex m_mutex;
    QList<waylandFrame> m_waylandList;
    QMutex m_bGetFrameMutex;
    bool m_bGetFrame;
    static int frameIndex;
};

#endif // DATEADMIN_H
