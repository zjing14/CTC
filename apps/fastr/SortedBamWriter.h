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
#ifndef _SORTEDBAMWRITER_H_
#define _SORTEDBAMWRITER_H_

#include "api/BamAlignment.h"
#include "api/BamReader.h"
#include "api/BamWriter.h"
#include "api/BamAux.h"

#include <vector>

using namespace BamTools;
using namespace std;

class SortedBamWriter
{
public:
    SortedBamWriter(int num_threads);
    SortedBamWriter();
    ~SortedBamWriter();
    void addRead(BamAlignment *read);
    void addRead(BamAlignment *read, bool finalReads);
    void addReads(vector<BamAlignment *>& reads);
    void open(const std::string filename, const std::string samHeader, const RefVector &referenceSequences);
    void close();
private:
    BamWriter *writer;
    vector<BamAlignment *> reads;
    vector<BamAlignment *> unsorted_reads;

    int32_t current_refID;
    int32_t furthest_read;

    void dump();
    void flush();

    vector<BamAlignment *> merge(vector<BamAlignment *>& list1, vector<BamAlignment *>& list2);
    void merge_new(vector<BamAlignment *>& list1, vector<BamAlignment *>& list2);
    static const int BUFFERLENGTH = 50000;
    static const int MAXREADSINMEMORY = 10000;
};

#endif
