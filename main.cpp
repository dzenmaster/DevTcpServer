#include <QCoreApplication>
#include <QTcpServer>
#include <QFile>
#include <QTextCodec>
#include <QRegExp>

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

    while (!fileConf.atEnd()) {
        QByteArray line = fileConf.readLine();
        QString str = QString::fromUtf8(line);

        QRegExp rx("([^0]+)0x(\\d+)([^\r\n]+)");
        int pos = rx.indexIn(str);
        QStringList list = rx.capturedTexts();
        if (list.size()>3){

        }
        //cout << str;
    }
    //cout.flush();


   // QString str = QString::fromUtf8(line);

    fileConf.close();

    QTcpServer  _server;
    unsigned int adr = (192<<24) + (168<<16) + (13<<8) + 3;
    QHostAddress ad(adr);
    bool res = _server.listen(ad, 4000);
    if (res)
        printf("listen good\n");
    else
        printf("listen bad\n");

    return a.exec();
}
