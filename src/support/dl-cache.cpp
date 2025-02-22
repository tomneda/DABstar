#include  <QString>
#include  <vector>
#include "dl-cache.h"

constexpr uint32_t cCacheSize = 16;
constexpr uint32_t cCacheMask = (cCacheSize - 1);

DynLinkCache::DynLinkCache(int size)
{
  (void) size;
  cache.resize(cCacheSize);
  this->size = cCacheSize;
  p = 0;
}

void DynLinkCache::add(const QString & s)
{
  cache[p] = s;
  p = (p + 1) & cCacheMask;
}

bool DynLinkCache::is_member(const QString & s)
{
  for (uint32_t i = 0; i < cCacheSize; i++)
  {
    if (cache[i] == s)
    {
      return true;
    }
  }
  return false;
}

bool DynLinkCache::add_if_new(const QString & s)
{
  for (uint16_t i = p; i < p + cCacheSize; i++)
  {
    if (cache[i & cCacheMask] == s)
    {
      for (uint16_t j = i; j < (p - 1) + cCacheSize; j++)
      {
        cache[j & cCacheMask] = cache[(j + 1) & cCacheMask];
      }
      cache[(p - 1 + cCacheSize) & cCacheMask] = s;
      return true;
    }
  }
  cache[p] = s;
  p = (p + 1) & cCacheMask;
  return false;
}
