#pragma once
#ifndef IMAGE_ANALYZER_H
#define IMAGE_ANALYZER_H

#include <string>
#include <memory>
#include <vector>
#include <tsk/libtsk.h>

class DatabaseManager;
class XFSHelper;
class NativeFilesystemWalker;

enum class XFSMode {
	Auto,      // Auto-detect (native on Linux, pure on Windows)
	Native,    // Linux native mount (requires sudo)
	Pure       // Pure XFS parser (cross-platform)
};

class ImageAnalyzer {
public:
	explicit ImageAnalyzer(const std::string& imagePath);
	~ImageAnalyzer();

	bool analyze();
	bool extractToDatabase(const std::string& dbPath);
	void setXFSMode(XFSMode mode) { xfsMode_ = mode; }

	TSK_IMG_INFO* getImageInfo() const { return imgInfo_; }
	TSK_FS_INFO* getFileSystemInfo() const { return fsInfo_; }

private:
	std::string imagePath_;
	TSK_IMG_INFO* imgInfo_;
	TSK_FS_INFO* fsInfo_;
	std::unique_ptr<DatabaseManager> dbManager_;
	std::unique_ptr<XFSHelper> xfsHelper_;
	std::unique_ptr<NativeFilesystemWalker> nativeWalker_;
	uint64_t partitionOffset_;
	bool isXFS_;
	XFSMode xfsMode_;

	bool openImage();
	bool openFileSystem();
	void closeImage();
	bool extractWithXFS(const std::string& dbPath);
	bool extractWithNativeMount(const std::string& dbPath);

	static TSK_WALK_RET_ENUM fileWalkCallback(TSK_FS_FILE* fsFile,
		const char* path,
		void* ptr);

	bool processFile(TSK_FS_FILE* fsFile, const std::string& path);
	std::string detectImageType();
	std::string detectOSType();
};

#endif // IMAGE_ANALYZER_H
