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
#include "ConsensusHashTable.h"
#include <limits.h>

ConsensusHashTableEntry::ConsensusHashTableEntry(uint32_t hash, Consensus *value, ConsensusHashTableEntry *next)
{
    this->hash = hash;
    this->value = value;
    this->next = next;
    before = NULL;
    after = NULL;
}

ConsensusHashTableEntry::~ConsensusHashTableEntry()
{
    delete value;
}

void ConsensusHashTableEntry::addBefore(ConsensusHashTableEntry *existingEntry)
{
    after  = existingEntry;
    before = existingEntry->before;
    before->after = this;
    after->before = this;
}

ConsensusHashTable::ConsensusHashTable()
{
    capacity = 16;
    loadFactor = 0.75;
    size = 0;

    threshold = (int)(capacity * loadFactor);

    header = new ConsensusHashTableEntry(0, NULL, NULL);
    header->before = header->after = header;

    table = new ConsensusHashTableEntry*[capacity];
    for(int i = 0; i < capacity; i++) {
        table[i] = NULL;
    }
}

ConsensusHashTable::~ConsensusHashTable()
{
    deleteTable(table, capacity);
    delete header;
}

int ConsensusHashTable::getSize()
{
    return size;
}

Consensus *ConsensusHashTable::getIndex(int idx)
{
    for(int i = 0; i < capacity; i++) {
        ConsensusHashTableEntry *e = table[i];
        while(e != NULL) {
            if(idx == 0) {
                return e->value;
            }
            idx--;
            e = e->next;
        }
    }
    return NULL;
}

void ConsensusHashTable::toArray(vector<Consensus *>& c)
{
    ConsensusHashTableEntry *n = header->after;
    while(n != header) {
        c.push_back(n->value);
        n = n->after;
    }
    //  for(int i = 0; i < capacity; i++)
    //  {
    //      ConsensusHashTableEntry* e = table[i];
    //      while(e != NULL)
    //      {
    //          cout << "###\t" << i << endl;
    //          c.push_back(e->value);
    //          e = e->next;
    //      }
    //  }
}

void ConsensusHashTable::deleteTable(ConsensusHashTableEntry **table, int size)
{
    for(int i = 0; i < size; i++) {
        ConsensusHashTableEntry *e = table[i];
        if(e != NULL) {
            while(e->next != NULL) {
                ConsensusHashTableEntry *next = e->next;
                delete e;
                e = next;
            }
            delete e;
        }
    }
    delete[] table;
}

uint32_t hashFunc(uint32_t h)
{
    h ^= (h >> 20) ^(h >> 12);
    h ^= (h >> 7) ^(h >> 4);
    return h;
}

int indexFor(uint32_t hash, int capacity)
{
    return hash & (capacity - 1);
}

void ConsensusHashTable::resize(long newCapacity)
{
    //printf("###\tResizing\n");
    ConsensusHashTableEntry **oldTable = table;
    int oldCapacity = capacity;
    if(oldCapacity == 1 << 30) {
        threshold = LONG_MAX;
        return;
    }

    ConsensusHashTableEntry **newTable = new ConsensusHashTableEntry*[newCapacity];
    for(int i = 0; i < newCapacity; i++) {
        newTable[i] = NULL;
    }
    transfer(newTable, newCapacity);
    deleteTable(oldTable, oldCapacity);
    table = newTable;
    capacity = newCapacity;
    threshold = (long)(newCapacity * loadFactor);
}

void ConsensusHashTable::transfer(ConsensusHashTableEntry **newTable, long newCapacity)
{
    ConsensusHashTableEntry **src = table;
    for(int j = 0; j < capacity; j++) {
        ConsensusHashTableEntry *e = src[j];
        if(e != NULL) {
            src[j] = NULL;
            do {
                ConsensusHashTableEntry *next = e->next;
                int i = indexFor(e->hash, newCapacity);
                e->next = newTable[i];
                newTable[i] = e;
                e = next;
            } while(e != NULL);
        }
    }
}

void ConsensusHashTable::createEntry(uint32_t hash, Consensus *key, int bucketIndex)
{
    ConsensusHashTableEntry *old = table[bucketIndex];
    ConsensusHashTableEntry *e = new ConsensusHashTableEntry(hash, key, old);
    table[bucketIndex] = e;
    e->addBefore(header);
    size++;
}

void ConsensusHashTable::addEntry(uint32_t hash, Consensus *key, int bucketIndex)
{
    createEntry(hash, key, bucketIndex);

    // Remove eldest entry if instructed, else grow capacity if appropriate
    //ConsensusHashTableEntry* eldest = header->after;
    if(size >= threshold) {
        resize(2 * capacity);
    }

    //  ConsensusHashTableEntry* e = table[bucketIndex];
    //  table[bucketIndex] = new ConsensusHashTableEntry(hash, key, e);
    //  if(size++ >= threshold)
    //      resize(2 * capacity);

}

bool ConsensusHashTable::put(Consensus *value)
{
    uint32_t hash = hashFunc(value->hashCode());
    int i = indexFor(hash, capacity);
    for(ConsensusHashTableEntry *e = table[i]; e != NULL; e = e->next) {
        if(e->hash == hash) {
            return false;
        }
    }

    addEntry(hash, value, i);
    return true;
}


