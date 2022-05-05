/*
* Copyright (c) 1993-1995 by Argonaut Technologies Limited. All rights reserved.
*
* $Id: quat.h 2.2 1996/10/03 11:10:08 sam Exp $
* $Locker: $
*
* Type descibing a unit quaternion
*/
#ifndef _QUAT_H_
#define _QUAT_H_

#include "scalar.h"

typedef struct br_quat
{
	br_scalar x;
	br_scalar y;
	br_scalar z;
	br_scalar w;
} br_quat;

#define BR_QUAT(x,y,z,w) {BR_SCALAR(x),BR_SCALAR(y),BR_SCALAR(z),BR_SCALAR(w)}

#endif


