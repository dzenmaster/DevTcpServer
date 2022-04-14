#include <QCoreApplication>
#include <QTcpServer>
#include <QFile>
#include <QTextCodec>
#include <QRegExp>

class CTcpDevice
{

public:
    CTcpDevice(const QHostAddress& addr)
    {
        m_hostAddr = addr;
        bool res = m_server.listen(m_hostAddr, 4000);
        if (res)
            printf("listen success %s\n", m_hostAddr.toString().toLocal8Bit().constData());
        else
            printf("listen bad %s\n", m_hostAddr.toString().toLocal8Bit().constData());
    };
    const QHostAddress& getIP()
    {
        return m_hostAddr;
    };

    QTcpServer  m_server;


private:
    QHostAddress m_hostAddr;
};


class CMicTM : public CTcpDevice
{
public:
    CMicTM(const QHostAddress& addr):CTcpDevice(addr)
    {};
    void addChannel(unsigned int id, QString name)
    {};
};


QVector<CTcpDevice*> G_devices;

CTcpDevice* searchDevByIP(const QHostAddress& addr)
{
    QVectorIterator<CTcpDevice*> itDevs(G_devices);
    while (itDevs.hasNext()) {
        CTcpDevice* tDev = itDevs.next();
        if ((tDev)&&(tDev->getIP()==addr))
            return tDev;
    }
    return nullptr;
};

int main(int argc, char *argv[])
{   
    QTextStream cout(stdout);
    cout.setCodec("CP866");

    QCoreApplication a(argc, argv);
    QString confFileName("devices.cfg");
    if (!QFile::exists(confFileName))
    {
        cout <<  QString::fromUtf8("Ошибка конфигурационного файла ") <<  confFileName;
        return 0;
    }

    QFile fileConf(confFileName);
    if (!fileConf.open(QIODevice::ReadOnly | QIODevice::Text)){
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
        if (list.size()>3){
            name = list[1].trimmed();
            if (!addr.setAddress(list[3].trimmed())) {
                cout <<  QString::fromUtf8("Неверный IP ") << list[3];
                continue;
            }
            int id = list[2].trimmed().toUInt();
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
                if (!ptrMic)
                    ptrMic = new CMicTM(addr);
                ptrMic->addChannel(id, name);
                G_devices.push_back(ptrMic);
            }
        }
        //cout << str;
    }
    //cout.flush();
    fileConf.close();
    cout.flush();

    return a.exec();
}
