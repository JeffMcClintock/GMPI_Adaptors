#include "./ProcessorWrapper.h"
#include <algorithm>
#include "Extensions/PinCount.h"

#define VST3_USE_MIDI_EXTENSION 0

#if	VST3_USE_MIDI_EXTENSION
#include "ivstmidi2extension.h"
DEF_CLASS_IID(vst3_ext_midi::IProcessMidiProtocol)
#endif

using namespace std;
using namespace gmpi;
using namespace Steinberg;
using namespace Steinberg::Vst;

ProcessorWrapper::ProcessorWrapper() :
	currentVstSubProcess(&ProcessorWrapper::subProcessNotLoaded)
	,midiConverter_1_0(nullptr)
	,midiConverter(
		// provide a lambda to accept converted MIDI 2.0 messages
		[this](const midi::message_view& msg, int offset)
		{
			onMidi2Message(msg, offset);
		}
	) 
{
	memset(&vstTime_, 0, sizeof(vstTime_));
	vstTime_.state =
		ProcessContext::kTempoValid |
		ProcessContext::kTimeSigValid |
		ProcessContext::kBarPositionValid |
		ProcessContext::kContTimeValid;

	// reasonable defaults for now.
	vstTime_.tempo = 100;
	vstTime_.timeSigDenominator = 4;
	vstTime_.timeSigNumerator = 4;

	processData.processContext = &vstTime_;
}

ProcessorWrapper::~ProcessorWrapper()
{
	if (component_)
	{
        component_->setActive(false);
	}

	if (vstEffect_)
	{
		vstEffect_->release();
	}
}

void ProcessorWrapper::process(int32_t count, const gmpi::api::Event* events)
{
	(this->*(currentVstSubProcess))(count, events);
}

struct PinSink : public synthedit::IProcessorPinsCallback
{
	std::function<void(gmpi::PinDirection, gmpi::PinDatatype)> callback;

	PinSink(std::function<void(gmpi::PinDirection, gmpi::PinDatatype)> cb) : callback(cb) {}

	// IProcessorPinsCallback
	gmpi::ReturnCode onPin(PinDirection direction, PinDatatype datatype) override
	{
		callback(direction, datatype);
		return gmpi::ReturnCode::Ok;
	}

	GMPI_QUERYINTERFACE_METHOD(synthedit::IProcessorPinsCallback);
	GMPI_REFCOUNT_NO_DELETE
};

gmpi::ReturnCode ProcessorWrapper::open(gmpi::api::IUnknown* phost)
{
	Processor::open(phost);

	vstTime_.sampleRate = (double)host->getSampleRate();
	processData.inputEvents = &vstEventList;
	processData.inputParameterChanges = &parameterEvents;

	// get pins info
	std::vector<std::pair<gmpi::PinDirection, gmpi::PinDatatype>> pinlist;

	PinSink sink(
		[&pinlist](gmpi::PinDirection dir, gmpi::PinDatatype dt)
		{
			pinlist.push_back({ dir , dt });
		}
	);

	auto synthEditExtension = host.as<synthedit::IPinCount>();
	synthEditExtension->listPins(&sink);

	// Setup IO.
	if(pinlist.empty())
		return gmpi::ReturnCode::Fail;

	for (int idx = 0; idx < pinlist.size() ; idx++)
	{
		auto& [direction, datatype] = pinlist[idx];

		if(direction == gmpi::PinDirection::In)
		{
			switch(datatype)
			{
			case gmpi::PinDatatype::Midi:
				parameterAccessPinIndex = idx;
				break;

			case gmpi::PinDatatype::Audio:
				AudioIns.push_back(std::make_unique<AudioInPin>());
				init(*(AudioIns.back()));
				break;

			default:
				break;
			}
		}
		else
		{
			AudioOuts.push_back(std::make_unique<AudioOutPin>());
			init(*(AudioOuts.back()));
		}
	}

	for (auto it = AudioOuts.begin(); it != AudioOuts.end(); ++it)
	{
		(*it)->setStreaming(true);
	}

	return gmpi::ReturnCode::Ok;
}

void ProcessorWrapper::initVst()
{
	if (!vstEffect_)
	{
		return;
	}

	processSetup = {
		pinOfflineRenderMode.getValue() == 2 ? kOffline : kRealtime,
		kSample32,
		host->getBlockSize(),
		host->getSampleRate()
	};

	if(vstEffect_->setupProcessing(processSetup) != kResultOk)
	{
		return;
	}

	processData.prepare (*component_, 0, processSetup.symbolicSampleSize);

	// init busses
	{
		for (MediaType type = kAudio; type <= kNumMediaTypes; type++) // audio or MIDI
		{
			for (int busDirection = kInput; busDirection <= kOutput; busDirection++)
			{
				auto busCount = component_->getBusCount(type, busDirection);

				if (kAudio == type && busCount > 0)
				{
					std::vector<int>& channelCounts = (busDirection == kInput) ? inputBusses : outputBusses;

					for (int busIndex = 0; busIndex < busCount; ++busIndex)
					{
						BusInfo busInfo{};
						if (component_->getBusInfo(kAudio, busDirection, busIndex, busInfo) == kResultTrue)
						{
							channelCounts.push_back(busInfo.channelCount);
						}
						else
						{
							break;
						}
					}
				}

				for (auto busIndex = 0; busIndex < busCount; ++busIndex)
				{
					component_->activateBus(type, busDirection, busIndex, true);
				}
			}
		}
	}

	component_->setActive(true);

	// init buffers to cope with latency
	latency = vstEffect_->getLatencySamples();
	const auto bs = host->getBlockSize();
	bypassDelaysize = ((bs + latency + bs - 1) / bs) * bs; // rounded up to nearest full block
	bypassDelays.resize(AudioIns.size());
	for (auto& delay : bypassDelays)
	{
		delay.resize(bypassDelaysize);
	}

	_RPT1(0, "LATENCY INITAL: %d\n", latency);
// TODO	host.SetLatency(latency);

	currentVstSubProcess = &ProcessorWrapper::subProcess2<ST_PROCESS>;
}

void ProcessorWrapper::addParameterEvent(int clock, int id, float value)
{
	Steinberg::int32 returnIndexUnused = {};
	parameterEvents.addParameterData(id, returnIndexUnused)->addPoint(clock, value, returnIndexUnused);
}

void ProcessorWrapper::onMidiMessage(int timeDelta, int pin, std::span<const uint8_t> midiMessage)
{
	assert(timeDelta >= 0);
	timeDelta = max(timeDelta, 0); // should never be nesc, but is safer to do

	if(pin == parameterAccessPinIndex)
	{
#if 0 // TODO
		if(GmpiMidiHdProtocol::isWrappedHdProtocol(midiData, size))
		{
			const auto m2 = (GmpiMidiHdProtocol::Midi2*) midiData;

			const int paramId = m2->key;
			const float normalized = *(float*)&m2->value;
			addParameterEvent(timeDelta, paramId, normalized);

#if 1 // def SE_TARGET_SEM
			// Also send parameter to Controller
			if (controller)
			{
				controller->setParameterFromProcessorUnsafe(paramId, normalized);
			}
#endif
		}
#endif
		return;
	}

	midiConverter.processMidi(midiMessage, timeDelta);
}

void ProcessorWrapper::onMidi2Message(const midi::message_view msg, int timeDelta)
{
	const auto header = gmpi::midi_2_0::decodeHeader(msg);

	// only 8-byte messages supported.
	if (header.messageType != gmpi::midi_2_0::ChannelVoice64)
		return;

	Steinberg::Vst::Event m = {};
	const int unusedType = 666;
	m.type = unusedType;

#if	VST3_USE_MIDI_EXTENSION

	// determine what MIDI protocol the plugin requires. (you could cache this part).
	int pluginsMidiProtocol = -1; // none.

	vst3_ext_midi::IProcessMidiProtocol* midi2Processor = nullptr;
	vstEffect_->queryInterface(vst3_ext_midi::IProcessMidiProtocol::iid, (void**)&midi2Processor);

	if (midi2Processor)
	{
		pluginsMidiProtocol = midi2Processor->getProcessMidiProtocol();
	}

	if (pluginsMidiProtocol == 1) // MIDI 1.0
	{
		// This DAW uses MIDI 2.0 by default, so we need to convert the message to MIDI 1.0
		// If you are using MIDI 1.0 already you can skip this step.
		uint8_t midiBytes[3]{};

		midiConverter_1_0.setSink(
			// this lambda accepts the MIDI 1.0 messages.
			[&midiBytes](const midi::message_view& msg, int offset)
			{
				memcpy(midiBytes, msg.data(), msg.size());
			}
		);

		midiConverter_1_0.processMidi(
			msg,
			timeDelta
		);

		// Wrap your MIDI 1.0 message in a UMP packet.
		// 'ChannelVoice32' = means MIDI 1.0 Message
		constexpr int channelGroup = 0;

		uint8_t wrappedMidi_1_0_message[4] =
		{
			static_cast<uint8_t>((gmpi::midi_2_0::ChannelVoice32 << 4) | (channelGroup & 0x0f)),
			midiBytes[0],
			midiBytes[1],
			midiBytes[2],
		};

		// tag the Steinberg event as a UMP event.
		m.type = vst3_ext_midi::UMPEvent::kType;
		auto& midi2event = *reinterpret_cast<vst3_ext_midi::UMPEvent*>(&m.noteOn);

		// copy the MIDI message into the Steinberg VST3 event.
		memcpy(&midi2event.words, &wrappedMidi_1_0_message, sizeof(wrappedMidi_1_0_message));
	}
	else if (pluginsMidiProtocol == 2) // MIDI 2.0
	{
		// DAW already uses MIDI 2.0, so we can pass a simple copy of the message.

		// tag the Steinberg event as a UMP event.
		m.type = vst3_ext_midi::UMPEvent::kType;
		auto& midi2event = *reinterpret_cast<vst3_ext_midi::UMPEvent*>(&m.noteOn);

		// copy the MIDI message into the Steinberg VST3 event.
		memcpy(&midi2event.words, msg.data(), msg.size());
	}

#else // convert to VST3 note events

	switch (header.status)
	{

	case gmpi::midi_2_0::NoteOn:
	{
		const auto note = gmpi::midi_2_0::decodeNote(msg);

		m.type = Event::kNoteOnEvent;
		m.noteOn.channel = header.channel;
		m.noteOn.noteId = -1;
		m.noteOn.pitch = note.noteNumber;
		m.noteOn.velocity = note.velocity;
	}
	break;

	case gmpi::midi_2_0::NoteOff:
	{
		const auto note = gmpi::midi_2_0::decodeNote(msg);

		m.type = Event::kNoteOffEvent;
		m.noteOff.channel = header.channel;
		m.noteOff.noteId = -1;
		m.noteOff.pitch = note.noteNumber;
		m.noteOff.velocity = note.velocity;
	}
	break;

	case gmpi::midi_2_0::PolyAfterTouch:
	{
		const auto aftertouch = gmpi::midi_2_0::decodePolyController(msg);
		m.type = Event::kPolyPressureEvent;
		m.polyPressure.channel = header.channel;
		m.polyPressure.noteId = -1;
		m.polyPressure.pitch = aftertouch.noteNumber;
		m.polyPressure.pressure = aftertouch.value;
	}
	break;

	default:
		break;
	};
#endif

	if(m.type != unusedType)
	{
		m.sampleOffset = timeDelta;
		vstEventList.events.push_back(m);
	}
}

void ProcessorWrapper::ProcessEvents(int32_t count, const gmpi::api::Event* events)
{
	assert(count > 0);

#if defined(_DEBUG)
	blockPosExact_ = false;
#endif

	vstEventList.events.clear();

	blockPos_ = 0;
	int lblockPos = blockPos_;
	int remain = count;
	const auto* next_event = events;

	for (;;)
	{
		if (next_event == 0) // fast version, when no events on list.
		{
			break;
		}

		assert(next_event->timeDelta < count); // Event will happen in this block

		int delta_time = next_event->timeDelta - lblockPos;

		if (delta_time > 0) // then process intermediate samples
		{
			eventsComplete_ = false;

			remain -= delta_time;

			eventsComplete_ = true;

			assert(remain != 0); // BELOW NEEDED?, seems non sense. If we are here, there is a event to process. Don't want to exit!
			if (remain == 0) // done
			{
				break;
			}

			lblockPos += delta_time;
		}

#if defined(_DEBUG)
		blockPosExact_ = true;
#endif
		assert(lblockPos == next_event->timeDelta);

		// PRE-PROCESS EVENT
		bool pins_set_flag = false;
		auto e = next_event;
		do
		{
			preProcessEvent(e); // updates all pins_ values
			pins_set_flag = pins_set_flag || e->eventType == gmpi::api::EventType::PinSet || e->eventType == gmpi::api::EventType::PinStreamingStart || e->eventType == gmpi::api::EventType::PinStreamingStop;
			e = e->next;
		} while (e != 0 && e->timeDelta == lblockPos);

		// PROCESS EVENT
		e = next_event;
		do
		{
			if (e->eventType == gmpi::api::EventType::Midi)
			{
				onMidiMessage(e->timeDelta, e->pinIdx, e->payload());
			}
			else
			{
				processEvent(e); // notify all pins_ values
			}
			e = e->next;
		} while (e != 0 && e->timeDelta == lblockPos);

		if (pins_set_flag)
		{
			onSetPins();
		}

		// POST-PROCESS EVENT
		do
		{
			postProcessEvent(next_event);
			next_event = next_event->next;
		} while (next_event != 0 && next_event->timeDelta == lblockPos);

#if defined(_DEBUG)
		blockPosExact_ = false;
#endif
	}
}

void ProcessorWrapper::subProcessNotLoaded(int32_t count, const gmpi::api::Event* events)
{
	ProcessEvents(count, events);

	for (auto& outBuffer : AudioOuts)
	{
		auto out = getBuffer(*outBuffer);

		// output silence.
		for (int s = count; s > 0; --s)
		{
			*out++ = 0.0f;
		}
	}

	vstTime_.continousTimeSamples += count;
}

void ProcessorWrapper::onSetPins(void)
{
	// Warning this method is not interleaved with processing like normal, all events are processed before running plugin process block.

	if (pinHostBpm.isUpdated())
	{
		vstTime_.tempo = pinHostBpm;
	}
	if (pinNumerator.isUpdated())
	{
		vstTime_.timeSigNumerator = pinNumerator;
	}
	if (pinDenominator.isUpdated())
	{
		vstTime_.timeSigDenominator = pinDenominator;
	}

	if (pinControllerPointer.isUpdated() && pinControllerPointer.getValue().size() == sizeof(ControllerWrapper*))
	{
		controller = *(ControllerWrapper**)pinControllerPointer.getValue().data();

		controller->registerProcessor(&component_, &vstEffect_);

		initVst();
	}

	if (pinOfflineRenderMode.isUpdated() && vstEffect_)
	{
		const int32 newProcessMode = pinOfflineRenderMode.getValue() == 2 ? kOffline : kRealtime;
		if (newProcessMode != processSetup.processMode && vstEffect_)
		{
			// reset processor
			vstEffect_->setProcessing(false); // nesc?
			component_->setActive(false);

			processSetup = {
				newProcessMode,
				kSample32,
				host->getBlockSize(),
				host->getSampleRate()
			};

			if (vstEffect_->setupProcessing(processSetup) == kResultOk)
			{
				component_->setActive(true);
				vstEffect_->setProcessing(true);
			}
		}
	}

	if (pinOnOffSwitch.isUpdated())
    {
		targetLevel = pinOnOffSwitch.getValue() ? 1.0f : 0.0f;
    }
}
