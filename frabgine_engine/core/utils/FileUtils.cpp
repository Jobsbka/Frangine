#include "FileUtils.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace frabgine {

std::string FileUtils::readTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<uint8_t> FileUtils::readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }
    
    return buffer;
}

bool FileUtils::writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

bool FileUtils::writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool FileUtils::fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool FileUtils::createDirectory(const std::string& path) {
    return std::filesystem::create_directories(path);
}

bool FileUtils::deleteFile(const std::string& path) {
    return std::filesystem::remove(path);
}

bool FileUtils::deleteDirectory(const std::string& path) {
    return std::filesystem::remove_all(path);
}

std::string FileUtils::getDirectoryName(const std::string& path) {
    return std::filesystem::path(path).parent_path().string();
}

std::string FileUtils::getFileName(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

std::string FileUtils::getExtension(const std::string& path) {
    return std::filesystem::path(path).extension().string();
}

std::string FileUtils::getStem(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

std::vector<std::string> FileUtils::listFiles(const std::string& directory, 
                                               const std::string& extension,
                                               bool recursive) {
    std::vector<std::string> files;
    
    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    if (extension.empty() || entry.path().extension() == extension) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    if (extension.empty() || entry.path().extension() == extension) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        }
    } catch (const std::exception&) {
        // Игнорируем ошибки доступа
    }
    
    return files;
}

uintmax_t FileUtils::getFileSize(const std::string& path) {
    try {
        return std::filesystem::file_size(path);
    } catch (const std::exception&) {
        return 0;
    }
}

} // namespace frabgine
