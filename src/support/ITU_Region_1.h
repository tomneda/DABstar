#ifndef  ITU_Region_1_H
#define  ITU_Region_1_H

#include "glob_data_types.h"
#include <QString>

QString find_Country(u8 ecc, u8 countryId);
QString find_ITU_code(u8 ecc, u8 countryId);
#endif
