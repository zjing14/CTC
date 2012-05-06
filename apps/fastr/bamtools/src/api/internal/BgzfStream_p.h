// ***************************************************************************
// BgzfStream_p.h (c) 2011 Derek Barnett
// Marth Lab, Department of Biology, Boston College
// All rights reserved.
// ---------------------------------------------------------------------------
// Last modified: 5 April 2011(DB)
// ---------------------------------------------------------------------------
// Based on BGZF routines developed at the Broad Institute.
// Provides the basic functionality for reading & writing BGZF files
// Replaces the old BGZF.* files to avoid clashing with other toolkits
// ***************************************************************************

#ifndef BGZFSTREAM_P_H
#define BGZFSTREAM_P_H

//  -------------
//  W A R N I N G
//  -------------
//
// This file is not part of the BamTools API.  It exists purely as an
// implementation detail. This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.

#include <api/BamAux.h>
#include <api/BamConstants.h>
#include "zlib.h"
#include <cstdio>
#include <string>
#include <vector>
#include <queue>

namespace BamTools {
namespace Internal {

class BgzfStream {

    // constructor & destructor
    public:
        BgzfStream(void);
        virtual ~BgzfStream(void);

    // main interface methods
    public:
        // closes BGZF file
        virtual void Close(void);
        // opens the BGZF file (mode is either "rb" for reading, or "wb" for writing)
        bool Open(const std::string& filename, const char* mode);
        // reads BGZF data into a byte buffer
        int Read(char* data, const unsigned int dataLength);
        // seek to position in BGZF file
        bool Seek(const int64_t& position);
        // enable/disable compressed output
        void SetWriteCompressed(bool ok);
        // get file position in BGZF file
        int64_t Tell(void) const;
        // writes the supplied data into the BGZF buffer
        virtual unsigned int Write(const char* data, const unsigned int dataLen);

    // internal methods
    protected:
        // compresses the current block
        int DeflateBlock(void);

    private:
        // flushes the data in the BGZF block
        virtual void FlushBlock(void);
        // de-compresses the current block
        virtual int InflateBlock(const int& blockLength);
        // reads a BGZF block
        virtual bool ReadBlock(void);

    // static 'utility' methods
    public:
        // checks BGZF block header
        static inline bool CheckBlockHeader(char* header);

    // data members
    public:
        unsigned int UncompressedBlockSize;
        unsigned int CompressedBlockSize;
        unsigned int BlockLength;
        unsigned int BlockOffset;
        uint64_t BlockAddress;
        bool IsOpen;
        bool IsWriteOnly;
        bool IsWriteCompressed;
        FILE* Stream;
        char* UncompressedBlock;
        char* CompressedBlock;
};

// -------------------------------------------------------------
// static 'utility' method implementations

// checks BGZF block header
inline
bool BgzfStream::CheckBlockHeader(char* header) {
    return (header[0] == Constants::GZIP_ID1 &&
            header[1] == (char)Constants::GZIP_ID2 &&
            header[2] == Z_DEFLATED &&
            (header[3] & Constants::FLG_FEXTRA) != 0 &&
            BamTools::UnpackUnsignedShort(&header[10]) == Constants::BGZF_XLEN &&
            header[12] == Constants::BGZF_ID1 &&
            header[13] == Constants::BGZF_ID2 &&
            BamTools::UnpackUnsignedShort(&header[14]) == Constants::BGZF_LEN );
}

#define BLOCKS_PER_THREAD       5

class DataBlock {
    public:
        DataBlock(unsigned int uncompressed_block_size, unsigned int compressed_block_size, bool is_write_compressed);
        ~DataBlock();

        void CopyData(char* data, unsigned int size);
        void Deflate(void);
        int DeflateBlock(void);
        int GetBlockLength();
        std::vector <char*> WriteBuffers;
        std::vector <int> BufferLengths;

    private:
        unsigned int UncompressedBlockSize;
        unsigned int CompressedBlockSize;
        bool IsWriteCompressed;
        unsigned int BlockOffset;
        char* UncompressedBlock;
        char* CompressedBlock;
};

class ReadDataBlock {
	public:
		ReadDataBlock(unsigned int uncompressed_block_size, unsigned int compressed_block_size, bool is_write_compressed);
		ReadDataBlock(const ReadDataBlock&);
		~ReadDataBlock();
		
		uint64_t GetBlockAddress();
		unsigned int GetBlockLength();	
        
		unsigned int UncompressedBlockSize;
        unsigned int CompressedBlockSize;
		char* UncompressedBlock;
        char* CompressedBlock;
		uint64_t BlockAddress;
		unsigned int BlockLength;		
		unsigned int InflateBlockLength;		
		int InflateBlock();
	
	private:
};

class BgzfStreamMT :public BgzfStream {
    public:
        BgzfStreamMT(int num_threads);
        ~BgzfStreamMT(void);

		//Writing Functions
        void FlushBlock(void);
        unsigned int Write(const char* data, const unsigned int dataLen);
        void Close(void);

		//ReadingFunctions
		bool ReadBlock();
		bool PopulateReadBuffer();

    private:
        unsigned int NumThreads;
        unsigned int BlocksPerThread;
        unsigned int NumBufferedBlocks;
        std::vector <DataBlock*> BufferedBlocks;
        DataBlock*** BlockGrid;

		//Reading Variables
		std::queue <ReadDataBlock*> ReadBufferedBlocks; 
};

} // namespace Internal
} // namespace BamTools

#endif // BGZFSTREAM_P_H
