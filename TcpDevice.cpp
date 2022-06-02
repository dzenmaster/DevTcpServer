#include <QTcpSocket>
#include <QSettings>
#include <QMutex>
#include "TcpDevice.h"
#include "ini/FastIni.h"

extern CIniFile g_conf;

//QMutex G_mutex;

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

CMicTM::CMicTM(const QHostAddress& addr)
: CTcpDevice("", 0, addr)
{
    m_IDs[0]=m_IDs[1]=0;
    m_brts[0]=m_brts[1]=0;
    m_inf[0]=m_inf[1]=0;
    m_kadrLen[0]=m_kadrLen[1]=0;
    m_freq[0]=m_freq[0]=0;

    m_allowIDs << CHANNUM << BRTS << INF << FREQ << NCLIENTS << KADRLENGTH;
    m_currChannel = 0;


}

void CMicTM::addChannel(unsigned int id, QString name)
{
    m_name += name + " ";
    int tch=0;
    if (name.contains("К2"))
         tch=1;

    m_IDs[tch]=id;

    CIniFileSection* sect = g_conf.getSection(QString::number(id), false);
    if (sect){
        m_brts[tch]  = sect->getInt("brts", 0);
        m_inf[tch]  = sect->getInt("inf", 0);
        m_kadrLen[tch] = sect->getInt("kadrLen", 0);
        m_freq[tch] = sect->getInt("freq", 0);
        int nClients = sect->getInt("nClients", 0);
        m_clients[tch].clear();
        for(int i=0; i<nClients; ++i) {
           QString ip;
           sect->get(QString("IP%1").arg(1),"",ip);
           unsigned short port = sect->getInt(QString("Port%1").arg(1),0);
           m_clients[tch].push_back(QPair<QString,unsigned short>(ip, port));
        }
    }
}



bool CMicTM::processControlPacket(const unsigned char* cd, int sz)
{
    CIniFileSection* sect = 0;
    bool res = true;
    int pos = 0;
    while(pos < sz) {
        CCSDSID id=(CCSDSID)cd[pos];
        if (!m_allowIDs.contains(id)){
            printf("unknown key %d\n", cd[pos]);
            res = false;
            break;
        }
        if (GSRecs[id].type==1){
            int val = cd[pos+1];
            printf("%s %d\n",GSRecs[id].name, val);
            if (id==CHANNUM){
                m_currChannel = val;
                sect = g_conf.getSection(QString::number(m_IDs[m_currChannel]), true);
            }
            if (id ==BRTS){
                m_brts[m_currChannel] = val;
                sect->setValue("brts", QString::number(val) );
            }
            if (id==NCLIENTS){
                sect->setValue("nClients", QString::number(val) );
                m_clients[m_currChannel].clear();
                for ( int i = 0; i < val; ++i){
                   QString ip = QString("%1.%2.%3.%4").arg(cd[pos+2]).arg(cd[pos+3]).arg(cd[pos+4]).arg(cd[pos+5]);
                   unsigned short port = (((unsigned int)cd[pos+6])<<8) + cd[pos+7];
                   m_clients[m_currChannel].push_back(QPair<QString,unsigned short>(ip, port));
                   sect->setValue(QString("IP%1").arg(i), ip );
                   sect->setValue(QString("Port%1").arg(i), QString::number(port) );
                   printf("ip=%d.%d.%d.%d : %d\n",cd[pos+2],cd[pos+3],cd[pos+4],cd[pos+5], port);
                   pos+=6;
                }
            }
        }
        else if (GSRecs[id].type==2) {//uint
            unsigned int val = (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
           +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4]));
            if (id==INF){
                m_inf[m_currChannel]=val;
                sect->setValue("inf", QString::number(val) );
            }

            else if (id==KADRLENGTH){
                m_kadrLen[m_currChannel]=val;
                sect->setValue("kadrLen", QString::number(val) );
            }

            if (id==FREQ){
                m_freq[m_currChannel]=val;
                sect->setValue("freq", QString::number(val) );
            }

            printf("%s %d\n",GSRecs[id].name,
                   (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                  +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4]))
                    );
        }
        else if (GSRecs[id].type==4) {//int

            printf("%s %d\n",GSRecs[id].name,
                  (int)( (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                  +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4])))
                    );
        }
        pos += (GSRecs[id].size + 1);
    }
    g_conf.flush(AutoSaveFile);
    return res;
}
void CMicTM::makeCcsdsState(int ch)
{
    unsigned char clSz = m_clients[ch].size();
    unsigned short dataSz = 19 + m_clients[ch].size()*6 + (m_clients[ch].size()==0 ? 0 : 2);
    //body
    m_ccsdsBody[6] = CCSDSID::CHANNUM;
    m_ccsdsBody[7] = (ch&0xFF);
    m_ccsdsBody[8] = CCSDSID::BRTS;
    m_ccsdsBody[9] = (m_brts[ch]&0xFF);
    m_ccsdsBody[10] = CCSDSID::INF;
    m_ccsdsBody[11] = ((m_inf[ch]>>24)&0xFF);
    m_ccsdsBody[12] = ((m_inf[ch]>>16)&0xFF);
    m_ccsdsBody[13] = ((m_inf[ch]>>8)&0xFF);
    m_ccsdsBody[14] = (m_inf[ch]&0xFF);
    m_ccsdsBody[15] = CCSDSID::KADRLENGTH;
    m_ccsdsBody[16] = ((m_kadrLen[ch]>>24)&0xFF);
    m_ccsdsBody[17] = ((m_kadrLen[ch]>>16)&0xFF);
    m_ccsdsBody[18] = ((m_kadrLen[ch]>>8)&0xFF);
    m_ccsdsBody[19] = (m_kadrLen[ch]&0xFF);
    m_ccsdsBody[20] = CCSDSID::FREQ;
    m_ccsdsBody[21] = ((m_freq[ch]>>24)&0xFF);
    m_ccsdsBody[22] = ((m_freq[ch]>>16)&0xFF);
    m_ccsdsBody[23] = ((m_freq[ch]>>8)&0xFF);
    m_ccsdsBody[24] = (m_freq[ch]&0xFF);
    if (clSz>0) {
        m_ccsdsBody[25] = CCSDSID::NCLIENTS;
        m_ccsdsBody[26] = clSz;
        for(int i = 0; i < clSz; ++i) {
            QHostAddress ha(m_clients[ch][i].first);
            qint32 ip = ha.toIPv4Address();
            m_ccsdsBody[27 + 6*i] = ((ip>>24)&0xFF);
            m_ccsdsBody[28 + 6*i] = ((ip>>16)&0xFF);
            m_ccsdsBody[29 + 6*i] = ((ip>>8)&0xFF);
            m_ccsdsBody[30 + 6*i] = (ip&0xFF);
            //port
            m_ccsdsBody[31 + 6*i] = ((m_clients[ch][i].second>>8)&0xFF);
            m_ccsdsBody[32 + 6*i] = ((m_clients[ch][i].second)&0xFF);
        }
    }
    makeCcsdsHeader(dataSz);
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

void CME427::makeCcsdsState(int ch)
{
    unsigned short dataSz = 2 + m_size;
    //body
    m_ccsdsBody[6] = CCSDSID::COMCHANNELS;
    m_ccsdsBody[7] = (m_size&0xFF);
    for (int i = 0; i < m_size; ++i)
        m_ccsdsBody[8+i] = m_matrix[i];
    makeCcsdsHeader(dataSz);
}


CME725::CME725(const QString& name, unsigned int id, const QHostAddress& addr)
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


bool CME725::processControlPacket(const unsigned char* cd, int sz)
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

void CME725::makeCcsdsState(int ch)
{
    unsigned short dataSz = 2 + m_size;
    //body
    m_ccsdsBody[6] = CCSDSID::COMCHANNELS;
    m_ccsdsBody[7] = (m_size&0xFF);
    for (int i = 0; i < m_size; ++i)
        m_ccsdsBody[8+i] = m_matrix[i];
    makeCcsdsHeader(dataSz);
}


CBSU::CBSU(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CME719::CME719(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr),
m_brts(0),
m_inv(0),
m_diff(0),
m_code01(0),
m_inf(0),
m_kadrLen(0),
m_freq(0),
m_power(0),
m_freqShift(0)
{
    m_allowIDs << BRTS << INF << FREQ << KADRLENGTH << POWER << FREQSHIFT << INVERSION << DIFFER << CODE01;

    CIniFileSection* sect = g_conf.getSection(QString::number(m_id), false);
    if (sect){
        m_brts  = sect->getInt("brts", 0);
        m_inv  = sect->getInt("inversion", 0);
        m_diff  = sect->getInt("diff", 0);
        m_code01  = sect->getInt("code01", 0);
        m_inf  = sect->getInt("inf", 0);
        m_kadrLen = sect->getInt("kadrLen", 0);
        m_freq = sect->getInt("freq", 0);
        m_power = sect->getInt("power", 0);
        m_freqShift = sect->getInt("freqShift", 0);
    }
}

bool CME719::processControlPacket(const unsigned char* cd, int sz)
{
    bool res = true;
    int pos = 0;
    //autosave to cfg
    CIniFileSection* sect = g_conf.getSection(QString::number(m_id), true);
    while(pos < sz) {
        CCSDSID id=(CCSDSID)cd[pos];
        if (!m_allowIDs.contains(id)) {
            printf("unknown key %d\n", cd[pos]);
            res = false;
            break;
        }
        if (GSRecs[id].type==1) {
            int val = cd[pos+1];
            switch (id) {
                case BRTS:
                    sect->setValue("brts", QString::number(m_brts=val) );
                    break;
                case INVERSION:
                    sect->setValue("inversion", QString::number(m_inv=val) );
                    break;
                case DIFFER:
                    sect->setValue("diff", QString::number(m_diff=val) );
                    break;
                case CODE01:
                    sect->setValue("code01", QString::number(m_code01=val) );
                    break;
            }
            printf("%s %d\n",GSRecs[id].name, val);
        }
        else if (GSRecs[id].type==2){
            unsigned int val = (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                    +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4]));
            switch (id) {
                case INF:
                    sect->setValue("inf", QString::number(m_inf=val) );
                    break;
                case FREQ:
                    sect->setValue("freq", QString::number(m_freq=val) );
                    break;
                case KADRLENGTH:
                    sect->setValue("kadrLen", QString::number(m_kadrLen=val) );
                    break;
            }
            printf("%s %d\n",GSRecs[id].name,
                   (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                  +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4]))
                    );
        }
        else if (GSRecs[id].type==4){
            int val = (int)( (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                      +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4])));
            switch (id) {
                case POWER:
                    sect->setValue("power", QString::number(m_power=val) );
                    break;
                case FREQSHIFT:
                    sect->setValue("freqShift", QString::number(m_freqShift=val) );
                    break;
            }
            printf("%s %d\n",GSRecs[id].name,
                  (int)( (((unsigned int)cd[pos+1])<<24) + (((unsigned int)cd[pos+2])<<16)
                  +(((unsigned int)cd[pos+3])<<8) + (((unsigned int)cd[pos+4])))
                    );
        }
        pos += (GSRecs[id].size + 1);
    }
    g_conf.flush(AutoSaveFile);
    return res;
}

void CME719::makeCcsdsState(int ch)
{
    unsigned short dataSz = 33;
    //body
    m_ccsdsBody[6] = CCSDSID::BRTS;
    m_ccsdsBody[7] = (m_brts&0xFF);
    m_ccsdsBody[8] = CCSDSID::INF;
    m_ccsdsBody[9] = ((m_inf>>24)&0xFF);
    m_ccsdsBody[10] = ((m_inf>>16)&0xFF);
    m_ccsdsBody[11] = ((m_inf>>8)&0xFF);
    m_ccsdsBody[12] = (m_inf&0xFF);
    m_ccsdsBody[13] = CCSDSID::FREQ;
    m_ccsdsBody[14] = ((m_freq>>24)&0xFF);
    m_ccsdsBody[15] = ((m_freq>>16)&0xFF);
    m_ccsdsBody[16] = ((m_freq>>8)&0xFF);
    m_ccsdsBody[17] = (m_freq&0xFF);
    m_ccsdsBody[18] = CCSDSID::KADRLENGTH;
    m_ccsdsBody[19] = ((m_kadrLen>>24)&0xFF);
    m_ccsdsBody[20] = ((m_kadrLen>>16)&0xFF);
    m_ccsdsBody[21] = ((m_kadrLen>>8)&0xFF);
    m_ccsdsBody[22] = (m_kadrLen&0xFF);
    m_ccsdsBody[23] = CCSDSID::POWER;
    m_ccsdsBody[24] = ((m_power>>24)&0xFF);
    m_ccsdsBody[25] = ((m_power>>16)&0xFF);
    m_ccsdsBody[26] = ((m_power>>8)&0xFF);
    m_ccsdsBody[27] = (m_power&0xFF);
    m_ccsdsBody[28] = CCSDSID::FREQSHIFT;
    m_ccsdsBody[29] = ((m_freqShift>>24)&0xFF);
    m_ccsdsBody[30] = ((m_freqShift>>16)&0xFF);
    m_ccsdsBody[31] = ((m_freqShift>>8)&0xFF);
    m_ccsdsBody[32] = (m_freqShift&0xFF);
    m_ccsdsBody[33] = CCSDSID::INVERSION;
    m_ccsdsBody[34] = m_inv ? 1 : 0;
    m_ccsdsBody[35] = CCSDSID::DIFFER;
    m_ccsdsBody[36] = m_diff ? 1 : 0;
    m_ccsdsBody[37] = CCSDSID::CODE01;
    m_ccsdsBody[38] = m_code01 ? 1 : 0;

    makeCcsdsHeader(dataSz);
}



CLVS::CLVS(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}

CRodos::CRodos(const QString& name, unsigned int id, const QHostAddress& addr)
: CTcpDevice(name, id, addr)
{}
