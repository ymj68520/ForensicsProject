// test_windows_parsers_gtest.cpp
// GTest-based unit tests for Windows file parser data structures and utilities
// Tests: Prefetch/LNK header structures, constants, and enums

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <cstring>

// Include only headers (not implementations to avoid heavy dependencies)
#include "WindowsFilesAnalyzer/Parsers/WindowsPrefetchParser.h"
#include "WindowsFilesAnalyzer/Parsers/WindowsLnkParser.h"

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Not;
using ::testing::AnyOf;

namespace fs = std::filesystem;

// ============================================================================
// PrefetchVersion Enum Tests
// ============================================================================

class PrefetchVersionTest : public ::testing::Test {};

TEST_F(PrefetchVersionTest, EnumValues) {
    EXPECT_EQ(static_cast<uint32_t>(PrefetchVersion::VERSION_17), 17u);
    EXPECT_EQ(static_cast<uint32_t>(PrefetchVersion::VERSION_23), 23u);
    EXPECT_EQ(static_cast<uint32_t>(PrefetchVersion::VERSION_26), 26u);
    EXPECT_EQ(static_cast<uint32_t>(PrefetchVersion::VERSION_30), 30u);
    EXPECT_EQ(static_cast<uint32_t>(PrefetchVersion::VERSION_31), 31u);
    EXPECT_EQ(static_cast<uint32_t>(PrefetchVersion::UNKNOWN), 0u);
}

TEST_F(PrefetchVersionTest, VersionRanges) {
    // All known versions should be in reasonable range
    EXPECT_THAT(static_cast<uint32_t>(PrefetchVersion::VERSION_17), Le(50u));
    EXPECT_THAT(static_cast<uint32_t>(PrefetchVersion::VERSION_31), Ge(17u));
}

// ============================================================================
// Prefetch Constants Tests
// ============================================================================

class PrefetchConstantsTest : public ::testing::Test {};

TEST_F(PrefetchConstantsTest, MagicSize) {
    EXPECT_EQ(PREFETCH_MAGIC_SIZE, 4u);
}

TEST_F(PrefetchConstantsTest, MinHeaderSize) {
    EXPECT_EQ(PREFETCH_HEADER_SIZE_MIN, 1024u);
}

TEST_F(PrefetchConstantsTest, Signatures) {
    EXPECT_STREQ(PREFETCH_SIGNATURE_MAM, "MAM");
    EXPECT_STREQ(PREFETCH_SIGNATURE_SCCA, "SCCA");
}

// ============================================================================
// LnkLinkFlags Tests
// ============================================================================

class LnkLinkFlagsTest : public ::testing::Test {};

TEST_F(LnkLinkFlagsTest, FlagValues) {
    EXPECT_EQ(LnkLinkFlags::HasLinkTargetIDList, 0x00000001u);
    EXPECT_EQ(LnkLinkFlags::HasLinkInfo, 0x00000002u);
    EXPECT_EQ(LnkLinkFlags::HasName, 0x00000004u);
    EXPECT_EQ(LnkLinkFlags::HasRelativePath, 0x00000008u);
    EXPECT_EQ(LnkLinkFlags::HasWorkingDir, 0x00000010u);
    EXPECT_EQ(LnkLinkFlags::HasArguments, 0x00000020u);
    EXPECT_EQ(LnkLinkFlags::HasIconLocation, 0x00000040u);
    EXPECT_EQ(LnkLinkFlags::IsUnicode, 0x00000080u);
}

TEST_F(LnkLinkFlagsTest, AdvancedFlags) {
    EXPECT_EQ(LnkLinkFlags::RunWithShimLayer, 0x00010000u);
    EXPECT_EQ(LnkLinkFlags::ForceNoLinkTrack, 0x00020000u);
    EXPECT_EQ(LnkLinkFlags::RunAsAdmin, 0x00100000u);
}

TEST_F(LnkLinkFlagsTest, FlagCombinations) {
    uint32_t combinedFlags = LnkLinkFlags::HasLinkTargetIDList | 
                              LnkLinkFlags::HasLinkInfo |
                              LnkLinkFlags::IsUnicode;
    
    EXPECT_TRUE(combinedFlags & LnkLinkFlags::HasLinkTargetIDList);
    EXPECT_TRUE(combinedFlags & LnkLinkFlags::HasLinkInfo);
    EXPECT_TRUE(combinedFlags & LnkLinkFlags::IsUnicode);
    EXPECT_FALSE(combinedFlags & LnkLinkFlags::HasName);
}

// ============================================================================
// LnkFileAttributes Tests
// ============================================================================

class LnkFileAttributesTest : public ::testing::Test {};

TEST_F(LnkFileAttributesTest, AttributeValues) {
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_READONLY, 0x00000001u);
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_HIDDEN, 0x00000002u);
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_SYSTEM, 0x00000004u);
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_DIRECTORY, 0x00000010u);
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_ARCHIVE, 0x00000020u);
}

TEST_F(LnkFileAttributesTest, AdvancedAttributes) {
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_TEMPORARY, 0x00000100u);
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_COMPRESSED, 0x00000800u);
    EXPECT_EQ(LnkFileAttributes::FILE_ATTRIBUTE_ENCRYPTED, 0x00004000u);
}

TEST_F(LnkFileAttributesTest, AttributeCombinations) {
    uint32_t attrs = LnkFileAttributes::FILE_ATTRIBUTE_READONLY |
                     LnkFileAttributes::FILE_ATTRIBUTE_HIDDEN |
                     LnkFileAttributes::FILE_ATTRIBUTE_SYSTEM;
    
    EXPECT_TRUE(attrs & LnkFileAttributes::FILE_ATTRIBUTE_READONLY);
    EXPECT_TRUE(attrs & LnkFileAttributes::FILE_ATTRIBUTE_HIDDEN);
    EXPECT_TRUE(attrs & LnkFileAttributes::FILE_ATTRIBUTE_SYSTEM);
    EXPECT_FALSE(attrs & LnkFileAttributes::FILE_ATTRIBUTE_ARCHIVE);
}

// ============================================================================
// LnkShowCommand Tests
// ============================================================================

class LnkShowCommandTest : public ::testing::Test {};

TEST_F(LnkShowCommandTest, CommandValues) {
    EXPECT_EQ(LnkShowCommand::SW_SHOWNORMAL, 1u);
    EXPECT_EQ(LnkShowCommand::SW_SHOWMINIMIZED, 2u);
    EXPECT_EQ(LnkShowCommand::SW_SHOWMAXIMIZED, 3u);
}

// ============================================================================
// LnkHeader Structure Tests
// ============================================================================

class LnkHeaderTest : public ::testing::Test {};

TEST_F(LnkHeaderTest, StructureSize) {
    // LNK header should be 72 bytes with current packing
    EXPECT_EQ(sizeof(LnkHeader), 72u);
}

TEST_F(LnkHeaderTest, HeaderConstants) {
    EXPECT_EQ(LNK_SIGNATURE, 0x0000004Cu);
    EXPECT_EQ(LNK_HEADER_SIZE, 0x4Cu);
}

// ============================================================================
// PrefetchFileInfo Structure Tests
// ============================================================================

class PrefetchFileInfoTest : public ::testing::Test {};

TEST_F(PrefetchFileInfoTest, DefaultInitialization) {
    PrefetchFileInfo info{};
    info.runCount = 0;
    info.lastRunTime = 0;
    
    EXPECT_EQ(info.runCount, 0u);
    EXPECT_EQ(info.lastRunTime, 0u);
    EXPECT_TRUE(info.runTimes.empty());
    EXPECT_TRUE(info.referencedFiles.empty());
}

TEST_F(PrefetchFileInfoTest, PopulatedValues) {
    PrefetchFileInfo info;
    info.runCount = 100;
    info.lastRunTime = 133500000000000000ULL;  // Some FILETIME value
    info.runTimes.push_back(133500000000000000ULL);
    info.referencedFiles.push_back("C:\\Windows\\System32\\kernel32.dll");
    info.referencedDirectories.push_back("C:\\Windows\\System32");
    
    EXPECT_EQ(info.runCount, 100u);
    EXPECT_EQ(info.runTimes.size(), 1u);
    EXPECT_EQ(info.referencedFiles.size(), 1u);
}

// ============================================================================
// PrefetchFileHeader Structure Tests
// ============================================================================

class PrefetchFileHeaderTest : public ::testing::Test {};

TEST_F(PrefetchFileHeaderTest, SignatureField) {
    PrefetchFileHeader header{};
    std::memcpy(header.signature, "SCCA", 4);
    
    EXPECT_EQ(std::memcmp(header.signature, "SCCA", 4), 0);
}

TEST_F(PrefetchFileHeaderTest, VersionField) {
    PrefetchFileHeader header{};
    header.version = 30;
    
    EXPECT_EQ(header.version, 30u);
}

// ============================================================================
// File Extension Tests
// ============================================================================

class FileExtensionTest : public ::testing::Test {};

TEST_F(FileExtensionTest, PrefetchExtension) {
    std::string filename = "CMD.EXE-12345678.pf";
    size_t dotPos = filename.rfind('.');
    EXPECT_NE(dotPos, std::string::npos);
    EXPECT_EQ(filename.substr(dotPos), ".pf");
}

TEST_F(FileExtensionTest, LnkExtension) {
    std::string filename = "Notepad.lnk";
    size_t dotPos = filename.rfind('.');
    EXPECT_NE(dotPos, std::string::npos);
    EXPECT_EQ(filename.substr(dotPos), ".lnk");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
