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
#include "GenomeLoc.h"

GenomeLoc::GenomeLoc()
{
    start = -1;
    stop = -1;
    contigIndex = -1;
    contigName = "";
}

GenomeLoc::GenomeLoc(int start, int stop, int index, string name)
{
    this->start = start;
    this->stop = stop;
    this->contigIndex = index;
    this->contigName = name;
}

GenomeLoc::GenomeLoc(int start, int stop)
{
    this->start = start;
    this->stop = stop;
    contigIndex = -1;
    contigName = "";
}

GenomeLoc::GenomeLoc(const BamAlignment &al, const BamReader &reader)
{
    contigIndex = al.RefID;
    if(contigIndex == -1) {
        this->start = 0;
        this->stop = 0;
        contigName = "";
        return;
    }

    this->start = al.Position;
    this->stop = al.GetEndPosition(false, true);
    contigName = reader.GetReferenceData()[contigIndex].RefName;
}

int GenomeLoc::compareContigs(const GenomeLoc &other)
{
    if(this->contigIndex == other.contigIndex) {
        return 0;
    } else if(this->contigIndex > other.contigIndex) {
        return 1;
    }
    return -1;
}

bool GenomeLoc::startsBefore(const GenomeLoc &other)
{
    int comparison = compareContigs(other);
    return (comparison == -1 || (comparison == 0 && start < other.start));
}

bool GenomeLoc::isBefore(const GenomeLoc &other)
{
    int comparison = compareContigs(other);
    return (comparison == -1 || (comparison == 0 && stop < other.start));
}

bool GenomeLoc::isBefore(const GenomeLoc &other, int padding)
{
    int comparison = compareContigs(other);
    return (comparison == -1 || (comparison == 0 && stop + padding < other.start));
}

bool GenomeLoc::isAfter(const GenomeLoc &other)
{
    int comparison = compareContigs(other);
    return (comparison == 1 || (comparison == 0 && start > other.stop));
}

bool GenomeLoc::contains(const GenomeLoc &other)
{
    if(this->contigIndex != other.contigIndex) {
        return false;
    }
    return other.stop >= start && other.start <= stop;
}

int GenomeLoc::getStart()
{
    return start;
}

int GenomeLoc::getStop()
{
    return stop;
}

string GenomeLoc::getContigName()
{
    return contigName;
}

int GenomeLoc::getContigIndex()
{
    return contigIndex;
}
