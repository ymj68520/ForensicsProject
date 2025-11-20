#include "ImageAnalyzer.h"
#include "DatabaseManager.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>

ImageAnalyzer::ImageAnalyzer(const std::string& imagePath)
    : imagePath_(imagePath), imgInfo_(nullptr), fsInfo_(nullptr) {
}

ImageAnalyzer::~ImageAnalyzer() {
    closeImage();
}

bool ImageAnalyzer::analyze() {
    if (!openImage()) {
        return false;
    }

    if (!openFileSystem()) {
        return false;
    }

    return true;
}

bool ImageAnalyzer::openImage() {
    // Detect image type
    TSK_IMG_TYPE_ENUM imgType = TSK_IMG_TYPE_DETECT;

    // Open the image
    imgInfo_ = tsk_img_open(1, &imagePath_[0], imgType, 0);
    if (!imgInfo_) {
        std::cerr << "Error opening image: " << tsk_error_get() << std::endl;
        return false;
    }

    std::cout << "Image opened successfully" << std::endl;
    std::cout << "  Type: " << detectImageType() << std::endl;
    std::cout << "  Size: " << imgInfo_->size << " bytes" << std::endl;

    return true;
}

bool ImageAnalyzer::openFileSystem() {
    // Try to open volume system first
    TSK_VS_INFO* vsInfo = tsk_vs_open(imgInfo_, 0, TSK_VS_TYPE_DETECT);

    TSK_OFF_T offset = 0;

    if (vsInfo) {
        std::cout << "Volume system detected" << std::endl;
        std::cout << "  Partitions: " << vsInfo->part_count << std::endl;

        // Try to find the first valid partition
        for (TSK_PNUM_T i = 0; i < vsInfo->part_count; i++) {
            const TSK_VS_PART_INFO* part = tsk_vs_part_get(vsInfo, i);
            if (part && (part->flags & TSK_VS_PART_FLAG_ALLOC)) {
                offset = part->start * vsInfo->block_size;
                std::cout << "  Using partition " << i << " at offset " << offset << std::endl;
                break;
            }
        }
    }

    // Open filesystem
    fsInfo_ = tsk_fs_open_img(imgInfo_, offset, TSK_FS_TYPE_DETECT);
    if (!fsInfo_) {
        std::cerr << "Error opening filesystem: " << tsk_error_get() << std::endl;
        if (vsInfo) tsk_vs_close(vsInfo);
        return false;
    }

    std::cout << "Filesystem opened successfully" << std::endl;
    std::cout << "  Type: " << tsk_fs_type_toname(fsInfo_->ftype) << std::endl;
    std::cout << "  Block size: " << fsInfo_->block_size << std::endl;
    std::cout << "  Block count: " << fsInfo_->block_count << std::endl;

    if (vsInfo) tsk_vs_close(vsInfo);

    return true;
}

void ImageAnalyzer::closeImage() {
    if (fsInfo_) {
        tsk_fs_close(fsInfo_);
        fsInfo_ = nullptr;
    }

    if (imgInfo_) {
        tsk_img_close(imgInfo_);
        imgInfo_ = nullptr;
    }
}

bool ImageAnalyzer::extractToDatabase(const std::string& dbPath) {
    dbManager_ = std::make_unique<DatabaseManager>(dbPath);

    if (!dbManager_->initialize()) {
        std::cerr << "Error initializing database" << std::endl;
        return false;
    }

    std::cout << "Walking filesystem..." << std::endl;

    // Walk the filesystem
    if (tsk_fs_dir_walk(fsInfo_, fsInfo_->root_inum,
        static_cast<TSK_FS_DIR_WALK_FLAG_ENUM>(
            TSK_FS_DIR_WALK_FLAG_RECURSE | TSK_FS_DIR_WALK_FLAG_ALLOC |
            TSK_FS_DIR_WALK_FLAG_UNALLOC),
        fileWalkCallback, this) != 0) {
        std::cerr << "Error walking filesystem: " << tsk_error_get() << std::endl;
        return false;
    }

    std::cout << "Filesystem walk completed" << std::endl;

    return true;
}

TSK_WALK_RET_ENUM ImageAnalyzer::fileWalkCallback(TSK_FS_FILE* fsFile,
    const char* path,
    void* ptr) {
    ImageAnalyzer* analyzer = static_cast<ImageAnalyzer*>(ptr);

    if (!fsFile || !fsFile->name) {
        return TSK_WALK_CONT;
    }

    std::string fullPath = std::string(path) + fsFile->name->name;
    analyzer->processFile(fsFile, fullPath);

    return TSK_WALK_CONT;
}

bool ImageAnalyzer::processFile(TSK_FS_FILE* fsFile, const std::string& path) {
    if (!fsFile || !fsFile->name || !fsFile->meta) {
        return false;
    }

    FileRecord record;
    record.inode = fsFile->name->meta_addr;
    record.name = fsFile->name->name;
    record.path = path;
    record.size = fsFile->meta->size;
    record.atime = fsFile->meta->atime;
    record.mtime = fsFile->meta->mtime;
    record.ctime = fsFile->meta->ctime;
    record.crtime = fsFile->meta->crtime;

    // Determine file type
    switch (fsFile->meta->type) {
    case TSK_FS_META_TYPE_REG:
        record.type = "REG";
        break;
    case TSK_FS_META_TYPE_DIR:
        record.type = "DIR";
        break;
    case TSK_FS_META_TYPE_LNK:
        record.type = "LNK";
        break;
    default:
        record.type = "OTHER";
        break;
    }

    record.isDeleted = (fsFile->name->flags & TSK_FS_NAME_FLAG_UNALLOC) ? 1 : 0;
    record.isAllocated = (fsFile->meta->flags & TSK_FS_META_FLAG_ALLOC) ? 1 : 0;
    record.uid = fsFile->meta->uid;
    record.gid = fsFile->meta->gid;

    // Format permissions
    std::ostringstream perms;
    perms << std::oct << fsFile->meta->mode;
    record.permissions = perms.str();

    return dbManager_->insertFileRecord(record);
}

std::string ImageAnalyzer::detectImageType() {
    if (!imgInfo_) return "Unknown";

    switch (imgInfo_->itype) {
    case TSK_IMG_TYPE_RAW:
        return "RAW/DD";
    case TSK_IMG_TYPE_AFF_AFF:
        return "AFF";
    case TSK_IMG_TYPE_EWF_EWF:
        return "E01/EWF";
    default:
        return "Other";
    }
}

std::string ImageAnalyzer::detectOSType() {
    if (!fsInfo_) return "Unknown";

    switch (fsInfo_->ftype) {
    case TSK_FS_TYPE_NTFS:
        return "Windows (NTFS)";
    case TSK_FS_TYPE_FAT32:
    case TSK_FS_TYPE_FAT16:
    case TSK_FS_TYPE_FAT12:
        return "Windows/DOS (FAT)";
    case TSK_FS_TYPE_EXT2:
    case TSK_FS_TYPE_EXT3:
    case TSK_FS_TYPE_EXT4:
        return "Linux (EXT)";
    case TSK_FS_TYPE_HFS:
    case TSK_FS_TYPE_HFS_DETECT:
        return "macOS (HFS)";
    default:
        return "Unknown";
    }
}
