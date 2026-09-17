#include "mpq/mpq_sdl_rwops.hpp"

#include <limits>
#include <memory>
#include <mpqfs/mpqfs.h>

namespace devilution {
namespace {
struct Data {
	std::optional<MpqArchive> ownedArchive;
	mpqfs_stream_t *stream = nullptr;
	~Data() { mpqfs_stream_close(stream); }
};
Data *GetData(SDL_RWops *context)
{
	return static_cast<Data *>(context->hidden.unknown.data1);
}
#ifndef USE_SDL1
using OffsetType = Sint64;
using SizeType = size_t;
static Sint64 Size(SDL_RWops *context)
{
	return mpqfs_stream_size(GetData(context)->stream);
}
#else
using OffsetType = int;
using SizeType = int;
#endif
static OffsetType Seek(SDL_RWops *context, OffsetType offset, int whence)
{
	const auto result = mpqfs_stream_seek(GetData(context)->stream, offset, whence);
	if (result < 0)
		SDL_SetError("MPQ seek failed: %s", MpqArchive::ErrorMessage(-1));
	return static_cast<OffsetType>(result);
}
static SizeType Read(SDL_RWops *context, void *ptr, SizeType size, SizeType count)
{
	if (size == 0 || count == 0)
		return 0;
	if (static_cast<size_t>(count) > std::numeric_limits<size_t>::max() / static_cast<size_t>(size)) {
		SDL_SetError("MPQ read size overflow");
		return 0;
	}
	const size_t bytes = mpqfs_stream_read(GetData(context)->stream, ptr, static_cast<size_t>(size) * count);
	if (bytes == static_cast<size_t>(-1)) {
		SDL_SetError("MPQ read failed: %s", MpqArchive::ErrorMessage(-1));
		return 0;
	}
	return static_cast<SizeType>(bytes / size);
}
static SizeType Write(SDL_RWops *, const void *, SizeType, SizeType)
{
	SDL_SetError("MPQ stream is read-only");
	return 0;
}
static int Close(SDL_RWops *context)
{
	delete GetData(context);
	SDL_FreeRW(context);
	return 0;
}
} // namespace

SDL_RWops *SDL_RWops_FromMpqFile(MpqArchive &mpqArchive, uint32_t /*fileNumber*/, const char *filename, bool threadsafe)
{
	auto data = std::make_unique<Data>();
	MpqArchive *archive = &mpqArchive;
	if (threadsafe) {
		int32_t error = 0;
		data->ownedArchive = mpqArchive.Clone(error);
		if (!data->ownedArchive) {
			SDL_SetError("MPQ clone failed: %s", MpqArchive::ErrorMessage(error));
			return nullptr;
		}
		archive = &*data->ownedArchive;
	}
	data->stream = mpqfs_stream_open(archive->handle(), filename);
	if (data->stream == nullptr) {
		SDL_SetError("MPQ stream open failed: %s", MpqArchive::ErrorMessage(-1));
		return nullptr;
	}
	auto *result = SDL_AllocRW();
	if (result == nullptr)
		return nullptr;
#ifndef USE_SDL1
	result->size = &Size;
	result->type = SDL_RWOPS_UNKNOWN;
#else
	result->type = 0;
#endif
	result->seek = &Seek;
	result->read = &Read;
	result->write = &Write;
	result->close = &Close;
	result->hidden.unknown.data1 = data.release();
	return result;
}
} // namespace devilution
