#ifndef TCPDEVICE_H
#define TCPDEVICE_H
#include <QObject>
#include <QTcpServer>
#include "EnumsAndStructs.h"

#define AutoSaveFile "AutoSave.ini"

class QTcpSocket;

struct SettingsRec{
    char name[32];
    int size;
    int type; // 1-uChar 2-uInt32 3-uShort 4-Int32
};

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
    bool sendState(int ch=0);
    void makeCcsdsHeader(int bodySize, unsigned short procId=4);
    virtual void makeCcsdsState(int ch=0){};

private slots:
    void slNewConnection();
    void slServerRead();
    void slClientDisconnected();
};


class CBSU : public CTcpDevice
{
public:
    CBSU(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz){return true;}
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
