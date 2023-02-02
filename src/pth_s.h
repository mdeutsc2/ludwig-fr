/*****************************************************************************
 *
 *****************************************************************************/

#ifndef PTH_S_H
#define PTH_S_H

#include "phi_force_stress.h"

struct pth_s {
  pe_t * pe;            /* Parallel environment */
  cs_t * cs;            /* Coordinate system */
  int method;           /* Method for force computation */
  int nsites;           /* Number of sites allocated */
  double * str;         /* Stress may be antisymmetric */
  pth_t * target;       /* Target memory */
};

#endif
