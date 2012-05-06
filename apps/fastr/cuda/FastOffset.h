#include "../BamRegionData.h"

void clean_cuda(int iter_num, const size_t globalWorkSize,  const size_t localWorkSize, int *BestScore, int *ScoreIndex);
int clean_cuda_prep(vector<BamRegionData *> &reads, int iter_num, int **ScoreIndex, int **BestScore, size_t &globalWorkSize, size_t &localWorkSize);
void cpy2d_lookup(char *ch_lookup);
void free_lookup();
void free_resource();
