#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([fastr], [1.0])
AM_INIT_AUTOMAKE([-Wall foreign 1.10])
AC_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Search nvcc compiler
        AC_PATH_PROG(NVCC, nvcc, "no")
        AS_IF([test "x$NVCC" = "xno"],[
            AC_MSG_ERROR([NVCC compiler not found!])
            ])

# Check nvcc version, should be 4.0
        AS_IF([nvcc --version | grep -q "release 4.0"],
            [],
            [AC_MSG_WARN([NVCC compiler version is NOT 3.0!])
            ])

CUDA_ARCH=" -arch=sm_20"

# Set CUDA_CFLAGS to $NVCC, where substring "bin/nvcc"
# is substituted by "include".
CUDA_CFLAGS=" -I${NVCC//bin\/nvcc/include} -Xcompiler -fPIC -Xptxas -v "
CPPFLAGS=" -I${NVCC//bin\/nvcc/include} "

#Set CUDA_CFLAGS to $NVCC, where substring "bin/nvcc"
#is substituted by "lib".
CUDA_LIBS=" -L${NVCC//bin\/nvcc/lib}"

# If $build_cpu contains "_64", append "64" to CUDA_LIBS
        AS_IF([uname -m |  grep -q "_64"],
                [CUDA_LIBS+="64"])

# Append " -lcudart" to CUDA_LIBS
        CUDA_LIBS+=" -lcudart "

# Symbolize that cuda is wanted
        with_cuda=$enableval

# Set this conditional if cuda is wanted
AM_CONDITIONAL([WANT_CUDA], [test -n "$with_cuda"])

AC_ARG_WITH([opencl_inc], AC_HELP_STRING([--with-opencl-inc],[path to OpenCL include]),
	[OPENCL_INC_PREFIX=$withval],[OPENCL_INC_PREFIX=])
if test ! -z "$OPENCL_INC_PREFIX" ; then
   CPPFLAGS+=-I${OPENCL_INC_PREFIX}" "
    if ! test -d ${OPENCL_INC_PREFIX}; then
        AC_MSG_ERROR([${OPENCL_INC_PREFIX} does not exist!])
    fi
fi

AC_ARG_WITH([bamtools], AC_HELP_STRING([--with-bamtools],[path to bamtools]),
	[BAMTOOLS_PREFIX=$withval],[BAMTOOLS_PREFIX=])
if test ! -z "$BAMTOOLS_PREFIX" ; then
   LDFLAGS+=-L${BAMTOOLS_PREFIX}"/lib " 
   CPPFLAGS+=-I${BAMTOOLS_PREFIX}"/include " 
fi

# Checks for libraries.
AC_CHECK_LIB(OpenCL,clGetPlatformIDs, LIBS+=" -lOpenCL",
	AC_MSG_ERROR([OpenCL library not found]))

AC_CHECK_LIB(bamtools, main, LIBS+=" -lbamtools",
	AC_MSG_ERROR([bamtools library not found]))

AC_CHECK_LIB(z, inflate, LIBS+=" -lz",
	AC_MSG_ERROR([zlib library not found]))

# Checks for header files.
AC_CHECK_HEADERS([opencl.h limits.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strdup])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
