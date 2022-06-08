#include "tcpdevme725.h"
#include "ini/FastIni.h"

extern CIniFile g_conf;
extern QMap<CCSDSID,SettingsRec> GSRecs;

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
