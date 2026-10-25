// OWNER: WP23
#pragma once
#include "PreviewCommandQueue.h"
#include "BoundedMidi.h"
#include <array>
#include <bitset>
namespace luthier::preview
{
// Processor adapter. Producers touch only commands; the remaining state has one
// audio owner (or quiescent prepare/release). Legacy fret conversion/previewLock
// remain outside this adapter; this is not a tuning-publication or no-lock fix.
class MidiState
{
public:
    bool postOn(int channel,int note,float velocity) { return commands.postNoteOn(note,velocity,channel); }
    bool postOff(int channel,int note) { return commands.postNoteOff(note,channel); }
    void invalidate() { commands.requestPanic(); } // control/lifecycle only
    uint32_t dropped() const noexcept { return commands.droppedCount(); }
    void resetAudioState() noexcept
    {
        held={}; pending={}; beforeHeld={}; beforePending={}; requestedOff={}; fresh={};
    }
    void beginSlice(juce::MidiBuffer& destination,bool renderable=true) noexcept
    {
        beforeHeld=held; beforePending=pending; requestedOff={}; fresh={};
        if(renderable) flushPending(destination);
    }
    int mergeQueued(juce::MidiBuffer& destination) noexcept
    {
        const int count=commands.drain(batch.data(),(int)batch.size()); int droppedAttacks=0;
        for(int i=0;i<count;++i)
        {
            const auto& command=batch[(size_t)i];
            switch(command.kind)
            {
                case CommandKind::NoteOn:
                    droppedAttacks+=appendOn(destination,command.channel,command.note,command.velocity); break;
                case CommandKind::NoteOff:
                    requestOff(destination,command.channel,command.note,0); break;
                case CommandKind::Panic:
                    for(size_t channel=0;channel<held.size();++channel)
                    {
                        requestedOff[channel]|=held[channel];
                        pending[channel]|=held[channel];
                    }
                    flushPending(destination); break;
                default:
                    // This adapter accepts resolved MIDI only. String/fret
                    // conversion remains the legacy producer's responsibility.
                    jassertfalse; ++droppedAttacks; break;
            }
        }
        return droppedAttacks;
    }
    // The audition player already runs on audio. Preserve original successful
    // bytes/offsets, while remembering a failed release for a later empty slice.
    int appendAudition(juce::MidiBuffer& destination,const juce::MidiMessage& message,int offset) noexcept
    {
        const int channel=message.getChannel();
        const int note=message.isNoteOnOrOff()?message.getNoteNumber():-1;
        if(message.isNoteOff() && valid(channel,note))
        {
            const size_t c=(size_t)(channel-1),n=(size_t)note;
            requestedOff[c].set(n); pending[c].set(n);
            releaseVelocity[c][n]=message.getRawData()[2];
            if(BoundedMidi::add(destination,message.getRawData(),message.getRawDataSize(),offset))
                acceptedOff(c,n);
            return 0;
        }
        if(message.isNoteOn() && valid(channel,note))
        {
            const size_t c=(size_t)(channel-1),n=(size_t)note;
            if(pending[c].test(n) && !requestOff(destination,channel,note,releaseVelocity[c][n])) return 1;
            if(!BoundedMidi::add(destination,message.getRawData(),message.getRawDataSize(),offset)) return 1;
            held[c].set(n); fresh[c].set(n); return 0;
        }
        return BoundedMidi::add(destination,message.getRawData(),message.getRawDataSize(),offset)?0:1;
    }
    // The bridge excluded this renderer: inserted releases were not consumed.
    // Retain obligations for previously accepted addresses; do not invent held
    // voices for this slice's attacks. This preserves the existing attack-drop
    // behavior during renderer exclusion and fixes losing old note releases.
    void rendererExcluded() noexcept
    {
        held=beforeHeld;
        for(size_t c=0;c<pending.size();++c)
            pending[c]=beforePending[c]|(requestedOff[c]&beforeHeld[c]);
        fresh={};
    }
    // Called only after the actual engine panic. Current-slice note-ons remain
    // in its input and will play after that reset; older obligations are gone.
    void enginePanicked() noexcept
    {
        for(size_t c=0;c<held.size();++c)
        { held[c]&=fresh[c]; pending[c]&=fresh[c]; }
    }
private:
    using Notes=std::array<std::bitset<128>,16>;
    static bool valid(int channel,int note) noexcept { return channel>=1 && channel<=16 && note>=0 && note<128; }
    void acceptedOff(size_t channel,size_t note) noexcept
    { held[channel].reset(note); pending[channel].reset(note); fresh[channel].reset(note); }
    bool requestOff(juce::MidiBuffer& destination,int channel,int note,uint8_t velocity) noexcept
    {
        if(!valid(channel,note)) return true;
        const size_t c=(size_t)(channel-1),n=(size_t)note;
        requestedOff[c].set(n); pending[c].set(n); releaseVelocity[c][n]=velocity;
        const uint8_t data[]{(uint8_t)(0x80|c),(uint8_t)note,velocity};
        if(!BoundedMidi::add(destination,data,3,0)) return false;
        acceptedOff(c,n); return true;
    }
    int appendOn(juce::MidiBuffer& destination,int channel,int note,float velocity) noexcept
    {
        if(!valid(channel,note)) return 1;
        const size_t c=(size_t)(channel-1),n=(size_t)note;
        if(pending[c].test(n) && !requestOff(destination,channel,note,releaseVelocity[c][n])) return 1;
        const auto message=juce::MidiMessage::noteOn(channel,note,velocity);
        if(!BoundedMidi::add(destination,message.getRawData(),message.getRawDataSize(),0)) return 1;
        held[c].set(n); fresh[c].set(n); return 0;
    }
    void flushPending(juce::MidiBuffer& destination) noexcept
    {
        for(size_t c=0;c<pending.size();++c)
            if(pending[c].any())
                for(size_t note=0;note<128;++note)
                    if(pending[c].test(note)) requestOff(destination,(int)c+1,(int)note,releaseVelocity[c][note]);
    }
    CommandQueue commands;
    std::array<Command,CommandQueue::maxDrain> batch;
    Notes held{},pending{},beforeHeld{},beforePending{},requestedOff{},fresh{};
    std::array<std::array<uint8_t,128>,16> releaseVelocity{};
};
}
