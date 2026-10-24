// OWNER: WP4
#include "AssetStore.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
namespace luthier::reference
{
struct AssetStore::Impl
{
    juce::File directory;
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::function<void()>> jobs;
    bool stopping = false;
    std::thread worker;
    explicit Impl(juce::File dir) : directory(std::move(dir)), worker([this]
    {
        for (;;)
        {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock,[this]{return stopping || ! jobs.empty();});
                if (jobs.empty() && stopping) return;
                job = std::move(jobs.front()); jobs.pop_front();
            }
            // A client callback must not terminate the worker or lose later jobs.
            try { job(); } catch (...) {}
        }
    }) {}
    void enqueue(std::function<void()> job)
    {
        { const std::lock_guard<std::mutex> lock(mutex); jobs.push_back(std::move(job)); }
        ready.notify_one();
    }
    ~Impl()
    {
        { const std::lock_guard<std::mutex> lock(mutex); stopping = true; }
        ready.notify_one(); worker.join();
    }
};
AssetStore::AssetStore(juce::File directory) : impl(std::make_unique<Impl>(std::move(directory))) {}
AssetStore::~AssetStore() = default;
AssetHash AssetStore::hashBlob(const uint8_t* data, size_t size)
{
    AssetHash result;
    if (size != 0 && data == nullptr) return result;
    auto raw = juce::SHA256(data,size).getRawData();
    std::copy_n(static_cast<const uint8_t*>(raw.getData()),result.bytes.size(),result.bytes.begin());
    return result;
}
juce::String AssetStore::reference(const AssetHash& hash) { return "sha256:" + hash.toHex(); }
bool AssetStore::putBlob(const AssetHash& hash, const uint8_t* data, size_t size)
{
    if ((size != 0 && data == nullptr) || hash != hashBlob(data,size)) return false;
    if (! impl->directory.createDirectory()) return false;
    const auto target = impl->directory.getChildFile(hash.toHex() + ".blob");
    // Same-directory temporary + OS atomic replacement. Never delete the old
    // target first (File::moveFileTo may fall back to delete+copy on failure).
    juce::TemporaryFile temporary(target);
    {
        juce::FileOutputStream stream(temporary.getFile());
        if (! stream.openedOk() || (size != 0 && ! stream.write(data,size))) return false;
        stream.flush();
        if (stream.getStatus().failed()) return false;
    }
    return temporary.overwriteTargetFileWithTemporary();
}
juce::File AssetStore::getBlobPath(const AssetHash& hash) const
{
    const auto file = impl->directory.getChildFile(hash.toHex() + ".blob");
    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return {};
    const auto raw=juce::SHA256(stream).getRawData();
    AssetHash actual;std::copy_n(static_cast<const uint8_t*>(raw.getData()),actual.bytes.size(),actual.bytes.begin());
    if (stream.getStatus().failed() || actual!=hash) return {};
    return file;
}
bool AssetStore::hasBlob(const AssetHash& hash) const { return getBlobPath(hash) != juce::File(); }
void AssetStore::putBlobAsync(const AssetHash& hash, AssetBlob blob, std::function<void(bool)> done)
{
    impl->enqueue([this,hash,blob=std::move(blob),done=std::move(done)]
    { const bool ok = blob && putBlob(hash,blob->data(),blob->size()); if (done) done(ok); });
}
void AssetStore::getBlobAsync(const AssetHash& hash, std::function<void(AssetBlob)> done)
{
    impl->enqueue([this,hash,done=std::move(done)]
    {
        const auto file = impl->directory.getChildFile(hash.toHex() + ".blob");
        juce::MemoryBlock data; AssetBlob blob;
        if (file.loadFileAsData(data) && hashBlob(static_cast<const uint8_t*>(data.getData()),data.getSize()) == hash)
        {
            auto bytes = std::make_shared<std::vector<uint8_t>>(data.getSize());
            if (data.getSize()) std::copy_n(static_cast<const uint8_t*>(data.getData()),data.getSize(),bytes->data());
            blob = bytes;
        }
        if (done) done(std::move(blob));
    });
}
std::vector<AssetHash> AssetStore::findMissing(const std::vector<AssetHash>& references) const
{
    std::vector<AssetHash> missing;
    for (const auto& hash : references)
        if (! hasBlob(hash) && std::find(missing.begin(),missing.end(),hash) == missing.end()) missing.push_back(hash);
    return missing;
}
}
