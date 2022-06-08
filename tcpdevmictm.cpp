#include "tcpdevmictm.h"
#include "ini/FastIni.h"

extern CIniFile g_conf;
extern QMap<CCSDSID,SettingsRec> GSRecs;

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
