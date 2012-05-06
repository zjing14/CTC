bin_PROGRAMS = IndelRealigner
IndelRealigner_SOURCES = Consensus.cpp HelperFunctions.cpp SWPairwiseAlignment.cpp AlignedRead.cpp  Fasta.cpp disorder.cpp split.cpp ReadBin.cpp GenomeLoc.cpp ConsensusHashTable.cpp cuda/swat_cuda.cu cuda/FastOffset.cu BamRegionData.cpp IndelRealigner.cpp SortedBamWriter.cpp
.cu.o:
	$(NVCC) $(NVCCFLAGS) $(CUDA_CFLAGS) $(CUDA_LIBS)-c -o $@ $<
