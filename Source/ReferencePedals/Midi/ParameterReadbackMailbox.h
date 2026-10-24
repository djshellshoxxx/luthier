// OWNER: WP6
#pragma once
#include "MidiTypes.h"
#include <array>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cmath>
#include <cstring>
namespace luthier::reference::midi
{
struct ReadbackIdentity
{
    std::array<uint8_t,16> instance {};
    uint64_t modelGeneration=0, restoreGeneration=0;
    bool operator==(const ReadbackIdentity& b) const noexcept
    { return instance==b.instance && modelGeneration==b.modelGeneration && restoreGeneration==b.restoreGeneration; }
};
struct ReadbackExpectation
{
    ReadbackIdentity identity;
    uint64_t modelIntent=0, hostIntent=0;
    bool operator==(const ReadbackExpectation& b) const noexcept
    { return identity==b.identity && modelIntent==b.modelIntent && hostIntent==b.hostIntent; }
};
struct ParameterReadback
{
    ReadbackExpectation expected;
    uint16_t target=0;
    float normalised=0;
};
enum class ReadbackPersistence { Control, Action };
enum class ReadbackStatus { Empty, Ready, Stale, Busy, Invalid };
namespace detail
{
constexpr uint64_t nextReadbackSequence(uint64_t current) noexcept
{ const auto next=(current&~uint64_t(1))+2; return next==0?2:next; }
}
// One audio producer and one externally serialized control consumer. The model
// generation is an exact full-model/revision token issued off audio, never a hash
// or a truncated model id. Restore generation invalidates old source state.
// Matching a snapshot DOES NOT authorize persistence: caller must conditionally
// check+commit under model mutation protection and linearize host serial changes.
class ParameterReadbackMailbox
{
public:
    static constexpr size_t capacity=kTargetBoardBase+16; // E36's explicit board reserve
    static_assert(sizeof(float)==sizeof(uint32_t),"Readback requires 32-bit normalized values");
    static_assert(std::atomic<uint64_t>::is_always_lock_free,"Readback must not hide audio-thread locks");
    ParameterReadbackMailbox() noexcept = default;
    ParameterReadbackMailbox(const ParameterReadbackMailbox&)=delete;
    ParameterReadbackMailbox& operator=(const ParameterReadbackMailbox&)=delete;
    bool publish(uint16_t target,const ReadbackExpectation& expected,float value,ReadbackPersistence persistence) noexcept
    {
        if(target>=capacity || persistence==ReadbackPersistence::Action || !valid(expected)
            || !std::isfinite(value) || value<0.f || value>1.f) return false;
        std::array<uint64_t,wordsPerCell> words{};
        std::memcpy(words.data(),expected.identity.instance.data(),expected.identity.instance.size());
        words[2]=expected.identity.modelGeneration; words[3]=expected.identity.restoreGeneration;
        words[4]=expected.modelIntent; words[5]=expected.hostIntent;
        uint32_t bits=0; std::memcpy(&bits,&value,sizeof(value)); words[6]=bits;
        auto& cell=cells[target]; const auto next=detail::nextReadbackSequence(cell.sequence.load(std::memory_order_seq_cst));
        cell.sequence.store(next-1,std::memory_order_seq_cst);
        for(size_t i=0;i<wordsPerCell;++i) cell.words[i].store(words[i],std::memory_order_seq_cst);
        cell.sequence.store(next,std::memory_order_seq_cst); return true;
    }
    ReadbackStatus readMatching(uint16_t target,const ReadbackExpectation& current,ParameterReadback& out) noexcept
    {
        if(target>=capacity || !valid(current)) return ReadbackStatus::Invalid;
        const auto& cell=cells[target];
        for(int attempt=0;attempt<2;++attempt)
        {
            const auto sequence=cell.sequence.load(std::memory_order_seq_cst);
            if(sequence==0 || sequence==consumed[target]) return ReadbackStatus::Empty;
            if((sequence&1)!=0) continue;
            std::array<uint64_t,wordsPerCell> words{};
            for(size_t i=0;i<wordsPerCell;++i) words[i]=cell.words[i].load(std::memory_order_seq_cst);
            if(cell.sequence.load(std::memory_order_seq_cst)!=sequence) continue;
            consumed[target]=sequence;
            ParameterReadback record; record.target=target;
            std::memcpy(record.expected.identity.instance.data(),words.data(),record.expected.identity.instance.size());
            record.expected.identity.modelGeneration=words[2]; record.expected.identity.restoreGeneration=words[3];
            record.expected.modelIntent=words[4]; record.expected.hostIntent=words[5];
            const auto bits=static_cast<uint32_t>(words[6]); std::memcpy(&record.normalised,&bits,sizeof(record.normalised));
            if(!(record.expected==current)) return ReadbackStatus::Stale;
            out=record; return ReadbackStatus::Ready;
        }
        return ReadbackStatus::Busy; // latest value remains available for a later read
    }
private:
    static constexpr size_t wordsPerCell=7;
    struct alignas(64) Cell
    {
        std::atomic<uint64_t> sequence{0};
        std::array<std::atomic<uint64_t>,wordsPerCell> words{};
    };
    static bool valid(const ReadbackExpectation& expected) noexcept
    { return expected.identity.modelGeneration!=0 && expected.identity.restoreGeneration!=0; }
    std::array<Cell,capacity> cells{};
    std::array<uint64_t,capacity> consumed{}; // consumer-owned; serialize all control readers
};
}
