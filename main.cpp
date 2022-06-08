#include <QCoreApplication>
#include <QTcpServer>
#include <QFile>
#include <QTextCodec>
#include <QRegExp>
#include "TcpDevice.h"
#include "tcpdevme427.h"
#include "tcpdevme719.h"
#include "tcpdevme725.h"
#include "tcpdevmictm.h"
#include "ini/FastIni.h"

QVector<CTcpDevice*> G_devices;
CIniFile g_conf;


CTcpDevice* searchDevByIP(const QHostAddress& addr)
{
    QVectorIterator<CTcpDevice*> itDevs(G_devices);
    while (itDevs.hasNext()) {
        CTcpDevice* tDev = itDevs.next();
        if ((tDev)&&(tDev->getIP()==addr))
            return tDev;
    }
    return nullptr;
}

void startListenAll()
{
    QVectorIterator<CTcpDevice*> itDevs(G_devices);
    while (itDevs.hasNext()) {
        CTcpDevice* tDev = itDevs.next();
        if (tDev)
           tDev->startListen();
    }
}

int main(int argc, char *argv[])
{   

    if (!g_conf.open(AutoSaveFile)){
        g_conf.flush(AutoSaveFile);//create
    }


    QTextStream cout(stdout);
    cout.setCodec("CP866");

    QCoreApplication a(argc, argv);
    QString confFileName("devices.cfg");
    if (!QFile::exists(confFileName)){
        cout <<  QString::fromUtf8("Конфигурационный файл отсутствует ") <<  confFileName;
        return 0;
    }

    QFile fileConf(confFileName);
    if (!fileConf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        cout <<  QString::fromUtf8("Ошибка открытия конфигурационного файла ") <<  confFileName;
        return 0;
    }

    QString  name;
    QHostAddress addr;
    while (!fileConf.atEnd()) {
        QByteArray line = fileConf.readLine();
        QString str = QString::fromUtf8(line);

        QRegExp rx("([^0]+)0x([0-9A-F]+)([^\r\n]+)");
        int pos = rx.indexIn(str);
        QStringList list = rx.capturedTexts();
        list.removeAll(QString(""));
        if (list.size()<4)
            continue;
        name = list[1].trimmed();
        if (!addr.setAddress(list[3].trimmed())) {
            cout <<  QString::fromUtf8("Неверный IP ") << list[3];
            continue;
        }
        int id = list[2].trimmed().toUInt(Q_NULLPTR,16);
        CTcpDevice* ptrDev = searchDevByIP(addr);
        if (name.startsWith("mic",Qt::CaseInsensitive)){
            CMicTM* ptrMic = nullptr;
            if (ptrDev){
               ptrMic = static_cast<CMicTM*>(ptrDev);
               if (ptrMic==nullptr){
                   cout <<  QString::fromUtf8("Конфликт типов устройств");
                   continue;
               }
            }
            if (!ptrMic){
                ptrMic = new CMicTM(addr);
                G_devices.push_back(ptrMic);
            }
            ptrMic->addChannel(id, name);

        }
        else {
            if (ptrDev){
                cout <<  QString::fromUtf8("Устройство уже существует");
                continue;
            }
            if ((name.startsWith("me-427",Qt::CaseInsensitive))||
                 (name.startsWith("МЕ-427",Qt::CaseInsensitive))){//cyr
                CME427* ptrME427 = new CME427(name, id, addr);
                G_devices.push_back(ptrME427);
            }
            else if ((name.startsWith("me-725",Qt::CaseInsensitive))||
                 (name.startsWith("МЕ-725",Qt::CaseInsensitive))){//cyr
                CME725* ptrME725 = new CME725(name, id, addr);
                G_devices.push_back(ptrME725);
            }
            else if ((name.startsWith("bsu",Qt::CaseInsensitive))||
                 (name.startsWith("бсу",Qt::CaseInsensitive))){//cyr
                CBSU* ptrBSU = new CBSU(name, id, addr);
                G_devices.push_back(ptrBSU);
            }
            else if ((name.startsWith("me-719",Qt::CaseInsensitive))||
                 (name.startsWith("МЕ-719",Qt::CaseInsensitive))){//cyr
                CME719* ptrME719 = new CME719(name, id, addr);
                G_devices.push_back(ptrME719);
            }
            else if ((name.startsWith("LVS",Qt::CaseInsensitive))||
                 (name.startsWith("лвс",Qt::CaseInsensitive))){//cyr
                CLVS* ptrLVS = new CLVS(name, id, addr);
                G_devices.push_back(ptrLVS);
            }
            else if ((name.startsWith("rodos",Qt::CaseInsensitive))||
                 (name.startsWith("родос",Qt::CaseInsensitive))){//cyr
                CRodos* ptrRodos = new CRodos(name, id, addr);
                G_devices.push_back(ptrRodos);
            }
        }
    }
    fileConf.close();
    cout.flush();

    startListenAll();

    return a.exec();
}
