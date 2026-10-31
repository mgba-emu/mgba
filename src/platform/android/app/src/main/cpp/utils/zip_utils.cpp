#include "zip_utils.h"
#include "../log/log.h"
#include "minizip/unzip.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

bool ZipUtils::exportUserData(const std::string& folderPath, int outFd, const std::string& appId) {
    fs::path rootPath(folderPath);
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        LOGE("Folder does not exist or is not a directory.");
        return false;
    }

    zlib_filefunc_def filefunc;
    ZipUtils::fill_fd_filefunc(&filefunc, outFd);

    zipFile zf = zipOpen2(nullptr, APPEND_STATUS_CREATE, nullptr, &filefunc);
    if (!zf) {
        LOGE("Failed to initialize zip stream on target FD.");
        return false;
    }

    std::string manifestContent = "{\"app_id\":\"" + appId + "\",\"version\":1}";
    zip_fileinfo manifestInfo{};

    if (zipOpenNewFileInZip(zf, ".app_manifest.json", &manifestInfo, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION) == ZIP_OK) {
        zipWriteInFileInZip(zf, manifestContent.c_str(), manifestContent.length());
        zipCloseFileInZip(zf);
    } else {
        zipClose(zf, nullptr);
        return false;
    }

    constexpr size_t BUFFER_SIZE = 16384;
    std::vector<char> buffer(BUFFER_SIZE);

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            fs::path relPath = fs::relative(entry.path(), rootPath);
            std::string zipEntryName = relPath.generic_string();

            zip_fileinfo zi{};

            if (entry.is_directory()) {
                zipEntryName += '/';

                int err = zipOpenNewFileInZip(
                        zf,
                        zipEntryName.c_str(),
                        &zi,
                        nullptr, 0,
                        nullptr, 0,
                        nullptr,
                        0,
                        Z_NO_COMPRESSION
                );

                if (err == ZIP_OK) {
                    zipCloseFileInZip(zf);
                }
            }
            else if (entry.is_regular_file()) {
                int err = zipOpenNewFileInZip(
                        zf,
                        zipEntryName.c_str(),
                        &zi,
                        nullptr, 0,
                        nullptr, 0,
                        nullptr,
                        Z_DEFLATED,
                        Z_DEFAULT_COMPRESSION
                );

                if (err != ZIP_OK) {
                    zipClose(zf, nullptr);
                    return false;
                }

                std::ifstream inFile(entry.path(), std::ios::binary);
                if (!inFile.is_open()) {
                    zipCloseFileInZip(zf);
                    zipClose(zf, nullptr);
                    return false;
                }

                while (inFile.read(buffer.data(), buffer.size()) || inFile.gcount() > 0) {
                    std::streamsize bytesRead = inFile.gcount();
                    err = zipWriteInFileInZip(zf, buffer.data(), static_cast<unsigned int>(bytesRead));
                    if (err < 0) {
                        inFile.close();
                        zipCloseFileInZip(zf);
                        zipClose(zf, nullptr);
                        return false;
                    }
                }

                inFile.close();
                zipCloseFileInZip(zf);
            }
        }
    } catch (const std::exception& e) {
        LOGE("Exception during compression: {}", e.what());
        zipClose(zf, nullptr);
        return false;
    }

    close(outFd);
    return zipClose(zf, nullptr) == ZIP_OK;
}

bool ZipUtils::importUserData(int inFd, const std::string& targetExtractDir, const std::string& expectedAppId) {
    zlib_filefunc_def filefunc;
    ZipUtils::fill_fd_filefunc(&filefunc, inFd);

    unzFile uz = unzOpen2(nullptr, &filefunc);
    if (!uz) return false;

    if (unzLocateFile(uz, ".app_manifest.json", 1) != UNZ_OK) {
        LOGE("Invalid backup: Missing manifest.");
        unzClose(uz);
        return false;
    }

    if (unzOpenCurrentFile(uz) == UNZ_OK) {
        char manifestBuf[1024] = {0};
        unzReadCurrentFile(uz, manifestBuf, sizeof(manifestBuf) - 1);
        unzCloseCurrentFile(uz);

        std::string manifestContent(manifestBuf);
        std::string targetSignature = "\"app_id\":\"" + expectedAppId + "\"";

        if (manifestContent.find(targetSignature) == std::string::npos) {
            LOGE("Invalid backup: App ID mismatch.");
            unzClose(uz);
            return false;
        }
    } else {
        unzClose(uz);
        return false;
    }

    fs::path baseExtractDir(targetExtractDir);
    fs::create_directories(baseExtractDir);

    constexpr size_t BUFFER_SIZE = 16384;
    std::vector<char> buffer(BUFFER_SIZE);

    unzGoToFirstFile(uz);
    do {
        char filename[256];
        unz_file_info file_info;
        unzGetCurrentFileInfo(uz, &file_info, filename, sizeof(filename), nullptr, 0, nullptr, 0);

        std::string entryName(filename);
        if (entryName == ".app_manifest.json") continue;

        if (entryName.find("..") != std::string::npos || (!entryName.empty() && entryName[0] == '/')) {
            LOGW("Skipping invalid path: %s", entryName.c_str());
            continue;
        }

        fs::path targetPath = baseExtractDir / entryName;

        if (!entryName.empty() && entryName.back() == '/') {
            fs::create_directories(targetPath);
            continue;
        }

        fs::create_directories(targetPath.parent_path());

        if (unzOpenCurrentFile(uz) == UNZ_OK) {
            std::ofstream outFile(targetPath, std::ios::binary | std::ios::trunc);
            if (outFile.is_open()) {
                int bytesRead;
                while ((bytesRead = unzReadCurrentFile(uz, buffer.data(), buffer.size())) > 0) {
                    outFile.write(buffer.data(), bytesRead);
                }
                outFile.close();
            }
            unzCloseCurrentFile(uz);
        }

    } while (unzGoToNextFile(uz) == UNZ_OK);

    unzClose(uz);
    close(inFd);
    return true;
}