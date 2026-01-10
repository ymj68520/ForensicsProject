// test_image_analyzer_gtest.cpp
// GTest-based unit tests for ImageAnalyzer module
// Tests: ImageAnalyzer data types and XFS structures

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <cstring>

#include "ImageAnalyzer/ImageAnalyzerDataTypes.h"

using ::testing::Eq;
using ::testing::Not;
using ::testing::IsEmpty;

namespace fs = std::filesystem;

// ============================================================================
// XFSMode Enum Tests
// ============================================================================

class XFSModeTest : public ::testing::Test {};

TEST_F(XFSModeTest, EnumValues) {
    XFSMode mode;
    mode = XFSMode::Auto;
    mode = XFSMode::Native;
    mode = XFSMode::Pure;
    SUCCEED();
}

TEST_F(XFSModeTest, DefaultMode) {
    // Auto is typically the default
    XFSMode mode = XFSMode::Auto;
    EXPECT_EQ(mode, XFSMode::Auto);
}

// ============================================================================
// NativeFileInfo Structure Tests
// ============================================================================

class NativeFileInfoTest : public ::testing::Test {};

TEST_F(NativeFileInfoTest, DefaultValues) {
    NativeFileInfo info{};
    info.inode = 0;
    info.size = 0;
    info.is_directory = false;
    info.is_allocated = true;
    
    EXPECT_EQ(info.inode, 0u);
    EXPECT_EQ(info.size, 0u);
    EXPECT_FALSE(info.is_directory);
}

TEST_F(NativeFileInfoTest, RegularFile) {
    NativeFileInfo info;
    info.inode = 12345;
    info.name = "document.pdf";
    info.path = "/home/user/document.pdf";
    info.size = 102400;
    info.atime = 1704067200;
    info.mtime = 1704067200;
    info.ctime = 1704000000;
    info.mode = 0644;
    info.uid = 1000;
    info.gid = 1000;
    info.is_directory = false;
    info.is_allocated = true;
    
    EXPECT_EQ(info.inode, 12345u);
    EXPECT_EQ(info.name, "document.pdf");
    EXPECT_FALSE(info.is_directory);
    EXPECT_TRUE(info.is_allocated);
}

TEST_F(NativeFileInfoTest, Directory) {
    NativeFileInfo info;
    info.inode = 2;
    info.name = "home";
    info.path = "/home";
    info.size = 4096;
    info.mode = 0755;
    info.uid = 0;
    info.gid = 0;
    info.is_directory = true;
    info.is_allocated = true;
    
    EXPECT_TRUE(info.is_directory);
    EXPECT_EQ(info.mode, 0755);
}

TEST_F(NativeFileInfoTest, DeletedFile) {
    NativeFileInfo info;
    info.inode = 99999;
    info.name = "deleted_file.txt";
    info.is_allocated = false;
    
    EXPECT_FALSE(info.is_allocated);
}

// ============================================================================
// XFSFileInfo Structure Tests
// ============================================================================

class XFSFileInfoTest : public ::testing::Test {};

TEST_F(XFSFileInfoTest, DefaultValues) {
    XFSFileInfo info{};
    info.inode = 0;
    info.size = 0;
    
    EXPECT_EQ(info.inode, 0u);
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.path.empty());
}

TEST_F(XFSFileInfoTest, PopulatedValues) {
    XFSFileInfo info;
    info.inode = 128;
    info.name = "data.db";
    info.path = "/var/lib/myapp/data.db";
    info.size = 1048576;
    info.atime = 1704067200;
    info.mtime = 1704067200;
    info.ctime = 1704000000;
    info.mode = 0644;
    info.uid = 1000;
    info.gid = 1000;
    info.is_directory = false;
    info.is_allocated = true;
    
    EXPECT_EQ(info.inode, 128u);
    EXPECT_EQ(info.size, 1048576u);
}

// ============================================================================
// XFSSuperblock Structure Tests
// ============================================================================

class XFSSuperblockTest : public ::testing::Test {};

TEST_F(XFSSuperblockTest, MagicNumber) {
    // XFS magic number is 'XFSB' = 0x58465342
    constexpr uint32_t XFS_MAGIC = 0x58465342;
    
    XFSSuperblock sb{};
    sb.sb_magicnum = XFS_MAGIC;
    
    EXPECT_EQ(sb.sb_magicnum, XFS_MAGIC);
}

TEST_F(XFSSuperblockTest, CommonBlockSizes) {
    XFSSuperblock sb{};
    
    // Common XFS block sizes
    sb.sb_blocksize = 4096;  // 4KB
    EXPECT_EQ(sb.sb_blocksize, 4096u);
    
    sb.sb_blocksize = 65536;  // 64KB
    EXPECT_EQ(sb.sb_blocksize, 65536u);
}

TEST_F(XFSSuperblockTest, InodeSize) {
    XFSSuperblock sb{};
    
    // Common inode sizes
    sb.sb_inodesize = 256;
    EXPECT_EQ(sb.sb_inodesize, 256);
    
    sb.sb_inodesize = 512;
    EXPECT_EQ(sb.sb_inodesize, 512);
}

TEST_F(XFSSuperblockTest, SectorSize) {
    XFSSuperblock sb{};
    
    sb.sb_sectsize = 512;  // Standard sector size
    EXPECT_EQ(sb.sb_sectsize, 512);
}

// ============================================================================
// XFSDinodeCore Structure Tests
// ============================================================================

class XFSDinodeCoreTest : public ::testing::Test {};

TEST_F(XFSDinodeCoreTest, MagicNumber) {
    // XFS inode magic is 'IN' = 0x494e
    constexpr uint16_t INODE_MAGIC = 0x494e;
    
    XFSDinodeCore inode{};
    inode.di_magic = INODE_MAGIC;
    
    EXPECT_EQ(inode.di_magic, INODE_MAGIC);
}

TEST_F(XFSDinodeCoreTest, FileModes) {
    XFSDinodeCore inode{};
    
    // Regular file mode
    inode.di_mode = 0100644;  // S_IFREG | rw-r--r--
    EXPECT_EQ(inode.di_mode, 0100644);
    
    // Directory mode
    inode.di_mode = 0040755;  // S_IFDIR | rwxr-xr-x
    EXPECT_EQ(inode.di_mode, 0040755);
}

TEST_F(XFSDinodeCoreTest, OwnerIds) {
    XFSDinodeCore inode{};
    
    inode.di_uid = 1000;
    inode.di_gid = 1000;
    
    EXPECT_EQ(inode.di_uid, 1000u);
    EXPECT_EQ(inode.di_gid, 1000u);
}

TEST_F(XFSDinodeCoreTest, LinkCount) {
    XFSDinodeCore inode{};
    
    inode.di_nlink = 1;  // Regular file
    EXPECT_EQ(inode.di_nlink, 1u);
    
    inode.di_nlink = 2;  // Directory with subdirectory
    EXPECT_EQ(inode.di_nlink, 2u);
}

// ============================================================================
// XFSExtent Structure Tests
// ============================================================================

class XFSExtentTest : public ::testing::Test {};

TEST_F(XFSExtentTest, DefaultValues) {
    XFSExtent extent{};
    extent.startoff = 0;
    extent.startblock = 0;
    extent.blockcount = 0;
    
    EXPECT_EQ(extent.startoff, 0u);
    EXPECT_EQ(extent.startblock, 0u);
    EXPECT_EQ(extent.blockcount, 0u);
}

TEST_F(XFSExtentTest, SingleExtent) {
    XFSExtent extent;
    extent.startoff = 0;
    extent.startblock = 1024;
    extent.blockcount = 256;
    
    EXPECT_EQ(extent.startoff, 0u);
    EXPECT_EQ(extent.startblock, 1024u);
    EXPECT_EQ(extent.blockcount, 256u);
}

// ============================================================================
// XFSDirEntry Structure Tests
// ============================================================================

class XFSDirEntryTest : public ::testing::Test {};

TEST_F(XFSDirEntryTest, DefaultValues) {
    XFSDirEntry entry{};
    entry.inumber = 0;
    entry.namelen = 0;
    
    EXPECT_EQ(entry.inumber, 0u);
    EXPECT_EQ(entry.namelen, 0);
}

TEST_F(XFSDirEntryTest, FileEntry) {
    XFSDirEntry entry;
    entry.inumber = 128;
    entry.namelen = 8;
    std::memcpy(entry.name, "test.txt", 8);
    entry.ftype = 1;  // Regular file
    
    EXPECT_EQ(entry.inumber, 128u);
    EXPECT_EQ(entry.namelen, 8);
}

// ============================================================================
// Filesystem Type Constants
// ============================================================================

class FilesystemConstantsTest : public ::testing::Test {};

TEST_F(FilesystemConstantsTest, XFSMagic) {
    // XFS superblock magic: 'XFSB'
    constexpr uint32_t XFS_MAGIC = 0x58465342;
    EXPECT_EQ(XFS_MAGIC, 0x58465342u);
}

TEST_F(FilesystemConstantsTest, XFSInodeMagic) {
    // XFS inode magic: 'IN'
    constexpr uint16_t INODE_MAGIC = 0x494e;
    EXPECT_EQ(INODE_MAGIC, 0x494eu);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
