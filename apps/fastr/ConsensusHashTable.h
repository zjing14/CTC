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
#ifndef CONSENSUSHASHTABLE_H
#define CONSENSUSHASHTABLE_H

#include "Consensus.h"
#include <stdint.h>

class ConsensusHashTableEntry
{
public:
    ConsensusHashTableEntry *before;
    ConsensusHashTableEntry *after;
    ConsensusHashTableEntry *next;
    Consensus *value;
    uint32_t hash;
    ConsensusHashTableEntry(uint32_t hash, Consensus *value, ConsensusHashTableEntry *next);
    ~ConsensusHashTableEntry();
    void addBefore(ConsensusHashTableEntry *e);
};

class ConsensusHashTable
{
private:
    ConsensusHashTableEntry **table;
    ConsensusHashTableEntry *header;
    long capacity;
    float loadFactor;
    long size;
    long threshold;
    void addEntry(uint32_t hash, Consensus *key, int bucketIndex);
    void resize(long newSize);
    void transfer(ConsensusHashTableEntry **newTable, long newCapacity);
    void deleteTable(ConsensusHashTableEntry **oldTable, int size);
    void createEntry(uint32_t hash, Consensus *e, int bucketIndex);
public:
    ConsensusHashTable();
    ~ConsensusHashTable();
    //      Consensus* get(int32_t key);
    bool put(Consensus *value);
    bool remove(uint32_t key);
    Consensus *getIndex(int idx);
    void toArray(vector<Consensus *>& c);
    int getSize();
};

#endif
