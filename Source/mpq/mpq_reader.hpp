#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "utils/stdcompat/cstddef.hpp"
#include "utils/stdcompat/optional.hpp"

struct mpqfs_archive;

namespace devilution {

// MPQ reader adapted for the existing DevilTouch 1.5.5 consumers.
// Uses the MIT mpqfs backend selected by upstream DevilutionX PR #8482.
class MpqArchive {
public:
	static std::optional<MpqArchive> Open(const char *path, int32_t &error);
	std::optional<MpqArchive> Clone(int32_t &error);
	static const char *ErrorMessage(int32_t errorCode);

	MpqArchive(MpqArchive &&other) noexcept
	    : path_(std::move(other.path_)), archive_(other.archive_)
	{
		other.archive_ = nullptr;
	}
	MpqArchive &operator=(MpqArchive &&other) noexcept;
	~MpqArchive();

	bool GetFileNumber(const char *filename, uint32_t &fileNumber);
	std::unique_ptr<byte[]> ReadFile(const char *filename, std::size_t &fileSize, int32_t &error);
	std::size_t GetUnpackedFileSize(uint32_t fileNumber, int32_t &error);
	bool HasFile(const char *filename) const;
	mpqfs_archive *handle() const { return archive_; }

private:
	MpqArchive(std::string path, mpqfs_archive *archive)
	    : path_(std::move(path)), archive_(archive) {}

	std::string path_;
	mpqfs_archive *archive_;
};

} // namespace devilution
