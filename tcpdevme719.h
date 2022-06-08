#ifndef TCPDEVME719_H
#define TCPDEVME719_H

#include "TcpDevice.h"

class CME719 : public CTcpDevice
{
public:
    CME719(const QString& name, unsigned int id, const QHostAddress& addr);

protected:
    bool processControlPacket(const unsigned char* cd, int sz);
    void makeCcsdsState(int ch=0);

private:
    unsigned char m_brts;
    unsigned char m_inv;
    unsigned char m_diff;
    unsigned char m_code01;
    unsigned int m_inf;
    unsigned int m_kadrLen;
    unsigned int m_freq;
    int m_power;
    int m_freqShift;
};
#endif // TCPDEVME719_H
