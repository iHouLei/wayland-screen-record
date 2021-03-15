#ifndef WRITEFRAMETHREAD_H
#define WRITEFRAMETHREAD_H
#include <QMutex>
#include <QObject>
#include <QThread>

class WriteFrameThread : public QThread
{
    Q_OBJECT

public:
    explicit WriteFrameThread(QObject *parent = nullptr);
    void run();

    bool bWriteFrame();
    void setBWriteFrame(bool bWriteFrame);

private:

    bool m_bWriteFrame;
    QMutex m_writeFrameMutex;
};

#endif // WRITEFRAMETHREAD_H
