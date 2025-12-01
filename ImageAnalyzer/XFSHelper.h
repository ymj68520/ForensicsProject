#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// Forward declaration
struct TSK_IMG_INFO;

// XFS Superblock structure (simplified)
#pragma pack(push, 1)
struct XFSSuperblock {
    uint32_t sb_magicnum;       // 0x58465342 ('XFSB')
    uint32_t sb_blocksize;      // Filesystem block size
    uint64_t sb_dblocks;        // Number of data blocks
    uint64_t sb_rblocks;        // Number of realtime blocks
    uint64_t sb_rextents;       // Number of realtime extents
    uint8_t  sb_uuid[16];       // UUID
    uint64_t sb_logstart;       // Log start block
    uint64_t sb_rootino;        // Root inode number
    uint64_t sb_rbmino;         // Realtime bitmap inode
    uint64_t sb_rsumino;        // Realtime summary inode
    uint32_t sb_rextsize;       // Realtime extent size
    uint32_t sb_agblocks;       // Allocation group size in blocks
    uint32_t sb_agcount;        // Number of allocation groups
    uint32_t sb_rbmblocks;      // Realtime bitmap size
    uint32_t sb_logblocks;      // Log size in blocks
    uint16_t sb_versionnum;     // Version number
    uint16_t sb_sectsize;       // Sector size
    uint16_t sb_inodesize;      // Inode size
    uint16_t sb_inopblock;      // Inodes per block
    char     sb_fname[12];      // Filesystem name
    uint8_t  sb_blocklog;       // log2 of sb_blocksize
    uint8_t  sb_sectlog;        // log2 of sb_sectsize
    uint8_t  sb_inodelog;       // log2 of sb_inodesize
    uint8_t  sb_inopblog;       // log2 of sb_inopblock
    uint8_t  sb_agblklog;       // log2 of sb_agblocks
    uint8_t  sb_rextslog;       // log2 of sb_rextents
    uint8_t  sb_inprogress;     // mkfs in progress
    uint8_t  sb_imax_pct;       // max % of fs for inode space
};

// XFS Dinode core structure (simplified)
struct XFSDinodeCore {
    uint16_t di_magic;          // 0x494e ('IN')
    uint16_t di_mode;           // File mode
    int8_t   di_version;        // Inode version
    int8_t   di_format;         // Format of di_c data
    uint16_t di_onlink;         // Old number of links
    uint32_t di_uid;            // Owner's user id
    uint32_t di_gid;            // Owner's group id
    uint32_t di_nlink;          // Number of links
    uint16_t di_projid;         // Project ID
    uint8_t  di_pad[8];         // Padding
    uint16_t di_flushiter;      // Flush iteration
    int64_t  di_atime_sec;      // Access time seconds
    int32_t  di_atime_nsec;     // Access time nanoseconds
    int64_t  di_mtime_sec;      // Modification time seconds
    int32_t  di_mtime_nsec;     // Modification time nanoseconds
    int64_t  di_ctime_sec;      // Change time seconds
    int32_t  di_ctime_nsec;     // Change time nanoseconds
    uint64_t di_size;           // File size in bytes
    uint64_t di_nblocks;        // Number of blocks
    uint32_t di_extsize;        // Extent size
    uint32_t di_nextents;       // Number of data extents
    uint16_t di_anextents;      // Number of attr extents
    uint8_t  di_forkoff;        // Attr fork offset
    int8_t   di_aformat;        // Attr fork format
    uint32_t di_dmevmask;       // DMIG event mask
    uint16_t di_dmstate;        // DMIG state
    uint16_t di_flags;          // Flags
    uint32_t di_gen;            // Generation number
};

// XFS Directory entry (simplified)
struct XFSDirEntry {
    uint64_t inumber;           // Inode number
    uint8_t  namelen;           // Name length
    uint8_t  name[256];         // Name (variable length)
    uint8_t  ftype;             // File type
};

// XFS Extent structure
struct XFSExtent {
    uint64_t startoff;          // Logical file block offset
    uint64_t startblock;        // Absolute block number
    uint64_t blockcount;        // Block count
};
#pragma pack(pop)

// File record for callback
struct XFSFileInfo {
    uint64_t inode;
    std::string name;
    std::string path;
    uint64_t size;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
    uint16_t mode;
    uint32_t uid;
    uint32_t gid;
    bool is_directory;
    bool is_allocated;
};

// XFS Helper class
class XFSHelper {
public:
    XFSHelper(TSK_IMG_INFO* imgInfo, uint64_t partitionOffset);
    ~XFSHelper();

    // Initialize and read superblock
    bool initialize();

    // Get filesystem info
    uint64_t getBlockSize() const { return blockSize_; }
    uint64_t getInodeSize() const { return inodeSize_; }
    uint64_t getRootInode() const { return rootInode_; }
    std::string getFilesystemName() const { return fsName_; }

    // Walk filesystem and call callback for each file
    using FileCallback = std::function<bool(const XFSFileInfo&)>;
    bool walkFilesystem(FileCallback callback);

private:
    // Read data from image
    bool readData(uint64_t offset, void* buffer, size_t size);

    // Read and parse superblock
    bool readSuperblock();

    // Read inode
    bool readInode(uint64_t inodeNum, XFSDinodeCore& inode, std::vector<uint8_t>& data);

    // Calculate inode location
    uint64_t getInodeOffset(uint64_t inodeNum);

    // Parse directory
    bool parseDirectory(uint64_t inodeNum, const std::string& path, FileCallback callback);

    // Parse directory data (format-specific)
    bool parseDirectoryData(const XFSDinodeCore& inode, const std::vector<uint8_t>& data,
                           const std::string& path, FileCallback callback);

    // Parse short form directory (in-inode)
    bool parseShortFormDirectory(const uint8_t* data, size_t dataSize,
                                const std::string& path, FileCallback callback);

    // Parse block/leaf form directory
    bool parseBlockDirectory(const XFSDinodeCore& inode, const std::vector<uint8_t>& data,
                           const std::string& path, FileCallback callback);

    // Parse extent list
    bool parseExtents(const uint8_t* data, size_t dataSize, std::vector<XFSExtent>& extents);

    // Read file data via extents
    bool readFileData(const std::vector<XFSExtent>& extents, std::vector<uint8_t>& buffer, uint64_t fileSize);

    // Convert XFS timestamps to Unix time
    int64_t xfsTimeToUnix(int64_t sec, int32_t nsec);

    // Byte swapping for big-endian XFS
    uint16_t swapBytes16(uint16_t val);
    uint32_t swapBytes32(uint32_t val);
    uint64_t swapBytes64(uint64_t val);

    TSK_IMG_INFO* imgInfo_;
    uint64_t partitionOffset_;
    uint64_t blockSize_;
    uint64_t inodeSize_;
    uint64_t rootInode_;
    uint32_t agCount_;
    uint32_t agBlocks_;
    std::string fsName_;
    bool initialized_;
};
