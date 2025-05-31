#include "glob_data_types.h"
#include "dl-cache.h"
#include <vector>
#include <QString>

constexpr u32 cCacheSize = 16;
constexpr u32 cCacheMask = (cCacheSize - 1);

DynLinkCache::DynLinkCache(i32 size)
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
  for (u32 i = 0; i < cCacheSize; i++)
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
  for (u16 i = p; i < p + cCacheSize; i++)
  {
    if (cache[i & cCacheMask] == s)
    {
      for (u16 j = i; j < (p - 1) + cCacheSize; j++)
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
