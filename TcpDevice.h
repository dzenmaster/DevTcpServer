#ifndef TCPDEVICE_H
#define TCPDEVICE_H
#include <QObject>
#include <QTcpServer>
#include "EnumsAndStructs.h"

#define AutoSaveFile "AutoSave.ini"

class QTcpSocket;

class CTcpDevice : public QObject
{
    Q_OBJECT

public:
    CTcpDevice(const QString& name, unsigned int id, const QHostAddress& addr);
    virtual ~CTcpDevice(){};

    const QHostAddress& getIP();
    bool startListen();

protected:
    QHostAddress m_hostAddr;
    QTcpServer  m_server;
    QTcpSocket* m_socket;
    QString     m_name;
    unsigned int m_id;
    QSet<CCSDSID> m_allowIDs;
    unsigned char   m_ccsdsBody[1600];
    int             m_ccsdsSize;
    int             m_ccsdsCounter;

    virtual bool processControlPacket(const unsigned char* cd, int sz)=0;
    virtual bool sendState() { return true; };
    void makeCcsdsHeader(int bodySize, unsigned short procId=4);
    void makeCcsdsState(){};

private slots:
    void slNewConnection();
    void slServerRead();
    void slClientDisconnected();
};


class CMicTM : public CTcpDevice
{
public:
    CMicTM(const QHostAddress& addr);
    void addChannel(unsigned int id, QString name);

protected:
    bool processControlPacket(const unsigned char* cd, int sz);

private:
    int m_currChannel;
};

class CME427 : public CTcpDevice
{
public:
    CME427(const QString& name, unsigned int id, const QHostAddress& addr);

protected:    
    bool processControlPacket(const unsigned char* cd, int sz);
    bool sendState();
    void makeCcsdsState();

private:
    int m_size;
    int m_matrix[64];
};

class CME725 : public CTcpDevice
{
public:
    CME725(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz){return true;}
};

class CBSU : public CTcpDevice
{
public:
    CBSU(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz){return true;}
};

class CME719 : public CTcpDevice
{
public:
    CME719(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz);
};

class CLVS : public CTcpDevice
{
public:
    CLVS(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz){return true;}
};

class CRodos : public CTcpDevice
{
public:
    CRodos(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
  bool processControlPacket(const unsigned char* cd, int sz){return true;}
};

#endif // TCPDEVICE_H
