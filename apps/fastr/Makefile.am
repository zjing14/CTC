bin_PROGRAMS = IndelRealigner
IndelRealigner_SOURCES = SortedBamWriter.cpp Consensus.cpp HelperFunctions.cpp SWPairwiseAlignment.cpp AlignedRead.cpp IndelRealigner.cu Fasta.cpp disorder.cpp split.cpp ReadBin.cpp GenomeLoc.cpp ConsensusHashTable.cpp BamRegionData.cpp oclHelper.cpp IndelRealigner.cpp
install-exec-local:
	cp $(top_srcdir)/IndelRealigner_kernel.cl ${bindir}
