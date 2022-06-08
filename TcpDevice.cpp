#include <QTcpSocket>
#include <QSettings>
//#include <QMutex>
#include "TcpDevice.h"
#include "ini/FastIni.h"

extern CIniFile g_conf;

//QMutex G_mutex;


QMap<CCSDSID,SettingsRec> GSRecs{
    { CCSDSID::CHANNUM, SettingsRec{"Channel", 1,1}},
    { CCSDSID::BRTS, SettingsRec{"BRTS", 1,1}},
    { CCSDSID::INF, SettingsRec{"INF", 4,2}},
    { CCSDSID::FREQ, SettingsRec{"FREQ", 4,2}},
    { CCSDSID::NCLIENTS, SettingsRec{"NClients", 1,1}},
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
: m_name(name),
  m_id(id),
  m_hostAddr(addr),
  m_ccsdsSize(0),
  m_ccsdsCounter(0)
{
     connect(&m_server, SIGNAL(newConnection()), SLOT(slNewConnection()));
}

void CTcpDevice::slNewConnection()
{
 //   G_mutex.lock();
    m_socket = m_server.nextPendingConnection(); 
    printf("NewConnection IP %s : %d\n", m_socket->localAddress().toString().toLocal8Bit().data(), m_socket->localPort());
    connect(m_socket, SIGNAL(readyRead()),SLOT(slServerRead()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(slClientDisconnected()));
 //   G_mutex.unlock();
}

void CTcpDevice::slServerRead()
{
   // G_mutex.lock();
    while(m_socket->bytesAvailable()>0)
    {
        QByteArray array = m_socket->readAll();
        printf("\n\n");
        printf("from peer IP %s : %d\n", m_socket->peerAddress().toString().toLocal8Bit().data(), m_socket->peerPort());
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
        int dataSz = (cd[4]&0x3F)*256+cd[5];
        int procID = (cd[0]&7)*256+cd[1];
        printf("packet type=%d second header=%d id=%d n=%d dataSz=%d\n",
               (cd[0]>>4)&1,(cd[0]>>3)&1,procID,(cd[2]&0x3F)*256+cd[3], dataSz   );
        if (dataSz+6!=sz){
            printf("incorrect sz=%d, zsData=%d\n", sz, dataSz);
           // G_mutex.unlock();
            return;
        }
        if (procID==4){
            bool res = processControlPacket(&cd[6], dataSz - 2);
            printf("Content of control packet is %s", res ? "correct" : "incorrect" );
        }
        else if (procID==5){
            int ch = (cd[7]==1)?1:0;
            bool res = sendState(ch);
            printf("State was sended res=%d", res );
        }
    }
  //  G_mutex.unlock();
}

void CTcpDevice::slClientDisconnected()
{
  //  G_mutex.lock();
    //printf("slClientDisconnected peer IP %s : %d\n", m_socket->peerAddress().toString().toLocal8Bit().data(), m_socket->peerPort());
    m_socket->disconnect();
    m_socket->close();
 //   G_mutex.unlock();
}
const QHostAddress& CTcpDevice::getIP()
{
   return m_hostAddr;
}

bool CTcpDevice::startListen()
{
  //  G_mutex.lock();
    QTextStream cout(stdout);
    cout.setCodec("CP866");
    cout << "TCP Device " << m_name << " ";
    cout.flush();

    bool res = m_server.listen(m_hostAddr, 4000);
    if (res)
        printf("listen success %s\n", m_hostAddr.toString().toLocal8Bit().constData());
    else
        printf("listen bad %s\n", m_hostAddr.toString().toLocal8Bit().constData());
 //   G_mutex.unlock();
    return res;    
}

bool CTcpDevice::sendState(int ch)
{
    if (m_socket){
        makeCcsdsState(ch);
        qint64 res =  m_socket->write((char*)m_ccsdsBody, m_ccsdsSize);
        if (m_socket->waitForBytesWritten(3000)){
            printf("answer is sended %d\n", res);
            //printf("CME427::sendState IP %s : %d\n", m_socket->localAddress().toString().toLocal8Bit().data(), m_socket->localPort());
        }
    }
    return true;
}

void CTcpDevice::makeCcsdsHeader(int bodySize, unsigned short procId)
{
    //mainHeader
    m_ccsdsBody[0] = 0;
    m_ccsdsBody[1] = procId&0xFF;//conf device
    m_ccsdsBody[2] = 0xC0 + ((m_ccsdsCounter>>8)&0x3F);
    m_ccsdsBody[3] = (m_ccsdsCounter&0xFF);
    unsigned short dataSz = bodySize+2;
    m_ccsdsBody[4] = dataSz>>8;
    m_ccsdsBody[5] = dataSz&0xFF;
    //body
    //already filled

    //crc16
    unsigned short rCrc = crc16( m_ccsdsBody, 6 + dataSz - 2 );
    m_ccsdsBody[6 + dataSz - 2] = (rCrc>>8)&0xFF;
    m_ccsdsBody[6 + dataSz - 1] = rCrc&0xFF;
    m_ccsdsSize = 6 + dataSz;
    ++m_ccsdsCounter;
}


CBSU::CBSU(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}


CLVS::CLVS(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}


CRodos::CRodos(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}
