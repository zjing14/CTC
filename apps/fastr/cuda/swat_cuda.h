#define byte char
#include "api/BamReader.h"          // (or "BamMultiReader.h") as needed
#include "api/BamWriter.h"          // as needed
#include <vector>
#include <algorithm>

using namespace std;
using namespace BamTools;

int swat(const byte a[], unsigned int an, const int a_offset[], const int ans[], const int n_a, 
        const byte b[], unsigned int bn,  const int b_offset[], const int bns[], const int n_b, 
        float openPenalty, float extensionPenalty, float match, float mismatch, vector< vector<CigarOp> >& Cigars, int *alignment_offset, int max_len);
