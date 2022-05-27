/*!
@file
@brief Быстрое чтение из ini файлов
*/

#ifndef FASTINI_H
#define FASTINI_H

#include <QHash>
#include <QPair>
#include <QVector>
#include <QTextCodec>

typedef QHash<QString, QPair<QString,int>*> strimap; //ключ значение

///Ини секция
class CIniFileSection : public strimap
{
public:
	CIniFileSection(int a_num) : m_num(a_num){m_cntKeys=0;};
	virtual ~CIniFileSection() {
		strimap::const_iterator i = constBegin();
		while (i != constEnd()) {
		//	if (i.value()) 
				delete i.value();
			++i;
		}
	};

	int m_cntKeys;
	int m_num;

	bool keyExists(const QString&);	
	bool get( const QString& keyStr, const QString& defStr, QString& retStr);
	int  getInt(const QString& sKey, int defVal=-1);
	double  getDouble(const QString& sKey, double defVal=0);
	void getVInt(const QString& sKey, QVector<int>& outV);
	void setValue(const QString&, const QString&);

	const QList<QString>& sortedKeys();

protected:
	QList<QString> m_sortedKeys;

};

typedef QHash< QString, CIniFileSection* > IniFileSection_strimap;

///Быстрое чтение из ini файлов
class CIniFile : public IniFileSection_strimap 
{
public:
	CIniFile() {
		m_cnt=0;
		m_codec = QTextCodec::codecForName("UTF-8");		
	};
	virtual ~CIniFile() {
		IniFileSection_strimap::const_iterator i = constBegin();
		while (i != constEnd()) {
//			if (i.value()) 
				delete i.value();
			++i;
		}
	};
	int m_cnt;
	bool				open( const QString& fName);
	CIniFileSection*	getSection( const QString& a_secName, bool create = false);

	int getInt(	const QString& sSection, const QString& sKey, int defVal=-1);
	bool getString(
		const QString& sSection,
		const QString& sKey,
		const QString& sDefault,
		QString&  sRetString
		);			
	///скинуть все данные в указаннный ини файл	
	void flush(const QString& _s); 

private:
	QTextCodec* m_codec;
	void determineCoding(FILE*);
};

#endif // FASTINI_H
