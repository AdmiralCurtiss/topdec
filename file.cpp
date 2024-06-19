#ifndef _MSC_VER
#define _FILE_OFFSET_BITS 64
#endif

#include "file.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#ifdef FILE_WRAPPER_WITH_STD_FILESYSTEM
#include <filesystem>
#endif

#include "text.h"

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#undef CreateDirectory
#else
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define INVALID_HANDLE_VALUE nullptr
#define DWORD unsigned int
#endif

namespace HyoutaUtils::IO {
File::File() noexcept : Filehandle(INVALID_HANDLE_VALUE) {}

File::File(std::string_view p, OpenMode mode) noexcept : Filehandle(INVALID_HANDLE_VALUE) {
    Open(p, mode);
}

#ifdef FILE_WRAPPER_WITH_STD_FILESYSTEM
File::File(const std::filesystem::path& p, OpenMode mode) noexcept
  : Filehandle(INVALID_HANDLE_VALUE) {
    Open(p, mode);
}
#endif

#ifdef _MSC_VER
File::File(void* handle) noexcept : Filehandle(handle) {}

File File::FromHandle(void* handle) noexcept {
    return File(handle);
}
#endif

File::File(File&& other) noexcept
  : Filehandle(other.Filehandle)
#ifndef _MSC_VER
  , Path(std::move(other.Path))
#endif
{
    other.Filehandle = INVALID_HANDLE_VALUE;
}

File& File::operator=(File&& other) noexcept {
    Close();
    this->Filehandle = other.Filehandle;
    other.Filehandle = INVALID_HANDLE_VALUE;
#ifndef _MSC_VER
    this->Path = std::move(other.Path);
#endif
    return *this;
}

File::~File() noexcept {
    Close();
}

bool File::Open(std::string_view p, OpenMode mode) noexcept {
    Close();
    switch (mode) {
        case OpenMode::Read: {
#ifdef _MSC_VER
            auto s = HyoutaUtils::TextUtils::Utf8ToWString(p.data(), p.size());
            if (!s) {
                return false;
            }
            Filehandle = CreateFileW(s->c_str(),
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     nullptr,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
#else
            std::string s(p);
            Filehandle = fopen(s.c_str(), "rb");
            if (Filehandle != INVALID_HANDLE_VALUE) {
                Path = std::move(s);
            }
#endif
            return Filehandle != INVALID_HANDLE_VALUE;
        }
        case OpenMode::Write: {
#ifdef _MSC_VER
            auto s = HyoutaUtils::TextUtils::Utf8ToWString(p.data(), p.size());
            if (!s) {
                return false;
            }
            Filehandle = CreateFileW(s->c_str(),
                                     GENERIC_WRITE | DELETE,
                                     0,
                                     nullptr,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
#else
            std::string s(p);
            Filehandle = fopen(s.c_str(), "wb");
            if (Filehandle != INVALID_HANDLE_VALUE) {
                Path = std::move(s);
            }
#endif
            return Filehandle != INVALID_HANDLE_VALUE;
        }
        default: Filehandle = INVALID_HANDLE_VALUE; return false;
    }
}

#ifdef FILE_WRAPPER_WITH_STD_FILESYSTEM
bool File::Open(const std::filesystem::path& p, OpenMode mode) noexcept {
    Close();
    switch (mode) {
        case OpenMode::Read: {
#ifdef _MSC_VER
            Filehandle = CreateFileW(p.c_str(),
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     nullptr,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
#else
            std::string s(p);
            Filehandle = fopen(s.c_str(), "rb");
            if (Filehandle != INVALID_HANDLE_VALUE) {
                Path = std::move(s);
            }
#endif
            return Filehandle != INVALID_HANDLE_VALUE;
        }
        case OpenMode::Write: {
#ifdef _MSC_VER
            Filehandle = CreateFileW(p.c_str(),
                                     GENERIC_WRITE | DELETE,
                                     0,
                                     nullptr,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
#else
            std::string s(p);
            Filehandle = fopen(s.c_str(), "wb");
            if (Filehandle != INVALID_HANDLE_VALUE) {
                Path = std::move(s);
            }
#endif
            return Filehandle != INVALID_HANDLE_VALUE;
        }
        default: Filehandle = INVALID_HANDLE_VALUE; return false;
    }
}
#endif

bool File::IsOpen() const noexcept {
    return Filehandle != INVALID_HANDLE_VALUE;
}

void File::Close() noexcept {
    if (IsOpen()) {
#ifdef _MSC_VER
        CloseHandle(Filehandle);
#else
        fclose((FILE*)Filehandle);
#endif
        Filehandle = INVALID_HANDLE_VALUE;
#ifndef _MSC_VER
        Path.clear();
#endif
    }
}

std::optional<uint64_t> File::GetPosition() noexcept {
    assert(IsOpen());
#ifdef _MSC_VER
    LARGE_INTEGER offset;
    offset.QuadPart = static_cast<LONGLONG>(0);
    LARGE_INTEGER position;
    if (SetFilePointerEx(Filehandle, offset, &position, FILE_CURRENT) != 0) {
        return static_cast<uint64_t>(position.QuadPart);
    }
#else
    off_t offset = ftello((FILE*)Filehandle);
    if (offset >= 0) {
        return static_cast<uint64_t>(offset);
    }
#endif
    return std::nullopt;
}

bool File::SetPosition(uint64_t position) noexcept {
    if (position > static_cast<uint64_t>(INT64_MAX)) {
        return false;
    }
    return SetPosition(static_cast<int64_t>(position), SetPositionMode::Begin);
}

bool File::SetPosition(int64_t position, SetPositionMode mode) noexcept {
    assert(IsOpen());
#ifdef _MSC_VER
    DWORD moveMethod;
    switch (mode) {
        case SetPositionMode::Begin: moveMethod = FILE_BEGIN; break;
        case SetPositionMode::Current: moveMethod = FILE_CURRENT; break;
        case SetPositionMode::End: moveMethod = FILE_END; break;
        default: return false;
    }
    LARGE_INTEGER offset;
    offset.QuadPart = static_cast<LONGLONG>(position);
    if (SetFilePointerEx(Filehandle, offset, nullptr, moveMethod) != 0) {
        return true;
    }
#else
    int origin;
    switch (mode) {
        case SetPositionMode::Begin: origin = SEEK_SET; break;
        case SetPositionMode::Current: origin = SEEK_CUR; break;
        case SetPositionMode::End: origin = SEEK_END; break;
        default: return false;
    }
    int result = fseeko((FILE*)Filehandle, (off_t)position, origin);
    if (result == 0) {
        return true;
    }
#endif
    return false;
}

std::optional<uint64_t> File::GetLength() noexcept {
    assert(IsOpen());
#ifdef _MSC_VER
    LARGE_INTEGER size;
    if (GetFileSizeEx(Filehandle, &size) != 0) {
        return static_cast<uint64_t>(size.QuadPart);
    }
#else
    auto oldPos = GetPosition();
    if (!oldPos) {
        return std::nullopt;
    }
    if (!SetPosition(0, SetPositionMode::End)) {
        return std::nullopt;
    }
    auto length = GetPosition();
    if (!length) {
        return std::nullopt;
    }
    SetPosition(*oldPos, SetPositionMode::Begin);
    return length;
#endif
    return std::nullopt;
}

size_t File::Read(void* data, size_t length) noexcept {
    assert(IsOpen());

    char* buffer = static_cast<char*>(data);
    size_t totalRead = 0;
    size_t rest = length;
    while (rest > 0) {
        DWORD blockSize = rest > 0xffff'0000 ? 0xffff'0000 : static_cast<DWORD>(rest);
        DWORD blockRead = 0;
#ifdef _MSC_VER
        if (ReadFile(Filehandle, buffer, blockSize, &blockRead, nullptr) == 0) {
            return totalRead;
        }
#else
        blockRead = (DWORD)fread(buffer, 1, blockSize, (FILE*)Filehandle);
#endif
        if (blockRead == 0) {
            return totalRead;
        }

        rest -= blockRead;
        totalRead += blockRead;
        buffer += blockRead;
    }
    return totalRead;
}

size_t File::Write(const void* data, size_t length) noexcept {
    assert(IsOpen());

    const char* buffer = static_cast<const char*>(data);
    size_t totalWritten = 0;
    size_t rest = length;
    while (rest > 0) {
        DWORD blockSize = rest > 0xffff'0000 ? 0xffff'0000 : static_cast<DWORD>(rest);
        DWORD blockWritten = 0;
#ifdef _MSC_VER
        if (WriteFile(Filehandle, buffer, blockSize, &blockWritten, nullptr) == 0) {
            return totalWritten;
        }
#else
        blockWritten = (DWORD)fwrite(buffer, 1, blockSize, (FILE*)Filehandle);
#endif
        if (blockWritten == 0) {
            return totalWritten;
        }

        rest -= blockWritten;
        totalWritten += blockWritten;
        buffer += blockWritten;
    }
    return totalWritten;
}

bool File::Delete() noexcept {
    assert(IsOpen());

#ifdef _MSC_VER
    FILE_DISPOSITION_INFO info{};
    info.DeleteFile = TRUE;
    if (SetFileInformationByHandle(Filehandle, FileDispositionInfo, &info, sizeof(info)) != 0) {
        return true;
    }
#else
    int result = unlink(Path.c_str());
    if (result == 0) {
        return true;
    }
#endif

    return false;
}

#ifdef _MSC_VER
bool RenameInternalWindows(void* filehandle, const wchar_t* wstr_data, size_t wstr_len) {
    // This struct has a very odd definition, because its size is dynamic, so we must do something
    // like this...
    if (wstr_len > (32767 + 4)) {
        // not sure what the actual limit is, but this is max path length 32767 + '\\?\' prefix
        return false;
    }

    // note that this looks like it has an off-by-one error, but it doesn't, because
    // sizeof(FILE_RENAME_INFO) includes a single WCHAR which accounts for the nullterminator
    size_t structLength = sizeof(FILE_RENAME_INFO) + (wstr_len * sizeof(wchar_t));
    size_t allocationLength = structLength + alignof(FILE_RENAME_INFO);
    auto buffer = std::make_unique<char[]>(allocationLength);
    if (!buffer) {
        return false;
    }
    void* alignedBuffer = buffer.get();
    if (std::align(alignof(FILE_RENAME_INFO), structLength, alignedBuffer, allocationLength)
        == nullptr) {
        return false;
    }
    FILE_RENAME_INFO* info = reinterpret_cast<FILE_RENAME_INFO*>(alignedBuffer);
    info->ReplaceIfExists = TRUE;
    info->RootDirectory = nullptr;
    info->FileNameLength = static_cast<DWORD>(wstr_len * sizeof(wchar_t));
    std::memcpy(info->FileName, wstr_data, wstr_len * sizeof(wchar_t));
    if (SetFileInformationByHandle(
            filehandle, FileRenameInfo, info, static_cast<DWORD>(allocationLength))
        != 0) {
        return true;
    }
    return false;
}
#endif

bool File::Rename(const std::string_view p) noexcept {
    assert(IsOpen());

#ifdef _MSC_VER
    auto wstr = HyoutaUtils::TextUtils::Utf8ToWString(p.data(), p.size());
    if (!wstr) {
        return false;
    }
    return RenameInternalWindows(Filehandle, wstr->data(), wstr->size());
#else
    std::string newName(p);
    int result = rename(Path.c_str(), newName.c_str());
    return result == 0;
#endif
}

#ifdef FILE_WRAPPER_WITH_STD_FILESYSTEM
bool File::Rename(const std::filesystem::path& p) noexcept {
    assert(IsOpen());

#ifdef _MSC_VER
    const auto& wstr = p.native();
    return RenameInternalWindows(Filehandle, wstr.data(), wstr.size());
#else
    int result = rename(Path.c_str(), p.c_str());
    return result == 0;
#endif
}
#endif

void* File::ReleaseHandle() noexcept {
    void* h = Filehandle;
    Filehandle = INVALID_HANDLE_VALUE;
#ifndef _MSC_VER
    Path.clear();
#endif
    return h;
}

#ifdef _MSC_VER
static bool FileExistsWindows(const wchar_t* path) {
    const auto attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
#else
static bool FileExistsLinux(const char* path) {
    struct stat buf {};
    if (stat(path, &buf) != 0) {
        return false;
    }
    if (S_ISREG(buf.st_mode)) {
        return true;
    }
    return false;
}
#endif

bool FileExists(std::string_view p) noexcept {
#ifdef _MSC_VER
    auto wstr = HyoutaUtils::TextUtils::Utf8ToWString(p.data(), p.size());
    if (!wstr) {
        return false;
    }
    return FileExistsWindows(wstr->data());
#else
    std::string s(p);
    return FileExistsLinux(s.c_str());
#endif
}

#ifdef FILE_WRAPPER_WITH_STD_FILESYSTEM
bool FileExists(const std::filesystem::path& p) noexcept {
#ifdef _MSC_VER
    return FileExistsWindows(p.native().data());
#else
    return FileExistsLinux(p.c_str());
#endif
}
#endif

#ifdef _MSC_VER
static bool DirectoryExistsWindows(const wchar_t* path) {
    const auto attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
#else
static bool DirectoryExistsLinux(const char* path) {
    struct stat buf {};
    if (stat(path, &buf) != 0) {
        return false;
    }
    if (S_ISDIR(buf.st_mode)) {
        return true;
    }
    return false;
}
#endif

bool DirectoryExists(std::string_view p) noexcept {
#ifdef _MSC_VER
    auto wstr = HyoutaUtils::TextUtils::Utf8ToWString(p.data(), p.size());
    if (!wstr) {
        return false;
    }
    return DirectoryExistsWindows(wstr->data());
#else
    std::string s(p);
    return DirectoryExistsLinux(s.c_str());
#endif
}

#ifdef FILE_WRAPPER_WITH_STD_FILESYSTEM
bool DirectoryExists(const std::filesystem::path& p) noexcept {
#ifdef _MSC_VER
    return DirectoryExistsWindows(p.native().data());
#else
    return DirectoryExistsLinux(p.c_str());
#endif
}
#endif

#ifdef _MSC_VER
static bool CreateDirectoryWindows(const wchar_t* path) {
    return CreateDirectoryW(path, nullptr);
}
#else
static bool CreateDirectoryLinux(const char* path) {
    if (DirectoryExistsLinux(path)) {
        return true;
    }
    int result = mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
    if (result == 0) {
        return true;
    }
    return false;
}
#endif

bool CreateDirectory(std::string_view p) noexcept {
#ifdef _MSC_VER
    auto wstr = HyoutaUtils::TextUtils::Utf8ToWString(p.data(), p.size());
    if (!wstr) {
        return false;
    }
    return CreateDirectoryWindows(wstr->data());
#else
    std::string s(p);
    return CreateDirectoryLinux(s.c_str());
#endif
}

#ifdef FILE_WRAPPER_WITH_STD_FILESYSTEM
bool CreateDirectory(const std::filesystem::path& p) noexcept {
#ifdef _MSC_VER
    return CreateDirectoryWindows(p.native().data());
#else
    return CreateDirectoryLinux(p.c_str());
#endif
}
#endif

} // namespace HyoutaUtils::IO
