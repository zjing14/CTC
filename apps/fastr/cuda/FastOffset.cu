#include <stdlib.h>
#include <stdio.h>
#include "FastOffset.h"


#define GetreadSeq(a) &(readSeq[a * 100])
#define Getquals(a) &(quals[a * 100])
#define byte char
#define MAX_QUAL 99
#define MAX_ReadLength 100

#define byte char
byte *dreadSeq;
byte *dquals;
byte *dref;
int *dScoreIndex;
int *dBestScore;
int *dReadLength;
int *doriginalAlignment;
int *dn_consensus;
int *daltReads;
int *drefLens;
char *dch_lookup;
extern int score_offset;

using namespace std;
using namespace BamTools;

cudaError_t cudaRes;

#define CHECKERR(cudaRes)\
{\
    if (cudaRes != cudaSuccess)\
    printf("cuda allocate on device error: %d in line:%d (%s)\n", cudaRes, __LINE__, __FILE__);\
}

#define CHECKERR1()\
{\
    cudaRes = cudaGetLastError();\
    if(cudaRes != cudaSuccess)\
    printf("cudaError: %d %s in line:%d (%s) \n", cudaRes, cudaGetErrorString(cudaRes), __LINE__, __FILE__);\
}


__device__ int mismatchQualitySumIgnoreCigar2_CUDA(byte *readSeq, byte *quals, byte *refSeq, int refLen, int refIndex, int getReadLength, char *ch_lookup, int quitAboveThisValue)
{
    int sum = 0;
    for(int readIndex = 0 ; readIndex < getReadLength; refIndex++, readIndex++) {
        if(refIndex >= refLen) {
            sum += MAX_QUAL;
            if(sum > quitAboveThisValue) {
                return sum;
            }
        } else {
            byte refChr = refSeq[refIndex];
            byte readChr = readSeq[readIndex];
            if(!ch_lookup[readChr] || !ch_lookup[refChr]) {
                continue;    // do not count Ns/Xs/etc ?
            }
            if((readChr != refChr)) {
                sum += (int)quals[readIndex];
                if(sum > quitAboveThisValue) {
                    return sum;
                }
            }
        }
    }
    return sum;
}


__device__ int mismatchQualitySumIgnoreCigar_CUDA(byte *readSeq, byte *quals, byte *refSeq, int refLen, int refIndex, int getReadLength, char *ch_lookup)
{
    int sum = 0;
    for(int readIndex = 0 ; readIndex < getReadLength; refIndex++, readIndex++) {
        if(refIndex >= refLen) {
            sum += MAX_QUAL;
        } else {
            byte refChr = refSeq[refIndex];
            byte readChr = readSeq[readIndex];
            if(!ch_lookup[readChr] || !ch_lookup[refChr]) {
                continue;    // do not count Ns/Xs/etc ?
            }
            if((readChr != refChr)) {
                sum += (int)quals[readIndex];
            }
        }
    }
    return sum;
}

__global__ void findBestOffset_CUDA(byte *ref_g,  byte *readSeq_g,  byte *quals_g,  int *ReadLength_g,  int *originalAlignment_g,  int *bestScore_g,
                                    int *bestIndex_g,  char *ch_lookup, int iter_num,  int *n_consensus, int *altReads,  int *refLens)
{
    int b = blockIdx.x;
    int i = threadIdx.x;
    int ii = threadIdx.x + blockDim.x;
    __shared__ int score[1024];
    __shared__ int scoreindex[1024];
    int ref_offset = 0;
    int quals_offset = 0;
    int ReadLength_offset = 0;
    int originalAlignment_offset = 0;
    int score_offset = 0;
    int n_refLen = 0;
    int readSeq_offset = 0;

    for(int t = 0; t < iter_num; t++) {
        for(int k = 0; k < n_consensus[t]; k++) {
            if(b < altReads[t]) {

                score[i] = INT_MAX;
                score[ii] = INT_MAX;
                scoreindex[i] = i;
                scoreindex[ii] = ii;

                byte *ref = ref_g + ref_offset;
                byte *readSeq = readSeq_g + readSeq_offset;
                byte *quals = quals_g + quals_offset;
                int *ReadLength = ReadLength_g + ReadLength_offset;
                int *originalAlignment = originalAlignment_g + originalAlignment_offset;
                int *bestScore = bestScore_g + score_offset;
                int *bestIndex = bestIndex_g + score_offset;

                if(i == 0) {
                    bestScore[b] = mismatchQualitySumIgnoreCigar_CUDA(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], originalAlignment[b], ReadLength[b], ch_lookup);
                    bestIndex[b] = originalAlignment[b];
                }

                __syncthreads();

                if(i < originalAlignment[b]) {
                    score[i] = mismatchQualitySumIgnoreCigar2_CUDA(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], i, ReadLength[b], ch_lookup, bestScore[b]);
                }
                if(ii < originalAlignment[b]) {
                    score[ii] = mismatchQualitySumIgnoreCigar2_CUDA(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], ii, ReadLength[b], ch_lookup, bestScore[b]);
                }

                __syncthreads();

                for(unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
                    if(i < s) {
                        if(score[i] > score[i + s] || (scoreindex[i + s] < scoreindex[i] && score[i] == score[i + s])) {
                            score[i] = score[i + s];
                            scoreindex[i] = scoreindex[i + s];
                        }
                        if(score[ii] > score[ii + s] || (scoreindex[ii + s] < scoreindex[ii] && score[ii] == score[ii + s])) {
                            score[ii] = score[ii + s];
                            scoreindex[ii] = scoreindex[ii + s];
                        }
                    }
                    __syncthreads();
                }

                if(i == 0) {
                    if(score[0] > score[blockDim.x] || (scoreindex[blockDim.x] < scoreindex[0] && score[0] ==  score[blockDim.x])) {
                        score[0] = score[blockDim.x];
                        scoreindex[0] = scoreindex[blockDim.x];
                    }
                    if(bestScore[b] > score[0]) {
                        bestScore[b] = score[0];
                        bestIndex[b] = scoreindex[0];
                    }
                }

                __syncthreads();

                score[i] = INT_MAX;
                scoreindex[i] = i ;
                score[ii] = INT_MAX;
                scoreindex[ii] = ii ;
                int maxPossibleStart = refLens[n_refLen] - ReadLength[b];
                maxPossibleStart = maxPossibleStart - (originalAlignment[b] + 1);

                if(i <= maxPossibleStart) {
                    score[i] = mismatchQualitySumIgnoreCigar2_CUDA(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], i + originalAlignment[b] + 1, ReadLength[b], ch_lookup, bestScore[b]);
                }
                if(ii <= maxPossibleStart) {
                    score[ii] = mismatchQualitySumIgnoreCigar2_CUDA(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], ii + originalAlignment[b] + 1, ReadLength[b], ch_lookup, bestScore[b]);
                }

                __syncthreads();

                for(unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
                    if(i < s) {
                        if(score[i] > score[i + s] || (scoreindex[i + s] < scoreindex[i] && score[i] == score[i + s])) {
                            score[i] = score[i + s];
                            scoreindex[i] = scoreindex[i + s];
                        }
                        if(score[ii] > score[ii + s] || (scoreindex[ii + s] < scoreindex[ii] && score[ii] == score[ii + s])) {
                            score[ii] = score[ii + s];
                            scoreindex[ii] = scoreindex[ii + s];
                        }
                    }
                    __syncthreads();
                }

                if(i == 0) {
                    if(score[0] > score[blockDim.x] || (scoreindex[blockDim.x] < scoreindex[0] && score[0] ==  score[blockDim.x])) {
                        score[0] = score[blockDim.x];
                        scoreindex[0] = scoreindex[blockDim.x];
                    }
                    if(bestScore[b] > score[0]) {
                        bestScore[b] = score[0];
                        bestIndex[b] = scoreindex[0] + originalAlignment[b] + 1;
                    }
                }
            }
            ref_offset += refLens[n_refLen];
            n_refLen += 1;
            score_offset += altReads[t];
        }
        readSeq_offset += altReads[t] * MAX_ReadLength;
        quals_offset += MAX_ReadLength * altReads[t];
        ReadLength_offset += altReads[t];
        originalAlignment_offset += altReads[t];
    }
}

void cpy2d_lookup(char *ch_lookup)
{
    cudaError_t cudaRes0;
    cudaRes0 = cudaMalloc((void **)&dch_lookup, sizeof(char) * 128);
    CHECKERR(cudaRes0);
    cudaMemcpy(dch_lookup, ch_lookup, sizeof(char) * 128, cudaMemcpyHostToDevice);
}

void free_lookup()
{
    cudaError_t cudaRes;
    cudaFree(dch_lookup);
    CHECKERR1();
}

void free_resource()
{
    cudaError_t cudaRes;
    cudaFree(dn_consensus);
    cudaFree(daltReads);
    cudaFree(drefLens);
    cudaFree(dScoreIndex);
    cudaFree(dBestScore);
    cudaFree(dref);
    cudaFree(dreadSeq);
    cudaFree(dquals);
    cudaFree(dReadLength);
    cudaFree(doriginalAlignment);
    CHECKERR1();
}



void clean_cuda(int iter_num, const size_t globalWorkSize,  const size_t localWorkSize, int *BestScore, int *ScoreIndex)
{
    cudaError_t cudaRes;
    findBestOffset_CUDA <<< globalWorkSize, localWorkSize>>> (dref,  dreadSeq,  dquals, dReadLength,  doriginalAlignment,
            dBestScore, dScoreIndex,  dch_lookup, iter_num,  dn_consensus, daltReads,  drefLens);
    cudaDeviceSynchronize();
    CHECKERR1();

    cudaMemcpy(BestScore, dBestScore, sizeof(int) * score_offset, cudaMemcpyDeviceToHost);
    CHECKERR1();

    cudaMemcpy(ScoreIndex, dScoreIndex, sizeof(int) * score_offset, cudaMemcpyDeviceToHost);
    CHECKERR1();
}

int clean_cuda_prep(vector<BamRegionData *> &reads, int iter_num, int **ScoreIndex, int **BestScore, size_t &globalWorkSize, size_t &localWorkSize)
{

    int t_altReads = 0;
    int t_refLen = 0;
    int t_score = 0;
    int n_refLen = 0;
    for(int i = 0; i < iter_num; i++) {
        vector<Consensus *> finalConsensus;
        vector<Consensus *>::iterator itr;
        BamRegionData *rd = reads[i];
        ConsensusHashTable *altConsensus = rd->altConsensus;
        altConsensus->toArray(finalConsensus);
        vector<AlignedRead *>& altReads = rd->altReads;
        t_altReads += altReads.size();
        for(itr = finalConsensus.begin(); itr != finalConsensus.end(); itr++) {
            Consensus *consensus = *itr;
            t_refLen += consensus->strLen;
            t_score += altReads.size();
            n_refLen += 1;
        }
    }
    score_offset = t_score;
    if(t_altReads == 0 || t_refLen == 0 || t_score == 0) {
        return 0;
    }


    *ScoreIndex = (int *)malloc(sizeof(int) * t_score);
    *BestScore = (int *)malloc(sizeof(int) * t_score);
    byte *h_readSeq = (byte *)malloc(sizeof(byte) * MAX_ReadLength * t_altReads);
    byte *h_quals = (byte *)malloc(sizeof(byte) * MAX_ReadLength * t_altReads);
    byte *h_ref = (byte *)malloc(sizeof(byte) * t_refLen);
    int *ReadLength = (int *)malloc(sizeof(int) * t_altReads);
    int *originalAlignment = (int *)malloc(sizeof(int) * t_altReads);
    int *h_n_consensus = (int *)malloc(sizeof(int) * iter_num);
    int *h_altReads = (int *)malloc(sizeof(int) * iter_num);
    int *h_refLens = (int *)malloc(sizeof(int) * n_refLen);

    localWorkSize = 256;
    globalWorkSize = 1;
    int reads_off = 0, ref_off = 0, Consensus_off = 0;

    for(int i = 0; i < iter_num; i++) {
        vector<Consensus *> finalConsensus;
        vector<Consensus *>::iterator itr;
        BamRegionData *rd = reads[i];
        ReadBin *readsToClean = rd->rb;
        vector<AlignedRead *>& altReads = rd->altReads;
        ConsensusHashTable *altConsensus = rd->altConsensus;
        altConsensus->toArray(finalConsensus);
        int leftmostIndex = readsToClean->getStart();
        h_altReads[i] = altReads.size();

        if(altReads.size() > globalWorkSize) {
            globalWorkSize = altReads.size();
        }

        for(unsigned int j = 0; j < altReads.size(); j++) {
            AlignedRead *toTest = altReads[j];
            byte *readSeq = (*toTest).getReadBases();
            byte *quals = (*toTest).getBaseQualities();
            ReadLength[reads_off] = (*toTest).getReadLength();
            originalAlignment[reads_off] = (*toTest).getOriginalAlignmentStart() - leftmostIndex;
            memcpy(h_readSeq + reads_off * MAX_ReadLength, readSeq, sizeof(byte) * ReadLength[reads_off]);
            memcpy(h_quals + reads_off * MAX_ReadLength, quals, sizeof(byte) * ReadLength[reads_off]);
            if(originalAlignment[reads_off] > 512) {
                localWorkSize = 512;
            }
            reads_off += 1;
        }

        h_n_consensus[i] = finalConsensus.size();
        for(itr = finalConsensus.begin(); itr != finalConsensus.end(); itr++) {
            Consensus *consensus = *itr;
            memcpy(h_ref + ref_off, consensus->str, sizeof(byte) * consensus->strLen);
            ref_off += consensus->strLen;
            h_refLens[Consensus_off] = consensus->strLen;
            Consensus_off += 1;
        }
    }


    CHECKERR(cudaMalloc((void **)&dreadSeq, sizeof(byte) * MAX_ReadLength * t_altReads));
    CHECKERR(cudaMalloc((void **)&dquals, sizeof(byte) * MAX_ReadLength * t_altReads));
    CHECKERR(cudaMalloc((void **)&dReadLength, sizeof(int) * t_altReads));
    CHECKERR(cudaMalloc((void **)&doriginalAlignment, sizeof(int) * t_altReads));
    CHECKERR(cudaMalloc((void **)&dref, sizeof(byte) * t_refLen));
    CHECKERR(cudaMalloc((void **)&dn_consensus, sizeof(int) * iter_num));
    CHECKERR(cudaMalloc((void **)&daltReads, sizeof(int) * iter_num));
    CHECKERR(cudaMalloc((void **)&drefLens, sizeof(int) * n_refLen));

    CHECKERR(cudaMalloc((void **)&dScoreIndex, sizeof(int) * t_score));
    CHECKERR(cudaMalloc((void **)&dBestScore, sizeof(int) * t_score));

    cudaMemcpy(dreadSeq, h_readSeq, sizeof(byte) * MAX_ReadLength * t_altReads, cudaMemcpyHostToDevice);
    CHECKERR1();
    cudaMemcpy(dquals, h_quals, sizeof(byte) * MAX_ReadLength * t_altReads, cudaMemcpyHostToDevice);
    CHECKERR1();
    cudaMemcpy(dReadLength, ReadLength, sizeof(int) * t_altReads, cudaMemcpyHostToDevice);
    CHECKERR1();
    cudaMemcpy(doriginalAlignment, originalAlignment, sizeof(int) * t_altReads, cudaMemcpyHostToDevice);
    CHECKERR1();
    cudaMemcpy(dref, h_ref, sizeof(byte) * t_refLen, cudaMemcpyHostToDevice);
    CHECKERR1();
    cudaMemcpy(dn_consensus, h_n_consensus, sizeof(int) * iter_num, cudaMemcpyHostToDevice);
    CHECKERR1();
    cudaMemcpy(daltReads, h_altReads, sizeof(int) * iter_num, cudaMemcpyHostToDevice);
    CHECKERR1();
    cudaMemcpy(drefLens, h_refLens, sizeof(int) * n_refLen, cudaMemcpyHostToDevice);
    CHECKERR1();



    free(h_n_consensus);
    free(h_altReads);
    free(h_refLens);
    free(h_ref);
    free(h_readSeq);
    free(h_quals);
    free(ReadLength);
    free(originalAlignment);
    return 1;
}


