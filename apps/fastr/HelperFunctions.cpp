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
#include "HelperFunctions.h"

#include "api/BamAux.h"
#include <iostream>
#include <sstream>
#include "string.h"

using namespace std;
using namespace BamTools;

string CigarToString(const Cigar &c)
{
    string toRet;
    stringstream ss;
    for(Cigar::const_iterator itr = c.begin(); itr != c.end(); itr++) {
        CigarOp op = *itr;
        ss << op.Length;
        ss << op.Type;
    }
    toRet = ss.str();
    return toRet;
}

Cigar StringToCigar(const string &s)
{
    Cigar toRet;
    stringstream ss(s, stringstream::in);
    while(!ss.eof()) {
        unsigned long length;
        unsigned char c;
        ss >> length;
        ss >> c;
        if(ss.good()) {
            //          cout << length << c << endl;
            toRet.push_back(CigarOp(c, length));
        }
    }
    return toRet;
}

bool CigarsEqual(Cigar a, Cigar b)
{
    Cigar::iterator itr, itr2;
    for(itr = a.begin(), itr2 = b.begin(); itr != a.end() && itr2 != b.end(); itr++, itr2++) {
        CigarOp aop = *itr;
        CigarOp bop = *itr2;
        if(aop.Length != bop.Length) {
            return false;
        }
        if(aop.Type != bop.Type) {
            return false;
        }
    }
    if(itr != a.end() || itr2 != b.end()) {
        return false;
    }
    return true;
}


byte *createIndelString(const Cigar cigar, const int indexOfIndel,
                        const byte refSeq[], const int refSeqLen,
                        const byte readSeq[], const int readSeqLen,
                        int refIndex, int readIndex, int &altLength)
{
    CigarOp indel = cigar[indexOfIndel];
    int indelLength = indel.Length;

    int totalRefBases = 0;
    for(int i = 0; i < indexOfIndel; i++) {
        CigarOp ce = cigar[i];
        int length = ce.Length;

        switch(ce.Type) {
        case 'M':
            readIndex += length;
            refIndex += length;
            totalRefBases += length;
            break;
        case 'S':
            readIndex += length;
            break;
        case 'N':
            refIndex += length;
            totalRefBases += length;
            break;
        default:
            break;
        }
    }

    // sometimes, when there are very large known indels, we won't have enough reference sequence to cover them
    if(totalRefBases + indelLength > refSeqLen) {
        indelLength -= (totalRefBases + indelLength - refSeqLen);
    }

    // the indel-based reference string
    altLength = refSeqLen + (indelLength * (indel.Type == 'D' ? -1 : 1));
    byte *alt = new byte[altLength];

    // add the bases before the indel, making sure it's not aligned off the end of the reference
    if(refIndex > altLength || refIndex > refSeqLen) {
        delete[] alt;
        return NULL;
    }
    memcpy(alt, refSeq, refIndex);
    //    System.arraycopy(refSeq, 0, alt, 0, refIndex);
    int currentPos = refIndex;

    // take care of the indel
    if(indel.Type == 'D') {
        refIndex += indelLength;
    } else {
        memcpy(alt + currentPos, readSeq + readIndex, indelLength);
        //        System.arraycopy(readSeq, readIndex, alt, currentPos, indelLength);
        currentPos += indelLength;
    }

    // add the bases after the indel, making sure it's not aligned off the end of the reference
    if(refSeqLen - refIndex > altLength - currentPos) {
        delete[] alt;
        return NULL;
    }
    memcpy(alt + currentPos, refSeq + refIndex, refSeqLen - refIndex);
    //System.arraycopy(refSeq, refIndex, alt, currentPos, refSeq.length - refIndex);

    return alt;
}


Cigar leftAlignIndel(Cigar cigar, const byte refSeq[],
                     const int refSeqLen, const byte readSeq[],
                     const int readSeqLen, const int refIndex,
                     const int readIndex)
{
    int indexOfIndel = -1;
    for(unsigned int i = 0; i < cigar.size(); i++) {
        CigarOp ce = cigar[i];
        if(ce.Type == 'D' || ce.Type == 'I') {
            // if there is more than 1 indel, don't left align
            if(indexOfIndel != -1) {
                return cigar;
            }
            indexOfIndel = i;
        }
    }

    // if there is no indel or if the alignment starts with an insertion (so that there
    // is no place on the read to move that insertion further left), we are done
    if(indexOfIndel < 1) {
        return cigar;
    }

    const int indelLength = cigar[indexOfIndel].Length;

    int altLength;
    byte *altString = createIndelString(cigar, indexOfIndel, refSeq, refSeqLen, readSeq, readSeqLen, refIndex, readIndex, altLength);
    if(altString == NULL) {
        return cigar;
    }


    Cigar newCigar = cigar;
    for(int i = 0; i < indelLength; i++) {
        newCigar = moveCigarLeft(newCigar, indexOfIndel);
        int newAltLength;
        byte *newAltString = createIndelString(newCigar, indexOfIndel, refSeq, refSeqLen, readSeq, readSeqLen, refIndex, readIndex, newAltLength);

        // check to make sure we haven't run off the end of the read
        bool reachedEndOfRead = cigarHasZeroSizeElement(newCigar);

        if(strncmp(altString, newAltString, altLength) == 0) {
            cigar = newCigar;
            i = -1;
            if(reachedEndOfRead) {
                cigar = cleanUpCigar(cigar);
            }
        }

        delete[] newAltString;

        if(reachedEndOfRead) {
            break;
        }
    }

    delete[] altString;

    return cigar;
}

int getNumAlignmentBlocks(const BamAlignment &r)
{
    int n = 0;
    Cigar c = r.CigarData;
    if(c.size() == 0) {
        return 0;
    }

    for(Cigar::iterator itr = c.begin(); itr != c.end(); itr++) {
        CigarOp op = (*itr);
        if(op.Type == 'M') {
            n++;
        }
    }

    return n;
}

Cigar moveCigarLeft(Cigar cigar, unsigned int indexOfIndel)
{
    // get the first few elements
    Cigar newCigar;
    //ArrayList<CigarElement> elements = new ArrayList<CigarElement>(cigar.numCigarElements());
    for(unsigned int i = 0; i < indexOfIndel - 1; i++) {
        newCigar.push_back(cigar[i]);
    }

    // get the indel element and move it left one base
    CigarOp ce = cigar[indexOfIndel - 1];
    newCigar.push_back(CigarOp(ce.Type, ce.Length - 1));
    newCigar.push_back(cigar[indexOfIndel]);
    if(indexOfIndel + 1 < cigar.size()) {
        ce = cigar[indexOfIndel + 1];
        newCigar.push_back(CigarOp(ce.Type, ce.Length + 1));
    } else {
        newCigar.push_back(CigarOp('M', 1));
    }

    // get the last few elements
    for(unsigned int i = indexOfIndel + 2; i < cigar.size(); i++) {
        newCigar.push_back(cigar[i]);
    }
    return newCigar;
}

bool cigarHasZeroSizeElement(Cigar c)
{
    for(Cigar::iterator itr = c.begin(); itr != c.end(); itr++) {
        CigarOp ce = (*itr);
        if(ce.Length == 0) {
            return true;
        }
    }
    return false;
}

Cigar cleanUpCigar(Cigar c)
{
    //ArrayList<CigarElement> elements = new ArrayList<CigarElement>(c.numCigarElements()-1);
    Cigar newCigar;
    for(Cigar::iterator itr = c.begin(); itr != c.end(); itr++) {
        CigarOp ce = (*itr);
        if(ce.Length != 0 && (newCigar.size() != 0 || ce.Type != 'D')) {
            newCigar.push_back(ce);
        }
    }
    return newCigar;
}

long mismatchingQualities(const BamAlignment &r, const byte refSeq[], const int refSeqLen, int refIndex)
{
    MismatchCount mc = getMismatchCount(r, refSeq, refSeqLen, refIndex, 0, r.QueryBases.length());

    return mc.mismatchQualities;
}

MismatchCount getMismatchCount(const BamAlignment &r, const byte refSeq[], const int refSeqLen, int refIndex, int startOnRead, int nReadBases)
{
    MismatchCount mc;
    mc.numMismatches = 0;
    mc.mismatchQualities = 0;

    int readIdx = 0;
    int endOnRead = startOnRead + nReadBases - 1; // index of the last base on read we want to count
    const byte *readSeq = r.QueryBases.c_str();
    Cigar c = r.CigarData;
    for(unsigned int i = 0 ; i < c.size() ; i++) {

        if(readIdx > endOnRead) {
            break;
        }

        CigarOp ce = c[i];
        switch(ce.Type) {
        case 'M':
            for(unsigned int j = 0 ; j < ce.Length ; j++, refIndex++, readIdx++) {
                if(refIndex >= refSeqLen) {
                    continue;
                }
                if(readIdx < startOnRead) {
                    continue;
                }
                if(readIdx > endOnRead) {
                    break;
                }
                byte refChr = refSeq[refIndex];
                byte readChr = readSeq[readIdx];
                // Note: we need to count X/N's as mismatches because that's what SAM requires
                //if ( BaseUtils.simpleBaseToBaseIndex(readChr) == -1 ||
                //     BaseUtils.simpleBaseToBaseIndex(refChr)  == -1 )
                //    continue; // do not count Ns/Xs/etc ?
                if(readChr != refChr) {
                    mc.numMismatches++;
                    mc.mismatchQualities += r.Qualities[readIdx] - 33;
                }
            }
            break;
        case 'I':
        case 'S':
            readIdx += ce.Length;
            break;
        case 'D':
        case 'N':
            refIndex += ce.Length;
            break;
        case 'H':
        case 'P':
            break;
            //default:// throw new ReviewedStingException("The " + ce.getOperator() + " cigar element is not currently supported");
        }

    }
    return mc;
}

bool isRegularBase(const char c)
{
    switch(c) {
    case 'A':
    case 'a':
    case 'C':
    case 'c':
    case 'G':
    case 'g':
    case 'T':
    case 't':
        return true;
    default:
        return false;
    }
}
