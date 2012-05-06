#include "global.h"
__device__ float wd(char x, char y, float match, float mismatch)
{
    return (x == y ? match : mismatch);
}


__device__ SEQ_ALIGN makeElement_d(int state, int segment_length)
{
    SEQ_ALIGN t;
    t.o = '\0';
    switch(state) {
    case MSTATE:
        t.o = 'M';
        break;
    case ISTATE:
        t.o = 'I';
        break;
    case DSTATE:
        t.o = 'D';
        break;
    case 'S':
        t.o = 'S';
        break;
    }
    t.length = segment_length;
    return t;
}

__device__ void compare_t(SEQ_ALIGN *t, int &p_flag, int &c_flag, int &seg_length, int &n_Cigar)
{
    if(p_flag != c_flag) {
        *t = makeElement_d(p_flag, seg_length);
        seg_length = 1;
        p_flag = c_flag;
        n_Cigar++;
    } else {
        seg_length++;
    }
}

__global__ void MatchString(
    char *pathFlags,
    char *extFlags,
    char  *instr1Ds,
    char  *instr2Ds,
    int *a_offset,
    int *b_offset,
    float open_penalty,
    float ext_penalty,
    float match,
    float mismatch,
    int *rowNums,
    int *columnNums,
    SEQ_ALIGN *backtrace,
    int *n_C,
    int *alignment_offset,
    int maxMatrix)

{
    int npos, npos_b;
    int npreposngap, npreposhgap, npreposvgap;
    int npreposngap_b, npreposhgap_b, npreposvgap_b;
    int indexi, indexj;
    int tid = threadIdx.x;
    int bid = blockIdx.x;

    int preOffsetArray[3] = {2, -1, -1};
    int prePreOffsetArray[3] = {1, 1, -2};

    float fdistngap, fdisthgap, fdistvgap;
    float ext_dist;
    float fmaxdist;

    __shared__ float str_fngapdistp_s[128 * 3];
    __shared__ float str_fhgapdistp_s[128 * 3];
    __shared__ float str_fvgapdistp_s[128 * 3];

    __shared__ float maxscore;
    __shared__ int nposi_s;
    __shared__ int nposj_s;

    for(int tt = 0; tt < 3; tt++) {
        int offset = tt * 128;
        str_fngapdistp_s[tid + offset] = 0.0f;
        str_fhgapdistp_s[tid + offset] = 0.0f;
        str_fvgapdistp_s[tid + offset] = 0.0f;

    }

    SEQ_ALIGN *t = backtrace + bid * 100;

    char  *instr1D = instr1Ds + a_offset[bid];
    char  *instr2D = instr2Ds + b_offset[bid];
    char  *pathFlag = pathFlags + maxMatrix * bid;
    char  *extFlag = extFlags + maxMatrix * bid;
    int seq1Pos = 0;
    int seq2Pos = 1;
    int nOffset = 1;
    int threadsOfLaunch = 0;
    int rowNum = rowNums[bid] + 1;
    int columnNum = columnNums[bid] + 1;
    int launchNum = rowNum + columnNum - 1;

    if(tid == 0) {
        maxscore = 0;
    }

    for(int launchNo = 2; launchNo < launchNum; launchNo++) {
        if(launchNo <= rowNum) {
            seq1Pos++;
            if(launchNo <= columnNum) {
                threadsOfLaunch++;
            }
        } else {
            seq2Pos++;
            nOffset++;
            threadsOfLaunch--;
        }


        int nposi = seq1Pos;
        int nposj = seq2Pos;
        int   launchNoMod3 = launchNo % 3;


        if(tid < threadsOfLaunch) {
            npos = launchNoMod3 * columnNum + nOffset + tid;
            indexj = nposj + tid;
            indexi = nposi - tid;
            npreposhgap = npos + preOffsetArray[launchNoMod3] * columnNum;
            npreposvgap = npreposhgap - 1;
            npreposngap = npos + prePreOffsetArray[launchNoMod3] * columnNum - 1;


            npos_b = indexi * columnNum + indexj;
            npreposngap_b = (indexi - 1) * columnNum + indexj - 1;
            npreposhgap_b = npreposngap_b + 1;
            npreposvgap_b = npreposngap_b + columnNum;


            fdistngap = str_fngapdistp_s[npreposngap] + wd(instr1D[indexi - 1], instr2D[indexj - 1], match, mismatch);
            fdistvgap = str_fngapdistp_s[npreposvgap] + open_penalty;
            ext_dist  = str_fvgapdistp_s[npreposvgap] + ext_penalty;

            if(fdistvgap <= ext_dist && indexj > 1) {
                fdistvgap = ext_dist;
                pathFlag[npreposvgap_b] += 8;
            }

            fdisthgap = str_fngapdistp_s[npreposhgap] + open_penalty;
            ext_dist  = str_fhgapdistp_s[npreposhgap] + ext_penalty;

            if(fdisthgap <= ext_dist && indexi > 1) {
                fdisthgap = ext_dist;
                extFlag[npreposhgap_b] = 1;
            }

            str_fhgapdistp_s[npos] = fdisthgap;
            str_fvgapdistp_s[npos] = fdistvgap;

            int step_down = fdistvgap;
            int step_right = fdisthgap;
            int step_diag = fdistngap;

            if(step_down >= step_right) {
                if(step_down > step_diag) {
                    fmaxdist = step_down;
                    pathFlag[npos_b] = 3;
                } else {
                    fmaxdist = step_diag;
                    pathFlag[npos_b] = 2;
                }
            } else {
                if(step_right > step_diag) {
                    fmaxdist = step_right;
                    pathFlag[npos_b] = 1;
                } else {
                    fmaxdist = step_diag;
                    pathFlag[npos_b] = 2;
                }
            }


            str_fngapdistp_s[npos] = fmaxdist;
            if(fmaxdist >= maxscore && indexj == columnNum - 1) {
                maxscore = fmaxdist;
                nposj_s = columnNum - 1;
                nposi_s = indexi;
            }
            if((fmaxdist >= maxscore) && abs(rowNum - indexj) < abs(rowNum - nposj_s) && (nposi_s == rowNum - 1)) {
                maxscore = fmaxdist;
                nposj_s = indexj;
                nposi_s = rowNum - 1;
            }
        }
        __syncthreads();
    }


    if(tid == 0) {
        int i, j;
        int nlen;
        int npathflag;
        int n_Cigar = 0;
        i = nposi_s;
        j = nposj_s;
        npathflag = pathFlag[i * columnNum + j] & 0x3;
        nlen = 0;
        int prev_flag = npathflag;
        int segment_length = 0;
        while(1) {
            if(npathflag == 3) {
                nlen++;
                j--;
                compare_t(&t[n_Cigar], prev_flag, npathflag, segment_length, n_Cigar);
            } else if(npathflag == 1) {
                nlen++;
                i--;
                compare_t(&t[n_Cigar], prev_flag, npathflag, segment_length, n_Cigar);
            } else if(npathflag == 2) {
                nlen++;
                i--;
                j--;
                compare_t(&t[n_Cigar], prev_flag, npathflag, segment_length, n_Cigar);
            } else {
                break;
            }

            npos = i * columnNum + j;
            int nExtFlag = pathFlag[npos] / 4;

            if(npathflag == 3 && (nExtFlag == 2 || nExtFlag == 3)) {
                npathflag = 3;
            } else if(npathflag == 1 && extFlag[npos] == 1) {
                npathflag = 1;
            } else {
                npathflag = pathFlag[npos] & 0x3;
            }

            if(i == 0 || j == 0) {
                break;
            }

            if(npathflag == PATH_END) {
                break;
            }
        }

        compare_t(&t[n_Cigar], prev_flag, npathflag, segment_length, n_Cigar);
        alignment_offset[bid] = i - j;
        n_C[bid] = n_Cigar;

    }
}


