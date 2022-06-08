#ifndef TCPDEVMICTM_H
#define TCPDEVMICTM_H

#include "TcpDevice.h"

class CMicTM : public CTcpDevice
{
public:
    CMicTM(const QHostAddress& addr);
    void addChannel(unsigned int id, QString name);

protected:
    bool processControlPacket(const unsigned char* cd, int sz);
    void makeCcsdsState(int ch=0);

private:
    int m_currChannel;
    unsigned int m_IDs[2];
    int m_brts[2];
    int m_inf[2];
    int m_kadrLen[2];
    unsigned int m_freq[2];
    QVector<QPair<QString,unsigned short>> m_clients[2];
};

#endif // TCPDEVMICTM_H
