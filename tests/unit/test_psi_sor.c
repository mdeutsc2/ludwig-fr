/*****************************************************************************
 *
 *  test_psi_sor.c
 *
 *  This is specifically SOR.
 *
 *  Edinburgh Soft Matter and Statistical Physics Group and
 *  Edinburgh Parallel Computing Centre
 *
 *  (c) 2012-2022 The University of Edinburgh
 *
 *  Contributing authors:
 *  Kevin Stratford (kevin@epcc.ed.ac.uk)
 *
 *****************************************************************************/

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "pe.h"
#include "coords.h"
#include "control.h"
#include "psi_s.h"
#include "psi_sor.h"

#include "util.h"
#include "psi_stats.h"
#include "tests.h"

#define fe_fake_t void

int test_psi_sor_poisson(pe_t * pe);
int test_psi_sor_vare_poisson(pe_t * pe);

static int test_charge1_set(psi_t * psi);
static int test_charge1_exact(psi_t * obj, f_vare_t fepsilon);

#define REF_PERMEATIVITY 1.0
static int fepsilon_constant(fe_fake_t * fe, int index, double * epsilon);

/*****************************************************************************
 *
 *  test_psi_sor_suite
 *
 *****************************************************************************/

int test_psi_sor_suite(void) {

  pe_t * pe = NULL;

  pe_create(MPI_COMM_WORLD, PE_QUIET, &pe);

  {
    int mpisz = pe_mpi_size(pe);

    if (mpisz > 4) {
      /* It's really just a 1-d problem, so no large deompcositions */
      pe_info(pe, "SKIP     ./unit/test_psi_sor\n");
    }
    else {
      test_psi_sor_poisson(pe);
      test_psi_sor_vare_poisson(pe);

      pe_info(pe, "PASS     ./unit/test_psi_sor\n");
    }
  }

  pe_free(pe);

  return 0;
}

/*****************************************************************************
 *
 *  test_psi_sor_poisson
 *
 *  Set rho(z = 1)  = + (1/2NxNy)
 *      rho(z = Lz) = + (1/2NxNy)
 *      rho         = - 1/(NxNy*(Nz-2)) everywhere else.
 *
 *  This is a fully periodic system with zero total charge.
 *
 *****************************************************************************/

int test_psi_sor_poisson(pe_t * pe) {

  cs_t * cs = NULL;
  psi_t * psi = NULL;
  int ntotal[3] = {4, 4, 64};

  assert(pe);

  cs_create(pe, &cs);
  {
    /* We need to control the decomposition (not in z, please) */
    int ndims = 3;
    int dims[3] = {0,0,1};

    MPI_Dims_create(pe_mpi_size(pe), ndims, dims);
    cs_nhalo_set(cs, 1);
    cs_ntotal_set(cs, ntotal);
    cs_decomposition_set(cs, dims);
  }
  cs_init(cs);

  psi_create(pe, cs, 2, &psi);
  assert(psi);

  psi_valency_set(psi, 0, +1);
  psi_valency_set(psi, 1, -1);
  psi_beta_set(psi, 1.0);
  psi_epsilon_set(psi, REF_PERMEATIVITY);

  test_charge1_set(psi);

  psi_halo_psi(psi);
  psi_halo_rho(psi);

  /* Time step is -1 for no output. */

  psi_sor_poisson(psi, -1);

  test_charge1_exact(psi, fepsilon_constant);

  psi_free(psi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  test_psi_sor_vare_poisson
 *
 *  Same problem as above, but use variable epsilon solver (albeit with
 *  fixed epsilon here).
 *
 *  Note this needs something slightly tighter than the default tolerance
 *  cf. the uniform case.
 *
 *****************************************************************************/

int test_psi_sor_vare_poisson(pe_t * pe) {

  cs_t * cs = NULL;
  psi_t * psi = NULL;
  int ntotal[3] = {4, 4, 64};

  assert(pe);

  cs_create(pe, &cs);
  cs_nhalo_set(cs, 1);
  cs_ntotal_set(cs, ntotal);
  cs_init(cs);

  psi_create(pe, cs, 2, &psi);
  assert(psi);

  psi_valency_set(psi, 0, +1);
  psi_valency_set(psi, 1, -1);
  psi_beta_set(psi, 1.0);
  psi_reltol_set(psi, 0.01*FLT_EPSILON);
  psi_epsilon_set(psi, REF_PERMEATIVITY);

  test_charge1_set(psi);

  psi_halo_psi(psi);
  psi_halo_rho(psi);

  /* Time step is -1 to avoid output */
  psi_sor_vare_poisson(psi, NULL, fepsilon_constant, -1);

  test_charge1_exact(psi, fepsilon_constant);

  psi_free(psi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  test_charge1_set
 *
 *  Sets a uniform 'wall' charge at z = 1 and z = L_z and a uniform
 *  interior value elsewhere such that the system is overall charge
 *  neutral.
 *
 *  There is no sign, just a density. We expect valency[0] and valency[1]
 *  to be \pm 1.
 *
 *****************************************************************************/

static int test_charge1_set(psi_t * psi) {

  int nk;
  int ic, jc, kc, index;
  int nlocal[3];
  int mpi_cartsz[3];
  int mpi_cartcoords[3];
  
  double ltot[3];
  double rho0, rho1;

  double rho_min[4];  /* For psi_stats */
  double rho_max[4];  /* For psi_stats */
  double rho_tot[4];  /* For psi_stats */
  MPI_Comm comm;

  cs_ltot(psi->cs, ltot);
  cs_nlocal(psi->cs, nlocal);
  cs_cartsz(psi->cs, mpi_cartsz);
  cs_cart_coords(psi->cs, mpi_cartcoords);
  cs_cart_comm(psi->cs, &comm);

  rho0 = 1.0 / (2.0*ltot[X]*ltot[Y]);              /* Edge values */
  rho1 = 1.0 / (ltot[X]*ltot[Y]*(ltot[Z] - 2.0));  /* Interior values */

  psi_nk(psi, &nk);
  test_assert(nk == 2);
  
  /* Throughout set to rho1 */

  for (ic = 1; ic <= nlocal[X]; ic++) {
    for (jc = 1; jc <= nlocal[Y]; jc++) {
      for (kc = 1; kc <= nlocal[Z]; kc++) {

	index = cs_index(psi->cs, ic, jc, kc);

	psi_psi_set(psi, index, 0.0);
	psi_rho_set(psi, index, 0, 0.0);
	psi_rho_set(psi, index, 1, rho1);
      }
    }
  }

  /* Now overwrite at the edges with rho0 */

  if (mpi_cartcoords[Z] == 0) {

    kc = 1;
    for (ic = 1; ic <= nlocal[X]; ic++) {
      for (jc = 1; jc <= nlocal[Y]; jc++) {
	index = cs_index(psi->cs, ic, jc, kc);

	psi_rho_set(psi, index, 0, rho0);
	psi_rho_set(psi, index, 1, 0.0);
      }
    }
  }

  if (mpi_cartcoords[Z] == mpi_cartsz[Z] - 1) {

    kc = nlocal[Z];
    for (ic = 1; ic <= nlocal[X]; ic++) {
      for (jc = 1; jc <= nlocal[Y]; jc++) {
	index = cs_index(psi->cs, ic, jc, kc);

	psi_rho_set(psi, index, 0, rho0);
	psi_rho_set(psi, index, 1, 0.0);
      }
    }
  }

  psi_stats_reduce(psi, rho_min, rho_max, rho_tot, 0, comm);

  if (pe_mpi_rank(psi->pe) == 0) {
    /* psi all zero */
    test_assert(fabs(rho_min[0] - 0.0) < DBL_EPSILON);
    test_assert(fabs(rho_max[0] - 0.0) < DBL_EPSILON);
    test_assert(fabs(rho_tot[0] - 0.0) < DBL_EPSILON);
    /* First rho0 interior */
    test_assert(fabs(rho_min[1] - 0.0) < DBL_EPSILON);
    test_assert(fabs(rho_max[1] - rho0) < DBL_EPSILON);
    test_assert(fabs(rho_tot[1] - 1.0) < DBL_EPSILON);
    /* Next rho1 edge */
    test_assert(fabs(rho_min[2] - 0.0) < DBL_EPSILON);
    test_assert(fabs(rho_max[2] - rho1) < DBL_EPSILON);
    test_assert(fabs(rho_tot[2] - 1.0) < FLT_EPSILON);
    /* Total rho_elec */
    test_assert(fabs(rho_min[3] + rho1) < DBL_EPSILON); /* + because valency is - */
    test_assert(fabs(rho_max[3] - rho0) < DBL_EPSILON);
    test_assert(fabs(rho_tot[3] - 0.0) < FLT_EPSILON);
  }

  return 0;
}

/*****************************************************************************
 *
 *  test_charge1_exact
 *
 *  Solve the tri-diagonal system appropriate for the 3-point stencil
 *  in one dimension (which is the z-direction). In parallel, all
 *  processes perform the whole solution.
 *
 *  The precise numerical solution is then obtained by solving the
 *  linear system.
 *
 *  We compare this with the solution obtained via the SOR function.
 *  Note that the linear system gives an answer which is different
 *  by a constant offset \psi_0. (All solutions of the Poisson equation
 *  in periodic boundary conditions are the same to within an arbitrary
 *  constant provided the 'unit cell' is charge neutral.)
 *
 *  The two solutions may then be compared to within (roughly) the
 *  relative tolerance prescribed for the SOR. In turn, the solution
 *  of the Gauss Jordan routine has been checked agaisnt NAG F04AAF.
 *
 *  We also recompute the RHS by differencing the SOR solution with
 *  a three point stencil in one dimension to provide a final check.
 *
 *  For variable epsilon, described by the f_vare_t fepsilon,
 *  we set up a difference scheme using a three-point stencil:
 *
 *  e(i+1/2) psi(i+1) - [ e(i+1/2) + e(i-1/2) ] psi(i) + e(i-1/2) psi(i-1)
 *
 *  which is the same as that used in psi_cor.c and which collapses to the
 *  uniform case if e(r) is constant.
 *
 *****************************************************************************/

static int test_charge1_exact(psi_t * obj, f_vare_t fepsilon) {

  int k, kp1, km1, index;
  int nlocal[3];
  int nz;
  int ifail = 0;

  double * epsilon = NULL;             /* 1-d e = e(z) from fepsilon */
  double eph;                          /* epsilon(k + 1/2) */
  double emh;                          /* epsilon(k - 1/2) */
  double psi, psi0;                    /* Potential values */
  double tolerance;                    /* Absolute tolerance from psi_t */
  double rhotot;                       /* Charge conservation check */
  double rhodiff;                      /* Difference RHS check */

  double * a = NULL;                   /* A is matrix for linear system */
  double * b = NULL;                   /* B is RHS / solution vector */

  assert(obj);

  cs_nlocal(obj->cs, nlocal);

  nz = nlocal[Z];

  /* Compute and store the permeativity values for convenience */

  epsilon = (double *) calloc(nz, sizeof(double));
  assert(epsilon);
  if (epsilon == NULL) pe_fatal(obj->pe, "calloc(epsilon) failed\n");

  for (k = 0; k < nz; k++) {
    index = cs_index(obj->cs, 1, 1, 1+k);
    fepsilon(NULL, index, epsilon + k);
  }

  /* Allocate space for exact solution */

  a = (double *) calloc((size_t) nz*nz, sizeof(double));
  b = (double *) calloc(nz, sizeof(double));
  assert(a);
  assert(b);
  if (a == NULL) pe_fatal(obj->pe, "calloc(a) failed\n");
  if (b == NULL) pe_fatal(obj->pe, "calloc(b) failed\n");

  /* Set tridiagonal elements for periodic solution for the
   * three-point stencil. The logic is to remove the perioidic end
   * points which prevent a solution of the linear system. This
   * effectively sets a Dirichlet boundary condition with psi = 0
   * at both ends. */

  for (k = 0; k < nz; k++) {
    
    kp1 = k + 1;
    km1 = k - 1;
    if (k == 0) km1 = kp1;
    if (k == nz-1) kp1 = km1;

    eph = 0.5*(epsilon[k] + epsilon[kp1]);
    emh = 0.5*(epsilon[km1] + epsilon[k]);

    a[k*nz + kp1] = eph;
    a[k*nz + km1] = emh;
    a[k*nz + k  ] = -(eph + emh);
  }

  /* Set the right hand side and solve the linear system. */

  for (k = 0; k < nz; k++) {
    index = cs_index(obj->cs, 1, 1, k + 1);
    psi_rho_elec(obj, index, b + k);
    b[k] *= -1.0; /* Minus sign in RHS Poisson equation */
  }

  ifail = util_gauss_jordan(nz, a, b);
  assert(ifail == 0);

  /* Check the Gauss Jordan answer b[] against the answer from psi_t */

  psi_abstol(obj, &tolerance);
  rhotot = 0.0;
  psi0 = 0.0;

  for (k = 0; k < nz; k++) {
    index = cs_index(obj->cs, 1, 1, 1+k);
    psi_psi(obj, index, &psi);
    if (k == 0) psi0 = psi;

    assert(fabs(b[k] - (psi - psi0)) < tolerance);
    if (fabs(b[k] - (psi - psi0)) > tolerance) ifail += 1;

    /* Extra check on the differencing terms */

    kp1 = k + 1;
    km1 = k - 1;
    if (k == 0) km1 = kp1;
    if (k == nz-1) kp1 = km1;

    eph = 0.5*(epsilon[k] + epsilon[kp1]);
    emh = 0.5*(epsilon[km1] + epsilon[k]);
    {
      double psim1 = 0.0;
      double psip1 = 0.0;
      double rho0  = 0.0;

      psi_psi(obj, index-1, &psim1);
      psi_psi(obj, index+1, &psip1);
      psi_rho_elec(obj, index, &rho0);

      rhodiff = -(emh*psim1 - (emh + eph)*psi + eph*psip1);

      assert(fabs(rho0 - rhodiff) < tolerance);
      if (fabs(rho0 - rhodiff) > tolerance) ifail += 1;
      rhotot += rho0;
    }
  }

  /* Total rho should be unchanged at zero. */
  test_assert(fabs(rhotot) < tolerance);

  free(b);
  free(a);
  free(epsilon);

  return ifail;
}

/*****************************************************************************
 *
 *  fepsilon_constant
 *
 *  Returns constant epsilon REF_PERMEATIVITY
 *
 *****************************************************************************/

static int fepsilon_constant(fe_fake_t * fe, int index, double * epsilon) {

  assert(epsilon);

  *epsilon = REF_PERMEATIVITY;

  return 0;
}

/*****************************************************************************
 *
 *  fepsilon_sinz
 *
 *  Permeativity is a function of z only:
 *
 *    e = e0 sin(pi z / Lz)
 *
 *  The - 0.5 is to make it symmetric about the centre line.
 *
 *****************************************************************************/
