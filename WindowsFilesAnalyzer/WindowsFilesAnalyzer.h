// WindowsFilesAnalyzer.h
// Main header for Windows Files Analyzer module

#pragma once

// IO and streams
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

// Containers
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <queue>

// String processing
#include <string>
#include <cstring>
#include <cctype>
#include <regex>

// Algorithms and utilities
#include <algorithm>
#include <functional>
#include <memory>
#include <cstdint>

// External libraries
#include <sqlite3.h>
#include <tsk/libtsk.h>

// Project includes
#include "../DatabaseManager/DatabaseManager.h"
#include "../DatabaseManager/FileExtractor/FileExtractor.h"

// Module includes
#include "WindowsDataTypes.h"
#include "WindowsAnalysisDatabase.h"
#include "WindowsAnalyzerDeclarations.h"

// Implementation files:
// - WindowsFilesAnalyzerCore.cpp: Core functionality (initialize, analyzeWindowsData)
// - WindowsAnalysisDatabase.cpp: Database operations
// - WindowsRegistryParser.cpp: Registry hive parsing
// - WindowsEventLogParser.cpp: Event log (EVTX) parsing
// - WindowsArtifactsParsers.cpp: Prefetch, LNK, Jump Lists, Recycle Bin parsing
