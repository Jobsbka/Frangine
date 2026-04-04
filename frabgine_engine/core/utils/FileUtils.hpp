#ifndef FRABGINE_CORE_UTILS_FILEUTILS_HPP
#define FRABGINE_CORE_UTILS_FILEUTILS_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace frabgine {

class FileUtils {
public:
    // Чтение файлов
    static std::string readTextFile(const std::string& path);
    static std::vector<uint8_t> readBinaryFile(const std::string& path);
    
    // Запись файлов
    static bool writeTextFile(const std::string& path, const std::string& content);
    static bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data);
    
    // Операции с файловой системой
    static bool fileExists(const std::string& path);
    static bool createDirectory(const std::string& path);
    static bool deleteFile(const std::string& path);
    static bool deleteDirectory(const std::string& path);
    
    // Работа с путями
    static std::string getDirectoryName(const std::string& path);
    static std::string getFileName(const std::string& path);
    static std::string getExtension(const std::string& path);
    static std::string getStem(const std::string& path);
    
    // Поиск файлов
    static std::vector<std::string> listFiles(const std::string& directory, 
                                               const std::string& extension = "",
                                               bool recursive = false);
    
    // Информация о файле
    static uintmax_t getFileSize(const std::string& path);
};

} // namespace frabgine

#endif // FRABGINE_CORE_UTILS_FILEUTILS_HPP
