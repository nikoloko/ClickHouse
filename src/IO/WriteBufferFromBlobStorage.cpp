#if !defined(ARCADIA_BUILD)
#include <Common/config.h>
#endif

#if USE_AZURE_BLOB_STORAGE

#include <IO/WriteBufferFromBlobStorage.h>
#include <Disks/RemoteDisksCommon.h>
#include <Common/getRandomASCIIString.h>


namespace DB
{

WriteBufferFromBlobStorage::WriteBufferFromBlobStorage(
    std::shared_ptr<Azure::Storage::Blobs::BlobContainerClient> blob_container_client_,
    const String & blob_path_,
    size_t max_single_part_upload_size_,
    size_t buf_size_) :
    BufferWithOwnMemory<WriteBuffer>(buf_size_, nullptr, 0),
    blob_container_client(blob_container_client_),
    max_single_part_upload_size(max_single_part_upload_size_),
    blob_path(blob_path_) {}


WriteBufferFromBlobStorage::~WriteBufferFromBlobStorage()
{
    finalize();
}


void WriteBufferFromBlobStorage::nextImpl()
{
    if (!offset())
        return;

    auto * buffer_begin = working_buffer.begin();
    auto len = offset();
    auto block_blob_client = blob_container_client->GetBlockBlobClient(blob_path);

    size_t read = 0;
    while (read < len)
    {
        auto part_len = std::min(len - read, max_single_part_upload_size);

        auto block_id = getRandomASCIIString(64);
        block_ids.push_back(block_id);

        Azure::Core::IO::MemoryBodyStream tmp_buffer(reinterpret_cast<uint8_t *>(buffer_begin + read), part_len);
        block_blob_client.StageBlock(block_id, tmp_buffer);

        read += part_len;
    }
}


void WriteBufferFromBlobStorage::finalizeImpl()
{
    next();

    auto block_blob_client = blob_container_client->GetBlockBlobClient(blob_path);
    block_blob_client.CommitBlockList(block_ids);
    finalized = true;
}

}

#endif
