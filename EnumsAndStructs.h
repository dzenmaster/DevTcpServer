/*!
@file
@brief Структуры и перечисления общего пользования
*/

#ifndef ENUMSANDSTRUCTS_H
#define ENUMSANDSTRUCTS_H


#define SZDIAP 6

enum {
    PORT_SKUT,
    PORT_PIRIT,
    PORT_VIM,
    PORT_KIM,
    PORT_BITS,
    PORT_ORBITA,
    PORT_ORBITA16,
    PORT_RTSCM
};

enum CCSDSID {
    CHANNUM=1,
    BRTS,
    INF,
    FREQ,
    NCIENTS,
    COMCHANNELS
} ;

#endif 
