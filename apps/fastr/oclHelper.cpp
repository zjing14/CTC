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
#include "oclHelper.h"
#include <iostream>

using namespace std;

OclEnv::OclEnv(bool use_gpu)
{
    // Retrieve an OpenCL platform
    //    int err = clGetPlatformIDs(1, &platform_id, NULL);
    int err;
    cl_uint NumPlatforms;
    clGetPlatformIDs(0, NULL, &NumPlatforms);

    cl_platform_id *plat_ids = new cl_platform_id[NumPlatforms];
    err = clGetPlatformIDs(NumPlatforms, plat_ids, NULL);
    CHECKERR(err);

    bool found_gpu = false;
    bool found_cpu = false;
    cl_device_id dev_id;
    cl_device_id cpu_id;
    int i;
    for(i = 0; i < NumPlatforms; i++) {
        // Connect to a compute device
        if(use_gpu) {
            err = clGetDeviceIDs(plat_ids[i], CL_DEVICE_TYPE_GPU, 1, &dev_id, NULL);
            if(err == CL_SUCCESS) {
                device_id = dev_id;
                platform_id = plat_ids[i];
                found_gpu = true;
                break;
            }
        }

        if(!found_gpu) {
            err = clGetDeviceIDs(plat_ids[i], CL_DEVICE_TYPE_CPU, 1, &dev_id, NULL);
            if(err == CL_SUCCESS) {
                platform_id = plat_ids[i];
                found_cpu = true;
                cpu_id = dev_id;
                if(!use_gpu) {
                    break;
                }
            }
        }
    }

    if(use_gpu && !found_gpu) {
        if(found_cpu) {
            cout << "WARNING: Unable to find GPU. Using CPU instead." << endl;
            device_id = cpu_id;
        } else {
            cout << "ERROR: Unable to find suitable device. Aborting" << endl;
            exit(1);
        }
    }

    delete[] plat_ids;

    char platformName[100];
    err = clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, sizeof(platformName), platformName, NULL);
    CHECKERR(err);
    printf("Platform: %s\n", platformName);

    char DeviceName[100];
    err = clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(DeviceName), DeviceName, NULL);
    CHECKERR(err);
    printf("Device: %s\n", DeviceName);

    // Create a compute context
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    CHECKERR(err);

    // Create a command queue
    commands = clCreateCommandQueue(context, device_id, 0, &err);
    CHECKERR(err); 
}

cl_program oclBuildProgram(const char *source, cl_context context, cl_device_id device_id)
{
    int err;
    FILE *kernelFile = NULL;
    kernelFile = fopen(source, "r");
    if(!kernelFile) {
        printf("Error reading kernel file %s.\n", source), exit(0);
    }
    fseek(kernelFile, 0, SEEK_END);
    size_t kernelLength = (size_t) ftell(kernelFile);
    char *kernelSource = (char *) calloc(1, sizeof(char) * (kernelLength + 1));
    rewind(kernelFile);
    fread((void *) kernelSource, kernelLength, 1, kernelFile);
    fclose(kernelFile);

    // Create the compute program from the source buffer
    cl_program program = clCreateProgramWithSource(context, 1, (const char **) &kernelSource, NULL, &err);
    CHECKERR(err);

    free(kernelSource);

    // Build the program executable
    err = clBuildProgram(program, 0, NULL, "-cl-nv-verbose", NULL, NULL);
    if(err == CL_BUILD_PROGRAM_FAILURE)
    {
        char *log;
        size_t logLen;
        err = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &logLen);
        log = (char *) malloc(sizeof(char) * logLen);
        err = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, logLen, (void *) log, NULL);
        fprintf(stderr, "CL Error %d: Failed to build program! Log:\n%s", err, log);
        free(log);
        exit(1);
    }
    CHECKERR(err);

    return program;
}

cl_kernel oclCreateKernel(cl_program program, const char *kernel_name)
{
    int err;
    // Create the compute kernel in the program we wish to run
    cl_kernel kernel = clCreateKernel(program, kernel_name, &err);
    CHECKERR(err);
    return kernel;
}

int oclMemset(cl_command_queue commands, cl_mem mem, unsigned char val, int size)
{
    int err;
    void *ptr = clEnqueueMapBuffer(commands, mem, CL_TRUE, CL_MAP_WRITE, 0, size, 0, NULL, NULL, &err);
    memset(ptr, val, size);
    err |= clEnqueueUnmapMemObject(commands, mem, ptr, 0, NULL, NULL);
    return err;
}


char *print_cl_errstring(cl_int err)
{
    switch(err) {
    case CL_SUCCESS:
        return strdup("Success!");
    case CL_DEVICE_NOT_FOUND:
        return strdup("Device not found.");
    case CL_DEVICE_NOT_AVAILABLE:
        return strdup("Device not available");
    case CL_COMPILER_NOT_AVAILABLE:
        return strdup("Compiler not available");
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
        return strdup("Memory object allocation failure");
    case CL_OUT_OF_RESOURCES:
        return strdup("Out of resources");
    case CL_OUT_OF_HOST_MEMORY:
        return strdup("Out of host memory");
    case CL_PROFILING_INFO_NOT_AVAILABLE:
        return strdup("Profiling information not available");
    case CL_MEM_COPY_OVERLAP:
        return strdup("Memory copy overlap");
    case CL_IMAGE_FORMAT_MISMATCH:
        return strdup("Image format mismatch");
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
        return strdup("Image format not supported");
    case CL_BUILD_PROGRAM_FAILURE:
        return strdup("Program build failure");
    case CL_MAP_FAILURE:
        return strdup("Map failure");
    case CL_INVALID_VALUE:
        return strdup("Invalid value");
    case CL_INVALID_DEVICE_TYPE:
        return strdup("Invalid device type");
    case CL_INVALID_PLATFORM:
        return strdup("Invalid platform");
    case CL_INVALID_DEVICE:
        return strdup("Invalid device");
    case CL_INVALID_CONTEXT:
        return strdup("Invalid context");
    case CL_INVALID_QUEUE_PROPERTIES:
        return strdup("Invalid queue properties");
    case CL_INVALID_COMMAND_QUEUE:
        return strdup("Invalid command queue");
    case CL_INVALID_HOST_PTR:
        return strdup("Invalid host pointer");
    case CL_INVALID_MEM_OBJECT:
        return strdup("Invalid memory object");
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
        return strdup("Invalid image format descriptor");
    case CL_INVALID_IMAGE_SIZE:
        return strdup("Invalid image size");
    case CL_INVALID_SAMPLER:
        return strdup("Invalid sampler");
    case CL_INVALID_BINARY:
        return strdup("Invalid binary");
    case CL_INVALID_BUILD_OPTIONS:
        return strdup("Invalid build options");
    case CL_INVALID_PROGRAM:
        return strdup("Invalid program");
    case CL_INVALID_PROGRAM_EXECUTABLE:
        return strdup("Invalid program executable");
    case CL_INVALID_KERNEL_NAME:
        return strdup("Invalid kernel name");
    case CL_INVALID_KERNEL_DEFINITION:
        return strdup("Invalid kernel definition");
    case CL_INVALID_KERNEL:
        return strdup("Invalid kernel");
    case CL_INVALID_ARG_INDEX:
        return strdup("Invalid argument index");
    case CL_INVALID_ARG_VALUE:
        return strdup("Invalid argument value");
    case CL_INVALID_ARG_SIZE:
        return strdup("Invalid argument size");
    case CL_INVALID_KERNEL_ARGS:
        return strdup("Invalid kernel arguments");
    case CL_INVALID_WORK_DIMENSION:
        return strdup("Invalid work dimension");
    case CL_INVALID_WORK_GROUP_SIZE:
        return strdup("Invalid work group size");
    case CL_INVALID_WORK_ITEM_SIZE:
        return strdup("Invalid work item size");
    case CL_INVALID_GLOBAL_OFFSET:
        return strdup("Invalid global offset");
    case CL_INVALID_EVENT_WAIT_LIST:
        return strdup("Invalid event wait list");
    case CL_INVALID_EVENT:
        return strdup("Invalid event");
    case CL_INVALID_OPERATION:
        return strdup("Invalid operation");
    case CL_INVALID_GL_OBJECT:
        return strdup("Invalid OpenGL object");
    case CL_INVALID_BUFFER_SIZE:
        return strdup("Invalid buffer size");
    case CL_INVALID_MIP_LEVEL:
        return strdup("Invalid mip-map level");
    default:
        return strdup("Unknown");
    }
}



