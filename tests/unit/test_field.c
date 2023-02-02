/*****************************************************************************
 *
 *  test_field.c
 *
 *  Unit test for field structure.
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
#include <stdio.h>
#include <math.h>

#include "pe.h"
#include "coords.h"
#include "kernel.h"
#include "leesedwards.h"
#include "field.h"

#include "test_coords_field.h"
#include "tests.h"

static int do_test0(pe_t * pe);
static int do_test1(pe_t * pe);
static int do_test3(pe_t * pe);
static int do_test5(pe_t * pe);
static int do_test_io(pe_t * pe, int nf, int io_format_in, int io_format_out);
static int test_field_halo(cs_t * cs, field_t * phi);

int do_test_device1(pe_t * pe);
int test_field_halo_create(pe_t * pe);

__global__ void do_test_field_kernel1(field_t * phi);

/*****************************************************************************
 *
 *  test_field_suite
 *
 *****************************************************************************/

int test_field_suite(void) {

  pe_t * pe = NULL;

  pe_create(MPI_COMM_WORLD, PE_QUIET, &pe);

  do_test0(pe);
  do_test1(pe);
  do_test3(pe);
  do_test5(pe);
  do_test_device1(pe);

  do_test_io(pe, 1, IO_FORMAT_ASCII_SERIAL, IO_FORMAT_ASCII);
  do_test_io(pe, 1, IO_FORMAT_BINARY_SERIAL, IO_FORMAT_BINARY);
  do_test_io(pe, 5, IO_FORMAT_ASCII_SERIAL, IO_FORMAT_ASCII);
  do_test_io(pe, 5, IO_FORMAT_BINARY_SERIAL, IO_FORMAT_BINARY);

  test_field_halo_create(pe);

  pe_info(pe, "PASS     ./unit/test_field\n");
  pe_free(pe);

  return 0;
}

/*****************************************************************************
 *
 *  do_test0
 *
 *  Small system test.
 *
 *****************************************************************************/

static int do_test0(pe_t * pe) {

  int nfref = 1;
  int nhalo = 2;
  int ntotal[3] = {8, 8, 8};

  cs_t * cs = NULL;
  field_t * phi = NULL;
  field_options_t opts = field_options_ndata_nhalo(nfref, nhalo);

  assert(pe);
  
  cs_create(pe, &cs);
  cs_nhalo_set(cs, nhalo);
  cs_ntotal_set(cs, ntotal);
  cs_init(cs);

  field_create(pe, cs, NULL, "phi", &opts, &phi);

  /* Halo */
  test_field_halo(cs, phi);

  field_free(phi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  do_test1
 *
 *  Scalar order parameter.
 *
 *****************************************************************************/

int do_test1(pe_t * pe) {

  int nfref = 1;
  int nf;
  int nhalo = 2;
  int index = 1;
  double ref;
  double value;

  cs_t * cs = NULL;
  field_t * phi = NULL;
  field_options_t opts = field_options_ndata_nhalo(nfref, nhalo);

  assert(pe);

  cs_create(pe, &cs);
  cs_nhalo_set(cs, nhalo);
  cs_init(cs);

  field_create(pe, cs, NULL, "phi", &opts, &phi);
  assert(phi);

  field_nf(phi, &nf);
  assert(nf == nfref);

  ref = 1.0;
  field_scalar_set(phi, index, ref);
  field_scalar(phi, index, &value);
  assert(fabs(value - ref) < DBL_EPSILON);

  ref = -1.0;
  field_scalar_array_set(phi, index, &ref);
  field_scalar_array(phi, index, &value);
  assert(fabs(value - ref) < DBL_EPSILON);

  ref = 1.0/3.0;
  field_scalar_set(phi, index, ref);
  field_scalar(phi, index, &value);
  assert(fabs(value - ref) < DBL_EPSILON);

  /* Halo */
  test_field_halo(cs, phi);

  field_free(phi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  do_test_device1
 *
 *****************************************************************************/

int do_test_device1(pe_t * pe) {

  int nfref = 1;
  int nf;
  int nhalo = 2;
  dim3 nblk, ntpb;

  cs_t * cs = NULL;
  field_t * phi = NULL;
  field_options_t opts = field_options_ndata_nhalo(nfref, nhalo);

  assert(pe);

  cs_create(pe, &cs);
  cs_nhalo_set(cs, nhalo);
  cs_init(cs);

  field_create(pe, cs, NULL, "phi", &opts, &phi);
  assert(phi);

  field_nf(phi, &nf);
  assert(nf == nfref);

  kernel_launch_param(1, &nblk, &ntpb);
  ntpb.x = 1;

  tdpLaunchKernel(do_test_field_kernel1, nblk, ntpb, 0, 0, phi->target);
  tdpDeviceSynchronize();

  field_free(phi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  do_test_field_kernel1
 *
 *****************************************************************************/

__global__ void do_test_field_kernel1(field_t * phi) {

  int nf;
  int index = 1;
  int nsites;
  double q;
  double qref = 1.2;

  assert(phi);

  field_nf(phi, &nf);
  assert(nf == 1);

  cs_nsites(phi->cs, &nsites);
  assert(phi->nsites == nsites);

  field_scalar_set(phi, index, qref);
  field_scalar(phi, index, &q);
  assert(fabs(q - qref) < DBL_EPSILON);

  return;
}

/*****************************************************************************
 *
 *  do_test3
 *
 *  Vector order parameter.
 *
 *****************************************************************************/

static int do_test3(pe_t * pe) {

  int nfref = 3;
  int nf;
  int nhalo = 1;
  int index = 1;
  double ref[3] = {1.0, 2.0, 3.0};
  double value[3];
  double array[3];

  cs_t * cs = NULL;
  field_t * phi = NULL;
  field_options_t opts = field_options_ndata_nhalo(nfref, nhalo);

  assert(pe);

  cs_create(pe, &cs);
  cs_nhalo_set(cs, nhalo);
  cs_init(cs);

  field_create(pe, cs, NULL, "p", &opts, &phi);
  assert(phi);

  field_nf(phi, &nf);
  assert(nf == nfref);

  field_vector_set(phi, index, ref);
  field_vector(phi, index, value);
  assert(fabs(value[0] - ref[0]) < DBL_EPSILON);
  assert(fabs(value[1] - ref[1]) < DBL_EPSILON);
  assert(fabs(value[2] - ref[2]) < DBL_EPSILON);

  field_scalar_array(phi, index, array);
  assert(fabs(array[0] - ref[0]) < DBL_EPSILON);
  assert(fabs(array[1] - ref[1]) < DBL_EPSILON);
  assert(fabs(array[2] - ref[2]) < DBL_EPSILON);

  /* Halo */
  test_field_halo(cs, phi);

  field_free(phi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  do_test5
 *
 *  Tensor order parameter.
 *
 *****************************************************************************/

static int do_test5(pe_t * pe) {

  int nfref = 5;
  int nf;
  int nhalo = 1;
  int index = 1;
  double qref[3][3] = {{1.0, 2.0, 3.0}, {2.0, 4.0, 5.0}, {3.0, 5.0, -5.0}};
  double qvalue[3][3];
  double array[NQAB];

  cs_t * cs = NULL;
  field_t * phi = NULL;
  field_options_t opts = field_options_ndata_nhalo(nfref, nhalo);

  assert(pe);

  cs_create(pe, &cs);
  cs_nhalo_set(cs, nhalo);
  cs_init(cs);

  field_create(pe, cs, NULL, "q", &opts, &phi);
  assert(phi);

  field_nf(phi, &nf);
  assert(nf == nfref);

  field_tensor_set(phi, index, qref);
  field_tensor(phi, index, qvalue);
  assert(fabs(qvalue[X][X] - qref[X][X]) < DBL_EPSILON);
  assert(fabs(qvalue[X][Y] - qref[X][Y]) < DBL_EPSILON);
  assert(fabs(qvalue[X][Z] - qref[X][Z]) < DBL_EPSILON);
  assert(fabs(qvalue[Y][X] - qref[Y][X]) < DBL_EPSILON);
  assert(fabs(qvalue[Y][Y] - qref[Y][Y]) < DBL_EPSILON);
  assert(fabs(qvalue[Y][Z] - qref[Y][Z]) < DBL_EPSILON);
  assert(fabs(qvalue[Z][X] - qref[Z][X]) < DBL_EPSILON);
  assert(fabs(qvalue[Z][Y] - qref[Z][Y]) < DBL_EPSILON);
  assert(fabs(qvalue[Z][Z] - qref[Z][Z]) < DBL_EPSILON);

  /* This is the upper trianle minus the ZZ component */

  field_scalar_array(phi, index, array);
  assert(fabs(array[XX] - qref[X][X]) < DBL_EPSILON);
  assert(fabs(array[XY] - qref[X][Y]) < DBL_EPSILON);
  assert(fabs(array[XZ] - qref[X][Z]) < DBL_EPSILON);
  assert(fabs(array[YY] - qref[Y][Y]) < DBL_EPSILON);
  assert(fabs(array[YZ] - qref[Y][Z]) < DBL_EPSILON);

  /* Halo */
  test_field_halo(cs, phi);

  field_free(phi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  test_field_halo
 *
 *****************************************************************************/

static int test_field_halo(cs_t * cs, field_t * phi) {

  assert(phi);
  
  test_coords_field_set(cs, phi->nf, phi->data, MPI_DOUBLE, test_ref_double1);
  field_memcpy(phi, tdpMemcpyHostToDevice);
 
  field_halo_swap(phi, FIELD_HALO_TARGET);

  field_memcpy(phi, tdpMemcpyDeviceToHost);
  test_coords_field_check(cs, phi->nhcomm, phi->nf, phi->data, MPI_DOUBLE,
			  test_ref_double1);

  return 0;
} 

/*****************************************************************************
 *
 *  do_test_io
 *
 *****************************************************************************/

static int do_test_io(pe_t * pe, int nf, int io_format_in, int io_format_out) {

  int ntotal[3] = {16, 16, 8};
  int grid[3] = {1, 1, 1};
  int nhalo;
  const char * filename = "phi-test-io";

  MPI_Comm comm;

  cs_t * cs = NULL;
  field_t * phi = NULL;
  io_info_t * iohandler = NULL;
  field_options_t opts = field_options_default();

  assert(pe);

  cs_create(pe, &cs);
  cs_ntotal_set(cs, ntotal);
  cs_init(cs);
  cs_nhalo(cs, &nhalo);
  cs_cart_comm(cs, &comm);

  opts.ndata = nf;
  opts.nhcomm = nhalo;
  field_create(pe, cs, NULL, "phi-test", &opts, &phi);

  field_init_io_info(phi, grid, io_format_in, io_format_out);

  test_coords_field_set(cs, nf, phi->data, MPI_DOUBLE, test_ref_double1);

  field_io_info(phi, &iohandler);
  assert(iohandler);

  io_write_data(iohandler, filename, phi);

  field_free(phi);
  MPI_Barrier(comm);

  field_create(pe, cs, NULL, "phi-test", &opts,&phi);
  field_init_io_info(phi, grid, io_format_in, io_format_out);

  field_io_info(phi, &iohandler);
  assert(iohandler);

  /* Make sure the input format is handled correctly. */
  io_info_format_in_set(iohandler, io_format_in);
  io_info_single_file_set(iohandler);

  io_read_data(iohandler, filename, phi);

  field_halo(phi);
  test_coords_field_check(cs, 0, nf, phi->data, MPI_DOUBLE, test_ref_double1);

  MPI_Barrier(comm);

  io_remove(filename, iohandler);
  io_remove_metadata(iohandler, "phi-test");

  field_free(phi);
  cs_free(cs);

  return 0;
}

/*****************************************************************************
 *
 *  test_field_halo_create
 *
 *****************************************************************************/

int test_field_halo_create(pe_t * pe) {

  cs_t * cs = NULL;
  field_t * field = NULL;
  field_options_t opts = field_options_default();

  field_halo_t h = {0};

  {
    int nhalo = 2;
    int ntotal[3] = {32, 16, 8};
    cs_create(pe, &cs);
    cs_nhalo_set(cs, nhalo);
    cs_ntotal_set(cs, ntotal);
    cs_init(cs);
  }

  opts.ndata = 2;
  opts.nhcomm = 2;
  field_create(pe, cs, NULL, "halotest", &opts, &field);

  field_halo_create(field, &h);

  test_coords_field_set(cs, 2, field->data, MPI_DOUBLE, test_ref_double1);
  field_halo_post(field, &h);
  field_halo_wait(field, &h);
  test_coords_field_check(cs, 2, 2, field->data, MPI_DOUBLE, test_ref_double1);

  field_halo_free(&h);

  field_free(field);
  cs_free(cs);

  return 0;
}
