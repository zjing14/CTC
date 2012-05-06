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
// ***************************************************************************
// FastaIndex.h (c) 2010 Erik Garrison <erik.garrison@bc.edu>
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 5 February 2010 (EG)
// ---------------------------------------------------------------------------

#ifndef _FASTA_H
#define _FASTA_H

#include <map>
#include <iostream>
#include <fstream>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include "LargeFileSupport.h"
#include <sys/stat.h>
#include "split.h"
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

using namespace std;

class FastaIndexEntry
{
    friend ostream &operator<<(ostream &output, const FastaIndexEntry &e);
public:
    FastaIndexEntry(string name, int length, long long offset, int line_blen, int line_len);
    FastaIndexEntry(void);
    ~FastaIndexEntry(void);
    string name;  // sequence name
    int length;  // length of sequence
    long long offset;  // bytes offset of sequence from start of file
    int line_blen;  // line length in bytes, sequence characters
    int line_len;  // line length including newline
    void clear(void);
};

class FastaIndex : public map<string, FastaIndexEntry>
{
    friend ostream &operator<<(ostream &output, FastaIndex &i);
public:
    FastaIndex(void);
    ~FastaIndex(void);
    vector<string> sequenceNames;
    void indexReference(string refName);
    void readIndexFile(string fname);
    void writeIndexFile(string fname);
    ifstream indexFile;
    FastaIndexEntry entry(string key);
    void flushEntryToIndex(FastaIndexEntry &entry);
    string indexFileExtension(void);
};

class FastaReference
{
public:
    FastaReference(string reffilename);
    string filename;
    FastaReference();
    ~FastaReference(void);
    FILE *file;
    FastaIndex *index;
    vector<FastaIndexEntry> findSequencesStartingWith(string seqnameStart);
    string getSequence(string seqname);
    // potentially useful for performance, investigate
    // void getSequence(string seqname, string& sequence);
    string getSubSequence(string seqname, int start, int length);
    string sequenceNameStartingWith(string seqnameStart);
    long unsigned int sequenceLength(string seqname);
};

#endif
