#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <tsk/libtsk.h>
#include <map>

struct CarvingSignature {
    std::string name;
    std::string extension;
    std::vector<uint8_t> header;
    std::vector<uint8_t> footer;
    size_t maxSize;
};

class FileCarver {
public:
    FileCarver();
    ~FileCarver();

    /**
     * @brief Scan unallocated space in the image/partition for deleted files
     * @param imagePath Path to the raw image file
     * @param outputDir Directory to save recovered files
     * @param partitionOffset Offset of the partition (in bytes), 0 if full disk or image is partition
     * @return Number of files recovered
     */
    int carve(const std::string& imagePath, const std::string& outputDir, uint64_t partitionOffset = 0);
    const std::vector<CarvingSignature>& getSignatures() const { return signatures_; }

private:
    void initializeSignatures();
    void processUnallocatedBlock(const void* data, size_t len, uint64_t addr);
    void extractFile(TSK_FS_INFO* fs, uint64_t startBlock, const CarvingSignature& sig, const std::string& outputDir);
    
    // Internal callback wrapper
    friend TSK_WALK_RET_ENUM carving_block_walk(const TSK_FS_BLOCK* block, void* ptr);

    std::vector<CarvingSignature> signatures_;
    // Basic state tracking could be added here for multi-block headers, 
    // but for MVP we assume headers start within a block.
};
