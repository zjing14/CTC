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
#ifndef ALIGNEDREAD_H
#define ALIGNEDREAD_H

#include <utility>
#include <vector>

//from bamtools:
#include "api/BamReader.h"          // (or "BamMultiReader.h") as needed
#include "api/BamWriter.h"          // as needed
#include "api/BamAux.h"
#include "api/SamSequence.h"
#include "api/SamConstants.h"

#include "IndelRealigner.h"
#include "HelperFunctions.h"
#include "Types.h"

class AlignedRead
{
private:
    BamAlignment *read;
    byte *readBases;
    int readBasesLen;
    byte *baseQuals;
    int baseQualsLen;
    Cigar *newCigar;
    int newStart;
    int mismatchScoreToReference;
    long alignerMismatchScore;

    // pull out the bases that aren't clipped out
    void getUnclippedBases();

    // pull out the bases that aren't clipped out
    Cigar reclipCigar(Cigar *cigar);

public:
    AlignedRead(BamAlignment *read);
    AlignedRead(const AlignedRead &);
    ~AlignedRead();
    BamAlignment *getRead();
    int getReadLength();
    byte *getReadBases();
    byte *getBaseQualities();
    Cigar getCigar();
    void setCigar(Cigar *cigar);
    // tentatively sets the new Cigar, but it needs to be confirmed later
    void setCigar(Cigar *cigar, bool fixClippedCigar);
    // tentatively sets the new start, but it needs to be confirmed later
    void setAlignmentStart(int start);
    int getAlignmentStart();
    int getOriginalAlignmentStart();

    // finalizes the changes made.
    // returns true if this record actually changes, false otherwise
    bool finalizeUpdate();
    void setMismatchScoreToReference(int score);
    int getMismatchScoreToReference();
    void setAlignerMismatchScore(long score);
    long getAlignerMismatchScore();
};
#endif
