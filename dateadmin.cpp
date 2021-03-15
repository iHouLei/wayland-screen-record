#include "dateadmin.h"

#include <QMutexLocker>

int DateAdmin::DateAdmin::frameIndex = 0;

DateAdmin::DateAdmin(QObject *parent) : QObject(parent)
{

}

void DateAdmin::appendBuffer(unsigned char *frame, int width, int height, int stride, int64_t time)
{
    if(!bGetFrame()){
        return;
    }
    int size = height*stride;
    unsigned char *ch = nullptr;
    if(m_bInit)
    {
        m_bInit = false;
        m_width = width;
        m_height = height;
        m_stride = stride;
        m_ffmFrame = new unsigned char[static_cast<unsigned long>(size)];
        for (int i=0;i< m_bufferSize;i++)
        {
            ch = new unsigned char[static_cast<unsigned long>(size)];
            m_freeList.append(ch);
        }
    }
    QMutexLocker locker(&m_mutex);
    if(m_waylandList.size() >= m_bufferSize){
        waylandFrame wFrame = m_waylandList.first();
        memset(wFrame._frame,0,static_cast<size_t>(size));
        memcpy(wFrame._frame,frame,static_cast<size_t>(size));
        wFrame._time = time;
        wFrame._width = width;
        wFrame._height = height;
        wFrame._stride = stride;
        m_waylandList.removeFirst();
        m_waylandList.append(wFrame);
    }
    else if(0 <= m_waylandList.size() &&  m_waylandList.size() < m_bufferSize)
    {
        if(m_freeList.size()>0)
        {
            waylandFrame wFrame;
            wFrame._time = time;
            wFrame._width = width;
            wFrame._height = height;
            wFrame._stride = stride;
            wFrame._frame = m_freeList.first();
            memset(wFrame._frame,0,static_cast<size_t>(size));
            memcpy(wFrame._frame,frame,static_cast<size_t>(size));
            m_waylandList.append(wFrame);
            m_freeList.removeFirst();
        }
    }
}

bool DateAdmin::getFrame(DateAdmin::waylandFrame &frame)
{
    QMutexLocker locker(&m_mutex);
    if (m_waylandList.size() <= 0 || nullptr == m_ffmFrame) {
        frame._width = 0;
        frame._height = 0;
        frame._stride = 0;
        frame._time =0;
        frame._frame = nullptr;
        frame._index = 0;
        return false;
    } else {
        int size = m_height*m_stride;
        waylandFrame wFrame = m_waylandList.first();
        frame._width = wFrame._width;
        frame._height = wFrame._height;
        frame._stride = wFrame._stride;
        frame._time = wFrame._time;
        frame._frame = m_ffmFrame;
        frame._index = frameIndex++;
        memcpy(frame._frame,wFrame._frame,static_cast<size_t>(size));
        m_waylandList.removeFirst();
        m_freeList.append(wFrame._frame);
        return true;
    }
}

bool DateAdmin::bGetFrame()
{
    QMutexLocker locker(&m_bGetFrameMutex);
    return m_bGetFrame;
}

void DateAdmin::setBGetFrame(bool bGetFrame)
{
    QMutexLocker locker(&m_bGetFrameMutex);
    m_bGetFrame = bGetFrame;
}
