#include "global.h"
#include "kernels.cu"
#include "swat_cuda.h"

struct timeval start_t1[10], end_t1[10];
#define TIMER_START(t) gettimeofday(&(start_t1[t]), NULL)
#define TIMER_END(t) gettimeofday(&(end_t1[t]), NULL)
#define MICRO_SECONDS(t) ((end_t1[t].tv_sec - start_t1[t].tv_sec)*1e6 + (end_t1[t].tv_usec - start_t1[t].tv_usec))

double swat_kernel_t = 0;
CigarOp makeElement(int state, int segment_length)
{
    char o = '\0';
    switch(state) {
    case MSTATE:
        o = 'M';
        break;
    case ISTATE:
        o = 'I';
        break;
    case DSTATE:
        o = 'D';
        break;
    case 'S':
        o = 'S';
        break;
    }
    return CigarOp(o, segment_length);
}


int swat(const byte a[], unsigned int an, const int a_offset[], const int ans[], const int n_a,
         const byte b[], unsigned int bn,  const int b_offset[], const int bns[], const int n_b,
         float openPenalty, float extensionPenalty, float match, float mismatch, vector< vector<CigarOp> >& Cigars, int *alignment_offset, int max_len)
{
    cudaError_t cudaRes0, cudaRes1;
    int blockSize = 128;
    int subSequenceNum = n_b;
    char *pathFlagD;
    char *extFlagD;
    int maxElemNum = (max_len + 1);
    int maxMatrix = maxElemNum * 101;
    int *alignment_offsetD;

    for(int i = 0; i < n_a; i++) {
        if(ans[i] > MAX_LEN) {
            printf("an MAX: %d\n", ans[i]);
        }
    }

    for(int i = 0; i < n_b; i++) {
        if(bns[i] > MAX_LEN) {
            printf("bn MAX: %d\n", bns[i]);
        }
    }

    cudaRes0 = cudaMalloc((void **)&alignment_offsetD, sizeof(int) * subSequenceNum);
    if(cudaRes0 != cudaSuccess) {
        printf("Allocate sequence buffer on device error, %d\n",
               cudaRes0);
        return 1;
    }
    char *seq1D, *seq2D;

    cudaRes0 = cudaMalloc((void **)&seq1D, sizeof(char) * an);
    cudaRes1 = cudaMalloc((void **)&seq2D, sizeof(char) * bn);
    if(cudaRes0 != cudaSuccess ||
            cudaRes1 != cudaSuccess) {
        printf("Allocate sequence buffer on device error, %d, %d\n",
               cudaRes0,
               cudaRes1);
        return 1;
    }

    int *seq1_offD, *seq2_offD;

    cudaRes0 = cudaMalloc((void **)&seq1_offD, sizeof(int) * subSequenceNum);
    cudaRes1 = cudaMalloc((void **)&seq2_offD, sizeof(int) * subSequenceNum);
    if(cudaRes0 != cudaSuccess ||
            cudaRes1 != cudaSuccess) {
        printf("Allocate sequence buffer on device error, %d, %d\n",
               cudaRes0,
               cudaRes1);
        return 1;
    }


    int *rowNumsD, *columnNumsD;

    cudaRes0 = cudaMalloc((void **)&rowNumsD, sizeof(int) * subSequenceNum);
    cudaRes1 = cudaMalloc((void **)&columnNumsD, sizeof(int) * subSequenceNum);
    if(cudaRes0 != cudaSuccess ||
            cudaRes1 != cudaSuccess) {
        printf("Allocate sequence buffer on device error, %d, %d\n",
               cudaRes0,
               cudaRes1);
        return 1;
    }

    cudaRes0 = cudaMalloc((void **)&pathFlagD, sizeof(char) * maxMatrix * subSequenceNum);
    cudaRes1 = cudaMalloc((void **)&extFlagD,  sizeof(char) * maxMatrix * subSequenceNum);

    if((cudaRes0 != cudaSuccess) ||
            (cudaRes1 != cudaSuccess)) {
        printf("cuda allocate DP matrices on device  error: %d, %d\n",
               cudaRes0,
               cudaRes1);
        return 1;
    }

    SEQ_ALIGN *element, *elementD;
    element = new SEQ_ALIGN[100 * subSequenceNum];

    cudaRes0 = cudaMalloc((void **)&elementD, sizeof(SEQ_ALIGN) * 100 * subSequenceNum);
    if(cudaRes0 != cudaSuccess) {
        printf("Allocate maxInfo on device error!\n");
        return 1;
    }

    int *n_element, *n_elementD;
    n_element = new int[subSequenceNum];

    cudaRes0 = cudaMalloc((void **)&n_elementD, sizeof(int) * subSequenceNum);
    if(cudaRes0 != cudaSuccess) {
        printf("Allocate maxInfo on device error!\n");
        return 1;
    }

    //Initialize DP matrices
    cudaMemset(pathFlagD, 0, maxMatrix * sizeof(char) * subSequenceNum);
    cudaMemset(extFlagD,  0, maxMatrix * sizeof(char) * subSequenceNum);

    cudaRes0 = cudaGetLastError();
    if(cudaRes0 != cudaSuccess) {
        printf("cudaMemset Error: %d %s\n", cudaRes0, cudaGetErrorString(cudaRes0));
    }

    //copy input sequences to device
    cudaMemcpy(seq1D, a, an * sizeof(char), cudaMemcpyHostToDevice);
    cudaMemcpy(seq2D, b, bn * sizeof(char), cudaMemcpyHostToDevice);
    cudaMemcpy(seq1_offD, a_offset, subSequenceNum * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(seq2_offD, b_offset, subSequenceNum * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(rowNumsD, ans, subSequenceNum * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(columnNumsD, bns, subSequenceNum * sizeof(int), cudaMemcpyHostToDevice);

    cudaRes0 = cudaGetLastError();
    if(cudaRes0 != cudaSuccess) {
        printf("cudaMemcpy Error: %d %s\n", cudaRes0, cudaGetErrorString(cudaRes0));
    }
    TIMER_START(0);
    MatchString <<< subSequenceNum, blockSize>>>(
        pathFlagD,
        extFlagD,
        seq1D,
        seq2D,
        seq1_offD,
        seq2_offD,
        openPenalty,
        extensionPenalty,
        match,
        mismatch,
        rowNumsD,
        columnNumsD,
        elementD,
        n_elementD,
        alignment_offsetD,
        maxMatrix
    );
    cudaThreadSynchronize();
    TIMER_END(0);
    swat_kernel_t += MICRO_SECONDS(0);
    cudaRes0 = cudaGetLastError();
    if(cudaRes0 != 0)
        printf("Error: %d %s\n", cudaRes0, cudaGetErrorString(cudaRes0));

    //copy matrix score structure back
    cudaMemcpy(element,  elementD, sizeof(SEQ_ALIGN) * 100 * subSequenceNum, cudaMemcpyDeviceToHost);
    cudaMemcpy(n_element,  n_elementD, sizeof(int) * subSequenceNum, cudaMemcpyDeviceToHost);
    cudaMemcpy(alignment_offset,  alignment_offsetD, sizeof(int) * subSequenceNum, cudaMemcpyDeviceToHost);

    for(int k = 0; k < subSequenceNum; k++) {
        vector<CigarOp> lce;
        for(int t = n_element[k] - 1; t >= 0; t--) {
            lce.push_back(makeElement(element[t + k * 100].o, element[t + k * 100].length));
        }
        Cigars.push_back(lce);

    }
    cudaFree(seq1D);
    cudaFree(seq2D);
    cudaFree(pathFlagD);
    cudaFree(extFlagD);
    cudaFree(alignment_offsetD);
    free(element);
    cudaFree(elementD);
    free(n_element);
    cudaFree(n_elementD);
    return 0;
}


