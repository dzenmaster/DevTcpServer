#ifndef TCPDEVME427_H
#define TCPDEVME427_H

#include "TcpDevice.h"

class CME427 : public CTcpDevice
{
public:
    CME427(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz);
    void makeCcsdsState(int ch=0);

private:
    int m_size;
    int m_matrix[64];
};

#endif // TCPDEVME427_H
