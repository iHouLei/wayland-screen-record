#ifndef WAYLANDADMIN_H
#define WAYLANDADMIN_H

#include <QObject>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/remote_access.h>

class WaylandAdmin : public QObject
{
    Q_OBJECT
public:
    explicit WaylandAdmin(QObject *parent = nullptr);
    void init();
    void stop();
    bool bInit() const;
    void setBInit(bool bInit);

signals:

protected slots:
    void onConnected();
    void onConnectionDied();
    void onFailed();
    void onOutputAnnounced(quint32 name, quint32 version);
    void onOutputRemoved(quint32 name);
    void onChanged();
    void onBufferReady(const void* output, const KWayland::Client::RemoteBuffer *rbuf);
    void onParametersObtained();

private:
    QThread *m_thread;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Registry *m_registry = nullptr;
    KWayland::Client::Output *output = nullptr;
    KWayland::Client::RemoteAccessManager *m_remoteAccessManager = nullptr;
    const KWayland::Client::RemoteBuffer *m_rbuf = nullptr;
    bool m_bInit;
    int64_t frameStartTime;
    bool m_bInitRecordAdmin;
};

#endif // WAYLANDADMIN_H
