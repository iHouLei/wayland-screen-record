#include "waylandadmin.h"
#include "mainwindow.h"

#include <QThread>
#include <QDebug>

#include <sys/mman.h>
#include <unistd.h>

extern "C"
{
#include <libavutil/time.h>
}

WaylandAdmin::WaylandAdmin(QObject *parent) : QObject(parent)
{
}

void WaylandAdmin::init()
{
    m_thread = new QThread(this);
    m_connection = new KWayland::Client::ConnectionThread;
    connect(m_connection, &KWayland::Client::ConnectionThread::connected, this, &WaylandAdmin::onConnected);
    connect(m_connection, &KWayland::Client::ConnectionThread::connectionDied, this, &WaylandAdmin::onConnectionDied);
    connect(m_connection, &KWayland::Client::ConnectionThread::failed, this, &WaylandAdmin::onFailed);
    m_thread->start();
    m_connection->moveToThread(m_thread);
    m_connection->initConnection();
}

void WaylandAdmin::stop()
{
    if (m_connection) {
        disconnect(m_connection, &KWayland::Client::ConnectionThread::connected, this, &WaylandAdmin::onConnected);
        disconnect(m_connection, &KWayland::Client::ConnectionThread::connectionDied, this, &WaylandAdmin::onConnectionDied);
        disconnect(m_connection, &KWayland::Client::ConnectionThread::failed, this, &WaylandAdmin::onFailed);
    }

    if (m_registry) {
        disconnect(m_registry, &KWayland::Client::Registry::outputAnnounced, this, &WaylandAdmin::onOutputAnnounced);
        disconnect(m_registry, &KWayland::Client::Registry::outputRemoved, this, &WaylandAdmin::onOutputRemoved);
    }

    if (output) {
        disconnect(output, &KWayland::Client::Output::changed, this, &WaylandAdmin::onChanged);
    }

    if(m_rbuf){
        disconnect(m_rbuf, &KWayland::Client::RemoteBuffer::parametersObtained, this, &WaylandAdmin::onParametersObtained);
    }

    if (m_remoteAccessManager) {
        disconnect(m_remoteAccessManager, &KWayland::Client::RemoteAccessManager::bufferReady, this, &WaylandAdmin::onBufferReady);
        m_remoteAccessManager->release();
        m_remoteAccessManager->destroy();
    }
}

void WaylandAdmin::onConnected()
{
    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);
    m_registry = new KWayland::Client::Registry(this);
    connect(m_registry, &KWayland::Client::Registry::outputAnnounced, this, &WaylandAdmin::onOutputAnnounced);
    connect(m_registry, &KWayland::Client::Registry::outputRemoved, this, &WaylandAdmin::onOutputRemoved);
}

void WaylandAdmin::onConnectionDied()
{
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }

    m_connection->deleteLater();
    m_connection = nullptr;

    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(3000)) {
            m_thread->terminate();
            m_thread->wait();
        }
        delete m_thread;
        m_thread = nullptr;
    }
}

void WaylandAdmin::onFailed()
{
    m_thread->quit();
    m_thread->wait();
}

void WaylandAdmin::onOutputAnnounced(quint32 name, quint32 version)
{
    KWayland::Client::Output *output = new KWayland::Client::Output(this);
    output->setup(m_registry->bindOutput(name, version));
    connect(output, &KWayland::Client::Output::changed, this, &WaylandAdmin::onChanged);
}

void WaylandAdmin::onOutputRemoved(quint32 name)
{

}

void WaylandAdmin::onChanged()
{
    if (m_registry->hasInterface(KWayland::Client::Registry::Interface::RemoteAccessManager)) {
        KWayland::Client::Registry::AnnouncedInterface interface = m_registry->interface(KWayland::Client::Registry::Interface::RemoteAccessManager);
        if (!interface.name && !interface.version) {
            return;
        }
        m_remoteAccessManager = m_registry->createRemoteAccessManager(interface.name, interface.version);
        connect(m_remoteAccessManager, &KWayland::Client::RemoteAccessManager::bufferReady, this, &WaylandAdmin::onBufferReady);
        return;
    }
}

void WaylandAdmin::onBufferReady(const void *output, const KWayland::Client::RemoteBuffer *rbuf)
{
    Q_UNUSED(output);
    m_rbuf = rbuf;
    connect(m_rbuf, &KWayland::Client::RemoteBuffer::parametersObtained, this, &WaylandAdmin::onParametersObtained);
}

void WaylandAdmin::onParametersObtained()
{
    if (m_bInitRecordAdmin) {
        m_bInitRecordAdmin = false;
        frameStartTime = av_gettime();
    }
    setBInit(true);
    qint32  dma_fd = m_rbuf->fd();
    quint32 width = m_rbuf->width();
    quint32 height = m_rbuf->height();
    quint32 stride = m_rbuf->stride();
    unsigned char *mapData = static_cast<unsigned char *>(mmap(nullptr, stride * height, PROT_READ|PROT_WRITE, MAP_SHARED, dma_fd, 0));
    if (MAP_FAILED == mapData) {
        qDebug() << "mmap failed!dma fd:" << dma_fd;
    }
    qDebug() << Q_FUNC_INFO << dma_fd << width << height << stride;
    av_gettime();
    qContext->m_dateAdmin->appendBuffer(mapData, static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride), av_gettime() - frameStartTime);
    munmap(mapData, stride * height);
    close(dma_fd);
}

bool WaylandAdmin::bInit() const
{
    return m_bInit;
}

void WaylandAdmin::setBInit(bool bInit)
{
    m_bInit = bInit;
}


