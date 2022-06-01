#include <QTcpSocket>
#include <QSettings>
#include <QMutex>
#include "TcpDevice.h"
#include "ini/FastIni.h"

extern CIniFile g_conf;

QMutex G_mutex;

struct SettingsRec{
    char name[32];
    int size;
    int type; // 1-uChar 2-uInt32 3-uShort 4-Int32
};

static QMap<CCSDSID,SettingsRec> GSRecs{    
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
    G_mutex.lock();
    m_socket = m_server.nextPendingConnection(); 
    //printf("slNewConnection peer IP %s : %d\n", m_socket->peerAddress().toString().toLocal8Bit().data(), m_socket->peerPort());
    connect(m_socket, SIGNAL(readyRead()),SLOT(slServerRead()));
    connect(m_socket, SIGNAL(disconnected()), SLOT(slClientDisconnected()));
    G_mutex.unlock();
}

void CTcpDevice::slServerRead()
{
    G_mutex.lock();
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
            G_mutex.unlock();
            return;
        }
        if (procID==4){
            bool res = processControlPacket(&cd[6], dataSz - 2);
            printf("Content of control packet is %s", res ? "correct" : "incorrect" );
        }
        else if (procID==5){
            bool res = sendState();
            printf("State was sended res=%d", res );
        }
    }
    G_mutex.unlock();
}

void CTcpDevice::slClientDisconnected()
{
    G_mutex.lock();
    //printf("slClientDisconnected peer IP %s : %d\n", m_socket->peerAddress().toString().toLocal8Bit().data(), m_socket->peerPort());
    m_socket->disconnect();
    m_socket->close();
    G_mutex.unlock();
}
const QHostAddress& CTcpDevice::getIP()
{
   return m_hostAddr;
}

bool CTcpDevice::startListen()
{
    G_mutex.lock();
    QTextStream cout(stdout);
    cout.setCodec("CP866");
    cout << "TCP Device " << m_name << " ";
    cout.flush();

    bool res = m_server.listen(m_hostAddr, 4000);
    if (res)
        printf("listen success %s\n", m_hostAddr.toString().toLocal8Bit().constData());
    else
        printf("listen bad %s\n", m_hostAddr.toString().toLocal8Bit().constData());
    G_mutex.unlock();
    return res;    
}

CMicTM::CMicTM(const QHostAddress& addr)
: CTcpDevice("", 0, addr)
{
    m_allowIDs << CHANNUM << BRTS << INF << FREQ << NCLIENTS << KADRLENGTH;
    m_currChannel = 0;
}

void CMicTM::addChannel(unsigned int id, QString name)
{
    m_name += name + " ";
}

bool CMicTM::processControlPacket(const unsigned char* cd, int sz)
{
    int pos = 0;
    while(pos < sz) {
        CCSDSID id=(CCSDSID)cd[pos];
        if (!m_allowIDs.contains(id)){
            printf("unknown key %d\n", cd[pos]);
            return false;
        }
        if (GSRecs[id].type==1){
            int val = cd[pos+1];
            printf("%s %d\n",GSRecs[id].name, val);
            if (id==CHANNUM)
                m_currChannel = val;
            if (id==NCLIENTS){
                for ( int i = 0; i < val; ++i){
                   printf("ip=%d.%d.%d.%d : %d\n",cd[pos+2],cd[pos+3],cd[pos+4],cd[pos+5], cd[pos+6]*256+cd[pos+7]);
                   pos+=6;
                }
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

CME427::CME427(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{
     m_allowIDs <<  COMCHANNELS;
     m_size = 8;
     memset(m_matrix,0, 4*64);

     CIniFileSection* sect = g_conf.getSection(QString::number(m_id), false);
     if (sect){
         int tSz = sect->getInt("size",-1);
         if (tSz!=-1){
             m_size = tSz;
             for (int i=0;i<m_size;++i)
                 m_matrix[i] = sect->getInt(QString("ch%1").arg(i), -1);
         }
     }
}

bool CME427::processControlPacket(const unsigned char* cd, int sz)
{
    int pos = 0;
    bool matrixDataExists=false;
    while(pos < sz) {
        CCSDSID id=(CCSDSID)cd[pos];
        if (!m_allowIDs.contains(id)){
            printf("unknown key %d\n", cd[pos]);
            return false;
        }
        if (GSRecs[id].type==1){
            int val = cd[pos+1];
            printf("%s %d\n",GSRecs[id].name, val);
            if (id==CCSDSID::COMCHANNELS){
                if ((val>64)||(val<1)){
                    printf("incorrect matrix size %d\n", val);
                    return false;
                }
                m_size = val;
                for ( int i = 0; i < val; ++i){
                   m_matrix[i]= cd[pos+2+i];
                   printf("in = %d -> out = %d\n",m_matrix[i]+1,i+1);
                }
                matrixDataExists=true;
                break;
            }
        }
        pos += (GSRecs[id].size + 1);
    }
    if (!matrixDataExists){
        printf("matrix data is not exist\n");
        return false;
    }
    //autosave to cfg
    CIniFileSection* sect = g_conf.getSection(QString::number(m_id), true);
    sect->setValue("size", QString::number(m_size) );
    for(int i=0;i<m_size;++i){
        sect->setValue(QString("ch%1").arg(i), QString::number(m_matrix[i]) );
    }
    g_conf.flush(AutoSaveFile);
    return true;
}

void CME427::makeCcsdsState()
{
    unsigned short dataSz = 2 + m_size;
    //body
    m_ccsdsBody[6] = CCSDSID::COMCHANNELS;
    m_ccsdsBody[7] = (m_size&0xFF);
    for (int i = 0; i < m_size; ++i)
        m_ccsdsBody[8+i] = m_matrix[i];
    makeCcsdsHeader(dataSz);
}

bool CME427::sendState()
{

    if (m_socket){        
        //printf("CME427::sendState peer IP %s : %d\n", m_socket->peerAddress().toString().toLocal8Bit().data(), m_socket->peerPort());
        //printf("CME427::sendState IP %s : %d\n", m_socket->localAddress().toString().toLocal8Bit().data(), m_socket->localPort());
        //char chrTest[]="123";
        //if (m_socket->waitForConnected(3000)) {
          // qint64 ssz = m_socket->write(chrTest,3);
        makeCcsdsState();
        qint64 res =  m_socket->write((char*)m_ccsdsBody, m_ccsdsSize);
        if (m_socket->waitForBytesWritten(3000)){
            printf("answer is sended %d\n", res);
            //printf("CME427::sendState IP %s : %d\n", m_socket->localAddress().toString().toLocal8Bit().data(), m_socket->localPort());
        }
        //}
    }
    return true;
}

CME725::CME725(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CBSU::CBSU(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CME719::CME719(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{
    m_allowIDs << BRTS << INF << FREQ << KADRLENGTH << POWER << FREQSHIFT << INVERSION << DIFFER << CODE01;
}

bool CME719::processControlPacket(const unsigned char* cd, int sz)
{
    int pos = 0;
    while(pos < sz) {
        CCSDSID id=(CCSDSID)cd[pos];
        if (!m_allowIDs.contains(id)) {
            printf("unknown key %d\n", cd[pos]);
            return false;
        }
        if (GSRecs[id].type==1) {
            int val = cd[pos+1];
            printf("%s %d\n",GSRecs[id].name, val);
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

    return true;
}

CLVS::CLVS(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CRodos::CRodos(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}
