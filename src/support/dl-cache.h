#ifndef  DL_CACHE_H
#define  DL_CACHE_H

#include  <QString>
#include  <vector>

class DynLinkCache
{
private:
  std::vector<QString> cache;
  int p;
  int size;
public:
  DynLinkCache(int size);

  void add(const QString & s);
  bool is_member(const QString & s);
  bool add_if_new(const QString & s);
};

#endif
