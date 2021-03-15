#include "writeframethread.h"
#include "recordadmin.h"
#include "mainwindow.h"

WriteFrameThread::WriteFrameThread(QObject *parent) :
    QThread(parent),
    m_bWriteFrame(false)
{
}

void WriteFrameThread::run()
{
    DateAdmin::waylandFrame frame;
    while (bWriteFrame())
    {
        if (qContext->m_dateAdmin->getFrame(frame)) {
            qContext->m_recordAdmin->m_pOutputStream->writeVideoFrame(frame);
        }
    }
}

bool WriteFrameThread::bWriteFrame()
{
    QMutexLocker locker(&m_writeFrameMutex);
    return m_bWriteFrame;
}

void WriteFrameThread::setBWriteFrame(bool bWriteFrame)
{
    QMutexLocker locker(&m_writeFrameMutex);
    m_bWriteFrame = bWriteFrame;
}
