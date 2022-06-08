#include "tcpdevme719.h"
#include "ini/FastIni.h"

extern CIniFile g_conf;
extern QMap<CCSDSID,SettingsRec> GSRecs;

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
