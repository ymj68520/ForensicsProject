#include "MCPIntegration.h"

// Include MCP headers
#include "mcp_server.h"
#include "mcp_tool.h"
#include "json.hpp"
#include "ConfigManager/ConfigManager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace forensics {
namespace llm {

MCPIntegration::MCPIntegration(std::shared_ptr<ModelRouter> router, int port)
    : router_(router), port_(port) {
}

MCPIntegration::~MCPIntegration() {
    stop();
}

void MCPIntegration::start(bool blocking) {
    if (running_) {
        return;
    }
    
    // Create MCP server configuration
    mcp::server::configuration config;
    config.host = ConfigManager::instance().getMCPHost();
    config.port = port_;
    
    server_ = std::make_unique<mcp::server>(config);
    server_->set_server_info("ForensicsLLMServer", "1.0.0");
    
    // Set capabilities
    mcp::json capabilities = {
        {"tools", mcp::json::object()}
    };
    server_->set_capabilities(capabilities);
    
    registerBuiltinTools();

    // File tools are deny-by-default. Operators must configure explicit roots.
    setAllowedPaths(ConfigManager::instance().getMCPAllowedPaths());

    // Start server
    running_ = true;
    server_->start(blocking);
}

void MCPIntegration::stop() {
    if (server_ && running_) {
        server_->stop();
        running_ = false;
    }
}

bool MCPIntegration::isRunning() const {
    return running_;
}

void MCPIntegration::registerTool(const std::string& name,
                                   const std::string& description,
                                   const std::string& parametersJson,
                                   MCPToolHandler handler) {
    if (!server_) {
        customHandlers_[name] = handler;
        return;
    }
    
    // Parse parameters schema
    mcp::json params;
    try {
        params = mcp::json::parse(parametersJson);
    } catch (...) {
        params = mcp::json::object();
    }
    
    // Build tool
    mcp::tool_builder builder(name);
    builder.with_description(description);
    
    // Add parameters from schema
    if (params.contains("properties")) {
        for (auto& [key, value] : params["properties"].items()) {
            std::string type = value.value("type", "string");
            std::string desc = value.value("description", "");
            bool required = false;
            
            if (params.contains("required")) {
                for (const auto& req : params["required"]) {
                    if (req.get<std::string>() == key) {
                        required = true;
                        break;
                    }
                }
            }
            
            if (type == "string") {
                builder.with_string_param(key, desc, required);
            } else if (type == "number" || type == "integer") {
                builder.with_number_param(key, desc, required);
            } else if (type == "boolean") {
                builder.with_boolean_param(key, desc, required);
            }
        }
    }
    
    mcp::tool tool = builder.build();
    
    // Store handler and register with server
    customHandlers_[name] = handler;
    
    server_->register_tool(tool, 
        [this, name](const mcp::json& params, const std::string&) -> mcp::json {
            std::string result = customHandlers_[name](params.dump());
            return {
                {{"type", "text"}, {"text", result}}
            };
        });
}

int MCPIntegration::getPort() const {
    return port_;
}

void MCPIntegration::setAllowedPaths(const std::vector<std::string>& paths) {
    allowedPaths_.clear();
    for (const auto& path : paths) {
        if (!path.empty()) allowedPaths_.push_back(path);
    }
}

std::vector<std::string> MCPIntegration::getRegisteredTools() const {
    std::vector<std::string> tools = {
        "read_file",
        "analyze_file", 
        "list_files",
        "generate_description"
    };
    
    for (const auto& [name, _] : customHandlers_) {
        if (std::find(tools.begin(), tools.end(), name) == tools.end()) {
            tools.push_back(name);
        }
    }
    
    return tools;
}

void MCPIntegration::registerBuiltinTools() {
    if (!server_) return;
    
    // read_file tool
    {
        mcp::tool tool = mcp::tool_builder("read_file")
            .with_description("Read the contents of a file")
            .with_string_param("path", "Path to the file to read", true)
            .with_number_param("max_bytes", "Maximum bytes to read (0 = configured limit)", false)
            .build();
        
        server_->register_tool(tool,
            [this](const mcp::json& params, const std::string&) -> mcp::json {
                std::string result = handleReadFile(params.dump());
                return {{{"type", "text"}, {"text", result}}};
            });
    }
    
    // analyze_file tool
    {
        mcp::tool tool = mcp::tool_builder("analyze_file")
            .with_description("Analyze a file and generate a summary using LLM")
            .with_string_param("path", "Path to the file to analyze", true)
            .with_boolean_param("include_keywords", "Extract keywords from content", false)
            .build();
        
        server_->register_tool(tool,
            [this](const mcp::json& params, const std::string&) -> mcp::json {
                std::string result = handleAnalyzeFile(params.dump());
                return {{{"type", "text"}, {"text", result}}};
            });
    }
    
    // list_files tool
    {
        mcp::tool tool = mcp::tool_builder("list_files")
            .with_description("List files in a directory")
            .with_string_param("path", "Directory path", true)
            .with_boolean_param("recursive", "Include subdirectories", false)
            .build();
        
        server_->register_tool(tool,
            [this](const mcp::json& params, const std::string&) -> mcp::json {
                std::string result = handleListFiles(params.dump());
                return {{{"type", "text"}, {"text", result}}};
            });
    }
    
    // generate_description tool
    {
        mcp::tool tool = mcp::tool_builder("generate_description")
            .with_description("Generate a natural language description of a file or set of files")
            .with_string_param("paths", "Comma-separated list of file paths", true)
            .build();
        
        server_->register_tool(tool,
            [this](const mcp::json& params, const std::string&) -> mcp::json {
                std::string result = handleGenerateDescription(params.dump());
                return {{{"type", "text"}, {"text", result}}};
            });
    }
}

bool MCPIntegration::isPathAllowed(const std::string& path) const {
    if (allowedPaths_.empty()) return false;

    std::error_code ec;
    const fs::path candidate = fs::weakly_canonical(fs::path(path), ec);
    if (ec) return false;

    for (const auto& allowed : allowedPaths_) {
        ec.clear();
        const fs::path root = fs::weakly_canonical(fs::path(allowed), ec);
        if (ec) continue;
        const fs::path relative = candidate.lexically_relative(root);
        if (!relative.empty() && relative != "." && *relative.begin() != "..") {
            return true;
        }
        if (candidate == root) return true;
    }
    return false;
}

std::string MCPIntegration::readFileContent(const std::string& path, size_t maxBytes) {
    if (!isPathAllowed(path)) {
        return "Error: Path not allowed";
    }

    const size_t configuredLimit = static_cast<size_t>(
        ConfigManager::instance().getMCPMaxReadBytes());
    const size_t effectiveLimit = maxBytes == 0
        ? configuredLimit
        : std::min(maxBytes, configuredLimit);

    std::error_code ec;
    const fs::path canonicalPath = fs::weakly_canonical(fs::path(path), ec);
    if (ec || !fs::is_regular_file(canonicalPath, ec)) {
        return "Error: File not found or not a regular file";
    }

    std::ifstream file(canonicalPath, std::ios::binary);
    if (!file) return "Error: Cannot open file";

    std::vector<char> buffer(effectiveLimit);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return std::string(buffer.data(), static_cast<size_t>(file.gcount()));
}

std::string MCPIntegration::handleReadFile(const std::string& argsJson) {
    try {
        auto args = json::parse(argsJson);
        std::string path = args.value("path", "");
        size_t maxBytes = args.value("max_bytes", 0);
        
        if (path.empty()) {
            return "Error: path parameter is required";
        }
        
        return readFileContent(path, maxBytes);
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

std::string MCPIntegration::handleAnalyzeFile(const std::string& argsJson) {
    try {
        auto args = json::parse(argsJson);
        std::string path = args.value("path", "");
        bool includeKeywords = args.value("include_keywords", true);
        
        if (path.empty()) {
            return "Error: path parameter is required";
        }
        
        // Read file content
        std::string content = readFileContent(path, 50000);  // Limit to 50KB
        if (content.substr(0, 6) == "Error:") {
            return content;
        }
        
        if (!router_) {
            return "Error: No LLM router configured";
        }
        
        // Build analysis prompt
        std::string systemPrompt = 
            "You are a file analysis assistant. Analyze the following file content and provide:\n"
            "1. A concise summary (2-3 sentences)\n"
            "2. The main purpose or topic of the file\n";
        
        if (includeKeywords) {
            systemPrompt += "3. Key terms or concepts (comma-separated list)\n";
        }
        
        systemPrompt += "\nRespond in a structured format.";
        
        std::string userPrompt = "File: " + path + "\n\nContent:\n" + content;
        
        // Get LLM response
        auto response = router_->chat(userPrompt, systemPrompt);
        
        if (!response.success) {
            return "Error: LLM analysis failed - " + response.errorMessage;
        }
        
        return response.content;
        
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

std::string MCPIntegration::handleListFiles(const std::string& argsJson) {
    try {
        auto args = json::parse(argsJson);
        std::string path = args.value("path", "");
        bool recursive = args.value("recursive", false);
        
        if (path.empty()) {
            return "Error: path parameter is required";
        }
        
        if (!isPathAllowed(path)) {
            return "Error: Path not allowed";
        }
        
        std::error_code ec;
        const fs::path canonicalDir = fs::weakly_canonical(fs::path(path), ec);
        if (ec || !fs::is_directory(canonicalDir, ec)) {
            return "Error: Directory not found";
        }

        json result = json::array();
        const size_t maxEntries = static_cast<size_t>(
            ConfigManager::instance().getMCPMaxListEntries());

        auto addEntry = [this, &result, maxEntries](const fs::directory_entry& entry) {
            if (result.size() >= maxEntries) return false;
            std::error_code entryEc;
            const fs::path canonicalEntry = fs::weakly_canonical(entry.path(), entryEc);
            if (entryEc || !isPathAllowed(canonicalEntry.string())) return true;
            json item;
            item["path"] = canonicalEntry.string();
            item["name"] = canonicalEntry.filename().string();
            item["is_directory"] = entry.is_directory(entryEc);
            if (entryEc) return true;
            if (!item["is_directory"].get<bool>()) {
                item["size"] = entry.file_size(entryEc);
                item["extension"] = canonicalEntry.extension().string();
            }
            result.push_back(item);
            return true;
        };
        bool truncated = false;

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(canonicalDir)) {
                if (!addEntry(entry)) {
                    truncated = true;
                    break;
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(canonicalDir)) {
                if (!addEntry(entry)) {
                    truncated = true;
                    break;
                }
            }
        }

        if (truncated) {
            return json{{"entries", result}, {"truncated", true},
                        {"limit", maxEntries}}.dump(2);
        }
        return result.dump(2);
        
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

std::string MCPIntegration::handleGenerateDescription(const std::string& argsJson) {
    try {
        auto args = json::parse(argsJson);
        std::string pathsStr = args.value("paths", "");
        
        if (pathsStr.empty()) {
            return "Error: paths parameter is required";
        }
        
        // Split paths by comma
        std::vector<std::string> paths;
        std::istringstream iss(pathsStr);
        std::string path;
        while (std::getline(iss, path, ',')) {
            // Trim whitespace
            path.erase(0, path.find_first_not_of(" \t"));
            path.erase(path.find_last_not_of(" \t") + 1);
            if (!path.empty()) {
                paths.push_back(path);
            }
        }
        
        if (paths.empty()) {
            return "Error: No valid paths provided";
        }
        
        // Collect file info
        std::ostringstream fileInfo;
        for (const auto& p : paths) {
            if (!isPathAllowed(p)) {
                continue;
            }
            
            if (fs::exists(p)) {
                fileInfo << "File: " << p << "\n";
                if (fs::is_regular_file(p)) {
                    fileInfo << "  Size: " << fs::file_size(p) << " bytes\n";
                    fileInfo << "  Extension: " << fs::path(p).extension().string() << "\n";
                    
                    // Read first 2KB for context
                    std::string preview = readFileContent(p, 2000);
                    if (!preview.empty() && preview.substr(0, 6) != "Error:") {
                        fileInfo << "  Preview:\n" << preview << "\n\n";
                    }
                }
            }
        }
        
        if (!router_) {
            return "Error: No LLM router configured";
        }
        
        std::string systemPrompt = 
            "Generate a natural language description of the following files. "
            "Describe what they contain, their purpose, and how they relate to each other. "
            "Be concise but informative.";
        
        auto response = router_->chat(fileInfo.str(), systemPrompt);
        
        if (!response.success) {
            return "Error: Description generation failed - " + response.errorMessage;
        }
        
        return response.content;
        
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // namespace llm
} // namespace forensics
