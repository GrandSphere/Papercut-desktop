#include "SingleInstance.h"

#include "core/AppPaths.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QTimer>

namespace papercut {

namespace {

constexpr int kConnectTimeoutMs = 400;
constexpr int kWriteTimeoutMs = 1500;
constexpr int kReadTimeoutMs = 2000;
constexpr qint64 kMaxRequestBytes = 64 * 1024;

} // namespace

SingleInstance::SingleInstance(QObject *parent) : QObject(parent) {
    qRegisterMetaType<Request>("papercut::SingleInstance::Request");
}

QString SingleInstance::channelName() {
    const QByteArray digest =
        QCryptographicHash::hash(AppPaths::root().toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return QStringLiteral("papercut-") + QString::fromLatin1(digest);
}

bool SingleInstance::forward(const Request &request) {
    QLocalSocket socket;
    socket.connectToServer(channelName());
    if (!socket.waitForConnected(kConnectTimeoutMs)) return false;

    QJsonObject obj;
    obj.insert(QStringLiteral("file"), request.file);
    obj.insert(QStringLiteral("page"), request.page);
    obj.insert(QStringLiteral("password"), request.password);
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    line.append('\n');
    socket.write(line);
    if (!socket.waitForBytesWritten(kWriteTimeoutMs)) return false;
    if (!socket.waitForReadyRead(kReadTimeoutMs)) return false;
    return socket.read(1) == "1";
}

bool SingleInstance::listen() {
    if (m_server) return true;
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    const QString name = channelName();
    if (!m_server->listen(name)) {
        QLocalServer::removeServer(name);
        if (!m_server->listen(name)) return false;
    }
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection() {
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(kReadTimeoutMs * 2, socket, [socket] { socket->disconnectFromServer(); });
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (socket->bytesAvailable() > kMaxRequestBytes) {
                socket->disconnectFromServer();
                return;
            }
            if (!socket->canReadLine()) return;
            const QJsonDocument doc = QJsonDocument::fromJson(socket->readLine());
            if (!doc.isObject()) {
                socket->write("0");
                socket->disconnectFromServer();
                return;
            }
            const QJsonObject obj = doc.object();
            Request request;
            request.file = obj.value(QStringLiteral("file")).toString();
            request.page = obj.value(QStringLiteral("page")).toInt();
            request.password = obj.value(QStringLiteral("password")).toString();
            socket->write("1");
            socket->flush();
            socket->disconnectFromServer();
            emit requestReceived(request);
        });
    }
}

} // namespace papercut
