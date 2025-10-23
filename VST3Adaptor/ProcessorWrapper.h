#pragma once

#include <memory>
#include <unordered_map>
//#include "../se_sdk3/mp_sdk_audio.h"
//#include "../se_sdk3/mp_midi.h"
//#include "../shared/xplatform.h"
#include "Core/Processor.h"
#include "base/source/fobject.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include "./ControllerWrapper.h"

#if defined(SE_TARGET_WAVES)
#include "cancellation.h"
#endif

struct myParamValueQueue : public Steinberg::FObject, public Steinberg::Vst::IParamValueQueue
{
	myParamValueQueue(Steinberg::Vst::ParamID id) : paramId(id) {}

	Steinberg::Vst::ParamID paramId = {};
	std::vector< std::pair<Steinberg::int32, Steinberg::Vst::ParamValue> > events;

	Steinberg::Vst::ParamID PLUGIN_API getParameterId () override
	{
		return paramId;
	}

	/** Returns count of points in the queue. */
	Steinberg::int32 PLUGIN_API getPointCount () override
	{
		return static_cast<Steinberg::int32>(events.size());
	}

	Steinberg::tresult PLUGIN_API getPoint (Steinberg::int32 index, Steinberg::int32& sampleOffset /*out*/, Steinberg::Vst::ParamValue& value /*out*/) override
	{
		sampleOffset = events[index].first;
		value = events[index].second;
		return 0;
	}

	/** Adds a new value at the end of the queue, its index is returned. */
	Steinberg::tresult PLUGIN_API addPoint (Steinberg::int32 sampleOffset, Steinberg::Vst::ParamValue value, Steinberg::int32& index /*out*/) override
	{
		index = static_cast<Steinberg::int32>(events.size());
		events.push_back({ sampleOffset, value });
		return 0;
	}

	//---Interface---------
	OBJ_METHODS (myParamValueQueue, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Steinberg::Vst::IParamValueQueue)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

struct myParameterChanges : public Steinberg::FObject, public Steinberg::Vst::IParameterChanges
{
	std::vector<myParamValueQueue> queues;

	Steinberg::int32 PLUGIN_API getParameterCount() override
	{
		return static_cast<Steinberg::int32>(queues.size());
	}

	Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData(Steinberg::int32 index) override
	{
		return &queues[index];
	}

	Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData(const Steinberg::Vst::ParamID& id, Steinberg::int32& index /*out*/) override
	{
		index = 0;

		// Fix for Waves Child Plugins, presets parameters in order of ID.
		for(auto it = queues.begin(); it != queues.end() ; ++it)
		{
			auto& queue = *it;
			if(queue.getParameterId() == id)
			{
				return &queue;
			}

			if(queue.getParameterId() > id)
			{
				it = queues.insert(it, id);
				return &(*it);
			}  
			++index;
		}

		index = static_cast<Steinberg::int32>(queues.size());
		queues.push_back(id);
		return &queues.back();
	}

	void clear()
	{
		queues.clear();
	}

	//---Interface---------
	OBJ_METHODS (myParameterChanges, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Steinberg::Vst::IParameterChanges)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

struct myEventList : public Steinberg::FObject, public Steinberg::Vst::IEventList
{
	std::vector<Steinberg::Vst::Event> events;

	Steinberg::int32 PLUGIN_API getEventCount() override
	{
		return static_cast<Steinberg::int32>(events.size());
	}
	Steinberg::tresult PLUGIN_API getEvent(Steinberg::int32 index, Steinberg::Vst::Event& e /*out*/) override
	{
		e = events.at(index);
		return Steinberg::kResultOk;
	}
	Steinberg::tresult PLUGIN_API addEvent(Steinberg::Vst::Event& e /*in*/) override
	{
		events.push_back(e);
		return Steinberg::kResultOk;
	}

	//---Interface---------
	OBJ_METHODS (myEventList, Steinberg::FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (IEventList)
	END_DEFINE_INTERFACES (Steinberg::FObject)
	REFCOUNT_METHODS (Steinberg::FObject)
};

class ProcessorWrapper : public gmpi::Processor
{
	gmpi::BoolInPin pinOnOffSwitch;
	gmpi::FloatInPin pinHostBpm;
	gmpi::FloatInPin pinHostSP;
	gmpi::BoolInPin pinHostTransport;
	gmpi::IntInPin pinNumerator;
	gmpi::IntInPin pinDenominator;
	gmpi::FloatInPin pinHostBarStart;
	gmpi::IntInPin pinOfflineRenderMode;
	gmpi::BlobInPin pinControllerPointer;
	gmpi::MidiInPin pinMIDIIn;
	gmpi::MidiInPin pinParamBuss;

	Steinberg::Vst::IComponent* component_ = {};
	Steinberg::Vst::IAudioProcessor* vstEffect_ = {};
	Steinberg::Vst::ProcessContext vstTime_;
	myEventList vstEventList;
	myParameterChanges parameterEvents;
	Steinberg::Vst::HostProcessData processData;
	Steinberg::Vst::ProcessSetup processSetup;

	std::vector<int> inputBusses; // bus/chans
	std::vector<int> outputBusses; // bus/chans

	std::vector<std::vector<float>> bypassDelays;
	int bypassBufferPos = 0;
	int latency = 0;
	int bufferPrimingCounter = {};
	int bypassDelaysize = {};
	ControllerWrapper* controller = {};

	typedef void (ProcessorWrapper::* VstSubProcess_ptr)(int32_t count, const gmpi::api::Event* events);

	VstSubProcess_ptr currentVstSubProcess;
#ifdef CANCELLATION_TEST_ENABLE2
    bool cancellation_done = false;
	void debugDumpPresetToFile();
#endif
	enum { ST_PROCESS, ST_PRIME_BUFFERS, ST_FADING, ST_BYPASS };
	gmpi::midi_2_0::MidiConverter2 midiConverter;
	gmpi::midi_2_0::MidiConverter1 midiConverter_1_0;

public:
	ProcessorWrapper();
	~ProcessorWrapper();

	void onMidiMessage(int timeDelta, int pin, std::span<const uint8_t> midiMessage);
	void onMidi2Message(const gmpi::midi::message_view msg, int timestamp);
	void ProcessEvents(int32_t count, const gmpi::api::Event* events);
	void process(int32_t count, const gmpi::api::Event* events) override;

	template<int CURRENT_STATE>
	void subProcess2(const int32_t count, const gmpi::api::Event* events)
	{
		// 'ProcessEvents' can add parameter changes from MIDI
		parameterEvents.clear();

		ProcessEvents(count, events);

		// add input to bypass latency buffers
		if constexpr (CURRENT_STATE != ST_PROCESS)
		{
			for (size_t i = 0; i < AudioIns.size(); ++i)
			{
				const float* in = getBuffer(*(AudioIns[i]));
				float* dest = bypassDelays[i].data() + bypassBufferPos;

				int todo = count;
				int c = (std::min)(todo, bypassDelaysize - bypassBufferPos);
				while (todo)
				{
					for (int s = 0; s < c; ++s)
					{
						*dest++ = *in++;
					}
					dest = bypassDelays[i].data(); // wrap back arround.
					todo -= c;
					c = todo;
				}
			}
		}

		// process plugin
		if constexpr (CURRENT_STATE != ST_BYPASS)
		{
			for (int bus = 0; bus < inputBusses.size(); ++bus)
			{
				for (int i = 0; i < inputBusses[bus]; ++i)
				{
					processData.setChannelBuffer(
						Steinberg::Vst::kInput,
						bus,
						i,
						const_cast<float*>(getBuffer(*(AudioIns[i])))
					);
				}
			}

			for (int bus = 0; bus < outputBusses.size(); ++bus)
			{
				for (int i = 0; i < outputBusses[bus]; ++i)
				{
					processData.setChannelBuffer(
						Steinberg::Vst::kOutput,
						bus,
						i,
						getBuffer(*(AudioOuts[i]))
					);
				}
			}
			myParameterChanges outputParameterChanges;
			myEventList outputEvents;

			processData.outputParameterChanges = &outputParameterChanges; // todo
			processData.outputEvents = &outputEvents;
			processData.numSamples = count;

			// parameter from Editor
			{
//				parameterEvents.clear();

				if (controller && controller->parameters_dirty.exchange(false, std::memory_order_release))
				{
					for (auto& p : controller->parametersToProcessor)
					{
						const auto pdirty = p->dirty.load(std::memory_order_relaxed);
						if (pdirty)
						{
							p->dirty.store(false, std::memory_order_release);

							Steinberg::int32 index{};
							auto queue = processData.inputParameterChanges->addParameterData(p->id, index);
							Steinberg::int32 index2{};
							queue->addPoint(0, p->normalized.load(std::memory_order_relaxed), index2);
						}
					}
				}
			}

			vstEffect_->process(processData);
		}

		float fade = {};
		if constexpr (CURRENT_STATE != ST_PROCESS)
		{
			// Copy buffered audio input to output.
			for (size_t i = 0; i < AudioOuts.size(); ++i)
			{
				fade = fadeLevel;
				auto out = getBuffer(*(AudioOuts[i]));

				if (i < AudioIns.size())
				{
					int bypassBufferReadPos = (bypassDelaysize + bypassBufferPos - latency) % bypassDelaysize;

					const float* source = bypassDelays[i].data() + bypassBufferReadPos;
					int todo = count;
					while (todo)
					{
						const int c = (std::min)(todo, bypassDelaysize - bypassBufferReadPos);
						for (int s = 0; s < c; ++s)
						{
							fade = std::clamp(fade + fadeInc, 0.0f, 1.0f);
							*out = *source + fade * (*out - *source);

							++out;
							++source;
						}
						source = bypassDelays[i].data(); // wrap back arround.
						bypassBufferReadPos = 0;
						todo -= c;
					}
				}
				else
				{
					// if not audio input pins, output silence.
					for (int s = count; s > 0; --s)
					{
						fade = std::clamp(fade + fadeInc, 0.0f, 1.0f);
						// equivalent to cross-fade with zero.
						*out *= fade;

						++out;
					}
				}
			}

			fadeLevel = fade;
		}

		bypassBufferPos = (bypassBufferPos + count) % bypassDelaysize;

		vstTime_.continousTimeSamples += count;

		// switch state machine state if nesc.
		// ST_PROCESS -> ST_PRIME_BUFFERS -> ST_FADING (down) -> ST_BYPASS
		// ST_BYPASS  -> ST_PRIME_BUFFERS -> ST_FADING ( up ) -> ST_PROCESS
		// also ST_FADING (down) -> ST_FADING ( up ) -> ST_FADING (down) etc.

		if constexpr (ST_PROCESS == CURRENT_STATE)
		{
			if (1.0f != targetLevel)
			{
				bufferPrimingCounter = latency + 20;
				fadeInc = 0.0f;
				currentVstSubProcess = &ProcessorWrapper::subProcess2<ST_PRIME_BUFFERS>;
//				_RPT0(0, "ST_PRIME_BUFFERS\n");
			}
		}

		if constexpr (ST_PRIME_BUFFERS == CURRENT_STATE)
		{
			assert(fadeInc == 0.0f);

			bufferPrimingCounter -= count;
			if (bufferPrimingCounter < 0)
			{
				fadeInc = calcFade();
				currentVstSubProcess = &ProcessorWrapper::subProcess2<ST_FADING>;
//				_RPT0(0, "ST_FADING\n");
			}
		}

		if constexpr (ST_FADING == CURRENT_STATE)
		{
			assert(fadeInc != 0.0f);

			// account for possiblity of use switching mode *during* the fade.
			fadeInc = calcFade();

			if (fadeLevel == 0.0f) // faded down.
			{
				fadeInc = 0.0f;
				currentVstSubProcess = &ProcessorWrapper::subProcess2<ST_BYPASS>;
//				_RPT0(0, "ST_BYPASS\n");
			}
			else if (fadeLevel == 1.0f) // faded up.
			{
				fadeInc = 0.0f;
				currentVstSubProcess = &ProcessorWrapper::subProcess2<ST_PROCESS>;
//				_RPT0(0, "ST_PROCESS\n");
			}
		}

		if constexpr (ST_BYPASS == CURRENT_STATE)
		{
			if (1.0f == targetLevel)
			{
				bufferPrimingCounter = latency + 20;
				fadeInc = 0.0f;
				currentVstSubProcess = &ProcessorWrapper::subProcess2<ST_PRIME_BUFFERS>;
//				_RPT0(0, "ST_PRIME_BUFFERS\n");
			}
		}
	}
	void subProcessNotLoaded(int32_t count, const gmpi::api::Event* events);

	float calcFade() const
	{
		constexpr float fadeTimeS = 0.02f;
		return copysignf(1.f / (host->getSampleRate() * fadeTimeS), targetLevel - fadeLevel);
	}

	gmpi::ReturnCode open(gmpi::api::IUnknown* phost) override;
	void initVst();
	void onSetPins(void) override;

private:
	void addParameterEvent(int clock, int index, float value);

	//gmpi::MidiInPin pinMidi;
	//gmpi::MidiInPin pinParameterAccess;

	std::vector< std::unique_ptr<gmpi::AudioInPin> > AudioIns;
	std::vector< std::unique_ptr<gmpi::AudioOutPin> > AudioOuts;

	//gmpi::BoolInPin pinOnOffSwitch;
	//gmpi::BlobInPin pinControllerPointer;

	// Musical time
	//gmpi::FloatInPin pinHostBpm;
	//gmpi::FloatInPin pinHostSongPosition;
	//gmpi::IntInPin pinNumerator;
	//gmpi::IntInPin pinDenominator;
	//gmpi::BoolInPin pinHostTransport;
	//gmpi::FloatInPin pinHostBarStart;
	//gmpi::IntInPin pinOfflineRenderMode;

	int firstParameterPinIndex = {};
	int parameterAccessPinIndex = {};

	float fadeLevel = 1.0f;
	float targetLevel = 1.0f;
	float fadeInc = 0.0f;
};

class Vst3ParamSet : public gmpi::Processor
{
	bool initialUpdateDone = false;

	gmpi::FloatInPin pinFloatIn;
	gmpi::IntInPin pinParamIdx;
	gmpi::MidiOutPin pinParameterAccessOut;

public:
	Vst3ParamSet()
	{
		//initializePin(pinFloatIn);
		//initializePin(pinParamIdx);
		//initializePin(pinParameterAccessOut);
	}

	void sendPinValueAsMidi()
	{
		const int paramId = pinParamIdx.getValue();
        assert(paramId < 256);

		// Send MIDI HD-Protocol Note Expression message. key is paramId, value is normalised cast to int32.
		GmpiMidiHdProtocol::Midi2 msg;
		GmpiMidiHdProtocol::setMidiMessage(msg, GmpiMidi::MIDI_ControlChange, 0, paramId, 0);

		const float normalized = pinFloatIn.getValue();
        msg.value = *(int32_t*) &normalized;

		pinParameterAccessOut.send(msg.data(), msg.size(), getBlockPosition());// + paramId);
	}

	void onSetPins(void) override
	{
		if (pinFloatIn.isUpdated())
		{
			sendPinValueAsMidi();
		}
	}
};
