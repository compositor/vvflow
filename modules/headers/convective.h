#ifndef __CONVECTIVE_H__
#define __CONVECTIVE_H__
#include <math.h>
#include "libVVHD/core.h"

int InitConvective(Space *sS, double sEps);
int CalcConvective();
int CalcCirculation();
int SpeedSum(TList<TVortex> *List, double px, double py, double &resx, double &resy);

#endif
