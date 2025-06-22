#ifndef	__XML_HANDLER__
#define	__XML_HANDLER__

#include <QtXml>

#include	<QString>
#include	<stdint.h>
#include	<cstdio>
#include	<complex>

class Blocks
{
public:
  Blocks() {}
  ~Blocks() {}
  i32 blockNumber;
  i32 nrElements;
  QString typeofUnit;
  i32 frequency;
  QString modType;
};

class XmlHandler
{
public:
  XmlHandler(FILE *, i32, i32);
  ~XmlHandler();

  void add(std::complex<i16> *, i32);
  void computeHeader(QString &, QString &);

private:
  QFrame myFrame;
  i32 denominator;
  i32 frequency;
  QString create_xmltree(QString &, QString &);
  FILE * xmlFile;
  QString byteOrder;
  i32 nrElements;
};

#endif
