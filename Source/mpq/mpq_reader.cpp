#include "mpq/mpq_reader.hpp"

#include <cerrno>
#include <mpqfs/mpqfs.h>

namespace devilution {

std::optional<MpqArchive> MpqArchive::Open(const char *path, int32_t &error)
{
	errno = 0;
	auto *archive = mpqfs_open(path);
	if (archive == nullptr) {
		error = errno == ENOENT ? 0 : -1;
		return std::nullopt;
	}
	error = 0;
	return MpqArchive { path, archive };
}

std::optional<MpqArchive> MpqArchive::Clone(int32_t &error)
{
	auto *copy = mpqfs_clone(archive_);
	error = copy == nullptr ? -1 : 0;
	if (copy == nullptr)
		return std::nullopt;
	return MpqArchive { path_, copy };
}

const char *MpqArchive::ErrorMessage(int32_t /*errorCode*/)
{
	const char *message = mpqfs_last_error();
	return message != nullptr ? message : "MPQ read failed";
}

MpqArchive &MpqArchive::operator=(MpqArchive &&other) noexcept
{
	if (this != &other) {
		mpqfs_close(archive_);
		path_ = std::move(other.path_);
		archive_ = other.archive_;
		other.archive_ = nullptr;
	}
	return *this;
}

MpqArchive::~MpqArchive()
{
	mpqfs_close(archive_);
}

bool MpqArchive::GetFileNumber(const char *filename, uint32_t &fileNumber)
{
	fileNumber = mpqfs_find_hash(archive_, filename);
	return fileNumber != UINT32_MAX;
}

std::unique_ptr<byte[]> MpqArchive::ReadFile(const char *filename, std::size_t &fileSize, int32_t &error)
{
	error = -1;
	fileSize = 0;
	if (!HasFile(filename))
		return nullptr;
	const std::size_t size = mpqfs_file_size(archive_, filename);
	auto result = std::make_unique<byte[]>(size == 0 ? 1 : size);
	if (size != 0 && mpqfs_read_file_into(archive_, filename, result.get(), size) != size)
		return nullptr;
	fileSize = size;
	error = 0;
	return result;
}

std::size_t MpqArchive::GetUnpackedFileSize(uint32_t fileNumber, int32_t &error)
{
	error = mpqfs_has_file_hash(archive_, fileNumber) ? 0 : -1;
	return error == 0 ? mpqfs_file_size_from_hash(archive_, fileNumber) : 0;
}

bool MpqArchive::HasFile(const char *filename) const
{
	return mpqfs_has_file(archive_, filename);
}

} // namespace devilution
