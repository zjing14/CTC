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
**********************************************************************************/
#include "SortedBamWriter.h"
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <sys/time.h>

extern double sort_t;

static struct timeval start_t, end_t;
#define TIMER_START gettimeofday(&start_t, NULL)
#define TIMER_END gettimeofday(&end_t, NULL)
#define MICRO_SECONDS ((end_t.tv_sec - start_t.tv_sec)*1e6 + (end_t.tv_usec - start_t.tv_usec))

struct BamSortLessThanPositionPtr {
    bool operator()(const BamAlignment *lhs, const BamAlignment *rhs) {
        return (((uint64_t)lhs->RefID << 32 | (lhs->Position + 1)) < ((uint64_t)rhs->RefID << 32 | (rhs->Position + 1)));
    }
};

SortedBamWriter::SortedBamWriter(int num_threads)
{
    writer = new BamWriter(num_threads);
    current_refID = -1;
}

SortedBamWriter::SortedBamWriter()
{
    writer = new BamWriter();
}

SortedBamWriter::~SortedBamWriter()
{
    close();
    delete writer;
}

void SortedBamWriter::close()
{
    if(!writer->IsOpen()) {
        return;
    }
    //Write out all remaining reads
    dump();
    writer->Close();
}

void SortedBamWriter::dump()
{
    reads.insert(reads.end(), unsorted_reads.begin(), unsorted_reads.end());
    unsorted_reads.clear();

    TIMER_START;
    sort(reads.begin(), reads.end(), BamSortLessThanPositionPtr());
    TIMER_END;
    sort_t += MICRO_SECONDS;

    for(vector<BamAlignment *>::iterator itr = reads.begin(); itr != reads.end(); itr++) {
        writer->SaveAlignment(**itr);
        delete *itr;
    }
    reads.clear();
}

void SortedBamWriter::merge_new(vector<BamAlignment *>& list1, vector<BamAlignment *>& list2)
{
    list1.reserve(list1.size() + list2.size());
    vector<BamAlignment *>::iterator itr1 = list1.begin();
    vector<BamAlignment *>::iterator itr2 = list2.begin();

    for(int i = 0; i < list2.size(); i++) {
        while(itr1 != list1.end() && (*itr1)->Position < (*itr2)->Position) {
            itr1++;
        }
        if(itr1 == list1.end()) {
            list1.push_back(*itr2);
        } else {
            list1.insert(itr1, *itr2);
        }
        itr2++;
    }
}

vector<BamAlignment *> SortedBamWriter::merge(vector<BamAlignment *>& list1, vector<BamAlignment *>& list2)
{
    vector<BamAlignment *> toReturn;
    toReturn.reserve(list1.size() + list2.size());

    vector<BamAlignment *>::iterator itr1 = list1.begin();
    vector<BamAlignment *>::iterator itr2 = list2.begin();
    for(int i = 0; i < list1.size() + list2.size(); i++) {
        if(itr1 == list1.end()) {
            toReturn.push_back(*itr2);
            itr2++;
        } else if(itr2 == list2.end()) {
            toReturn.push_back(*itr1);
            itr1++;
        } else {
            if(((uint64_t)(*itr1)->RefID << 32 | ((*itr1)->Position + 1)) < ((uint64_t)(*itr2)->RefID << 32 | ((*itr2)->Position + 1))) {
                toReturn.push_back(*itr1);
                itr1++;
            } else {
                toReturn.push_back(*itr2);
                itr2++;
            }
        }
    }
    return toReturn;
}

void SortedBamWriter::flush()
{
    TIMER_START;
    sort(unsorted_reads.begin(), unsorted_reads.end(), BamSortLessThanPositionPtr());
    TIMER_END;
    sort_t += MICRO_SECONDS;

    int size = reads.size();
    reads = merge(reads, unsorted_reads);
    unsorted_reads.clear();
    BamAlignment *lastRead = reads.back();
    vector<BamAlignment *>::iterator itr = reads.begin();
    while(lastRead->RefID != (*itr)->RefID || lastRead->Position - (*itr)->Position > BUFFERLENGTH) {
        writer->SaveAlignment(**itr);
        delete(*itr);
        itr++;
        if(itr == reads.end()) {
            break;
        }
    }
    reads.erase(reads.begin(), itr);
    itr = reads.begin();
    lastRead = reads.back();
}

void SortedBamWriter::addRead(BamAlignment *read, bool finalReads)
{
    if(finalReads) {
        if(current_refID != -2) {
            dump();
            current_refID = -2;
        }

        writer->SaveAlignment(*read);
        delete read;

        return;
    }
    //  if(read->RefID != current_refID) // Change chromosome
    //  {
    //      dump();
    //      current_refID = read->RefID;
    //  }
    unsorted_reads.push_back(read);
}

void SortedBamWriter::addRead(BamAlignment *read)
{
    addRead(read, false);
}

void SortedBamWriter::addReads(vector<BamAlignment *>& in_reads)
{
    if(in_reads.size() == 0) {
        return;
    }
    //  if(in_reads[0]->RefID != current_refID)
    //  {
    //      dump();
    //      current_refID = in_reads[0]->RefID;
    //  }
    unsorted_reads.insert(unsorted_reads.end(), in_reads.begin(), in_reads.end());
    if(unsorted_reads.size() + reads.size() >= MAXREADSINMEMORY) {
        flush();
    }
}

void SortedBamWriter::open(const std::string filename, const std::string samHeader, const RefVector &referenceSequences)
{
    writer->Open(filename, samHeader, referenceSequences);
}
