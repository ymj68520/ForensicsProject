#include <iostream>
#include <string>
#include <filesystem>
#include <memory>
#include <map>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "ImageAnalyzer/ImageAnalyzer.h"
#include "DatabaseManager/EventExtractor/EventExtractor.h"
#include "DatabaseManager/FileClassifier/FileClassifier.h"
#include "DatabaseManager/FileExtractor/FileExtractor.h"
#include "HTTPServer/HTTPserver.h"


namespace fs = std::filesystem;

struct CommandLineArgs {
	std::string imagePath;
	std::string databasePath;
	std::string extractFile;
	std::string extractExt;
	std::string outputDir = "extracted_files";
	XFSMode xfsMode = XFSMode::Auto;
	bool extractAll = false;
	bool includeDeleted = false;
	bool extractMode = false;
	bool httpServer = false;
	int httpPort = 8080;
};

void printUsage(const char* programName) {
	std::cout << "Forensic Image Analyzer with File Extraction\n\n";
	std::cout << "Usage:\n";
	std::cout << "  Analysis mode:\n";
	std::cout << "    " << programName << " <image_path> [options]\n\n";
	std::cout << "  Extraction mode:\n";
	std::cout << "    " << programName << " --database <db_path> [extraction options]\n\n";
	std::cout << "Analysis options:\n";
	std::cout << "  --xfs-mode <mode>           XFS parsing mode for XFS filesystems:\n";
	std::cout << "                              - auto: Auto-detect (default)\n";
	std::cout << "                                Linux: native mount (full support, requires sudo)\n";
	std::cout << "                                Windows: pure parser (limited support)\n";
	std::cout << "                              - native: Force Linux native mount (Linux only)\n";
	std::cout << "                              - pure: Force pure XFS parser (cross-platform)\n\n";
	std::cout << "Extraction options:\n";
	std::cout << "  --extract-file <pattern>    Extract files by name (supports wildcards: *, ?)\n";
	std::cout << "                              Example: --extract-file \"vmlinuz*\"\n";
	std::cout << "  --extract-ext <extensions>  Extract files by extension (comma-separated)\n";
	std::cout << "                              Example: --extract-ext \".log,.conf\"\n";
	std::cout << "  --extract-all               Extract all files\n";
	std::cout << "  --output-dir <path>         Output directory (default: extracted_files)\n";
	std::cout << "  --include-deleted           Include deleted files in extraction\n\n";
	std::cout << "HTTP Server options:\n";
	std::cout << "  --http-server [port]        Start HTTP server (default port 8080)\n\n";
	std::cout << "Examples:\n";
	std::cout << "  # Analyze image (auto-detect XFS mode)\n";
	std::cout << "  " << programName << " image.dd\n\n";
	std::cout << "  # Analyze XFS image with native mount (Linux, requires sudo)\n";
	std::cout << "  sudo " << programName << " Server.dd --xfs-mode native\n\n";
	std::cout << "  # Analyze XFS image with pure parser (Windows or Linux)\n";
	std::cout << "  " << programName << " Server.dd --xfs-mode pure\n\n";
	std::cout << "  # Extract kernel files\n";
	std::cout << "  " << programName << " --database image_raw.db --extract-file \"vmlinuz*\" --output-dir kernels\n\n";
	std::cout << "  # Extract log and config files\n";
	std::cout << "  " << programName << " --database Server_raw.db --extract-ext \".log,.conf\" --output-dir configs\n\n";
	std::cout << "  # Extract all files\n";
	std::cout << "  " << programName << " --database image_raw.db --extract-all\n";
}

CommandLineArgs parseArgs(int argc, char* argv[]) {
	CommandLineArgs args;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];

		if (arg == "--database" && i + 1 < argc) {
			args.databasePath = argv[++i];
			args.extractMode = true;
		} else if (arg == "--extract-file" && i + 1 < argc) {
			args.extractFile = argv[++i];
		} else if (arg == "--extract-ext" && i + 1 < argc) {
			args.extractExt = argv[++i];
		} else if (arg == "--extract-all") {
			args.extractAll = true;
		} else if (arg == "--output-dir" && i + 1 < argc) {
			args.outputDir = argv[++i];
		} else if (arg == "--include-deleted") {
			args.includeDeleted = true;
		} else if (arg == "--xfs-mode" && i + 1 < argc) {
			std::string mode = argv[++i];
			if (mode == "auto") {
				args.xfsMode = XFSMode::Auto;
			} else if (mode == "native") {
				args.xfsMode = XFSMode::Native;
			} else if (mode == "pure") {
				args.xfsMode = XFSMode::Pure;
			} else {
				std::cerr << "Error: Invalid XFS mode '" << mode << "'" << std::endl;
				std::cerr << "Valid options: auto, native, pure" << std::endl;
			}
		} else if (arg == "--http-server") {
			args.httpServer = true;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				args.httpPort = std::stoi(argv[++i]);
			}
		} else if (arg[0] != '-') {
			// Assume it's image path if no leading dash
			args.imagePath = arg;
		}
	}

	return args;
}

std::string getBaseName(const std::string& path) {
	fs::path p(path);
	return p.stem().string();
}

#ifdef _WIN32
// Windows version with wide character support for Unicode paths
int wmain(int argc, wchar_t* wargv[]) {
	// Convert wide args to UTF-8
	std::vector<std::string> args;
	std::vector<char*> argv;

	for (int i = 0; i < argc; i++) {
		int size = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
		std::string arg(size - 1, 0);
		WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &arg[0], size, nullptr, nullptr);
		args.push_back(arg);
	}

	for (auto& arg : args) {
		argv.push_back(&arg[0]);
	}
#else
// Linux/Unix version - UTF-8 is default
int main(int argc, char* argv[]) {
#endif

	// Parse command line arguments
	if (argc < 2) {
#ifdef _WIN32
		printUsage(argv.data()[0]);
#else
		printUsage(argv[0]);
#endif
		return 1;
	}

#ifdef _WIN32
	CommandLineArgs cmdArgs = parseArgs(argc, argv.data());
#else
	CommandLineArgs cmdArgs = parseArgs(argc, argv);
#endif

	if (cmdArgs.httpServer) {
		std::cout << "Starting HTTP Server on port " << cmdArgs.httpPort << std::endl;
		asio::io_context ioc;
		forensics::HTTPServer server(ioc);
		server.run(cmdArgs.httpPort);
		return 0;
	}

	// Determine mode: extraction or analysis
	if (cmdArgs.extractMode) {
		// ========== EXTRACTION MODE ==========
		std::cout << "=== File Extraction Mode ===" << std::endl;

		// Validate arguments
		if (cmdArgs.databasePath.empty()) {
			std::cerr << "Error: --database is required for extraction mode" << std::endl;
			printUsage(argv[0]);
			return 1;
		}

		// If raw database is specified, use files database instead
		std::string dbPath = cmdArgs.databasePath;
		if (dbPath.length() > 7 && dbPath.substr(dbPath.length() - 7) == "_raw.db") {
			dbPath = dbPath.substr(0, dbPath.length() - 7) + "_files.db";
			std::cout << "Note: Using classified files database: " << dbPath << std::endl;
		}

		if (!fs::exists(dbPath)) {
			std::cerr << "Error: Database file not found: " << dbPath << std::endl;
			return 1;
		}

		// Determine image path from database name
		std::string imagePath;
		std::string dbName = fs::path(dbPath).stem().string();
		// Remove database type suffixes if present
		if (dbName.length() > 6 && dbName.substr(dbName.length() - 6) == "_files") {
			dbName = dbName.substr(0, dbName.length() - 6);
		} else if (dbName.length() > 7 && dbName.substr(dbName.length() - 7) == "_events") {
			dbName = dbName.substr(0, dbName.length() - 7);
		} else if (dbName.length() > 4 && dbName.substr(dbName.length() - 4) == "_raw") {
			dbName = dbName.substr(0, dbName.length() - 4);
		}

		// Try common extensions
		std::vector<std::string> extensions = {".dd", ".DD", ".001", ".e01", ".E01", ".raw", ".RAW"};
		for (const auto& ext : extensions) {
			std::string testPath = dbName + ext;
			if (fs::exists(testPath)) {
				imagePath = testPath;
				break;
			}
		}

		if (imagePath.empty()) {
			std::cerr << "Error: Cannot find image file (tried: " << dbName << ".dd, etc.)" << std::endl;
			std::cerr << "Please ensure the image file is in the current directory." << std::endl;
			return 1;
		}

		std::cout << "Database: " << cmdArgs.databasePath << std::endl;
		std::cout << "Image: " << imagePath << std::endl;
		std::cout << "Output: " << cmdArgs.outputDir << std::endl;
		std::cout << std::endl;

		try {
			auto extractor = std::make_unique<FileExtractor>(imagePath, dbPath);

			if (!extractor->initialize()) {
				std::cerr << "Error: Failed to initialize file extractor" << std::endl;
				return 1;
			}

			int extracted = 0;

			// Perform extraction based on options
			if (!cmdArgs.extractFile.empty()) {
				std::cout << "Extracting files matching: " << cmdArgs.extractFile << std::endl;
				extracted = extractor->extractByName(cmdArgs.extractFile, cmdArgs.outputDir);
			} else if (!cmdArgs.extractExt.empty()) {
				std::cout << "Extracting files with extensions: " << cmdArgs.extractExt << std::endl;
				extracted = extractor->extractByExtension(cmdArgs.extractExt, cmdArgs.outputDir);
			} else if (cmdArgs.extractAll) {
				std::cout << "Extracting all files" << std::endl;
				extracted = extractor->extractAll(cmdArgs.outputDir, cmdArgs.includeDeleted);
			} else {
				std::cerr << "Error: No extraction option specified" << std::endl;
				std::cerr << "Use --extract-file, --extract-ext, or --extract-all" << std::endl;
				return 1;
			}

			std::cout << "\n=== Extraction Complete ===" << std::endl;
			std::cout << "Files extracted: " << extracted << std::endl;
			std::cout << "Output directory: " << cmdArgs.outputDir << std::endl;

		} catch (const std::exception& e) {
			std::cerr << "Fatal error: " << e.what() << std::endl;
			return 1;
		}

	} else {
		// ========== ANALYSIS MODE ==========
		if (cmdArgs.imagePath.empty()) {
			std::cerr << "Error: Image path required" << std::endl;
			printUsage(argv[0]);
			return 1;
		}

		std::string imagePath = cmdArgs.imagePath;

		// Check if image exists
		if (!fs::exists(imagePath)) {
			std::cerr << "Error: Image file not found: " << imagePath << std::endl;
			return 1;
		}

		std::cout << "=== Forensic Image Analyzer ===" << std::endl;
		std::cout << "Image: " << imagePath << std::endl;
		std::cout << "Using The Sleuth Kit 4.14.0" << std::endl;
		std::cout << std::endl;

		// Generate database names
		std::string baseName = getBaseName(imagePath);
		std::string rawDbPath = baseName + "_raw.db";
		std::string eventDbPath = baseName + "_events.db";
		std::string fileDbPath = baseName + "_files.db";

		try {
			// Step 1: Analyze image and create raw database
			std::cout << "[1/3] Analyzing image and extracting raw data..." << std::endl;
			auto analyzer = std::make_unique<ImageAnalyzer>(imagePath);

			// Set XFS mode based on user selection
			analyzer->setXFSMode(cmdArgs.xfsMode);

			if (!analyzer->analyze()) {
				std::cerr << "Error: Failed to analyze image" << std::endl;
				return 1;
			}

			if (!analyzer->extractToDatabase(rawDbPath)) {
				std::cerr << "Error: Failed to extract data to database" << std::endl;
				return 1;
			}
			std::cout << "✓ Raw database created: " << rawDbPath << std::endl;
			std::cout << std::endl;

			// Step 2: Extract filesystem events
			std::cout << "[2/3] Extracting filesystem events..." << std::endl;
			auto eventExtractor = std::make_unique<EventExtractor>(rawDbPath, eventDbPath);

			if (!eventExtractor->extractEvents()) {
				std::cerr << "Error: Failed to extract events" << std::endl;
				return 1;
			}
			std::cout << "✓ Event database created: " << eventDbPath << std::endl;
			std::cout << std::endl;

			// Step 3: Classify files by type
			std::cout << "[3/3] Classifying files by type..." << std::endl;
			auto fileClassifier = std::make_unique<FileClassifier>(rawDbPath, fileDbPath);

			if (!fileClassifier->classifyAndExtract()) {
				std::cerr << "Error: Failed to classify files" << std::endl;
				return 1;
			}
			std::cout << "✓ File database created: " << fileDbPath << std::endl;
			std::cout << std::endl;

			// Summary
			std::cout << "=== Analysis Complete ===" << std::endl;
			std::cout << "Generated databases:" << std::endl;
			std::cout << "  1. " << rawDbPath << " (Raw TSK data)" << std::endl;
			std::cout << "  2. " << eventDbPath << " (Filesystem events)" << std::endl;
			std::cout << "  3. " << fileDbPath << " (Classified files)" << std::endl;
			std::cout << "\nTip: Use --database to extract files from the image" << std::endl;
		}
		catch (const std::exception& e) {
			std::cerr << "Fatal error: " << e.what() << std::endl;
			return 1;
		}
	}

	return 0;
}