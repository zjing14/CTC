#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([vt_lr], [1.0])
AM_INIT_AUTOMAKE([-Wall foreign 1.10])
AC_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

AC_ARG_WITH([opencl_lib], AC_HELP_STRING([--with-opencl-lib],[path to OpenCL library]),
	[OPENCL_PREFIX=$withval],[OPENCL_PREFIX=])
if test ! -z "$OPENCL_PREFIX" ; then
    if ! test -d ${OPENCL_PREFIX}; then
        AC_MSG_ERROR([${OPENCL_PREFIX} does not exist!])
    fi
    if ! test -s ${OPENCL_PREFIX}/libOpenCL.so; then
        AC_MSG_ERROR([OpenCL library does not exist!])
    fi
   LDFLAGS+=-L${OPENCL_PREFIX}" "
fi

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
