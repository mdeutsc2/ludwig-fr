/*****************************************************************************
 *
 *  coords.h
 *
 *  Edinburgh Soft Matter and Statistical Physics Group and
 *  Edinburgh Parallel Computing Centre
 *
 *  (c) 2010-2021 The University of Edinburgh
 *
 *  Contributing authors:
 *  Kevin Stratford (kevin@epcc.ed.ac.uk)
 *
 *****************************************************************************/

#ifndef LUDWIG_COORDS_H
#define LUDWIG_COORDS_H

#include "pe.h"
#include "cartesian.h"

typedef struct coords_s cs_t;

#define NSYMM 6      /* Elements for general symmetric tensor */

enum cartesian_neighbours {FORWARD, BACKWARD};
enum cs_mpi_cart_neighbours {CS_FORW=0,CS_BACK=1};
enum upper_triangle {XX, XY, XZ, YY, YZ, ZZ};

/* Host interface */

__host__ int cs_create(pe_t * pe, cs_t ** pcs);
__host__ int cs_free(cs_t * cs);
__host__ int cs_retain(cs_t * cs);
__host__ int cs_init(cs_t * cs);
__host__ int cs_commit(cs_t * cs);
__host__ int cs_target(cs_t * cs, cs_t ** target);

__host__ int cs_decomposition_set(cs_t * cs, const int irequest[3]);
__host__ int cs_periodicity_set(cs_t * cs, const int iper[3]);
__host__ int cs_ntotal_set(cs_t * cs, const int ntotal[3]);
__host__ int cs_nhalo_set(cs_t * cs, int nhalo);
__host__ int cs_reorder_set(cs_t * cs, int reorder);
__host__ int cs_info(cs_t * cs);
__host__ int cs_cart_comm(cs_t * cs, MPI_Comm * comm);
__host__ int cs_periodic_comm(cs_t * cs, MPI_Comm * comm);
__host__ int cs_cart_neighb(cs_t * cs, int forwback, int dim);
__host__ int cs_cart_rank(cs_t * cs);
__host__ int cs_pe_rank(cs_t * cs);

/* Host / device interface */

__host__ __device__ int cs_cartsz(cs_t * cs, int cartsz[3]);
__host__ __device__ int cs_cart_coords(cs_t * cs, int coords[3]);
__host__ __device__ int cs_lmin(cs_t * cs, double lmin[3]);
__host__ __device__ int cs_ltot(cs_t * cs, double ltot[3]);
__host__ __device__ int cs_periodic(cs_t * cs, int period[3]);
__host__ __device__ int cs_nlocal(cs_t * cs, int n[3]);
__host__ __device__ int cs_nlocal_offset(cs_t * cs, int n[3]);
__host__ __device__ int cs_nhalo(cs_t *cs, int * nhalo);
__host__ __device__ int cs_index(cs_t * cs, int ic, int jc, int kc);
__host__ __device__ int cs_ntotal(cs_t * cs, int ntotal[3]);
__host__ __device__ int cs_nsites(cs_t * cs, int * nsites);
__host__ __device__ int cs_minimum_distance(cs_t * cs, const double r1[3],
			    const double r2[3], double r12[3]);
__host__ __device__ int cs_index_to_ijk(cs_t * cs, int index, int coords[3]);
__host__ __device__ int cs_strides(cs_t * cs, int *xs, int *ys, int *zs);
__host__ __device__ int cs_nall(cs_t * cs, int nall[3]);

/* A "class" function */

__host__ int cs_cart_shift(MPI_Comm comm, int dim, int direction, int * rank);

#endif
