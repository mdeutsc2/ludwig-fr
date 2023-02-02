##############################################################################
#
#  unix-nvv-parallel.mk
#
#  unix nvcc parallel build.
#
##############################################################################

BUILD   = parallel
MODEL   = -D_D3Q15_

CC      = nvcc
#CFLAGS  = -ccbin=icpc -DADDR_SOA -DNDDEBUG -arch=native -x cu -dc
CFLAGS  = -DADDR_SOA -DNDDEBUG -arch=native -x cu -dc

AR      = ar
ARFLAGS = -cru
LDFLAGS = -arch=native
#LDFLAGS = -ccbin=icpc -arch=all-major

MPI_HOME		  = /opt/nvidia/hpc_sdk/Linux_x86_64/23.1/comm_libs/mpi
MPI_INC_PATH      = -I$(MPI_HOME)/include
MPI_LIB_PATH      = -L$(MPI_HOME)/lib -lmpi
#MPI_LIB           = -lmpi

LAUNCH_SERIAL_CMD =
LAUNCH_MPIRUN_CMD = 
#LAUNCH_MPIRUN_CMD = mpirun
#MPIRUN_NTRASK_FLAG = -np
