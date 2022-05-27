/*!
@file
@brief Быстрое чтение из ini файлов
*/

#include <QTextCodec>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>

#include "FastIni.h"

char* trimSpaces(char *spaced)
{
	int length = strlen(spaced);

    while (isspace((unsigned char)spaced[length - 1])){
		spaced[length - 1] = 0;
        --length;
	}
    while (*spaced && isspace((unsigned char)*spaced))
        ++spaced;

    return spaced;
}

//--------------------------------------------------------------------------
// class CIniFileSection
//--------------------------------------------------------------------------

bool CIniFileSection::keyExists(const QString& a_KeyName)
{	
	strimap::iterator it = find(a_KeyName);	
	if ( it != end( ) )			
		return true;	
	return false;
}

int  CIniFileSection::getInt(const QString& sKey, int defVal)
{
	QString _ts;
	if (get(sKey,"",_ts)) {
		if (_ts.startsWith("0x")) {	
			bool bRes = false;
			int intRes = -1;
			intRes = _ts.toInt(&bRes,16);
			if (bRes)
				return intRes;
		}
		return _ts.toInt();
	}
	else
		return defVal;
}

double  CIniFileSection::getDouble(const QString& sKey, double defVal)
{
	QString _ts;
	if (get(sKey,"",_ts))
		return _ts.toDouble();
	else
		return defVal;
}

void  CIniFileSection::getVInt(const QString& sKey, QVector<int>& outV)
{
	outV.clear();
	QString _ts;
	if (get(sKey,"",_ts)) {
		QRegExp rx("(\\d+)");		
		int pos = 0;
		while ((pos = rx.indexIn(_ts, pos)) != -1) {
			outV << rx.cap(1).toInt();
			pos += rx.matchedLength();
		}		
	}
}

bool CIniFileSection::get(const QString& keyStr, const QString& defStr, QString& retStr)
{		
	strimap::const_iterator it = find( keyStr);	
	if ( it != end( ) ) {		
		QPair<QString,int>* tPair = it.value();
		if (tPair) {
			retStr = tPair->first;
			return true;
		}		
	}
	retStr = defStr;
	return false;
}

void CIniFileSection::setValue(const QString& a_KeyName, const QString& a_val)
{	
	strimap::iterator it = find(a_KeyName);	
	if ( it != end( ) )			
//		if (it.value())
			delete it.value();
	(*this)[a_KeyName] = new QPair<QString,int>(a_val, m_cntKeys++);
}

//порядок в соответствии с номером при создании
const QList<QString>& CIniFileSection::sortedKeys()
{
	m_sortedKeys.clear();
	for (int i = 0; i < m_cntKeys; ++i) {
		const_iterator j=constBegin();
		while(j!=constEnd()) {
			QPair<QString,int>* tp = j.value();
			if ((tp)&&(tp->second==i)){
				m_sortedKeys << j.key();
				break;
			}
			++j;
		}
	}
	return m_sortedKeys;
}

//--------------------------------------------------------------------------
// class CIniFile
//--------------------------------------------------------------------------
void CIniFile::determineCoding(FILE* f)
{
	if (!f)
		return;
	int c = 0;

	do	{
			c = fgetc(f);
			if ((c&0xE0)==0xC0) { // возможно начало слова utf
				c = fgetc(f);
				if (c == EOF){
					break;
				}
				if (((c&0xC0)==0x80)&&(c!=0xA8)&&(c!=0xB8)){//это utf
					m_codec = QTextCodec::codecForName("UTF-8");
					fseek(f,0,SEEK_SET);
					return;
				}
			}
	} while (c != EOF); // пока не конец файла
	m_codec = QTextCodec::codecForName("Windows-1251");
	fseek(f,0,SEEK_SET);
}

bool CIniFile::open( const QString& fName)
{	
	FILE* f1 = 0;	    
#if defined(Q_OS_WIN)
   QTextCodec* codecForFileName = QTextCodec::codecForName("Windows-1251");
#else
   QTextCodec* codecForFileName = QTextCodec::codecForName("UTF-8");
#endif
    QByteArray ba = codecForFileName->fromUnicode(fName);

    if (!(f1 = fopen(ba.data(),"rt")))
        return false;//ошибка
	//автоопределить кодировку
    determineCoding(f1);

	CIniFileSection* sec = NULL;
	
	// Разбираем строковой буфер
	char *str = NULL, *section = NULL, *keyvalue = NULL, *tmpc=0;
	char ts[2048];	
	while ( (str=fgets (ts,2048,f1)) != NULL )	{		
		// 1) Обкусываем лидирующие пробелы (если есть)
		while (str[0]==' ') str++;
		// 2) Ищем "["

		if (str[0]!='[') { // 3) Ищем "="		
			keyvalue = strchr( str, '=' );
			if (keyvalue) {
				*keyvalue = 0;
				//на конце ключа могут быть пробелы
				str = trimSpaces(str);
				++keyvalue;
				if (sec) {
					keyvalue = trimSpaces(keyvalue);											
                    sec->setValue(m_codec->toUnicode(str), m_codec->toUnicode(keyvalue));
				}
			}
		}
		else {
			++str;
			section = str;
			tmpc = strchr( str, ']' );
			if(tmpc) {
				*tmpc = 0;
				if (section[0]) {
					section = trimSpaces(section);										
                    sec = (*this).getSection(m_codec->toUnicode(section), true);
				}
			}
		}
	} // while	
	fclose(f1);
	
	bool bResult = (sec!=NULL); // Т.е. успешно прочитали файл == нашли хотя бы одну секцию	
	return bResult;
}

CIniFileSection* CIniFile::getSection( const QString& a_secName, bool create)
{	
	if (a_secName.isEmpty())
		return 0;
	// Ищем секцию
	IniFileSection_strimap::iterator it = find(a_secName);
	// Если нашли - возвращаем результат
	if(it != end( )) {		 
		return it.value();	
	}
	else {
		if (create) 			 
			return (*this)[a_secName] = new CIniFileSection(m_cnt++);
		else 
			return 0;		
	}
}

int CIniFile::getInt( const QString& sSection, const QString& sKey,	int defVal)
{
	CIniFileSection* tSec = getSection(sSection);
	if (!tSec)
		return defVal;	
	return tSec->getInt(sKey, defVal);
}

bool CIniFile::getString(
	const QString& sSection,
	const QString& sKey,
	const QString& sDefault,
	QString&  sRetString )
{	
	CIniFileSection* tSec = getSection(sSection);
	if (!tSec)
		return false;	
	return tSec->get(sKey, sDefault, sRetString)!=0;
}


void CIniFile::flush(const QString& a_FileName)//сохранить базу в файл
{
	//если отсутствует каталог то все равно его создаем
	QFileInfo tFI(a_FileName);
	QDir tDir;
	tDir.mkpath(tFI.absoluteDir().absolutePath());	

	QFile file(a_FileName);	
	if(!file.open(QIODevice::WriteOnly | QIODevice::Text))	
		return;//ошибка
	for (int n = 0; n < m_cnt; ++n) {//нужно для сортировки внутри QHash
		//наверняка можно оптимизировать
		for(IniFileSection_strimap::iterator it = begin(); it != end(); ++it) {		
			if ((*it)->m_num==n) {
				//file.write( ("\n["+it.key()+"]\n").toLocal8Bit() );
				file.write( m_codec->fromUnicode("\n["+it.key()+"]\n") );
				if (it.value()){
					for (int nk = 0; nk < (*it)->m_cntKeys; ++nk){//нужно для сортировки внутри QHash
						//наверняка можно оптимизировать
						for (strimap::iterator it2=it.value()->begin() ;it2 !=it.value()->end(); ++it2)
						{//ходим по секциям			
							QPair<QString,int>* tp = it2.value();
							if ((tp)&&(tp->second==nk)) 												
								file.write( m_codec->fromUnicode(it2.key() +"="+ (tp->first)+"\n") );							
						}
					}
				}
			}
		}
	}
	file.close();		
}

