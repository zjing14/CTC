/**********************************************************************************
* Copyright (c) 2012 by Virginia Polytechnic Institute and State University. 
* 
* The local realignment portion of the code is derived from the Indel Realigner
* code of the GATK project, and the I/O of bam files is extended from the
* bamtools package, which is distributed under the MIT license. The licenses of
* GATK and bamtools are included below.
* 
* *********** GATK LICENSE ***************
* Copyright (c) 2010, The Broad Institute 
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
* of the Software, and to permit persons to whom the Software is furnished to do
* so, subject to the following conditions: 
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software. 
* 
* *********** BAMTOOLS LICENSE ***************
* The MIT License
* 
* Copyright (c) 2009-2010 Derek Barnett, Erik Garrison, Gabor Marth, Michael Stromberg
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
* of the Software, and to permit persons to whom the Software is furnished to do
* so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software. 
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
**********************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#define USEGPU 1
#ifndef OCLHELPERS_H
#define OCLHELPERS_H

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#define CHECKERR(err) \
    if (err != CL_SUCCESS) \
{ \
    fprintf(stderr, "Error: %d %s in line: %d file: %s\n", err, print_cl_errstring(err), __LINE__, __FILE__);\
    exit(1); \
}
char *print_cl_errstring(cl_int err);

class OclEnv
{
public:
    cl_platform_id platform_id;
    cl_device_id device_id;
    cl_context context;
    cl_command_queue commands;
    OclEnv(bool use_gpu);
};

int oclMemset(cl_command_queue commands, cl_mem mem, unsigned char val, int size);

cl_program oclBuildProgram(const char *source, cl_context context, cl_device_id device_id);
cl_kernel oclCreateKernel(cl_program program, const char *kernel_name);



cl_device_id GetDevice(int platform, int device);



#endif
