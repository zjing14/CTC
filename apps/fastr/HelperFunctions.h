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
#ifndef HELPERFUNCTIONS_H
#define HELPERFUNCTIONS_H

#include <string>
#include "api/BamAux.h"
#include "api/BamAlignment.h"
#include "Types.h"

using namespace BamTools;

std::string CigarToString(const Cigar &c);
Cigar StringToCigar(const std::string &s);

bool CigarsEqual(const Cigar a, const Cigar b);

byte *createIndelString(Cigar cigar, const int indexOfIndel, const byte *refSeq, const int refSeqLen, const byte *readSeq, const int readSeqLen, const int refIndex, const int readIndex, int &altLength);

Cigar leftAlignIndel(Cigar cigar, const byte refSeq[],
                     const int refSeqLen, const byte readSeq[],
                     const int readSeqLen, const int refIndex,
                     const int readIndex);

int getNumAlignmentBlocks(const BamAlignment &r);

Cigar moveCigarLeft(Cigar cigar, unsigned int indexOfIndel);
bool cigarHasZeroSizeElement(Cigar c);
Cigar cleanUpCigar(Cigar c);

long mismatchingQualities(const BamAlignment &r, const byte refSeq[], const int refSeqLen, int refIndex);

struct MismatchCount {
    int numMismatches;
    long mismatchQualities;
};

MismatchCount getMismatchCount(const BamAlignment &r, const byte refSeq[], const int refSeqLen, int refIndex, int startOnRead, int nReadBases);

bool isRegularBase(const char c);
#endif
