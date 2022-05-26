#include <QTcpSocket>
#include "TcpDevice.h"
#include "EnumsAndStructs.h"

struct SettingsRec{
    char name[32];
    int size;
    int type;//1-uChar 2-uInt32 3-uShort 4-Int32
};

static QMap<CCSDSID,SettingsRec> GSRecs{
    { CCSDSID::CHANNUM, SettingsRec{"Channel", 1,1}},
    { CCSDSID::BRTS, SettingsRec{"BRTS", 1,1}},
    { CCSDSID::INF, SettingsRec{"INF", 4,2}},
    { CCSDSID::FREQ, SettingsRec{"FREQ", 4,2}},
    { CCSDSID::NCIENTS, SettingsRec{"NClients", 1,1}},
    { CCSDSID::COMCHANNELS, SettingsRec{"COM Channels", 1,1}},
    { CCSDSID::KADRLENGTH, SettingsRec{"KadrLen", 4,2}},
    { CCSDSID::POWER, SettingsRec{"Power", 4,4}},
    { CCSDSID::FREQSHIFT, SettingsRec{"FreqShift", 4,4}},
    { CCSDSID::INVERSION, SettingsRec{"Inversion", 1,1}},
    { CCSDSID::DIFFER, SettingsRec{"Diff", 1,1}},
    { CCSDSID::CODE01, SettingsRec{"Code0-1", 1,1}}
};

unsigned short crc16(const unsigned char *pcBlock, unsigned len )
{
    unsigned short crc = 0xFFFF;
    unsigned char i;
    while( len-- ) {
        crc ^= ((unsigned short)(*pcBlock)) << 8;
        pcBlock++;
        for( i = 0; i < 8; i++ )
            crc = crc & 0x8000 ? ( crc << 1 ) ^ 0x1021 : crc << 1;
    }
    unsigned char* bptr = (unsigned char*)&crc;
    return (bptr[0] << 8) | bptr[1];
}

CTcpDevice::CTcpDevice(const QString& name, unsigned int id, const QHostAddress& addr)
: m_name(name), m_id(id), m_hostAddr(addr)
{
     connect(&m_server, SIGNAL(newConnection()), SLOT(slNewConnection()));
}

void CTcpDevice::slNewConnection()
{
    m_socket = m_server.nextPendingConnection();

 //printf("Hello, World!!! I am echo server!\r\n");

    connect(m_socket, SIGNAL(readyRead()),SLOT(slServerRead()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(slClientDisconnected()));
}

void CTcpDevice::slServerRead()
{
  //   printf("RR\n");
    while(m_socket->bytesAvailable()>0)
    {
        QByteArray array = m_socket->readAll();
        printf("\n");
        printf("IP %s : ", m_hostAddr.toString().toLocal8Bit().data());
        //printf(array.data());
        int sz = array.size();
        if (sz<3){
            printf("Size err %d", sz);
            return;
        }
        const unsigned char* cd = (const unsigned char*)array.data();
        unsigned short tCrc = crc16(cd, sz-2);
        unsigned short gotCrc = cd[sz-2]*256 + cd[sz-1];
        bool crcCorrect = crc16(cd, sz-2);
        printf("got %d bytes, crc %s\n", sz, gotCrc==tCrc?"correct":"incorrect");
        printf("packet type=%d second header=%d id=%d n=%d dataSz=%d\n",
               (cd[0]>>4)&1,(cd[0]>>3)&1,(cd[0]&7)*256+cd[1],(cd[2]&0x3F)*256+cd[3],(cd[4]&0x3F)*256+cd[5]   );
        int pos = 6;
        while(pos<sz-2){
            CCSDSID id=(CCSDSID)cd[pos];
            if (!GSRecs.contains(id)){
                printf("unknown key %d\n", cd[pos]);
                break;
            }
            if (GSRecs[id].type==1){
                int val = cd[pos+1];
                printf("%s %d\n",GSRecs[id].name, val);
                if (id==CCSDSID::NCIENTS){
                    for ( int i = 0; i < val; ++i){
                       printf("ip=%d.%d.%d.%d : %d\n",cd[pos+2],cd[pos+3],cd[pos+4],cd[pos+5], cd[pos+6]*256+cd[pos+7]);
                       pos+=6;
                    }

                }
                if (id==CCSDSID::COMCHANNELS){
                    for ( int i = 0; i < val; ++i){
                       printf("in = %d -> out = %d\n",cd[pos+2+i]+1,i+1);
                    }
                    break;
                }
            }
            else if (GSRecs[id].type==2)
                printf("%s %d\n",GSRecs[id].name,
                       (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                      +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4]))
                        );
            else if (GSRecs[id].type==4)
                printf("%s %d\n",GSRecs[id].name,
                      (int)( (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                      +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4])))
                        );
            pos += (GSRecs[id].size + 1);

        }

        //mTcpSocket->write(array);
    }
}

void CTcpDevice::slClientDisconnected()
{
    //printf("D\n");
    m_socket->close();
}
const QHostAddress& CTcpDevice::getIP()
{
   return m_hostAddr;
}

bool CTcpDevice::startListen()
{
    QTextStream cout(stdout);
    cout.setCodec("CP866");
    cout << "TCP Device " << m_name << " ";
    cout.flush();

    bool res = m_server.listen(m_hostAddr, 4000);
    if (res)
        printf("listen success %s\n", m_hostAddr.toString().toLocal8Bit().constData());
    else
        printf("listen bad %s\n", m_hostAddr.toString().toLocal8Bit().constData());
    return res;
}

CMicTM::CMicTM(const QHostAddress& addr)
: CTcpDevice("", 0, addr)
{}

void CMicTM::addChannel(unsigned int id, QString name)
{
    m_name += name + " ";
}

CME427::CME427(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CME725::CME725(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CBSU::CBSU(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CME719::CME719(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CLVS::CLVS(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CRodos::CRodos(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}
