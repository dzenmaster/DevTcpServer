#ifndef TCPDEVME725_H
#define TCPDEVME725_H

#include "TcpDevice.h"

class CME725 : public CTcpDevice
{
public:
    CME725(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz);
    void makeCcsdsState(int ch=0);

private:
    int m_size;
    int m_matrix[64];
};

#endif // TCPDEVME725_H
