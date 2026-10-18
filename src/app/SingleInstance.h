#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace papercut {

// Second launch forwards to primary instance.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    struct Request {
        QString file;
        int page = 0;
        QString password;
    };

    explicit SingleInstance(QObject *parent = nullptr);

    static QString channelName();
    static bool forward(const Request &request);
    bool listen();

signals:
    void requestReceived(const papercut::SingleInstance::Request &request);

private:
    void onNewConnection();

    QLocalServer *m_server = nullptr;
};

} // namespace papercut

Q_DECLARE_METATYPE(papercut::SingleInstance::Request)
