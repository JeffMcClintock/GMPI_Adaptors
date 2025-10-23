#pragma once
#include <optional>
#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/base/funknown.h"

// ivstmidi2extension.h

namespace vst3_ext_midi
{
    namespace sb = Steinberg;
    namespace sbv = sb::Vst;

    /** Extended IAudioProcessor interface for a component: IProcessMidiProtocol

    To receive MIDI events, it is now required to implement this interface and
    return the desired constant which your audio effect needs.

    The host asks for this information once between initialize and setActive. It cannot be changed afterwards.

    */
    class IProcessMidiProtocol : public Steinberg::FUnknown
    {
    public:
        enum Flags
        {
            kMIDIProtocol_1_0 = 1 << 0,
            kMIDIProtocol_2_0 = 1 << 1,
        };
        virtual Steinberg::uint32 PLUGIN_API getProcessMidiProtocol() = 0;
        //------------------------------------------------------------------------
        static const Steinberg::FUID iid;
    };

    DECLARE_CLASS_IID(IProcessMidiProtocol, 0x61C7B395, 0xC49643B4, 0x93DCEB01, 0x603E29EA)


    /// An event representing a Universal MIDI Packet.
    /// This is intended to be passed between host and client using the IEventQueue
    /// mechanism.
    struct UMPEvent
    {
        /// A stand-in EventType value for this event.
        /// Event::type should be set to this value to indicate that the payload is
        /// a UMPEvent.
        static constexpr auto kType = 0x100;

        /// Words of a Universal MIDI Packet.
        /// A UMPEvent will only ever contain a single packet, which means that
        /// the words at indices 2 and 3 may not be used for some events.
        /// Check the first word to find the real length of the packet, and avoid
        /// reading or writing to words that are not contained in the packet.
        sb::uint32 words[4];

        /// If the event is a UMPEvent, returns that event.
        /// Otherwise, returns nullopt.
        static std::optional<UMPEvent> fromEvent(const sbv::Event& e)
        {
            if (e.type != kType)
                return {};

            UMPEvent result;
            memcpy(&result, &e.noteOn, sizeof(UMPEvent));
            return result;
        }

        /// Returns an Event with this UMPEvent as the payload.
        sbv::Event toEvent(sb::int32 busIndex,
            sb::int32 sampleOffset,
            sbv::TQuarterNotes ppqPos,
            sb::uint16 flags) const
        {
            sbv::Event result{ busIndex, sampleOffset, ppqPos, flags, kType, {} };
            memcpy(&result.noteOn, this, sizeof(UMPEvent));
            return result;
        }
    };

    // UMPEvent will re-use the storage of NoteOnEvent.
    static_assert (sizeof(UMPEvent) <= sizeof(sbv::NoteOnEvent));
    static_assert (alignof (UMPEvent) <= alignof (sbv::NoteOnEvent));

} // namespace vst3_ext_midi