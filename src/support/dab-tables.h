#pragma once

#include "glob_data_types.h"

const char * getASCTy (i16 ASCTy);
const char * getDSCTy (i16 DSCTy);
const char * getLanguage (i16 language);
const char * getCountry	(u8 ecc, u8 countryId);
//const char * getProgramType_Not_NorthAmerica(i16 programType);
const char * getProgramType (i16 programType);
const char * getProgramType_For_NorthAmerica(i16 programType);
//const char * getProgramType(bool, u8 interTabId, i16 programType);
const char * getUserApplicationType(i16 appType);
const char * getFECscheme(i16 FEC_scheme);
const char * getProtectionLevel (bool shortForm, i16 protLevel);
const char * getCodeRate (bool shortForm, i16 protLevel);
const char * get_announcement_type_str(u16 a);
const char * get_DSCTy_AppType(i8 iDSCTy, i16 iAppType);
