#ifndef TCPDEVICE_H
#define TCPDEVICE_H

class CTcpDevice
{

public:
    CTcpDevice(const QString& name, unsigned int id, const QHostAddress& addr);
    const QHostAddress& getIP();
    bool startListen();

protected:
    QHostAddress m_hostAddr;
    QTcpServer  m_server;
    QString     m_name;
    unsigned int m_id;
};


class CMicTM : public CTcpDevice
{
public:
    CMicTM(const QHostAddress& addr);
    void addChannel(unsigned int id, QString name);
};

class CME427 : public CTcpDevice
{
public:
    CME427(const QString& name, unsigned int id, const QHostAddress& addr);
};

class CME725 : public CTcpDevice
{
public:
    CME725(const QString& name, unsigned int id, const QHostAddress& addr);
};

class CBSU : public CTcpDevice
{
public:
    CBSU(const QString& name, unsigned int id, const QHostAddress& addr);
};

class CME719 : public CTcpDevice
{
public:
    CME719(const QString& name, unsigned int id, const QHostAddress& addr);
};

class CLVS : public CTcpDevice
{
public:
    CLVS(const QString& name, unsigned int id, const QHostAddress& addr);
};

class CRodos : public CTcpDevice
{
public:
    CRodos(const QString& name, unsigned int id, const QHostAddress& addr);
};

#endif // TCPDEVICE_H
