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
#include "SWPairwiseAlignment.h"

#include <algorithm>
#include <stdlib.h>

//from bamtools:
#include "api/BamReader.h"          // (or "BamMultiReader.h") as needed
#include "api/BamWriter.h"          // as needed
#include "Types.h"
#include "HelperFunctions.h"

using namespace std;
using namespace BamTools;

const bool DO_SOFTCLIP = true;

/**
 * seq1n = length of seq1
 * seq2n = length of seq2
 */
SWPairwiseAlignment::SWPairwiseAlignment(const byte seq1[], unsigned int seq1n, const byte seq2[], unsigned int seq2n, double match, double mismatch, double open, double extend)
{
    w_match = match;
    w_mismatch = mismatch;
    w_open = open;
    w_extend = extend;
    align(seq1, seq1n, seq2, seq2n);
}

SWPairwiseAlignment::SWPairwiseAlignment(const byte seq1[], unsigned int seq1n, const byte seq2[], unsigned int seq2n)
{
    //this(seq1,seq2,1.0,-1.0/3.0,-1.0-1.0/3.0,-1.0/3.0); // match=1, mismatch = -1/3, gap=-(1+k/3)
    w_match = 1.0;
    w_mismatch = -1.0 / 3.0;
    w_open = -1.0 - 1.0 / 3.0;
    w_extend = -1.0 / 3.0;
    align(seq1, seq1n, seq2, seq2n);
}

Cigar SWPairwiseAlignment::getCigar()
{
    return alignmentCigar;
}

int SWPairwiseAlignment::getAlignmentStart2wrt1()
{
    return alignment_offset;
}

/**
 * an = length of a
 * bn = length of b
 */
void SWPairwiseAlignment::align(const byte a[], unsigned int an, const byte b[], unsigned int bn)
{
    int n = an;
    int m = bn;
    //  printf("sw (%d)\n", (n+1)*(m+1));
    double *sw = new double[(n + 1) * (m + 1)];
    for(int i = 0; i < (n + 1) * (m + 1); i++) {
        sw[i] = 0;
    }

    int *btrack = new int[(n + 1) * (m + 1)];
    for(int i = 0; i < (n + 1) * (m + 1); i++) {
        btrack[i] = 0;
    }

    calculateMatrix(a, an, b, bn, sw, btrack);
#ifdef PRINT_SCOREMATRIX
    for(int i = 0; i < (n + 1); i++) {
        for(int j = 0; j < (m + 1); j++) {
            cout << sw[i * (m + 1) + j] << " ";
        }
        cout << endl;
    }
#endif
    calculateCigar(n, m, sw, btrack); // length of the segment (continuous matches, insertions or deletions)

    delete[] sw;
    delete[] btrack;
}

void SWPairwiseAlignment::calculateMatrix(const byte a[], unsigned int an, const byte b[], unsigned int bn, double sw[], int btrack[])
{
    const int n = an + 1;
    const int m = bn + 1;
    const double MATRIX_MIN_CUTOFF = -1e100;

    double *best_gap_v = new double[m + 1];
    //Arrays.fill(best_gap_v,-1.0e40);
    for(int i = 0; i < m + 1; i++) {
        best_gap_v[i] = -1.0e40;
    }
    int *gap_size_v = new int[m + 1];
    double *best_gap_h = new double[n + 1];
    //Arrays.fill(best_gap_h,-1.0e40);
    for(int i = 0; i < n + 1; i++) {
        best_gap_h[i] = -1.0e40;
    }
    int *gap_size_h  = new int[n + 1];
    //printf("gsv gsh, %d %d\n", m+1, n+1);
    // build smith-waterman matrix and keep backtrack info:
    for(int i = 1, row_offset_1 = 0 ; i < n ; i++) {    // we do NOT update row_offset_1 here, see comment at the end of this outer loop
        char a_base = a[i - 1]; // letter in a at the current pos

        const int row_offset = row_offset_1 + m;

        // On the entrance into the loop, row_offset_1 is the (linear) offset
        // of the first element of row (i-1) and row_offset is the linear offset of the
        // start of row i

        for(int j = 1, data_offset_1 = row_offset_1 ; j < m ; j++, data_offset_1++) {

            // data_offset_1 is linearized offset of element [i-1][j-1]

            const byte b_base = b[j - 1]; // letter in b at the current pos

            // in other words, step_diag = sw[i-1][j-1] + wd(a_base,b_base);
            double step_diag = sw[data_offset_1] + wd(a_base, b_base);

            // optimized "traversal" of all the matrix cells above the current one (i.e. traversing
            // all 'step down' events that would end in the current cell. The optimized code
            // does exactly the same thing as the commented out loop below. IMPORTANT:
            // the optimization works ONLY for linear w(k)=wopen+(k-1)*wextend!!!!

            // if a gap (length 1) was just opened above, this is the cost of arriving to the current cell:
            double prev_gap = sw[data_offset_1 + 1] + w_open;

            best_gap_v[j] += w_extend; // for the gaps that were already opened earlier, extending them by 1 costs w_extend

            if(prev_gap > best_gap_v[j]) {
                // opening a gap just before the current cell results in better score than extending by one
                // the best previously opened gap. This will hold for ALL cells below: since any gap
                // once opened always costs w_extend to extend by another base, we will always get a better score
                // by arriving to any cell below from the gap we just opened (prev_gap) rather than from the previous best gap
                best_gap_v[j] = prev_gap;
                gap_size_v[j] = 1; // remember that the best step-down gap from above has length 1 (we just opened it)
            } else {
                // previous best gap is still the best, even after extension by another base, so we just record that extension:
                gap_size_v[j]++;
            }

            const double step_down = best_gap_v[j] ;
            const int kd = gap_size_v[j];

            /*
               for ( int k = 1, data_offset_k = data_offset_1+1 ; k < i ; k++, data_offset_k -= m ) {
            // data_offset_k is linearized offset of element [i-k][j]
            // in other words, trial = sw[i-k][j]+gap_penalty:
            final double trial = sw[data_offset_k]+wk(k);
            if ( step_down < trial ) {
            step_down=trial;
            kd = k;
            }
            }
            */

            // optimized "traversal" of all the matrix cells to the left of the current one (i.e. traversing
            // all 'step right' events that would end in the current cell. The optimized code
            // does exactly the same thing as the commented out loop below. IMPORTANT:
            // the optimization works ONLY for linear w(k)=wopen+(k-1)*wextend!!!!

            const int data_offset = row_offset + j; // linearized offset of element [i][j]
            prev_gap = sw[data_offset - 1] + w_open; // what would it cost us to open length 1 gap just to the left from current cell
            best_gap_h[i] += w_extend; // previous best gap would cost us that much if extended by another base

            if(prev_gap > best_gap_h[i]) {
                // newly opened gap is better (score-wise) than any previous gap with the same row index i; since
                // gap penalty is linear with k, this new gap location is going to remain better than any previous ones
                best_gap_h[i] = prev_gap;
                gap_size_h[i] = 1;
            } else {
                gap_size_h[i]++;
            }

            const double step_right = best_gap_h[i];
            const int ki = gap_size_h[i];

            /*
               for ( int k = 1, data_offset = row_offset+j-1 ; k < j ; k++, data_offset-- ) {
            // data_offset is linearized offset of element [i][j-k]
            // in other words, step_right=sw[i][j-k]+gap_penalty;
            final double trial = sw[data_offset]+wk(k);
            if ( step_right < trial ) {
            step_right=trial;
            ki = k;
            }
            }

            final int data_offset = row_offset + j; // linearized offset of element [i][j]
            */


            if(step_down > step_right) {
                if(step_down > step_diag) {
                    sw[data_offset] = max(MATRIX_MIN_CUTOFF, step_down);
                    btrack[data_offset] = kd ; // positive=vertical
                } else {
                    sw[data_offset] = max(MATRIX_MIN_CUTOFF, step_diag);
                    btrack[data_offset] = 0; // 0 = diagonal
                }
            } else {
                // step_down <= step_right
                if(step_right > step_diag) {
                    sw[data_offset] = max(MATRIX_MIN_CUTOFF, step_right);
                    btrack[data_offset] = -ki; // negative = horizontal
                } else {
                    sw[data_offset] = max(MATRIX_MIN_CUTOFF, step_diag);
                    btrack[data_offset] = 0; // 0 = diagonal
                }
            }
            //          sw[data_offset] = -1;
            //                sw[data_offset] = Math.max(0, Math.max(step_diag,Math.max(step_down,step_right)));
        }

        // IMPORTANT, IMPORTANT, IMPORTANT:
        // note that we update this (secondary) outer loop variable here,
        // so that we DO NOT need to update it
        // in the for() statement itself.
        row_offset_1 = row_offset;
    }
    delete[] best_gap_v;
    delete[] best_gap_h;
    delete[] gap_size_v;
    delete[] gap_size_h;
}

void SWPairwiseAlignment::calculateCigar(int n, int m, double sw[], int btrack[])
{
    //printf("###\tCalculating Cigar %d %d\n", n, m);
    // p holds the position we start backtracking from; we will be assembling a cigar in the backwards order
    //PrimitivePair.Int p = new PrimitivePair.Int();
    int p1 = 0, p2 = 0;

    double maxscore = 0.0;
    int segment_length = 0; // length of the segment (continuous matches, insertions or deletions)

    // look for largest score. we use >= combined with the traversal direction
    // to ensure that if two scores are equal, the one closer to diagonal gets picked
    for(int i = 1, data_offset = m + 1 + m ; i < n + 1 ; i++, data_offset += (m + 1)) {
        // data_offset is the offset of [i][m]
        if(sw[data_offset] >= maxscore) {
            p1 = i;
            p2 = m ;
            maxscore = sw[data_offset];
        }
    }

    for(int j = 1, data_offset = n * (m + 1) + 1 ; j < m + 1 ; j++, data_offset++) {
        // data_offset is the offset of [n][j]
        if((sw[data_offset] > maxscore || sw[data_offset] == maxscore) && abs(n - j) < abs(p1 - p2)) {
            p1 = n;
            p2 = j ;
            //                maxscore = sw[n][j];
            maxscore = sw[data_offset];
            segment_length = m - j ; // end of sequence 2 is overhanging; we will just record it as 'M' segment
        }
    }


#ifdef PRINT_SCOREMATRIX
    printf("maxScore: %d (%d, %d)\n", int(maxscore), p1, p2);
#endif
    // we will be placing all insertions and deletions into sequence b, so the state are named w/regard
    // to that sequence

    vector<CigarOp> lce;
    if(segment_length > 0 && DO_SOFTCLIP) {
        lce.push_back(makeElement('S', segment_length));
        segment_length = 0;
    }

    int state = MSTATE;

    int data_offset = p1 * (m + 1) + p2; // offset of element [p1][p2]

    do {
        //            int btr = btrack[p1][p2];
        int btr = btrack[data_offset];

        int new_state;
        int step_length = 1;

        //       System.out.print(" backtrack value: "+btr);

        if(btr > 0) {
            new_state = DSTATE;
            step_length = btr;
        } else if(btr < 0) {
            new_state = ISTATE;
            step_length = (-btr);
        } else {
            new_state = MSTATE;    // and step_length =1, already set above
        }


        // move to next best location in the sw matrix:
        switch(new_state) {
        case MSTATE:
            data_offset -= (m + 2);
            p1--;
            p2--;
            break; // move back along the diag in th esw matrix
        case ISTATE:
            data_offset -= step_length;
            p2 -= step_length;
            break; // move left
        case DSTATE:
            data_offset -= (m + 1) * step_length;
            p1 -= step_length;
            break; // move up
        }


        // now let's see if the state actually changed:
        if(new_state == state) {
            segment_length += step_length;
        } else {
            // state changed, lets emit previous segment, whatever it was (Insertion Deletion, or (Mis)Match).
            lce.push_back(makeElement(state, segment_length));
            segment_length = step_length;
            state = new_state;
        }
        //      next condition is equivalent to  while ( sw[p1][p2] != 0 ) (with modified p1 and/or p2:
    } while(p1 > 0 && p2 > 0);

    // post-process the last segment we are still keeping;
    // NOTE: if reads "overhangs" the ref on the left (i.e. if p2>0) we are counting
    // those extra bases sticking out of the ref into the first cigar element if DO_SOFTCLIP is false;
    // otherwise they will be softclipped. For instance,
    // if read length is 5 and alignment starts at offset -2 (i.e. read starts before the ref, and only
    // last 3 bases of the read overlap with/align to the ref), the cigar will be still 5M if
    // DO_SOFTCLIP is false or 2S3M if DO_SOFTCLIP is true.
    // The consumers need to check for the alignment offset and deal with it properly.
    if(DO_SOFTCLIP) {
        lce.push_back(makeElement(state, segment_length));
        if(p2 > 0) {
            lce.push_back(makeElement('S', p2));
        }
        alignment_offset = p1 ;
    } else {
        lce.push_back(makeElement(state, segment_length + p2));
        alignment_offset = p1 - p2;
    }


    reverse(lce.begin(), lce.end());
#ifdef PRINT_BACKTRACE
    for(vector<CigarOp>::iterator t = lce.begin(); t != lce.end(); t++) {
        printf("(%c %d)", t->Type, t->Length);
    }
    printf("\n");
#endif
    //Collections.reverse(lce);
    alignmentCigar = lce;//new Cigar(lce);
}

CigarOp SWPairwiseAlignment::makeElement(int state, int segment_length)
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

double SWPairwiseAlignment::wd(byte x, byte y)
{
    return (x == y ? w_match : w_mismatch);
}

double SWPairwiseAlignment::wk(int k)
{
    return w_open + (k - 1) * w_extend; // gap
}

/*void SWPairwiseAlignment::print(int[][] s, unsigned int s1n, unsigned int s2n) {
  for ( int i = 0 ; i < s1n ; i++) {
  for ( int j = 0; j < s2n ; j++ ) {
  printf(" %4d",s[i][j]);
  }
  printf("\n");
  }
  }

  void SWPairwiseAlignment::print(double[][] s, unsigned int s1n, unsigned int s2n) {
  for ( int i = 0 ; i < s1n ; i++) {
  for ( int j = 0; j < s2n ; j++ ) {
  printf(" %4g",s[i][j]);
  }
  printf("\n");
  }
  }

  void SWPairwiseAlignment::print(int[][] s, unsigned int s1n, unsigned int s2n, string a, string b) {

  printf("        ");
  for ( int j = 1 ; j < s[0].length ; j++) printf(" %4c",b.charAt(j-1)) ;
  printf("\n");

  for ( int i = 0 ; i < s1n ; i++) {
  if ( i > 0 ) printf("%c",a.charAt(i-1));
  else printf(' ');
  printf("  ");
  for ( int j = 0; j < s2n ; j++ ) {
  printf(" %4d",s[i][j]);
  }
  printf("\n");
  }
  }

  void SWPairwiseAlignment::print(double[][] s, unsigned int s1n, unsigned int s2n, String a, String b) {

  printf("");
  for ( int j = 1 ; j < s[0].length ; j++) printf(" %4c",b.charAt(j-1)) ;
  printf("\n");

  for ( int i = 0 ; i < s1n ; i++) {
  if ( i > 0 ) printf("%c",a.charAt(i-1));
  else printf(' ');
  printf("  ");
  for ( int j = 0; j < s2n ; j++ ) {
  printf(" %2.1f",s[i][j]);
  }
  printf("\n");
  }
  }*/
