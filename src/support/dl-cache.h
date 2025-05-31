#ifndef  DL_CACHE_H
#define  DL_CACHE_H

#include  <QString>
#include  <vector>

class DynLinkCache
{
private:
  std::vector<QString> cache;
  i32 p;
  i32 size;
public:
  DynLinkCache(i32 size);

  void add(const QString & s);
  bool is_member(const QString & s);
  bool add_if_new(const QString & s);
};

#endif
