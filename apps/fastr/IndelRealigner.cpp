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
//from bamtools:
#include "api/BamAlignment.h"
#include "api/BamReader.h"        // (or "BamMultiReader.h") as needed
#include "api/BamWriter.h"        // as needed
#include "api/BamAux.h"

#include "SWPairwiseAlignment.h"
#include "Consensus.h"
#include "AlignedRead.h"
#include "ReadBin.h"
#include "GenomeLoc.h"
#include "ConsensusHashTable.h"
#include "BamRegionData.h"

#include "Fasta.h"
#include "disorder.h"
#include "split.h"

#include <stdlib.h>
#include <climits>
#include <algorithm>
#include <iostream>
#include <set>
#include <getopt.h>
#include <sys/time.h>
#include "cuda/swat_cuda.h"
#include "cuda/FastOffset.h"
using namespace std;
using namespace BamTools;

double prep_t = 0;
double align_t = 0;
double output_t = 0;
double sort_t = 0;
double read_t = 0;

int equalConsensusCount = 0;
int loci = 0;

static struct timeval start_t, end_t;
#define TIMER_START gettimeofday(&start_t, NULL)
#define TIMER_END gettimeofday(&end_t, NULL)
#define MICRO_SECONDS ((end_t.tv_sec - start_t.tv_sec)*1e6 + (end_t.tv_usec - start_t.tv_usec))

static struct timeval start_t1, end_t1;
#define TIMER_START1 gettimeofday(&start_t1, NULL)
#define TIMER_END1 gettimeofday(&end_t1, NULL)
#define MICRO_SECONDS1 ((end_t1.tv_sec - start_t1.tv_sec)*1e6 + (end_t1.tv_usec - start_t1.tv_usec))

#define MAX_BATCHED_READ        5000
#define REGIONS_PER_THREAD      32 
// #define MAX_ReadLengh 100
static char ch_lookup[128];
int score_offset;
long int actual_cleaned;
int use_gpu_flag = 1;
//ref_offset, readSeq_offset, quals_offset, ReadLength_offset, originalAlignment_offset;
//class for parsing region argument:
//This will likely get put into its own class file in the near future.
class Region
{
public:
    string startSeq;
    int startPos;
    int stopPos;

    Region() {
        startSeq = "";
        startPos = -1;
        stopPos = -1;
    }

    Region(string &region) {
        startPos = -1;
        stopPos = -1;
        size_t foundFirstColon = region.find(":");
        // we only have a single string, use the whole sequence as the target
        if(foundFirstColon == string::npos) {
            startSeq = region;
        } else {
            startSeq = region.substr(0, foundFirstColon);
            size_t foundRangeDots = region.find("-", foundFirstColon);
            if(foundRangeDots == string::npos) {
                startPos = atoi(region.substr(foundFirstColon + 1).c_str()) - 1;
                stopPos = startPos; // just print one base if we don't give an end
            } else {
                startPos = atoi(region.substr(foundFirstColon + 1, foundRangeDots - foundRangeDots - 1).c_str()) - 1;
                stopPos = atoi(region.substr(foundRangeDots + 1).c_str()) - 1; // to the start of this chromosome
            }
        }
    }

    int length(void) {
        if(stopPos > 0) {
            return stopPos - startPos + 1;
        } else {
            return 1;
        }
    }
};


const double LOD_THRESHOLD = 5.0; //The minimum improvement necessary to realign reads [likely to be taken out]

//Smith-Waterman parameters
double SW_MATCH = 30.0;   // 1.0;
double SW_MISMATCH = -10.0;  //-1.0/3.0;
double SW_GAP = -10.0;     //-1.0-1.0/3.0;
double SW_GAP_EXTEND = -2.0; //-1.0/.0;

const unsigned int MAX_QUAL = 99;
const int MAX_ISIZE_FOR_MOVEMENT = 3000;

const double MISMATCH_THRESHOLD = 0.15;
const double MISMATCH_COLUMN_CLEANED_FRACTION = 0.75;


// create a Consensus from cigar/read strings which originate somewhere on the reference
Consensus *createAlternateConsensus(const int indexOnRef, const Cigar c, const byte reference[], const int refLen, const byte readStr[], const int readLen)
{
    if(indexOnRef < 0) {
        return NULL;
    }

    // if there are no indels, we do not need this consensus, can abort early:
    if(c.size() == 1 && c[0].Type == 'M') {
        return NULL;
    }

    // create the new consensus
    Cigar elements;
    string sb;

    for(int i = 0; i < indexOnRef; i++) {
        sb += reference[i];
    }

    int indelCount = 0;
    int altIdx = 0;
    int refIdx = indexOnRef;
    bool ok_flag = true;
    for(unsigned int i = 0 ; i < c.size() ; i++) {
        CigarOp ce = c[i];
        int elementLength = ce.Length;
        switch(ce.Type) {
        case 'D':
            refIdx += elementLength;
            indelCount++;
            elements.push_back(ce);
            break;
        case 'M':
            altIdx += elementLength;
        case 'N':
            if(refLen < refIdx + elementLength) {
                ok_flag = false;
            } else  {
                for(int j = 0; j < elementLength; j++) {
                    sb += (char)reference[refIdx + j];
                }
            }
            refIdx += elementLength;
            elements.push_back(CigarOp('M', elementLength));
            break;
        case 'I':
            for(int j = 0; j < elementLength; j++) {
                // if ( ! isRegularBase(readStr[altIdx+j]) ) {
                if(! ch_lookup[readStr[altIdx + j]]) {
                    // Insertions with N's in them cause real problems sometimes; it's better to drop them altogether
                    ok_flag = false;
                    break;
                }
                sb += (char)readStr[altIdx + j];
            }
            altIdx += elementLength;
            indelCount++;
            elements.push_back(ce);
            break;
        case 'S':
        default:
            break;
        }
    }


    // make sure that there is at most only a single indel and it aligns appropriately!
    if(!ok_flag || indelCount != 1 || refLen < refIdx) {
        return NULL;
    }

    for(int i = refIdx; i < refLen; i++) {
        sb += (char)reference[i];
    }
    const byte *altConsensus =  sb.c_str(); // alternative consensus sequence we just built from the current read

    byte *toPass = new byte[sb.length()];
    for(unsigned int i = 0; i < sb.length(); i++) {
        toPass[i] = altConsensus[i];
    }
    return new Consensus(toPass, sb.length(), elements, indexOnRef);
}

//Determines the number of mismatches between the read and the reference
//Scores the number of mismatches based on the quality of the read's base.
long mismatchQualitySumIgnoreCigar(AlignedRead &aRead, const byte *refSeq, long refLen, long refIndex, long quitAboveThisValue)
{
    byte *readSeq = aRead.getReadBases();
    byte *quals = aRead.getBaseQualities();
    int sum = 0;

    for(int readIndex = 0 ; readIndex < aRead.getReadLength() ; refIndex++, readIndex++) {

        if(refIndex >= refLen) {
            sum += MAX_QUAL;
            // optimization: once we pass the threshold, stop calculating
            if(sum > quitAboveThisValue) {
                return sum;
            }
        } else {
            byte refChr = refSeq[refIndex];
            byte readChr = readSeq[readIndex];
            // if ( !isRegularBase(readChr) || !isRegularBase(refChr) )
            if(!ch_lookup[readChr] || !ch_lookup[refChr]) {
                continue;    // do not count Ns/Xs/etc ?
            }
            if(readChr != refChr) {
                sum += (int)quals[readIndex];
                // optimization: once we pass the threshold, stop calculating
                if(sum > quitAboveThisValue) {
                    return sum;
                }
            }
        }
    }
    return sum;
}

//Populates the altAlignmentsToTest and altConsenses with alignments to generate
//consensuses from and consensuses to test, respectively.
long determineReadsThatNeedCleaning(const vector<BamAlignment *>& reads,
                                    vector<BamAlignment *>& refReadsToPopulate,
                                    vector<AlignedRead *>& altReadsToPopulate,
                                    vector<AlignedRead *>& altAlignmentsToTest,
                                    ConsensusHashTable &altConsenses,
                                    const int leftmostIndex,
                                    const byte reference[],
                                    const int refLen)
{
    long totalRawMismatchSum = 0L;


    for(vector<BamAlignment *>::const_iterator itr = reads.begin(); itr != reads.end(); itr++) {
        BamAlignment *read = *itr;

        // we can not deal with screwy records
        if(read->CigarData.size() == 0) {
            refReadsToPopulate.push_back(read);
            continue;
        }

        AlignedRead *aRead = new AlignedRead(read);

        // first, move existing indels (for 1 indel reads only) to leftmost position within identical sequence
        int numBlocks = getNumAlignmentBlocks(*read);
        if(numBlocks == 2) {
            Cigar newCigar = leftAlignIndel(IndelRealigner::unclipCigar(read->CigarData), reference, refLen, read->QueryBases.c_str(), read->QueryBases.length(), read->Position - leftmostIndex, 0);
            Cigar *s = new Cigar;
            *s = newCigar;
            aRead->setCigar(s, false);
        }

        const int startOnRef = read->Position - leftmostIndex;
        const long rawMismatchScore = mismatchQualitySumIgnoreCigar(*aRead, reference, refLen, startOnRef, LONG_MAX);

        // if this doesn't match perfectly to the reference, let's try to clean it
        if(rawMismatchScore > 0) {
            altReadsToPopulate.push_back(aRead);
            if(!read->IsDuplicate()) {
                totalRawMismatchSum += rawMismatchScore;
            }
            aRead->setMismatchScoreToReference(rawMismatchScore);
            aRead->setAlignerMismatchScore(mismatchingQualities(*aRead->getRead(), reference, refLen, startOnRef));

            // if it has an indel, let's see if that's the best consensus
            if(numBlocks == 2)  {
                Consensus *c = createAlternateConsensus(startOnRef, aRead->getCigar(), reference, refLen, aRead->getReadBases(), aRead->getReadLength());
                if(c != NULL) {
                    bool added = altConsenses.put(c);
                    if(!added) {
                        delete c;
                    }
                }
            } else {
                altAlignmentsToTest.push_back(aRead);
            }
        }
        // otherwise, we can emit it as is
        else {
            delete aRead;
            refReadsToPopulate.push_back(read);
        }
    }
    return totalRawMismatchSum;
}

//Creates an Alternate Consensus and add them to the altConsensesToPopulate table.
void createAndAddAlternateConsensus(const byte read[], const int readLen, ConsensusHashTable &altConsensesToPopulate, const byte reference[], const int referenceLen)
{

    // do a pairwise alignment against the reference
    SWPairwiseAlignment swConsensus(reference, referenceLen, read, readLen, SW_MATCH, SW_MISMATCH, SW_GAP, SW_GAP_EXTEND);
    Consensus *c = createAlternateConsensus(swConsensus.getAlignmentStart2wrt1(), swConsensus.getCigar(), reference, referenceLen, read, readLen);
    if(c != NULL) {
        if(!altConsensesToPopulate.put(c)) {
            delete c;
        }
    }
}
//Returns the number of blocks which contain "M", "=", or "X".
int getNumAlignmentBlocks(BamAlignment *al)
{
    Cigar cigar = al->CigarData;
    int blocks = 0;
    for(unsigned int i = 0; i < cigar.size(); i++) {
        switch(cigar[i].Type) {
        case 'M':
        case '=':
        case 'X':
            blocks++;
        default:
            break;
        }
    }
    return blocks;
}

//Determines if realigning the reads to a new reference would decrease the entropy of
//all of the reads.
bool alternateReducesEntropy(const vector<AlignedRead *>& reads, const byte *reference, const int refLen, const int leftmostIndex)
{
    int *originalMismatchBases = new int[refLen];
    int *cleanedMismatchBases = new int[refLen];
    int *totalOriginalBases = new int[refLen];
    int *totalCleanedBases = new int[refLen];

    // set to 1 to prevent dividing by zero
    for(int i = 0; i < refLen; i++) {
        originalMismatchBases[i] = totalOriginalBases[i] = cleanedMismatchBases[i] = totalCleanedBases[i] = 0;
    }

    for(int i = 0; i < reads.size(); i++) {
        AlignedRead *read = reads[i];
        if(getNumAlignmentBlocks(read->getRead()) > 1) {
            continue;
        }
        int refIdx = read->getOriginalAlignmentStart() - leftmostIndex;
        const byte *readStr = read->getReadBases();
        const byte *quals = read->getBaseQualities();

        for(int j = 0; j < read->getReadLength(); j++, refIdx++) {
            if(refIdx < 0 || refIdx >= refLen) {
                break;
            }
            totalOriginalBases[refIdx] += quals[j];
            if(readStr[j] != reference[refIdx]) {
                originalMismatchBases[refIdx] += quals[j];
            }
        }

        // reset and now do the calculation based on the cleaning
        refIdx = read->getAlignmentStart() - leftmostIndex;
        int altIdx = 0;
        Cigar c = read->getCigar();
        for(int j = 0 ; j < c.size() ; j++) {
            CigarOp ce = c[j];
            int elementLength = ce.Length;
            switch(ce.Type) {
            case 'M':
                for(int k = 0 ; k < elementLength ; k++, refIdx++, altIdx++) {
                    if(refIdx >= refLen) {
                        break;
                    }
                    totalCleanedBases[refIdx] += quals[altIdx];
                    if(readStr[altIdx] != reference[refIdx]) {
                        cleanedMismatchBases[refIdx] += quals[altIdx];
                    }
                }
                break;
            case 'I':
                altIdx += elementLength;
                break;
            case 'D':
                refIdx += elementLength;
                break;
            case 'S':
            default:
                break;
            }
        }
    }

    int originalMismatchColumns = 0, cleanedMismatchColumns = 0;
    for(int i = 0; i < refLen; i++) {
        if(cleanedMismatchBases[i] == originalMismatchBases[i]) {
            continue;
        }
        if(originalMismatchBases[i] > totalOriginalBases[i] * MISMATCH_THRESHOLD)  {
            originalMismatchColumns++;
            if(totalCleanedBases[i] > 0 && ((double)cleanedMismatchBases[i] / (double)totalCleanedBases[i]) > ((double)originalMismatchBases[i] / (double)totalOriginalBases[i]) * (1.0 - MISMATCH_COLUMN_CLEANED_FRACTION)) {
                cleanedMismatchColumns++;
            }
        } else if(cleanedMismatchBases[i] > totalCleanedBases[i] * MISMATCH_THRESHOLD) {
            cleanedMismatchColumns++;
        }
    }

    const bool reduces = (originalMismatchColumns == 0 || cleanedMismatchColumns < originalMismatchColumns);

    delete[] originalMismatchBases;
    delete[] cleanedMismatchBases;
    delete[] totalOriginalBases;
    delete[] totalCleanedBases;
    return reduces;
}

void generateAlternateConsensesFromReads(vector< vector<AlignedRead *> >& altAlignmentsToTest,
        vector< ConsensusHashTable * >& altConsensesToPopulate,
        vector< ReadBin * >& readBins,
        long randomSeed)
{
    for(int i = 0; i < altAlignmentsToTest.size(); i++) {
        for(vector<AlignedRead *>::iterator itr = altAlignmentsToTest[i].begin(); itr != altAlignmentsToTest[i].end(); itr++) {
            AlignedRead *aRead = *itr;
            aRead->getReadBases();
            createAndAddAlternateConsensus(aRead->getReadBases(), aRead->getReadLength(), *altConsensesToPopulate[i], readBins[i]->getReference(), readBins[i]->getReferenceLen());
        }
    }
}

//Generates alternate consenses from the reads given on the GPU.
void generateAlternateConsensesFromReads_CUDA(vector< vector<AlignedRead *> >& altAlignmentsToTest,
        vector< ConsensusHashTable * >& altConsensesToPopulate,
        vector< ReadBin * >& readBins,
        long randomSeed)
{

    int t_readLen = 0, n_read = 0;
    int t_refLen = 0, n_ref = 0;
    for(int i = 0; i < altAlignmentsToTest.size(); i++) {
        n_ref++;
        t_refLen += readBins[i]->getReferenceLen();
        for(vector<AlignedRead *>::iterator itr = altAlignmentsToTest[i].begin(); itr != altAlignmentsToTest[i].end(); itr++) {
            AlignedRead *aRead = *itr;
            //createAndAddAlternateConsensus(aRead->getReadBases(), aRead->getReadLength(), *altConsensesToPopulate[i], readBins[i]->getReference(), readBins[i]->getReferenceLen());
            n_read++;
            t_readLen += aRead->getReadLength();
        }
    }

    byte *reads = new byte[t_readLen];
    byte *refs = new byte[t_refLen];
    int *read_off = new int[n_read + 1];
    int *ref_off = new int[n_read + 1];
    int *readLens = new int[n_read];
    int *refLens = new int[n_read];
    int counter = 0;
    read_off[counter] = 0;
    ref_off[counter] = 0;
    counter++;
    int read_pos = 0;
    int ref_pos = 0;
    int max_len = 0;
    for(int i = 0; i < altAlignmentsToTest.size(); i++) {
        memcpy(refs + ref_pos, readBins[i]->getReference(), readBins[i]->getReferenceLen());
        for(vector<AlignedRead *>::iterator itr = altAlignmentsToTest[i].begin(); itr != altAlignmentsToTest[i].end(); itr++) {
            AlignedRead *aRead = *itr;
            memcpy(reads + read_pos, aRead->getReadBases(), aRead->getReadLength());
            read_pos += aRead->getReadLength();
            read_off[counter] = read_pos;
            ref_off[counter] = ref_pos;
            readLens[counter - 1] = aRead->getReadLength();
            refLens[counter - 1] = readBins[i]->getReferenceLen();
            if(refLens[counter - 1] > max_len) {
                max_len = refLens[counter - 1];
            }
            counter++;
        }
        ref_pos += readBins[i]->getReferenceLen();
        ref_off[counter - 1] = ref_pos;
    }

    vector< vector<CigarOp> > Cigars;
    int *alignment_offset = new int[n_read];
    swat(refs, t_refLen, ref_off, refLens, n_ref, reads, t_readLen,  read_off, readLens, n_read, SW_GAP, SW_GAP_EXTEND, SW_MATCH, SW_MISMATCH, Cigars, alignment_offset, max_len);
    int k = 0;
    for(int i = 0; i < altAlignmentsToTest.size(); i++) {
        for(vector<AlignedRead *>::iterator itr = altAlignmentsToTest[i].begin(); itr != altAlignmentsToTest[i].end(); itr++) {
            AlignedRead *aRead = *itr;
            Consensus *c = createAlternateConsensus(alignment_offset[k], Cigars[k], readBins[i]->getReference(), readBins[i]->getReferenceLen(), aRead->getReadBases(), aRead->getReadLength());
            if(c != NULL) {
                if(!(*altConsensesToPopulate[i]).put(c)) {
                    delete c;
                }
            }
            k++;
        }
    }


    delete alignment_offset;
    delete reads;
    delete refs;
    delete read_off;
    delete ref_off;
    delete readLens;
    delete refLens;
}


pair<int, int> findBestOffset(const byte ref[], const int refLen, AlignedRead &read, const int leftmostIndex)
{
    // optimization: try the most likely alignment first (to get a low score to beat)
    int originalAlignment = read.getOriginalAlignmentStart() - leftmostIndex;
    long bestScore = mismatchQualitySumIgnoreCigar(read, ref, refLen, originalAlignment, LONG_MAX);
    int bestIndex = originalAlignment;

    // optimization: we can't get better than 0, so we can quit now
    if(bestScore == 0) {
        return pair<int, int>(bestIndex, 0);
    }

    // optimization: the correct alignment shouldn't be too far from the original one (or else the read wouldn't have aligned in the first place)
    //printf("originalAlignment: %d refLen: %d\n",originalAlignment, refLen);
    for(int i = 0; i < originalAlignment; i++) {
        long score = mismatchQualitySumIgnoreCigar(read, ref, refLen, i, bestScore);
        if(score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
        // optimization: we can't get better than 0, so we can quit now
        if(bestScore == 0) {
            return pair<int, int>(bestIndex, 0);
        }
    }

    const int maxPossibleStart = refLen - read.getReadLength();
    for(int i = originalAlignment + 1; i <= maxPossibleStart; i++) {
        long score = mismatchQualitySumIgnoreCigar(read, ref, refLen, i, bestScore);
        if(score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
        // optimization: we can't get better than 0, so we can quit now
        if(bestScore == 0) {
            return pair<int, int>(bestIndex, 0);
        }
    }

    return pair<int, int>(bestIndex, bestScore);
}


bool updateRead(const Cigar &altCigar, const int altPosOnRef, const int myPosOnAlt, AlignedRead &aRead, const int leftmostIndex)
{
    Cigar *readCigar = new Cigar;

    // special case: there is no indel
    if(altCigar.size() == 1) {
        aRead.setAlignmentStart(leftmostIndex + myPosOnAlt);
        readCigar->push_back(CigarOp('M', aRead.getReadLength()));
        aRead.setCigar(readCigar);
        return true;
    }

    CigarOp altCE1 = altCigar[0];
    CigarOp altCE2 = altCigar[1];

    int leadingMatchingBlockLength = 0; // length of the leading M element or 0 if the leading element is I

    CigarOp indelCE;
    if(altCE1.Type == 'I') {
        indelCE = altCE1;
        if(altCE2.Type != 'M') {
            delete readCigar;
            return false;
        }
    } else {
        if(altCE1.Type != 'M') {
            delete readCigar;
            return false;
        }
        if(altCE2.Type == 'I' || altCE2.Type == 'D') {
            indelCE = altCE2;
        } else {
            delete readCigar;
            return false;
        }
        leadingMatchingBlockLength = altCE1.Length;
    }

    // the easiest thing to do is to take each case separately
    int endOfFirstBlock = altPosOnRef + leadingMatchingBlockLength;
    bool sawAlignmentStart = false;

    // for reads starting before the indel
    if(myPosOnAlt < endOfFirstBlock) {
        aRead.setAlignmentStart(leftmostIndex + myPosOnAlt);
        sawAlignmentStart = true;

        // for reads ending before the indel
        if(myPosOnAlt + aRead.getReadLength() <= endOfFirstBlock) {
            aRead.setCigar(NULL); // reset to original alignment
            delete readCigar;
            return true;
        }
        readCigar->push_back(CigarOp('M', endOfFirstBlock - myPosOnAlt));
    }

    // forward along the indel
    if(indelCE.Type == 'I') {
        // for reads that end in an insertion
        if((unsigned int)(myPosOnAlt + aRead.getReadLength()) < (unsigned int)(endOfFirstBlock + indelCE.Length)) {
            int partialInsertionLength = myPosOnAlt + aRead.getReadLength() - endOfFirstBlock;
            // if we also started inside the insertion, then we need to modify the length
            if(!sawAlignmentStart) {
                partialInsertionLength = aRead.getReadLength();
            }
            readCigar->push_back(CigarOp('I', partialInsertionLength));
            aRead.setCigar(readCigar);
            return true;
        }

        // for reads that start in an insertion
        if(!sawAlignmentStart && (unsigned int) myPosOnAlt < (unsigned int)(endOfFirstBlock + indelCE.Length)) {
            aRead.setAlignmentStart(leftmostIndex + endOfFirstBlock);
            readCigar->push_back(CigarOp('I', indelCE.Length - (myPosOnAlt - endOfFirstBlock)));
            sawAlignmentStart = true;
        } else if(sawAlignmentStart) {
            readCigar->push_back(indelCE);
        }
    } else if(indelCE.Type == 'D') {
        if(sawAlignmentStart) {
            readCigar->push_back(indelCE);
        }
    }

    // for reads that start after the indel
    if(!sawAlignmentStart) {
        aRead.setCigar(NULL); // reset to original alignment
        delete readCigar;
        return true;
    }

    int readRemaining = aRead.getReadLength();
    for(Cigar::iterator itr = readCigar->begin(); itr != readCigar->end(); itr++) {
        CigarOp ce = (*itr);
        if(ce.Type != 'D') {
            readRemaining -= ce.Length;
        }
    }
    if(readRemaining > 0) {
        readCigar->push_back(CigarOp('M', readRemaining));
    }
    aRead.setCigar(readCigar);

    return true;
}

bool tooBigToMove(const BamAlignment &read, const int maxInsertSize)
{
    return (read.IsPaired() && read.IsMateMapped() && read.RefID != read.MateRefID)
           || abs(read.InsertSize) > maxInsertSize;

}

bool doNotTryToClean(const BamAlignment &read)
{
    bool old = (!read.IsMapped() ||
                !read.IsPrimaryAlignment() ||
                read.IsFailedQC() ||
                read.MapQuality == 0 ||
                read.Position == -1 ||
                tooBigToMove(read, MAX_ISIZE_FOR_MOVEMENT)
               );
    return old;
}

int preclean(ReadBin *readsToClean, vector<BamAlignment *>& allReads,
             vector<BamAlignment *>& refReads,
             vector<AlignedRead *>& altReads,
             vector<AlignedRead *>& altAlignmentsToTest,
             ConsensusHashTable *altConsensus,
             long &totalRawMismatchSum)
{
    const byte *reference = readsToClean->getReference();
    const int referenceLen = readsToClean->getReferenceLen();

    if(readsToClean->size() == 0) {
        totalRawMismatchSum = 0;
        return -1;
    }

    int leftmostIndex = readsToClean->getStart();

    totalRawMismatchSum = determineReadsThatNeedCleaning(allReads, refReads, altReads, altAlignmentsToTest, *altConsensus, leftmostIndex, reference, referenceLen);
    if(altConsensus->getSize() == 0 && altAlignmentsToTest.size() == 0) {
        return -1;
    }

    return altAlignmentsToTest.size();
}

void clean(ReadBin *readsToClean, vector<BamAlignment *>& refReads, vector<AlignedRead *>& altReads, vector<AlignedRead *>& altAlignmentsToTest, ConsensusHashTable *altConsensus, long totalRawMismatchSum)
{
    //TIMER_START;
    const byte *reference = readsToClean->getReference();
    const int referenceLen = readsToClean->getReferenceLen();
    int leftmostIndex = readsToClean->getStart();

    Consensus *bestConsensus = NULL;
    vector<Consensus *>::iterator itr;

    vector<Consensus *> finalConsensus;
    altConsensus->toArray(finalConsensus);
    string equalConsensuses = "";

    for(itr = finalConsensus.begin(); itr != finalConsensus.end(); itr++) {
        Consensus *consensus = *itr;

        for(unsigned int j = 0; j < altReads.size(); j++) {
            AlignedRead *toTest = altReads[j];
            pair<int, int> altAlignment = findBestOffset(consensus->str, consensus->strLen, *toTest, leftmostIndex);
            int myScore = altAlignment.second;
            if(myScore > toTest->getAlignerMismatchScore() || myScore >= toTest->getMismatchScoreToReference()) {
                myScore = toTest->getMismatchScoreToReference();
            } else {
                consensus->readIndexes.push_back(pair<int, int>(j, altAlignment.first));
            }
            if(!toTest->getRead()->IsDuplicate()) {
                consensus->mismatchSum += myScore;
            }

            if(bestConsensus != NULL && consensus->mismatchSum > bestConsensus->mismatchSum) {
                break;
            }
        }

        if(bestConsensus == NULL || bestConsensus->mismatchSum > consensus->mismatchSum) {
            if(bestConsensus != NULL) {
                bestConsensus->readIndexes.clear();
            }
            bestConsensus = consensus;
            equalConsensuses = "";
        } else if(bestConsensus->mismatchSum == consensus->mismatchSum) {
            Cigar c = leftAlignIndel(consensus->cigar, reference, referenceLen,
                                     bestConsensus->str, bestConsensus->strLen, bestConsensus->positionOnReference,
                                     bestConsensus->positionOnReference);
            equalConsensuses += CigarToString(c) + ";";
            consensus->readIndexes.clear();
        } else {
            consensus->readIndexes.clear();
        }
    }


    const double improvement = (bestConsensus == NULL ? -1 : ((double)(totalRawMismatchSum - bestConsensus->mismatchSum)) / 10.0);
    loci++;
    if(equalConsensuses != "") {
        equalConsensusCount++;
    }
    if(improvement >= LOD_THRESHOLD) {
        bestConsensus->cigar = leftAlignIndel(bestConsensus->cigar, reference, referenceLen,
                                              bestConsensus->str, bestConsensus->strLen, bestConsensus->positionOnReference,
                                              bestConsensus->positionOnReference);
        for(vector< pair<int, int> >::iterator itr2 = bestConsensus->readIndexes.begin();
                itr2 != bestConsensus->readIndexes.end(); itr2++) {
            pair<int, int> indexPair = *itr2;
            AlignedRead *aRead = altReads[indexPair.first];
            if(!updateRead(bestConsensus->cigar, bestConsensus->positionOnReference, indexPair.second, *aRead, leftmostIndex)) {
                return;
            }
        }
        //Write out?
        if(alternateReducesEntropy(altReads, reference, referenceLen, leftmostIndex)) {
            for(vector< pair<int, int> >::iterator itr2 = bestConsensus->readIndexes.begin();
                    itr2 != bestConsensus->readIndexes.end(); itr2++) {
                pair<int, int> indexPair = *itr2;
                AlignedRead *aRead = altReads[indexPair.first];
                if(aRead->finalizeUpdate()) {
                    BamAlignment *read = aRead->getRead();
                    read->MapQuality = min(read->MapQuality + 10, 254);

                    int neededBasesToLeft = max(leftmostIndex - read->Position, 0);
                    int neededBasesToRight = max(read->Position - leftmostIndex - referenceLen + 1, 0);
                    int neededBases = max(neededBasesToLeft, neededBasesToRight);
                    actual_cleaned += 1;
                }
            }
        }
    }
    for(vector<AlignedRead *>::iterator itr = altReads.begin(); itr != altReads.end(); itr++) {
        delete(*itr);
    }
}

void clean_cpu(ReadBin *readsToClean, vector<BamAlignment *>& refReads, vector<AlignedRead *>& altReads,
               vector<AlignedRead *>& altAlignmentsToTest, ConsensusHashTable *altConsensus,
               long totalRawMismatchSum, int *BestScore, int *ScoreIndex)
{
    const byte *reference = readsToClean->getReference();
    const int referenceLen = readsToClean->getReferenceLen();
    int leftmostIndex = readsToClean->getStart();


    Consensus *bestConsensus = NULL;
    vector<Consensus *>::iterator itr;

    vector<Consensus *> finalConsensus;
    altConsensus->toArray(finalConsensus);
    string equalConsensuses = "";

    for(itr = finalConsensus.begin(); itr != finalConsensus.end(); itr++) {
        Consensus *consensus = *itr;
        for(unsigned int j = 0; j < altReads.size(); j++) {
            int myScore = BestScore[score_offset + j];
            AlignedRead *toTest = altReads[j];
            if(myScore > toTest->getAlignerMismatchScore() || myScore >= toTest->getMismatchScoreToReference()) {
                myScore = toTest->getMismatchScoreToReference();
            } else {
                consensus->readIndexes.push_back(pair<int, int>(j, ScoreIndex[score_offset + j]));
            }
            if(!toTest->getRead()->IsDuplicate()) {
                consensus->mismatchSum += myScore;
            }

            if(bestConsensus != NULL && consensus->mismatchSum > bestConsensus->mismatchSum) {
                break;
            }
        }
        score_offset += altReads.size();

        if(bestConsensus == NULL || bestConsensus->mismatchSum > consensus->mismatchSum) {
            if(bestConsensus != NULL) {
                bestConsensus->readIndexes.clear();
            }
            bestConsensus = consensus;
            equalConsensuses = "";
        } else if(bestConsensus->mismatchSum == consensus->mismatchSum) {
            Cigar c = leftAlignIndel(consensus->cigar, reference, referenceLen,
                                     bestConsensus->str, bestConsensus->strLen, bestConsensus->positionOnReference,
                                     bestConsensus->positionOnReference);
            equalConsensuses += CigarToString(c) + ";";
            consensus->readIndexes.clear();
        } else {
            consensus->readIndexes.clear();
        }
    }

    const double improvement = (bestConsensus == NULL ? -1 : ((double)(totalRawMismatchSum - bestConsensus->mismatchSum)) / 10.0);
    loci++;
    if(equalConsensuses != "") {
        equalConsensusCount++;
    }
    if(improvement >= LOD_THRESHOLD) {
        bestConsensus->cigar = leftAlignIndel(bestConsensus->cigar, reference, referenceLen,
                                              bestConsensus->str, bestConsensus->strLen, bestConsensus->positionOnReference,
                                              bestConsensus->positionOnReference);

        //      cout << CigarToString(bestConsensus->cigar) << endl;
        for(vector< pair<int, int> >::iterator itr2 = bestConsensus->readIndexes.begin();
                itr2 != bestConsensus->readIndexes.end(); itr2++) {
            pair<int, int> indexPair = *itr2;
            AlignedRead *aRead = altReads[indexPair.first];
            if(!updateRead(bestConsensus->cigar, bestConsensus->positionOnReference, indexPair.second, *aRead, leftmostIndex)) {
                return;
            }
        }
        //Write out?
        if(alternateReducesEntropy(altReads, reference, referenceLen, leftmostIndex)) {
            for(vector< pair<int, int> >::iterator itr2 = bestConsensus->readIndexes.begin();
                    itr2 != bestConsensus->readIndexes.end(); itr2++) {
                pair<int, int> indexPair = *itr2;
                AlignedRead *aRead = altReads[indexPair.first];
                if(aRead->finalizeUpdate()) {
                    BamAlignment *read = aRead->getRead();
                    read->MapQuality = min(read->MapQuality + 10, 254);

                    int neededBasesToLeft = max(leftmostIndex - read->Position, 0);
                    int neededBasesToRight = max(read->Position - leftmostIndex - referenceLen + 1, 0);
                    int neededBases = max(neededBasesToLeft, neededBasesToRight);
                    actual_cleaned += 1;
                    //read->EditTag("XT", "Z", equalConsensuses);
                }
            }
        }
    }
    for(vector<AlignedRead *>::iterator itr = altReads.begin(); itr != altReads.end(); itr++) {
        delete(*itr);
    }
}


int getIndexFromName(const string &name, const BamReader &reader)
{
    for(unsigned int i = 0; i < reader.GetReferenceData().size(); i++) {
        if(name == reader.GetReferenceData()[i].RefName) {
            return i;
        }
    }
    return -1;
}

bool getNextRegion(ifstream &region_stream, GenomeLoc &region, const BamReader &reader, vector<AlignedRead *>& mutation_reads)
{
    string region_str;
    if(!getline(region_stream, region_str)) {
        return false;
    }

    int substr = region_str[0] == '>' ? 1 : 0;
    region_str = region_str.substr(substr, region_str.find('\t', 0));     //erases all of region string after tab

    Region temp(region_str);
    region = GenomeLoc(temp.startPos, temp.stopPos, getIndexFromName(temp.startSeq, reader), temp.startSeq);

    //Read in Mutation Reads
    if(!getline(region_stream, region_str, '>')) {
        return true;
    }
    stringstream ss(region_str, stringstream::in);
    while(!ss.eof()) {
        unsigned long position;
        string cigar;
        string bases;
        ss >> position >> cigar >> bases;
        if(ss.good()) {
            cout << "pos: " << position << " cigar: " << cigar << " bases: " << bases << endl;
            BamAlignment *b = new BamAlignment();
            b->AlignedBases = bases;
            b->Position = position;
            b->Qualities = bases; //We just need dummy values
            b->CigarData = StringToCigar(cigar);//Convert the Cigar String to an actual cigar.

            AlignedRead *ar = new AlignedRead(b);
            mutation_reads.push_back(ar);
        }
    }

    return true;
}

void usage()
{
    printf("Usage: IndelRealigner [options] BAM INTERVALS REFERENCE OUTPUT\n");
    printf("Perform indel realingment for INTERVALS in the given BAM and using \n");
    printf("the specified REFERENCE.  The results are output to the OUTPUT file.\n");
    printf("\n");
    //        printf("      --cpu-only              Will disable the use of the GPU for computation.\n");
    printf("      --population            Uses population genetics. Instead of a\n");
    printf("                                single bam file, BAM must be text file\n");
    printf("                                containing a list of bam files in the \n");
    printf("                                population.\n");
    printf("      --mutation-profile      Turns on the mutation profile mode, which\n");
    printf("                                requires the intervals file to be in\n");
    printf("                                the mutation profile format.\n");
    printf("      --use-sw                Use the Smith-Waterman algorithm to compute consensus.\n");
    printf("      --sw-match=VAL          Sets the match scoring in Smith-Waterman.\n");
    printf("      --sw-mismatch=VAL       Sets the mismatch scoring in Smith-Waterman.\n");
    printf("      --sw-gap=VAL            Sets the gap penalty in Smith-Waterman.\n");
    printf("      --sw-gap-extend=VAL     Sets the gap extension penalty Smith-Waterman.\n");
    printf("      --threads=VAL           Runs this number of threads for faster data compression\n");
    printf("                                and decompression\n");
    printf("  -h, --help                  Displays this message and exits.\n");
    printf("\n");
}


void freeResource()
{
    if(use_gpu_flag) {
        free_lookup();
    }
}

int main(int argc, char *argv[])
{
    vector<string> bam_files;
    vector<string> bam_index_files;
    string fasta_file;
    string region_file;
    vector<string> output_files;
    actual_cleaned = 0;
    char buffer[500];
    string path1 = argv[0];
    string path2 = argv[0];
    path1.erase(0, path1.find_last_of('/') + 1);
    path2.erase(path2.find_last_of('/') + 1, path1.length());
    int num_threads = 1;
    int num_clean_threads = 1;
    int mutation_profile_flag = 0;
    int population_genetics_flag = 0;
    int use_sw_flag = 0;
    // Init char lookup table
    for(short i = 0; i < 128; i++) {
        char c = i;
        if(isRegularBase(c)) {
            ch_lookup[i] = 1;
        } else {
            ch_lookup[i] = 0;
        }
    }

    //Compute all the command line arguments.
    while(1) {
        static struct option long_options[] = {
            /* These options don't set a flag.
               We distinguish them by their indices. */
            {"cpu-only", no_argument, &use_gpu_flag, 0},
            {"mutation-profile", no_argument, &mutation_profile_flag, 1},
            {"population", no_argument, &population_genetics_flag, 1},
            {"use-sw", no_argument, &use_sw_flag, 1},
            {"sw-match", required_argument, 0, 0},
            {"sw-mismatch", required_argument, 0, 0},
            {"sw-gap", required_argument, 0, 0},
            {"sw-gap-extend", required_argument, 0, 0},
            {"threads", required_argument, 0, 0},
            {"kernel_path", required_argument, 0, 0},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        int option_index = 0;

        int c = getopt_long(argc, argv, "h",
                            long_options, &option_index);

        if(c == -1) {
            break;
        }

        switch(c) {
        case 0:
            if(long_options[option_index].flag != 0) {
                break;
            }
            if(string(long_options[option_index].name) == "sw-match") {
                SW_MATCH = atof(optarg);
            }
            if(string(long_options[option_index].name) == "sw-mismatch") {
                SW_MISMATCH = atof(optarg);
            }
            if(string(long_options[option_index].name) == "sw-gap") {
                SW_GAP = atof(optarg);
            }
            if(string(long_options[option_index].name) == "sw-gap-extend") {
                SW_GAP_EXTEND = atof(optarg);
            }
            if(string(long_options[option_index].name) == "threads") {
                num_threads = atoi(optarg);
            }
            //                    if(string(long_options[option_index].name) == "kernel_path") {
            //                        path2 = optarg;
            //                    }
            break;

        case 'h':
            usage();
            return 0;

        case '?':
            break;

        default:
            exit(1);
        }
    }

    if(argc - optind < 4) {
        usage(), abort();
    }

    if(use_gpu_flag) {
        cpy2d_lookup(ch_lookup);
    }
    //Get Input Bam Files
    if(population_genetics_flag) {
        ifstream input;
        input.open(argv[optind++]);
        string filename;
        getline(input, filename);
        while(!input.eof()) {
            bam_files.push_back(filename);
            bam_index_files.push_back(filename + ".bai");
            getline(input, filename);
        }
    } else {
        string bam_file = argv[optind++];
        bam_files.push_back(bam_file);
        bam_index_files.push_back(bam_file + ".bai");
    }

    region_file = argv[optind++];
    fasta_file = argv[optind++];

    //Get Output Bam Files
    if(population_genetics_flag) {
        ifstream input;
        input.open(argv[optind++]);
        string filename;
        getline(input, filename);
        while(!input.eof()) {
            output_files.push_back(filename);
            getline(input, filename);
        }
    } else {
        output_files.push_back(argv[optind++]);
    }


    //Open Readers and Writers
    BamReader **readers = new BamReader*[bam_files.size()];
    for(unsigned int i = 0; i < bam_files.size(); i++) {
        if(num_threads > 1) {
            readers[i] = new BamReader(num_threads);
        } else {
            readers[i] = new BamReader();
        }
        readers[i]->Open(bam_files[i]);
        if(!readers[i]->OpenIndex(bam_index_files[i])) {
            cerr << "Cannot open Index" << endl;
            exit(-1);
        }
    }
    SortedBamWriter **writers = new SortedBamWriter*[bam_files.size()];

    for(unsigned int i = 0; i < bam_files.size(); i++) {
        if(num_threads > 1) {
            writers[i] = new SortedBamWriter(num_threads);
        } else {
            writers[i] = new SortedBamWriter();
        }
    }
    for(unsigned int i = 0; i < output_files.size(); i++) {
        writers[i]->open(output_files[i], readers[i]->GetHeaderText(), readers[i]->GetReferenceData());
    }

    //parse BAM/FASTA file arguments:
    string fasta_index_file = fasta_file + ".fai";
    FastaReference *fr = new FastaReference(fasta_file);

    ifstream range_file;
    range_file.open(region_file.c_str());

    int readIdx = 0;

    int max_batched_reads = MAX_BATCHED_READ;
    if(!use_sw_flag) {
        max_batched_reads = 0;
    }

    int max_regions = 0;
    if(!use_sw_flag) {
        max_regions = num_clean_threads * REGIONS_PER_THREAD;
    }

    BamAlignment al;
    GenomeLoc currentInterval;
    vector<AlignedRead *> regionReads;
    bool regionsLeft = getNextRegion(range_file, currentInterval, *readers[0], regionReads);

    vector<ReadBin *> readBins;
    ReadBin *rb = new ReadBin(currentInterval.getContigName(), fr);
    readBins.push_back(rb);

    BamAlignment **nextAlignments = new BamAlignment*[bam_files.size()];

    vector<BamRegionData *> regionData; // (output_files.size());
    regionData.reserve(MAX_BATCHED_READ);
    vector< vector<AlignedRead *> > cleanAltAlignments;
    cleanAltAlignments.reserve(MAX_BATCHED_READ);
    vector<ConsensusHashTable * > cleanAltConsensus;
    cleanAltConsensus.reserve(MAX_BATCHED_READ);

    int totalReads = 0;
    for(unsigned int i = 0; i < bam_files.size(); i++) {
        BamAlignment b;
        if(readers[i]->GetNextAlignment(b)) {
            nextAlignments[i] = new BamAlignment(b);
        } else {
            nextAlignments[i] = NULL;
        }
    }

    while(regionsLeft) {
        TIMER_START;

        cleanAltAlignments.push_back(vector<AlignedRead *>());
        cleanAltConsensus.push_back(new ConsensusHashTable());
        //Read the rest of the region and do precleaning
        for(unsigned int i = 0; i < bam_files.size(); i++) {
            BamRegionData *region = new BamRegionData();
            region->writer = writers[i];
            region->reader = readers[i];
            region->rb = readBins.back();
            regionData.push_back(region);

            while(nextAlignments[i] != NULL) {
                GenomeLoc loc;  //the location of the current read.
                //Get next Read from reader
                BamAlignment b;

                if(nextAlignments[i]->RefID == -1) {
                    break;
                }

                loc = GenomeLoc(*nextAlignments[i], *readers[i]);
                if(!loc.isBefore(currentInterval) && !loc.contains(currentInterval)) {
                    break;
                }

                region->writeList.push_back(nextAlignments[i]);

                if(loc.contains(currentInterval)) {
                    if(!doNotTryToClean(*nextAlignments[i])) {
                        readBins[readIdx]->add(nextAlignments[i]);
                        region->allReads.push_back(nextAlignments[i]);
                    }
                }

                TIMER_START1;
                if(readers[i]->GetNextAlignment(b)) {
                    nextAlignments[i] = new BamAlignment(b);
                } else {
                    nextAlignments[i] = NULL;
                }
                TIMER_END1;
                read_t += MICRO_SECONDS1;
            }

            region->altConsensus = cleanAltConsensus.back();
            int toAdd = preclean(region->rb, region->allReads, region->refReads, region->altReads, region->altAlignmentsToTest, region->altConsensus, region->totalRawMismatchSum);
            if(toAdd != -1) {
                totalReads += toAdd;
            }
            for(unsigned int k = 0; k < region->altAlignmentsToTest.size(); k++) {
                cleanAltAlignments.back().push_back(region->altAlignmentsToTest[k]);
            }
        }

        if(mutation_profile_flag) {
            for(unsigned int i = 0; i < regionReads.size(); i++) {
                cleanAltAlignments.back().push_back(regionReads[i]);
            }
        }

        TIMER_END;
        prep_t += MICRO_SECONDS;

        regionsLeft = getNextRegion(range_file, currentInterval, *readers[0], regionReads);
        if((regionData.size() >= max_regions && totalReads >= max_batched_reads) || !regionsLeft) {

            if(use_sw_flag) {
                if(use_gpu_flag) {
                    generateAlternateConsensesFromReads_CUDA(cleanAltAlignments, cleanAltConsensus, readBins, 0);
                } else {
                    generateAlternateConsensesFromReads(cleanAltAlignments, cleanAltConsensus, readBins, 0);
                }
            }

            //Clean the reads that we have
            TIMER_START;
            int *BestScore, *ScoreIndex;
            int iter_num = regionData.size();
            size_t globalWorkSize, localWorkSize;
            int use_gpu = 0;

            if(use_gpu_flag) {
                use_gpu = clean_cuda_prep(regionData, iter_num, &BestScore, &ScoreIndex, globalWorkSize, localWorkSize);
            }

            if(use_gpu) {
                clean_cuda(iter_num, globalWorkSize, localWorkSize, BestScore, ScoreIndex);
                score_offset = 0;
                for(int t = 0; t < iter_num; t++) {
                    BamRegionData *rd = regionData[t];
                    clean_cpu(rd->rb, rd->refReads, rd->altReads, rd->altAlignmentsToTest, rd->altConsensus, rd->totalRawMismatchSum, BestScore, ScoreIndex);
                }

                free_resource();
                free(ScoreIndex);
                free(BestScore);
            } else {
                for(int t = 0; t < iter_num; t++) {
                    BamRegionData *rd = regionData[t];
                    clean(rd->rb, rd->refReads, rd->altReads, rd->altAlignmentsToTest, rd->altConsensus, rd->totalRawMismatchSum);
                }
            }
            TIMER_END;
            align_t += MICRO_SECONDS;

            TIMER_START;
            for(unsigned int i = 0; i < regionData.size(); i++) {
                regionData[i]->writer->addReads(regionData[i]->writeList);
                regionData[i]->writeList.clear();
            }
            TIMER_END;
            output_t += MICRO_SECONDS;

            //Clear all lists.
            for(unsigned int i = 0; i < regionData.size(); i++) {
                regionData[i]->refReads.clear();
                regionData[i]->allReads.clear();
                regionData[i]->altReads.clear();
                regionData[i]->altAlignmentsToTest.clear();
                regionData[i]->totalRawMismatchSum = 0;
                delete regionData[i];
            }
            regionData.clear();
            for(unsigned int i = 0; i < readBins.size(); i++) {
                delete readBins[i];
            }
            readBins.clear();
            readIdx = -1;
            totalReads = 0;

            cleanAltAlignments.clear();
            for(unsigned int i = 0; i < cleanAltConsensus.size(); i++) {
                delete cleanAltConsensus[i];
            }
            cleanAltConsensus.clear();
        }

        if(regionsLeft) {
            ReadBin *rb = new ReadBin(currentInterval.getContigName(), fr);
            readBins.push_back(rb);
            readIdx++;
        }

        for(unsigned int i = 0; i < regionReads.size(); i++) {
            delete regionReads[i]->getRead();
            delete regionReads[i];
        }
        regionReads.clear();
    }

    TIMER_START;
    //Output the rest of the reads
    for(unsigned int i = 0; i < bam_files.size(); i++) {
        while(nextAlignments[i] != NULL) {
            writers[i]->addRead(nextAlignments[i], true);

            BamAlignment b;
            if(readers[i]->GetNextAlignment(b)) {
                nextAlignments[i] = new BamAlignment(b);
            } else {
                nextAlignments[i] = NULL;
            }
        }
    }
    TIMER_END;
    output_t += MICRO_SECONDS;

    cout << "Preperation Time\t" << prep_t * 1e-6 << endl;
    cout << "Alignment Time\t" << align_t * 1e-6 << endl;
    cout << "Read Time\t" << read_t * 1e-6 << endl;
    cout << "Output Time\t" << output_t * 1e-6 << endl;
    cout << "Sort Time\t" << sort_t * 1e-6 << endl;

    delete[] nextAlignments;

    //Clean up all Writers
    for(unsigned int i = 0; i < output_files.size(); i++) {
        writers[i]->close();
        delete writers[i];
        delete readers[i];
    }
    delete[] writers;
    delete[] readers;
    delete fr;

    cout << "Actually Cleaned\t" << actual_cleaned << endl;
    cout << "Equal Consenuses: " << equalConsensusCount << endl;
    cout << "Loci Chagned: " << loci << endl;

    freeResource();

}

namespace IndelRealigner
{
bool isClipOperator(CigarOp op)
{
    return op.Type == 'S' || op.Type == 'H' || op.Type == 'P';
}
Cigar reclipCigar(Cigar *cigar, BamAlignment read)
{
    Cigar elements;

    int i = 0;
    int n = read.CigarData.size();
    while(i < n && isClipOperator(read.CigarData[i].Type)) {
        elements.push_back(read.CigarData[i++]);
    }

    for(Cigar::iterator itr = cigar->begin(); itr != cigar->end(); itr++) {
        elements.push_back(*itr);
    }

    i++;
    while(i < n && !isClipOperator(read.CigarData[i].Type)) {
        i++;
    }

    while(i < n && isClipOperator(read.CigarData[i].Type)) {
        elements.push_back(read.CigarData[i++]);
    }

    return Cigar(elements);
}

Cigar unclipCigar(const Cigar &cigar)
{
    Cigar newCigar;
    for(Cigar::const_iterator itr = cigar.begin(); itr != cigar.end(); itr++) {
        CigarOp op = *itr;
        if(!isClipOperator(op.Type)) {
            newCigar.push_back(op);
        }
    }
    return newCigar;
}
}
