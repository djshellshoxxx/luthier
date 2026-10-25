// OWNER: WP23
#pragma once
#include "../ReferencePedals/Rt/SpscRing.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <type_traits>
namespace luthier::preview
{
enum class CommandKind : uint8_t { NoteOn, NoteOff, StringOn, StringOff, Panic };
struct Command
{
    uint64_t epoch=0, order=0;
    double fret=0;
    float velocity=0;
    CommandKind kind=CommandKind::NoteOn;
    uint8_t channel=1, note=0, string=0;
};
static_assert(std::is_trivially_copyable_v<Command> && sizeof(Command)==32);
namespace detail
{
// Zero is a terminal panic epoch: never recycle an exhausted incarnation.
constexpr uint64_t nextEpoch(uint64_t epoch) noexcept { return epoch+1; }
}
// Isolated transport only. All post/request methods are OFF AUDIO and serialize
// their producers. drain() has exactly one audio consumer; it never takes that
// mutex, allocates, clears a ring, or reads engine state. Lifecycle is quiescent.
class CommandQueue
{
public:
    static constexpr int laneCapacity=256, maxDrain=2*laneCapacity+2;
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    CommandQueue()=default;
    CommandQueue(const CommandQueue&)=delete;
    CommandQueue& operator=(const CommandQueue&)=delete;

    bool postNoteOn(int note,float velocity,int channel=1)
    {
        if(!validNote(note,channel) || !std::isfinite(velocity)) return false;
        Command command; command.kind=CommandKind::NoteOn; command.note=(uint8_t)note;
        command.channel=(uint8_t)channel; command.velocity=std::clamp(velocity,.05f,1.f);
        const std::lock_guard<std::mutex> producer(nonRealtimeProducerMutex);
        return post(command,false);
    }
    bool postNoteOff(int note,int channel=1)
    {
        if(!validNote(note,channel)) return false;
        Command command; command.kind=CommandKind::NoteOff; command.note=(uint8_t)note; command.channel=(uint8_t)channel;
        const std::lock_guard<std::mutex> producer(nonRealtimeProducerMutex);
        return post(command,true);
    }
    bool postStringOn(int string,double fret,float velocity)
    {
        if(string<0 || string>=16 || !std::isfinite(fret) || fret<0 || !std::isfinite(velocity)) return false;
        Command command; command.kind=CommandKind::StringOn; command.string=(uint8_t)string;
        command.fret=fret; command.velocity=std::clamp(velocity,.05f,1.f);
        const std::lock_guard<std::mutex> producer(nonRealtimeProducerMutex);
        return post(command,false);
    }
    bool postStringOff(int string)
    {
        if(string<0 || string>=16) return false;
        Command command; command.kind=CommandKind::StringOff; command.string=(uint8_t)string;
        const std::lock_guard<std::mutex> producer(nonRealtimeProducerMutex);
        return post(command,true);
    }
    // Expansion is off audio, at most128 commands. As before, invalid individual
    // notes are ignored. Overflow may accept part of a chord; return its count.
    int postChord(const int* notes,int count,float velocity,bool release=false,int channel=1)
    {
        if(!notes || count<0 || count>128 || channel<1 || channel>16 || (!release && !std::isfinite(velocity))) return 0;
        const std::lock_guard<std::mutex> producer(nonRealtimeProducerMutex);
        int accepted=0;
        for(int i=0;i<count;++i)
        {
            if(!validNote(notes[i],channel)) continue;
            Command command; command.kind=release?CommandKind::NoteOff:CommandKind::NoteOn;
            command.note=(uint8_t)notes[i]; command.channel=(uint8_t)channel;
            command.velocity=release?0.f:std::clamp(velocity,.05f,1.f);
            accepted += post(command,release)?1:0;
        }
        return accepted;
    }
    void requestPanic()
    {
        const std::lock_guard<std::mutex> producer(nonRealtimeProducerMutex);
        advanceEpoch();
    }
    int drain(Command* output,int capacity) noexcept
    {
        if(!output || capacity<=0) return 0;
        const int limit=std::min(capacity,maxDrain); int count=0;
        // Fixed work bound, including old-epoch discard and concurrent resets.
        for(int work=0;work<maxDrain && count<limit;++work)
        {
            const auto epoch=publishedEpoch.load(std::memory_order_acquire);
            if(epoch!=consumerEpoch)
            {
                Command panic; panic.kind=CommandKind::Panic; panic.epoch=epoch;
                output[count++]=panic; consumerEpoch=epoch; continue;
            }
            if(!hasOn) hasOn=noteOns.pop(onHead);
            if(!hasRelease) hasRelease=releases.pop(releaseHead);
            // Seeing a later command in one lane establishes publication of
            // earlier commands in the other lane. Refresh a previously empty
            // lane after that acquire before choosing the smallest order.
            if(hasRelease && !hasOn) hasOn=noteOns.pop(onHead);
            if(hasOn && !hasRelease) hasRelease=releases.pop(releaseHead);
            // A just-acquired head can belong to a newer panic epoch. Refresh
            // the epoch first; it must not be discarded as an old command.
            if(publishedEpoch.load(std::memory_order_acquire)!=epoch) continue;
            bool stale=false;
            if(hasOn && onHead.epoch!=epoch) { hasOn=false; stale=true; }
            if(hasRelease && releaseHead.epoch!=epoch) { hasRelease=false; stale=true; }
            if(stale) continue;
            if(publishedEpoch.load(std::memory_order_acquire)!=epoch) continue;
            if(!hasOn && !hasRelease) break;
            // Preserve cross-lane order. Taking every release first could put
            // note-off before its earlier note-on, leaving a stuck preview.
            if(hasOn && (!hasRelease || onHead.order<releaseHead.order))
            { output[count++]=onHead; hasOn=false; }
            else { output[count++]=releaseHead; hasRelease=false; }
        }
        return count;
    }
    uint32_t droppedCount() const noexcept { return noteOns.dropped()+releases.dropped(); }
private:
    static bool validNote(int note,int channel) noexcept
    { return note>=0 && note<128 && channel>=1 && channel<=16; }
    void advanceEpoch() noexcept // serialized off-audio producer only
    {
        if(producerEpoch==0) return;
        producerEpoch=detail::nextEpoch(producerEpoch); producerOrder=0;
        publishedEpoch.store(producerEpoch,std::memory_order_release);
    }
    bool post(Command command,bool release) noexcept // producer guard held
    {
        if(producerEpoch==0) return false;
        if(producerOrder==std::numeric_limits<uint64_t>::max()) advanceEpoch();
        if(producerEpoch==0) return false;
        command.epoch=producerEpoch; command.order=++producerOrder;
        if(release)
        {
            if(releases.push(command)) return true;
            advanceEpoch(); // out-of-band release-all survives even a full ring
            return false;
        }
        return noteOns.push(command);
    }
    std::mutex nonRealtimeProducerMutex;
    uint64_t producerEpoch=1, producerOrder=0; // serialized producers only
    std::atomic<uint64_t> publishedEpoch{1};
    reference::SpscRing<Command,laneCapacity> noteOns, releases;
    uint64_t consumerEpoch=1;
    Command onHead, releaseHead;
    bool hasOn=false, hasRelease=false; // audio consumer only
};
}
