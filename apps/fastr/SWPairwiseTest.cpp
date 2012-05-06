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
#include "HelperFunctions.h"
#include "Types.h"

#include "api/BamAux.h"
#include "api/BamReader.h"

#include <fstream>
#include <string>

using namespace std;
using namespace BamTools;

const double SW_MATCH = 30.0;      // 1.0;
const double SW_MISMATCH = -10.0;  //-1.0/3.0;
const double SW_GAP = -10.0;       //-1.0-1.0/3.0;
const double SW_GAP_EXTEND = -2.0; //-1.0/.0;

int main(int argc, char **argv)
{
    if(argc < 2) {
        cout << "Usage: <ref> <read>" << endl;
        cout << "<ref> : Filename of reference string" << endl;
        cout << "<read>: Filename of read string" << endl;
    }
    ifstream input1;
    ifstream input2;

    string inputString1;
    string inputString2;

    input1.open(argv[1]);
    input2.open(argv[2]);

    getline(input1, inputString1);
    getline(input2, inputString2);

    cout << inputString1 << endl;
    cout << inputString2 << endl;

    char *seq1 = new char[inputString1.length()];
    char *seq2 = new char[inputString2.length()];

    for(int i = 0; i < inputString1.length(); i++) {
        seq1[i] = inputString1[i];
    }

    for(int i = 0; i < inputString2.length(); i++) {
        seq2[i] = inputString2[i];
    }

    SWPairwiseAlignment sw(seq1, inputString1.length(), seq2, inputString2.length(), SW_MATCH, SW_MISMATCH, SW_GAP, SW_GAP_EXTEND);
    Cigar c = sw.getCigar();
    std::cout << "Cigar: " << CigarToString(c) << endl;
    std::cout << "Offset from Reference: " << sw.getAlignmentStart2wrt1() << endl;
}
