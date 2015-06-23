#include "stdafx.h"

#ifndef _PROXY_ERROR_H_
#define _PROXY_ERROR_H_

#define _DEBUG
//-----------------------------------------
typedef enum
{ SRV_ERR_NONE,  SRV_ERR_PARAM,
  SRV_ERR_INPUT, SRV_ERR_LIBEV,
  SRV_ERR_RCMMN, SRV_ERR_RFREE,
  SRV_ERR_FCONF
} myerr;
myerr my_errno;
//-----------------------------------------
const char*  strmyerror (void);
//-----------------------------------------
#endif // _PROXY_ERROR_H_
