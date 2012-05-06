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

#include <vector>

//from bamtools:
#include "api/BamReader.h"          // (or "BamMultiReader.h") as needed
#include "api/BamWriter.h"          // as needed
#include "api/BamAux.h"
#include "Types.h"


using namespace std;
using namespace BamTools;

class SWPairwiseAlignment
{
private:
    int alignment_offset; // offset of s2 w/respect to s1
    Cigar alignmentCigar;

    double w_match;
    double w_mismatch;
    double w_open;
    double w_extend;

    static const int MSTATE = 0;
    static const int ISTATE = 1;
    static const int DSTATE = 2;

    void calculateMatrix(const byte a[], unsigned int an, const byte b[], unsigned int bn, double sw[], int btrack[]);
    void calculateCigar(int n, int m, double sw[], int btrack[]);
    CigarOp makeElement(int state, int segment_length);
    double wd(byte x, byte y);
    double wk(int k);
    //void print(int[][] s);
    //void print(double [][] s);
    //void print(int[][] s, String a, String b);
    //void print(double[][] s, String a, String b);

public:
    SWPairwiseAlignment(const byte seq1[], unsigned int seq1n, const byte seq2[], unsigned int seq2n,
                        double match, double mismatch, double open, double extend);
    SWPairwiseAlignment(const byte seq1[], unsigned int seq1n, const byte seq2[], unsigned int seq2n);
    Cigar getCigar();
    int getAlignmentStart2wrt1();
    void align(const byte a[], unsigned int an, const byte b[], unsigned int bn);
};
