#define GetreadSeq(a) &(readSeq[a * 100])
#define Getquals(a) &(quals[a * 100])
#define byte char
#define MAX_QUAL 99
#define MAX_ReadLengh 100

int mismatchQualitySumIgnoreCigar(__global byte *readSeq, __global byte* quals, __global byte* refSeq, int refLen, int refIndex, int getReadLength, __global char *ch_lookup) {
    int sum = 0;
    for (int readIndex = 0 ; readIndex < getReadLength; refIndex++, readIndex++ ) {
        if ( refIndex >= refLen ) {
            sum += MAX_QUAL;
        } else {
            byte refChr = refSeq[refIndex];
            byte readChr = readSeq[readIndex];
            if ( !ch_lookup[readChr] || !ch_lookup[refChr] )
                continue; // do not count Ns/Xs/etc ?
            if ( (readChr != refChr) ) {
                sum += (int)quals[readIndex];
            }
        }
    }
    return sum;
}

void mismatchQualitySumIgnoreCigar1(__global byte *readSeq, __global byte* quals, __global byte* refSeq, int refLen, int refIndex, int getReadLength, __global char *ch_lookup, __global int *bestScore, __global int *bestIndex)
{
    int i = get_local_id(0);
    __local int sum[128];

    int readIndex_t = i;
    int refIndex_t = i + refIndex;
    if(i < 128){
        sum[i] = 0;
        if(readIndex_t < getReadLength){	
            if ( refIndex_t >= refLen ) {
                sum[i] = MAX_QUAL;
            } else {
                byte refChr = refSeq[refIndex_t];
                byte readChr = readSeq[readIndex_t];
                if ( ch_lookup[readChr] && ch_lookup[refChr] )
                    if ( (readChr != refChr) ) {
                        sum[i] = (int)quals[readIndex_t];
                    }
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        for(unsigned int s=64; s>0; s>>=1) 
        {
            if (i < s)
            {
                sum[i] += sum[i+s];
            }
            barrier(CLK_LOCAL_MEM_FENCE);
        }
    }

    if(i == 0)
    {
        *bestScore = sum[0];
        *bestIndex = refIndex;
    }
}

int mismatchQualitySumIgnoreCigar2(__global byte *readSeq, __global byte* quals, __global byte* refSeq, int refLen, int refIndex, int getReadLength, __global char *ch_lookup, int quitAboveThisValue) {
    int sum = 0;
    for (int readIndex = 0 ; readIndex < getReadLength; refIndex++, readIndex++ ) {
        if ( refIndex >= refLen ) {
            sum += MAX_QUAL;
            if ( sum > quitAboveThisValue )
                return sum;
        } else {
            byte refChr = refSeq[refIndex];
            byte readChr = readSeq[readIndex];
            if ( !ch_lookup[readChr] || !ch_lookup[refChr] )
                continue; // do not count Ns/Xs/etc ?
            if ( (readChr != refChr) ) {
                sum += (int)quals[readIndex];
                if ( sum > quitAboveThisValue )
                    return sum;
            }
        }
    }
    return sum;
}

__kernel void findBestOffset(__global byte *ref, const int refLen, __global byte *readSeq, __global byte *quals, __global int *ReadLength, __global int *originalAlignment, __global int *bestScore, __global int *bestIndex, __global char *ch_lookup){
    int b = get_group_id(0);
    int i = get_local_id(0);
    __local int score[1024];
    __local int scoreindex[1024];
    score[i] = INT_MAX;
    scoreindex[i] = i;
    if(i==0)
    {
        bestScore[b] = mismatchQualitySumIgnoreCigar(GetreadSeq(b), Getquals(b), ref, refLen, originalAlignment[b], ReadLength[b], ch_lookup);
        bestIndex[b] = originalAlignment[b];
    }
    //mismatchQualitySumIgnoreCigar(GetreadSeq(b), Getquals(b), ref, refLen, originalAlignment[b], ReadLength[b], ch_lookup, &(bestScore[b]), &(bestIndex[b]));

    barrier(CLK_LOCAL_MEM_FENCE);

    if(i < originalAlignment[b])
        score[i] = mismatchQualitySumIgnoreCigar2(GetreadSeq(b), Getquals(b), ref, refLen, i, ReadLength[b], ch_lookup, bestScore[b]);

    barrier(CLK_LOCAL_MEM_FENCE);

    for(unsigned int s=get_local_size(0)/2; s>0; s>>=1) 
    {
        if (i < s)
        {
            if(score[i] > score[i + s]){
                score[i] = score[i+s];
                scoreindex[i] = scoreindex[i+s];}
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(i == 0){
        if (bestScore[b] > score[0])
        {
            bestScore[b] = score[0];
            bestIndex[b] = scoreindex[0];
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    score[i] = INT_MAX;
    scoreindex[i] = i + originalAlignment[b] + 1;
    int maxPossibleStart = refLen - ReadLength[b];
    maxPossibleStart = maxPossibleStart - (originalAlignment[b] + 1);

    if(i <= maxPossibleStart)
        score[i] = mismatchQualitySumIgnoreCigar2(GetreadSeq(b), Getquals(b), ref, refLen, i + originalAlignment[b] + 1, ReadLength[b], ch_lookup, bestScore[b]);

    barrier(CLK_LOCAL_MEM_FENCE);

    for(unsigned int s=get_local_size(0)/2; s>0; s>>=1) 
    {
        if (i < s)
        {
            if(score[i] > score[i + s]){
                score[i] = score[i+s];
                scoreindex[i] = scoreindex[i+s];}
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(i == 0){
        if (bestScore[b] > score[0])
        {
            bestScore[b] = score[0];
            bestIndex[b] = scoreindex[0];
        }
    }
}

__kernel void findBestOffset1(__global byte *ref_g, __global byte *readSeq_g, __global byte *quals_g, 
        __global int *ReadLength_g, __global int *originalAlignment_g, __global int *bestScore_g, 
        __global int *bestIndex_g, __global char *ch_lookup,
        int iter_num, __global int *n_consensus,
        __global int *altReads, __global int *refLens)
{
    int b = get_group_id(0);
    int i = get_local_id(0);
    int ii = get_local_id(0) + get_local_size(0);
    __local int score[1024];
    __local int scoreindex[1024];
    int ref_offset = 0;
    int quals_offset = 0;
    int ReadLength_offset = 0;
    int originalAlignment_offset = 0;
    int score_offset = 0;
    int n_refLen = 0;
    int readSeq_offset = 0;
    
    for(int t = 0; t < iter_num; t++)
    {
        for(int k =0; k < n_consensus[t]; k++)
        {
            if(b < altReads[t]){
                
                score[i] = INT_MAX;
                score[ii] = INT_MAX;
                scoreindex[i] = i;
                scoreindex[ii] = ii;
                
                __global byte *ref = ref_g + ref_offset;
                __global byte *readSeq = readSeq_g + readSeq_offset;
                __global byte *quals = quals_g + quals_offset;
                __global int *ReadLength = ReadLength_g + ReadLength_offset;
                __global int *originalAlignment = originalAlignment_g + originalAlignment_offset;
                __global int *bestScore = bestScore_g + score_offset;
                __global int *bestIndex = bestIndex_g + score_offset;

                if(i==0)
                {
                    bestScore[b] = mismatchQualitySumIgnoreCigar(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], originalAlignment[b], ReadLength[b], ch_lookup);
                    bestIndex[b] = originalAlignment[b];
                }

                barrier(CLK_LOCAL_MEM_FENCE);

                if(i < originalAlignment[b])
                    score[i] = mismatchQualitySumIgnoreCigar2(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], i, ReadLength[b], ch_lookup, bestScore[b]);
                if(ii < originalAlignment[b])
                    score[ii] = mismatchQualitySumIgnoreCigar2(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], ii, ReadLength[b], ch_lookup, bestScore[b]);

                barrier(CLK_LOCAL_MEM_FENCE);

                for(unsigned int s=get_local_size(0)/2; s>0; s>>=1) 
                {
                    if (i < s)
                    {
                        if(score[i] > score[i + s] || ( scoreindex[i+s] < scoreindex[i] && score[i] == score[i + s])){
                            score[i] = score[i+s];
                            scoreindex[i] = scoreindex[i+s];
                        }
                        if(score[ii] > score[ii + s] || ( scoreindex[ii+s] < scoreindex[ii] && score[ii] == score[ii + s])){
                            score[ii] = score[ii + s];
                            scoreindex[ii] = scoreindex[ii+s];
                        }
                    }
                    barrier(CLK_LOCAL_MEM_FENCE);
                }
                
                if(i == 0){
                    if (score[0] > score[get_local_size(0)] || (scoreindex[get_local_size(0)] < scoreindex[0] && score[0] ==  score[get_local_size(0)]))
                    {
                            score[0] = score[get_local_size(0)];
                            scoreindex[0] = scoreindex[get_local_size(0)];
                    }
                    if (bestScore[b] > score[0])
                    {
                            bestScore[b] = score[0];
                            bestIndex[b] = scoreindex[0];
                    }
                } 

                barrier(CLK_LOCAL_MEM_FENCE);

                score[i] = INT_MAX;
                scoreindex[i] = i ;
                score[ii] = INT_MAX;
                scoreindex[ii] = ii ;
                int maxPossibleStart = refLens[n_refLen] - ReadLength[b];
                maxPossibleStart = maxPossibleStart - (originalAlignment[b] + 1);

                if(i <= maxPossibleStart)
                    score[i] = mismatchQualitySumIgnoreCigar2(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], i + originalAlignment[b] + 1, ReadLength[b], ch_lookup, bestScore[b]);
                if(ii <= maxPossibleStart)
                    score[ii] = mismatchQualitySumIgnoreCigar2(GetreadSeq(b), Getquals(b), ref, refLens[n_refLen], ii + originalAlignment[b] + 1, ReadLength[b], ch_lookup, bestScore[b]);

                barrier(CLK_LOCAL_MEM_FENCE);

                for(unsigned int s=get_local_size(0)/2; s>0; s>>=1) 
                {
                    if (i < s)
                    {
                        if(score[i] > score[i + s] || ( scoreindex[i+s] < scoreindex[i] && score[i] == score[i + s])){
                            score[i] = score[i+s];
                            scoreindex[i] = scoreindex[i+s];
                        }
                        if(score[ii] > score[ii + s] || ( scoreindex[ii+s] < scoreindex[ii] && score[ii] == score[ii + s])){
                        score[ii] = score[ii + s];
                        scoreindex[ii] = scoreindex[ii+s];
                        }
                    }

                    barrier(CLK_LOCAL_MEM_FENCE);
                }

                if(i == 0){
                    if (score[0] > score[get_local_size(0)] || (scoreindex[get_local_size(0)] < scoreindex[0] && score[0] ==  score[get_local_size(0)]))
                    {
                            score[0] = score[get_local_size(0)];
                            scoreindex[0] = scoreindex[get_local_size(0)];
                    }
                    if (bestScore[b] > score[0])
                    {
                            bestScore[b] = score[0];
                            bestIndex[b] = scoreindex[0] + originalAlignment[b] + 1;
                    }
                }
            }
            ref_offset += refLens[n_refLen];
            n_refLen += 1;
            score_offset += altReads[t]; 
        }
        readSeq_offset += altReads[t] * MAX_ReadLengh; 
        quals_offset += MAX_ReadLengh * altReads[t];
        ReadLength_offset += altReads[t];
        originalAlignment_offset += altReads[t];
    }
}


