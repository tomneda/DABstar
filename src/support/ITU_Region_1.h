#ifndef  ITU_Region_1_H
#define  ITU_Region_1_H

#include  <QString>
#include  <stdint.h>

QString find_Country(uint8_t ecc, uint8_t countryId);
QString find_ITU_code(uint8_t ecc, uint8_t countryId);
#endif
