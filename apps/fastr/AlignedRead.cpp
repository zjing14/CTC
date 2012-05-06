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
#include <utility>
#include <vector>
#include <string.h>
#include <stdlib.h>
//from bamtools:
#include "api/BamReader.h"          // (or "BamMultiReader.h") as needed
#include "api/BamWriter.h"          // as needed
#include "api/BamAux.h"
#include "api/SamSequence.h"
#include "api/SamConstants.h"

#include "IndelRealigner.h"
#include "HelperFunctions.h"
#include "AlignedRead.h"
#include "Types.h"
// pull out the bases that aren't clipped out
void AlignedRead::getUnclippedBases()
{
    int readLen = getReadLength();
    readBases = new byte[readLen];
    readBasesLen = readLen;
    baseQuals = new byte[readLen];
    baseQualsLen = readLen;
    const char *actualReadBases = read->AlignedBases.c_str();//read.getReadBases();
    const char *actualBaseQuals = read->Qualities.c_str();//read.getBaseQualities();

    int fromIndex = 0, toIndex = 0;
    int qualD = 0;
    //cout << CigarToString(read->CigarData) << endl;
    for(Cigar::iterator itr = read->CigarData.begin(); itr != read->CigarData.end(); itr++) {
        CigarOp ce = *itr;
        int elementLength = ce.Length; //ce.getLength();
        switch(ce.Type) {
        case 'D':
            fromIndex += elementLength;
            qualD += elementLength;
            break;
        case 'S':
            qualD -= elementLength;
            break;
        case 'M':
        case 'I':
            //System.arraycopy(actualReadBases, fromIndex, readBases, toIndex, elementLength);
            for(int i = 0; i < elementLength;  i++) {
                readBases[toIndex + i] = actualReadBases[fromIndex + i];
            }
            //System.arraycopy(actualBaseQuals, fromIndex, baseQuals, toIndex, elementLength);
            for(int i = 0; i < elementLength;  i++) {
                baseQuals[toIndex + i] = actualBaseQuals[fromIndex + i - qualD] - 33; //CONVERT FROM FASTQ TO PHRED
            }
            fromIndex += elementLength;
            toIndex += elementLength;
            break;
        default:
            break;
        }
    }

    //  cout << "###\t" << toIndex << endl;
    // if we got clipped, trim the array
    if(fromIndex != toIndex) {
        char *trimmedRB = new char[toIndex];
        char *trimmedBQ = new char[toIndex];
        //System.arraycopy(readBases, 0, trimmedRB, 0, toIndex);
        for(int i = 0; i < toIndex;  i++) {
            trimmedRB[i] = readBases[i];
        }
        //System.arraycopy(baseQuals, 0, trimmedBQ, 0, toIndex);
        for(int i = 0; i < toIndex;  i++) {
            trimmedBQ[i] = baseQuals[i];
        }
        delete[] readBases;
        delete[] baseQuals;
        readBases = trimmedRB;
        readBasesLen = toIndex;
        baseQuals = trimmedBQ;
        baseQualsLen = toIndex;
    }
    readBasesLen = toIndex;
}

// pull out the bases that aren't clipped out
Cigar AlignedRead::reclipCigar(Cigar *cigar)
{
    return IndelRealigner::reclipCigar(cigar, *read);
}

AlignedRead::AlignedRead(BamAlignment *r)
{
    read = r;
    mismatchScoreToReference = 0;
    newStart = -1;
    alignerMismatchScore = 0;
    readBases = NULL;
    newCigar = NULL;
    baseQuals = NULL;
}

AlignedRead::AlignedRead(const AlignedRead &other)
{
    read = other.read;
    mismatchScoreToReference = other.mismatchScoreToReference;
    newStart = other.newStart;
    alignerMismatchScore = other.alignerMismatchScore;
    readBasesLen = other.readBasesLen;
    baseQualsLen = other.baseQualsLen;
    if(other.readBases != NULL) {
        readBases = new byte[other.readBasesLen];
        memcpy(readBases, other.readBases, other.readBasesLen);
    } else {
        readBases == NULL;
    }
    if(other.newCigar != NULL) {
        newCigar = new Cigar(*other.newCigar);
    } else {
        newCigar = NULL;
    }
    if(baseQuals != NULL) {
        baseQuals = new byte[other.baseQualsLen];
        memcpy(baseQuals, other.baseQuals, other.baseQualsLen);
    } else {
        baseQuals = NULL;
    }
}

AlignedRead::~AlignedRead()
{
    delete newCigar;
    delete[] baseQuals;
    delete[] readBases;
}

BamAlignment *AlignedRead::getRead()
{
    return read;
}

int AlignedRead::getReadLength()
{
    return readBases != NULL ? readBasesLen : read->Length;
}

byte *AlignedRead::getReadBases()
{
    if(readBases == NULL) {
        getUnclippedBases();
    }
    return readBases;
}

byte *AlignedRead::getBaseQualities()
{
    if(baseQuals == NULL) {
        getUnclippedBases();
    }
    return baseQuals;
}

Cigar AlignedRead::getCigar()
{
    return (newCigar != NULL ? *newCigar : read->CigarData);
}

void AlignedRead::setCigar(Cigar *cigar)
{
    setCigar(cigar, true);
}

// tentatively sets the new Cigar, but it needs to be confirmed later
void AlignedRead::setCigar(Cigar *cigar, bool fixClippedCigar)
{
    delete newCigar;
    if(cigar == NULL) {
        newCigar = NULL;
        return;
    }

    if(fixClippedCigar && readBasesLen < read->Length) {
        *cigar = reclipCigar(cigar);
    }

    // no change?
    if(CigarsEqual(read->CigarData, *cigar)) {
        delete cigar;
        newCigar = NULL;
        return;
    }

    // no indel?
    //string str = cigar.toString();
    //if ( !str.contains("D") && !str.contains("I") ) {
    //printf("Modifying a read with no associated indel; although this is possible, it is highly unlikely.  Perhaps this region should be double-checked: " + read.getReadName() + " near " + read.getReferenceName() + ":" + read.getAlignmentStart());
    //  newCigar = NULL;
    //  return;
    //  }

    newCigar = cigar;
}

// tentatively sets the new start, but it needs to be confirmed later
void AlignedRead::setAlignmentStart(int start)
{
    newStart = start;
}

int AlignedRead::getAlignmentStart()
{
    return (newStart != -1 ? newStart : read->Position);
}

int AlignedRead::getOriginalAlignmentStart()
{
    return read->Position;
}

// finalizes the changes made.
// returns true if this record actually changes, false otherwise
bool AlignedRead::finalizeUpdate()
{
    // if we haven't made any changes, don't do anything
    if(newCigar == NULL) {
        return false;
    }
    if(newStart == -1) {
        newStart = read->Position;
    } else if(abs(newStart - read->Position) > 200) {
        return false;
    } else if(abs(newStart - read->Position) > 30) {
        //printf("%d\n", newStart - read->Position);
    }

    // annotate the record with the original cigar (and optionally the alignment start)
    //if ( !NO_ORIGINAL_ALIGNMENT_TAGS ) { //NO_ORIGINAL_ALIGNMENT_TAGS=false
    read->EditTag(BamTools::Constants::SAM_CO_BEGIN_TOKEN, "Z", CigarToString(read->CigarData));
    if(newStart != read->Position) {
        read->EditTag("OP", "Z", read->Position);
    }
    //}

    read->CigarData = *newCigar;
    read->Position = newStart;

    return true;
}

void AlignedRead::setMismatchScoreToReference(int score)
{
    mismatchScoreToReference = score;
}

int AlignedRead::getMismatchScoreToReference()
{
    return mismatchScoreToReference;
}

void AlignedRead::setAlignerMismatchScore(long score)
{
    alignerMismatchScore = score;
}

long AlignedRead::getAlignerMismatchScore()
{
    return alignerMismatchScore;
}
