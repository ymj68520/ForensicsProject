#include "FileCarver.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

// Static wrapper for C-style callback - REMOVED UNUSED
// TSK_WALK_RET_ENUM carving_block_walk(const TSK_FS_BLOCK* block, void* ptr) ...

FileCarver::FileCarver() {
    initializeSignatures();
}

FileCarver::~FileCarver() {}

void FileCarver::initializeSignatures() {
    // JPG: FF D8 FF
    signatures_.push_back({
        "JPEG Image", "jpg",
        {0xFF, 0xD8, 0xFF},
        {0xFF, 0xD9}, // JPG Footer
        10 * 1024 * 1024 // 10 MB Max
    });

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    signatures_.push_back({
        "PNG Image", "png",
        {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A},
        {0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82}, // IEND chunk + CRC
        10 * 1024 * 1024
    });

    // PDF: %PDF
    signatures_.push_back({
        "PDF Document", "pdf",
        {0x25, 0x50, 0x44, 0x46},
        {0x25, 0x25, 0x45, 0x4F, 0x46}, // %%EOF
        20 * 1024 * 1024
    });
    
    // GIF: GIF87a or GIF89a
    signatures_.push_back({
        "GIF Image", "gif",
        {0x47, 0x49, 0x46, 0x38, 0x39, 0x61},
        {0x00, 0x3B},
        5 * 1024 * 1024
    });
}

// Global context to share state between the callback and the main loop if needed.
// For this MVP, we are simplistic: If we find a header in an unallocated block,
// we try to extract from that point independently.
// NOTE: TSK callbacks are synchronous, so we can access member variables safely.
// HOWEVER, tsk_fs_block_walk gives us one block at a time. To carve a multi-block file, 
// we normally need to read AHEAD raw data from disk, not just the current block.
// So, inside processUnallocatedBlock, if we find a header, we will use tsk_fs_read_block 
// or basic file IO to read subsequent bytes according to our linear carving policy.

// Context struct to pass to callback
struct CarvingContext {
    FileCarver* carver;
    TSK_FS_INFO* fs;
    std::string outputDir;
    int recoveredCount;
};

// Redefined wrapper to include FS info
TSK_WALK_RET_ENUM carving_block_walk_ctx(const TSK_FS_BLOCK* block, void* ptr) {
    CarvingContext* ctx = static_cast<CarvingContext*>(ptr);
    size_t blockSize = ctx->fs->block_size;
    
    if (block->flags & TSK_FS_BLOCK_FLAG_UNALLOC) {
        // Debug output (temporary)
        // std::cout << "Scanning unalloc block: " << block->addr << std::endl;

        // Access signatures via public method or friend
        // Since signatures_ is private, we should use a public getter or make this function a friend
        const auto& signatures = ctx->carver->getSignatures();
        
        for (const auto& sig : signatures) {
            // Search for signature in this block
            auto it = std::search(
                (uint8_t*)block->buf, (uint8_t*)block->buf + blockSize,
                sig.header.begin(), sig.header.end()
            );

            if (it != (uint8_t*)block->buf + blockSize) {
                // Found a header!
                size_t offsetInBlock = it - (uint8_t*)block->buf;
                uint64_t startByteAddr = block->addr * blockSize + offsetInBlock;
                
                // Construct output filename
                std::string filename = "carved_" + std::to_string(startByteAddr) + "." + sig.extension;
                std::string outPath = ctx->outputDir + "/" + filename;
                
                // Attempt extraction
                // We need to read continous bytes from disk starting at startByteAddr
                // LIMITATION: We assume logic continuity = physical continuity (linear carving)
                
                std::ofstream outFile(outPath, std::ios::binary);
                if (!outFile) continue;

                // Write header
                // outFile.write((char*)sig.header.data(), sig.header.size());

                // We read chunk by chunk from the Image (not FS blocks to avoid allocated check issues?)
                // Actually, for unallocated carving, we usually read Raw blocks.
                // But tsk_img_read is easier.
                
                size_t currentSize = 0;
                char buffer[4096];
                uint64_t currentOffset = startByteAddr;
                bool footerFound = false;

                while (currentSize < sig.maxSize) {
                    ssize_t cnt = tsk_img_read(ctx->fs->img_info, currentOffset, buffer, sizeof(buffer));
                    if (cnt <= 0) break;

                    // Check for footer if it exists
                    if (!sig.footer.empty()) {
                         auto fit = std::search(
                            (uint8_t*)buffer, (uint8_t*)buffer + cnt,
                            sig.footer.begin(), sig.footer.end()
                        );
                        if (fit != (uint8_t*)buffer + cnt) {
                            // Footer found
                            size_t writeLen = (fit - (uint8_t*)buffer) + sig.footer.size();
                            outFile.write(buffer, writeLen);
                            footerFound = true;
                            break;
                        }
                    }

                    outFile.write(buffer, cnt);
                    currentSize += cnt;
                    currentOffset += cnt;
                }
                
                outFile.close();
                ctx->recoveredCount++;
                // Skip ahead logic would go here to avoid re-detecting within the same file
            }
        }
    }
    return TSK_WALK_CONT;
}

int FileCarver::carve(const std::string& imagePath, const std::string& outputDir, uint64_t partitionOffset) {
    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    TSK_IMG_INFO* img = tsk_img_open_utf8_sing(imagePath.c_str(), TSK_IMG_TYPE_DETECT, 0);
    if (!img) {
        std::cerr << "FileCarver: Failed to open image: " << imagePath << std::endl;
        return 0;
    }

    TSK_FS_INFO* fs_info = tsk_fs_open_img(img, partitionOffset, TSK_FS_TYPE_DETECT);
    if (!fs_info) {
        std::cerr << "FileCarver: Failed to open filesystem at offset " << partitionOffset << std::endl;
        tsk_img_close(img);
        return 0;
    }

    CarvingContext ctx;
    ctx.carver = this;
    ctx.fs = fs_info;
    ctx.outputDir = outputDir;
    ctx.recoveredCount = 0;

    std::cout << "FileCarver: Scanning unallocated blocks..." << std::endl;

    // Walk unallocated blocks
    // TSK_FS_BLOCK_WALK_FLAG_UNALLOC (Retrieve content of unallocated blocks)
    if (tsk_fs_block_walk(fs_info, fs_info->first_block, fs_info->last_block,
        (TSK_FS_BLOCK_WALK_FLAG_ENUM)(TSK_FS_BLOCK_WALK_FLAG_UNALLOC),
        carving_block_walk_ctx, &ctx)) {
        
        // Errors are strictly printed by TSK, usually we ignore partial failures
    }

    std::cout << "FileCarver: Recovered " << ctx.recoveredCount << " files." << std::endl;

    tsk_fs_close(fs_info);
    tsk_img_close(img);

    return ctx.recoveredCount;
}

// Placeholder for member function used in friend declaration, 
// though we switched to struct context in implementation.
void FileCarver::processUnallocatedBlock(const void* data, size_t len, uint64_t addr) {
    // Intentionally empty, logic moved to carving_block_walk_ctx for easier access to TSK_IMG handle
}

// Friend definition
void FileCarver::extractFile(TSK_FS_INFO* fs, uint64_t startBlock, const CarvingSignature& sig, const std::string& outputDir) {
    // Logic integrated into context walker
}
