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
#include "Consensus.h"
#include "string.h"

Consensus::Consensus(byte str[], int strLen, Cigar cigar, int positionOnReference)
{
    this->str = str;
    this->strLen = strLen;
    this->cigar = cigar;
    this->positionOnReference = positionOnReference;
    mismatchSum = 0;
}

Consensus::Consensus(const Consensus &c)
{
    this->str = new byte[c.strLen];
    memcpy(this->str, c.str, c.strLen);
    this->strLen = c.strLen;
    this->cigar = c.cigar;
    this->positionOnReference = c.positionOnReference;
    this->mismatchSum = c.mismatchSum;
}

bool Consensus::Equals(const Consensus &con)
{
    if(con.strLen != strLen) {
        return false;
    }
    if(strncmp(con.str, str, strLen) != 0) {
        return false;
    }
    return true;
}

Consensus::~Consensus()
{
    delete[] str;
}

uint32_t Consensus::hashCode()
{
    if(str == NULL) {
        return 0;
    }

    int32_t result = 1;
    for(int32_t i = 0; i < strLen; i++) {
        result = 31 * result + str[i];
    }
    return (uint32_t)result;
}
