#include "meteosocket.h"


MeteoServer::MeteoServer(const quint16 moxaPort, const quint16 meteoKitPort, QObject *parent)
    : QObject(parent)
    , m_moxaPort(moxaPort)
    , m_meteoKitPort(meteoKitPort)
{
    CreateObjects();
    InitObjects();
    ConnectObjects();
}

MeteoServer::~MeteoServer()
{
    OnDisconnectedFromHost();
    delete m_noAnswerTimer;
    delete m_socketMeteo;
    delete m_serverMeteo;
}

void MeteoServer::CreateObjects()
{
    m_serverMeteo = new QTcpServer(this);
    m_socketMeteo = new QTcpSocket(this);
    m_noAnswerTimer = new QTimer(this);
}

void MeteoServer::InitObjects()
{
    m_serverMeteo->listen(QHostAddress::Any, m_moxaPort);
    m_noAnswerTimer->setInterval(std::chrono::seconds(2));
    m_noAnswerTimer->setTimerType(Qt::VeryCoarseTimer);
    m_noAnswerTimer->setSingleShot(true);
}

void MeteoServer::ConnectObjects()
{
    connect(m_serverMeteo, &QTcpServer::newConnection, this, &MeteoServer::OnNewSocketConnected);
    connect(m_noAnswerTimer, &QTimer::timeout, this, &MeteoServer::ToNoAnswerGet);
}

void MeteoServer::OnNewSocketConnected()
{
    QTcpSocket *socket = m_serverMeteo->nextPendingConnection();
    qDebug() << "MS: новый сокет подключен, адрес: " << socket->peerAddress() << " порт: " << socket->peerPort();

    if(socket->peerPort() == m_meteoKitPort)
    {
        qDebug() << "MS: Подключен!";
        m_socketMeteo = socket;
        ConnectMeteoSocketObject();
    }
    else
    {
        qDebug() << "MS: порт неправильный, слушаем порт: "<< m_meteoKitPort << " а имеем: " << socket->peerPort();
    }
}

void MeteoServer::OnReadyRead()
{
    StopNoAnswerTimer();
    qDebug() << "MS: Получено сообщение";
    m_readyReadBuffer.append(m_socketMeteo->readAll());
    if(m_readyReadBuffer.length() >= m_returnedMessageSize) // длина ответа 11 байт
    {
        QByteArray message = m_readyReadBuffer.mid(0, m_returnedMessageSize);
        m_readyReadBuffer.remove(0, m_returnedMessageSize);

        Q_EMIT ToGetStateFromMessage(message);
    }
}

void MeteoServer::OnDisconnectedFromHost()
{
    m_socketMeteo->disconnectFromHost();
    qDebug()<< "MS: Отключен...";
}

void MeteoServer::OnErrorOccurred(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::ConnectionRefusedError:
        qDebug()<< QStringLiteral("Истекло время ожидания");
        break;
    case QAbstractSocket::RemoteHostClosedError:
        qDebug()<< QStringLiteral("Удаленный хост закрыл соединение");
        break;
    case QAbstractSocket::SocketAccessError:
        qDebug()<< QStringLiteral("Адрес хоста не найден");
        break;
    case QAbstractSocket::SocketResourceError:
        qDebug()<< QStringLiteral("Приложению не хватало необходимых прав");
        break;
    case QAbstractSocket::SocketTimeoutError:
        qDebug()<< QStringLiteral("Слишком много сокетов");
        break;
    case QAbstractSocket::DatagramTooLargeError:
        qDebug()<< QStringLiteral("Размер дейтаграммы превышал предел операционной системы");
        break;
    case QAbstractSocket::NetworkError:
        qDebug()<< QStringLiteral("Произошла ошибка сети (например, сетевой кабель был случайно отключен)");
        break;
    case QAbstractSocket::AddressInUseError:
        qDebug()<< QStringLiteral("Слишком много сокетов");
        break;
    case QAbstractSocket::SocketAddressNotAvailableError:
        qDebug()<< QStringLiteral("Адрес, уже используется в каком то соединении");
        break;
    case QAbstractSocket::UnsupportedSocketOperationError:
        qDebug()<< QStringLiteral("Адрес не принадлежит хосту");
        break;
    case QAbstractSocket::UnfinishedSocketOperationError:
        qDebug()<< QStringLiteral("Запрошенная операция сокета не поддерживается локальной операционной системой");
        break;
    case QAbstractSocket::ProxyAuthenticationRequiredError:
        qDebug()<< QStringLiteral("Подтверждение связи SSL / TLS не удалось, поэтому соединение было закрыто ");
        break;
    case QAbstractSocket::SslHandshakeFailedError:
        qDebug()<< QStringLiteral("Последняя попытка операции еще не завершена");
        break;
    case QAbstractSocket::ProxyConnectionRefusedError:
        qDebug()<< QStringLiteral("Не удалось связаться с прокси-сервером, потому что соединение с этим сервером было отказано");
        break;
    case QAbstractSocket::ProxyConnectionClosedError:
        qDebug()<< QStringLiteral("Соединение с прокси-сервером было неожиданно закрыто");
        break;
    case QAbstractSocket::ProxyConnectionTimeoutError:
        qDebug()<< QStringLiteral("Время ожидания подключения к прокси-серверу истекло или прокси-сервер перестал отвечать на этапе проверки подлинности.");
        break;
    case QAbstractSocket::ProxyNotFoundError:
        qDebug()<< QStringLiteral("Адрес прокси, заданный с помощью setProxy () (или прокси приложения), не найден.");
        break;
    case QAbstractSocket::ProxyProtocolError:
        qDebug()<< QStringLiteral("Согласование соединения с прокси-сервером не удалось, потому что ответ прокси-сервера не был понят.");
        break;
    case QAbstractSocket::OperationError:
        qDebug()<< QStringLiteral("Была предпринята попытка выполнения операции, когда сокет находился в недопустимом состоянии.");
        break;
    case QAbstractSocket::SslInternalError:
        qDebug()<< QStringLiteral("Используемая библиотека SSL сообщила о внутренней ошибке.");
        break;
    case QAbstractSocket::SslInvalidUserDataError:
        qDebug()<< QStringLiteral("Были предоставлены неверные данные (сертификат, ключ, шифр и т. Д.), И их использование привело к ошибке в библиотеке SSL.");
        break;
    case QAbstractSocket::TemporaryError:
        qDebug()<< QStringLiteral("Произошла временная ошибка (например, операция будет заблокирована, а сокет не блокируется).");
        break;
    case QAbstractSocket::UnknownSocketError:
        qDebug()<< QStringLiteral("Произошла неопознанная ошибка.");
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug()<< QStringLiteral("Хост не найден");
        break;
    }
}

void MeteoServer::SendMessage(const QByteArray &array)
{
    if (m_socketMeteo->state()==QAbstractSocket::ConnectedState)
    {
        m_socketMeteo->write(array);
        m_socketMeteo->flush();
        m_noAnswerTimer->start();
    }
    else
    {
        qDebug() << "MS:SendMessage нет подключения при попытке отправить сообщение";
    }
}

bool MeteoServer::IsMeteoConnected()
{
    qDebug()<< QString::fromUtf8("MS::Метео");
    return m_socketMeteo->state()==QAbstractSocket::ConnectedState;
}

void MeteoServer::ConnectMeteoSocketObject()
{
    qDebug()<< "MS::ConnecMeteoSocketObject";
    connect(m_noAnswerTimer, &QTimer::timeout, this, &MeteoServer::ToNoAnswerGet);
    connect(m_socketMeteo, &QTcpSocket::connected, this, &MeteoServer::OnNewSocketConnected);
    connect(m_socketMeteo, &QTcpSocket::readyRead, this, &MeteoServer::OnReadyRead);
    connect(m_socketMeteo, &QTcpSocket::disconnected, this, &MeteoServer::OnDisconnectedFromHost);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(m_socketMeteo, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &MeteoServer::OnErrorOccurred);
#else
    connect(m_socketMeteo, &QTcpSocket::errorOccurred, this, &MeteoServer::OnErrorOccurred);
#endif
}

void MeteoServer::StopNoAnswerTimer()
{
    qDebug()<< "MS::StopNoAnswerTimer";
    if (m_noAnswerTimer->isActive())
    {
        m_noAnswerTimer->stop();
    }
}

