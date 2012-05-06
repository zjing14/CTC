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
#include "ReadBin.h"

const int REFERENCE_PADDING = 30;

ReadBin::ReadBin(string seq, FastaReference *f)
{
    reference = NULL;
    start = -1;
    stop = -1;
    refSeq = seq;
    refLen = -1;
    fr = f;
}

ReadBin::ReadBin(const ReadBin &other)
{
    reference = NULL;
    start = -1;
    stop = -1;
    refSeq = other.refSeq;
    refLen = -1;
    fr = other.fr;
    for(vector<BamAlignment *>::const_iterator itr = other.reads.begin(); itr != other.reads.end(); itr++) {
        add(*itr);
    }

}

ReadBin::~ReadBin()
{
    delete[] reference;
}

void ReadBin::add(BamAlignment *read)
{
    int readLeft = read->Position;
    int readRight = read->GetEndPosition(false, true);
    if(start == -1) {
        start = readLeft;
        stop = readRight;
    } else if(readRight > stop) {
        stop = readRight;
    }
    reads.push_back(read);
}

vector<BamAlignment *>& ReadBin::getReads()
{
    return reads;
}

const byte *ReadBin::getReference()
{
    if(reference == NULL) {
        int padLeft = max(start - REFERENCE_PADDING, 0);
        int padRight = min((unsigned long int)(stop + REFERENCE_PADDING), fr->sequenceLength(refSeq));

        string referenceSequence = fr->getSubSequence(refSeq, padLeft, padRight - padLeft + 1);
        reference = new byte[referenceSequence.length()];
        memcpy(reference, referenceSequence.c_str(), referenceSequence.length());
        refLen = referenceSequence.length();

        start = padLeft;
        stop = padRight;
        for(int t = 0; t < referenceSequence.length(); t++) {
            reference[t] = toupper(reference[t]);
        }
    }
    return reference;
}

int ReadBin::getReferenceLen()
{
    if(refLen == -1) {
        getReference();
    }
    return refLen;
}

int ReadBin::getStart()
{
    return start;
}

int ReadBin::getEnd()
{
    return stop;
}

int ReadBin::size()
{
    return reads.size();
}
