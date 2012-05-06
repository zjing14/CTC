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
#include <math.h> //for log2()
#include <stdio.h> //for NULL
#include "disorder.h"

#if defined(__FreeBSD__)
#define        log2(x) (log((x)) * (1./M_LN2))
#endif

/** Frequecies for each byte */
static int m_token_freqs[LIBDO_MAX_BYTES]; //frequency of each token in sample
static float m_token_probs[LIBDO_MAX_BYTES]; //P(each token appearing)
static int m_num_tokens = 0; //actual number of `seen' tokens, max 256
static float m_maxent = 0.0;
static float m_ratio = 0.0;
static int LIBDISORDER_INITIALIZED = 0;

static void
initialize_lib()
{
    int i = 0;
    if(1 == LIBDISORDER_INITIALIZED) {
        return;
    }

    m_num_tokens = 0;

    for(i = 0; i < LIBDO_MAX_BYTES; i++) {
        m_token_freqs[i] = 0;
        m_token_probs[i] = 0.0;
    }

    LIBDISORDER_INITIALIZED = 1;
}

/**
 * Set m_num_tokens by iterating over m_token_freq[] and maintaining
 * a counter of each position that does not hold the value of zero.
 */
static void
count_num_tokens()
{
    int i = 0;
    int counter = 0;
    for(i = 0; i < LIBDO_MAX_BYTES; i++) {
        if(0 != m_token_freqs[i]) {
            counter++;
        }
    }
    m_num_tokens = counter;
    return;
}

/**
 * Sum frequencies for each token (i.e., byte values 0 through 255)
 * We assume the `length' parameter is correct.
 *
 * This function is available only to functions in this file.
 */
static void
get_token_frequencies(char *buf,
                      long long length)
{
    int i = 0;
    char *itr = NULL;
    unsigned char c = 0;

    itr = buf;

    //reset number of tokens
    m_num_tokens = 0;

    //make sure freqency and probability arrays are cleared
    for(i = 0; i < LIBDO_MAX_BYTES; i++) {
        m_token_freqs[i] = 0;
        m_token_probs[i] = 0.0;
    }

    for(i = 0; i < length; i++) {
        c = (unsigned char) * itr;
        //assert(0<=c<LIBDO_MAX_BYTES);
        m_token_freqs[c]++;
        itr++;
    }
}

/**
 * Return entropy (in bits) of this buffer of bytes. We assume that the
 * `length' parameter is correct. This implementation is a translation
 * of the PHP code found here:
 *
 *    http://onlamp.com/pub/a/php/2005/01/06/entropy.html
 *
 * with a helpful hint on the `foreach' statement from here:
 *
 *    http://php.net/manual/en/control-structures.foreach.php
 */
float
shannon_H(char *buf,
          long long length)
{
    int i = 0;
    float bits = 0.0;
    char *itr = NULL; //values of itr should be zero to 255
    unsigned char token;
    int num_events = 0; //`length' parameter
    float freq = 0.0; //loop variable for holding freq from m_token_freq[]
    float entropy = 0.0; //running entropy sum

    if(NULL == buf || 0 == length) {
        return 0.0;
    }

    if(0 == LIBDISORDER_INITIALIZED) {
        initialize_lib();
    }

    itr = buf;
    m_maxent = 0.0;
    m_ratio = 0.0;
    num_events = length;
    get_token_frequencies(itr, num_events); //modifies m_token_freqs[]
    //set m_num_tokens by counting unique m_token_freqs entries
    count_num_tokens();

    if(m_num_tokens > LIBDO_MAX_BYTES) {
        //report error somehow?
        return 0.0;
    }

    //iterate through whole m_token_freq array, but only count
    //spots that have a registered token (i.e., freq>0)
    for(i = 0; i < LIBDO_MAX_BYTES; i++) {
        if(0 != m_token_freqs[i]) {
            token = i;
            freq = ((float)m_token_freqs[token]);
            m_token_probs[token] = (freq / ((float)num_events));
            entropy += m_token_probs[token] * log2(m_token_probs[token]);
        }
    }

    bits = -1.0 * entropy;
    m_maxent = log2(m_num_tokens);
    m_ratio = bits / m_maxent;

    return bits;
}

int
get_num_tokens()
{
    return m_num_tokens;
}

float
get_max_entropy()
{
    return m_maxent;
}

float
get_entropy_ratio()
{
    return m_ratio;
}
