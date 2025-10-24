#include "ControllerWrapper.h"
#include "myPluginProvider.h"
#include "./MyViewStream.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "GmpiSdkCommon.h"

using namespace gmpi;
using namespace Steinberg;
using namespace Steinberg::Vst;

class VstComponentHandler : public Steinberg::FObject, public IComponentHandler, public IComponentHandler2
{
public:
	class ControllerWrapper* controller_;

	// IComponentHandler
	Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID id) override;
	Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) override;
	Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID id) override;
	Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override;

	// IComponentHandler2
	Steinberg::tresult PLUGIN_API setDirty(Steinberg::TBool state) override;
	Steinberg::tresult PLUGIN_API requestOpenEditor(Steinberg::FIDString name) override { return kNotImplemented; }
	Steinberg::tresult PLUGIN_API startGroupEdit() override { return kNotImplemented; }
	Steinberg::tresult PLUGIN_API finishGroupEdit() override { return kNotImplemented; }

	//---Interface---------
	OBJ_METHODS(VstComponentHandler, Steinberg::FObject)
		DEFINE_INTERFACES
		DEF_INTERFACE(Steinberg::Vst::IComponentHandler)
		DEF_INTERFACE(Steinberg::Vst::IComponentHandler2)
		END_DEFINE_INTERFACES(Steinberg::FObject)
		REFCOUNT_METHODS(Steinberg::FObject)
};

ControllerWrapper::ControllerWrapper(const char* filename, const std::string& uuid) :
	  filename_(filename)
	, shellPluginId_(uuid)
	, handle_(0)
	, m_message_que_dsp_to_ui(4000)
{
	componentHandler = std::make_unique<VstComponentHandler>();
	componentHandler->controller_ = this;
	plugin = std::make_unique<myPluginProvider>();
}

ControllerWrapper::~ControllerWrapper()
{
	if (windowController)
	{
		windowController->destroyView();
	}

	plugin->terminatePlugin();
}

// A MIDI message has arrived from the host to set a parameter. Event has already been scheduled on Processor. Need to update Editor.
void ControllerWrapper::setParameterFromProcessorUnsafe(uint32_t paramId, double valueNormalized)
{
	const int32_t messageSize = sizeof(int32_t) + static_cast<int32_t>(sizeof(valueNormalized));

	gmpi::hosting::my_msg_que_output_stream strm(&m_message_que_dsp_to_ui, paramId, "ppc");
	strm << messageSize;
	strm << paramId;
	strm << valueNormalized;

	strm.Send();
}

void ControllerWrapper::serviceQueue()
{
	while (m_message_que_dsp_to_ui.readyBytes() > 0)
	{
		int32_t paramHandle;
		int32_t recievingMessageId;
		int32_t recievingMessageLength;

		gmpi::hosting::my_msg_que_input_stream strm(&m_message_que_dsp_to_ui);
		strm >> paramHandle;
		strm >> recievingMessageId;
		strm >> recievingMessageLength;

		assert(recievingMessageId != 0);
		assert(recievingMessageLength >= 0);

		assert(recievingMessageId == gmpi::hosting::id_to_long("ppc"));
		{
			int32_t paramId;
			double valueNormalized;

			strm >> paramId;
			strm >> valueNormalized;

			// broadcast to Editor
			plugin->controller->setParamNormalized(paramId, valueNormalized);
		}
	}
}

// setting state of wrapper, which will pass on the state to the VST3 plugin
//int32_t ControllerWrapper::setParameter(int32_t parameterHandle, int32_t fieldId, int32_t /*voice*/, const void* data, int32_t size)
gmpi::ReturnCode ControllerWrapper::setParameter(int32_t parameterHandle, gmpi::Field fieldId, int32_t voice, int32_t size, const uint8_t* data)
{
	// Avoid altering plugin state until we can determine if we are restoring a saved preset, or keeping init preset.
	if(!isOpen)
		return gmpi::ReturnCode::Ok;

	if(!inhibitFeedback && fieldId == gmpi::Field::Value)
	{
		/*
		int32_t moduleHandle = -1;
		int32_t moduleParameterId = -1;
		host_->getParameterModuleAndParamId(parameterHandle, &moduleHandle, &moduleParameterId);
		*/

const auto moduleParameterId = parameterHandle; // TODO!!! is this a posible simplification???

		if(!plugin->controller || !plugin->component)
		{
			return gmpi::ReturnCode::Fail;
		}

		const auto paramId = 0;
		if(paramId != moduleParameterId)
		{
			return gmpi::ReturnCode::Ok;
		}

		if((size_t)(size) < sizeof(int32_t)) // Size of zero implies first-time init, no preset stored yet.
		{
			isSynthEditPresetEmpty = true;
            return gmpi::ReturnCode::Ok;
		}
		else
		{
#if 0//defined (_DEBUG) & defined(_WIN32)
			auto effectName = ae->getName();

			_RPT0(_CRT_WARN, "{ ");
			auto d = (unsigned char*)data;
			for(int i = 0; i < 12; ++i)
			for(int i = 0; i < 12; ++i)
			{
				_RPT1(_CRT_WARN, "%02x ", (int)d[i]);
			}
			_RPT2(_CRT_WARN, "}; set: %s H %d\n", effectName.c_str(), moduleHandle);
#endif
			// Load preset chunk.
			const auto controllerStateSize = *((int32_t*)data);
			const auto controllaDataPtr = ((uint8_t*)data) + sizeof(int32_t);
			MyViewStream s(controllaDataPtr, controllerStateSize);
			plugin->controller->setState(&s);

			const auto streamPos = sizeof(int32_t) + controllerStateSize;
			const auto processorStateSize = size - streamPos;
			const auto processorDataPtr = ((uint8_t*)data) + streamPos;
			MyViewStream s2(processorDataPtr, static_cast<int32_t>(processorStateSize));
			plugin->component->setState(&s2);

			MyViewStream s3(processorDataPtr, static_cast<int32_t>(processorStateSize));
			plugin->controller->setComponentState(&s3);
		}
	}

	return gmpi::ReturnCode::Ok;
}

#if 0
int32_t ControllerWrapper::preSaveState()
{
	inhibitFeedback = true;

	if(!plugin->controller || !plugin->component)
	{
		return gmpi::ReturnCode::Fail;
	}
	{
		// get controller state. usually blank.
		MyBufferStream stream;
		int32_t controllerStateSize = {};
		stream.write(&controllerStateSize, sizeof(controllerStateSize));
		plugin->controller->getState(&stream);

		// update size of data written so far.
		*((int32_t*)stream.buffer_.data()) = static_cast<int32_t>(stream.buffer_.size() - sizeof(int32_t));

		// get processor state.
		plugin->component->getState(&stream);

#if 0 //defined (_DEBUG) & defined(_WIN32)
		_RPT0(_CRT_WARN, "{ ");
		auto d = (unsigned char*)chunkPtr;
		for (int i = 0; i < 12; ++i)
		{
			_RPT1(_CRT_WARN, "%02x ", (int)d[i]);
		}
		_RPT0(_CRT_WARN, "}; get\n");
#endif

#if 0 // ifdef _DEBUG
		// Waves Grand Rhapsody needs to load child plugin preset from the ALG,
		// print preset out in handy format to be pasted into ALG source code. (prints to debug window).
		{
			_RPT0(_CRT_WARN, "{ ");
			const unsigned char* d = (const unsigned char*)chunkPtr;
			for (int i = 0 ; i < chunkSize; ++i)
			{
				if ((i % 20) == 0)
				{
					_RPT0(_CRT_WARN, "\n");
				}
				_RPT1(_CRT_WARN, "0x%02x, ", d[i]);
			}
			_RPT0(_CRT_WARN, "};\n");
		}
#endif

		const int voiceId = 0;
		host_->setParameter(
			host_->getParameterHandle(handle_, chunkParamId),
			gmpi::Field::Value,
			voiceId,
			(char*)stream.buffer_.data(),
			(int32_t) stream.buffer_.size()
		);
	}

	stateDirty = false;
	inhibitFeedback = false;

	return gmpi::ReturnCode::Ok;
}

#endif

//int32_t ControllerWrapper::setHost(gmpi::IMpUnknown* host)
gmpi::ReturnCode ControllerWrapper::initialize(gmpi::api::IUnknown* host, int32_t handle)
{
	handle_ = handle;

	gmpi::shared_ptr<gmpi::api::IUnknown> unknown(host);
	host_ = unknown.as<gmpi::api::IControllerHost_x>();

	if( !host_ )
		return gmpi::ReturnCode::NoSupport; //  host Interfaces not supported

	LoadPlugin( filename_, shellPluginId_);

	if(plugin && plugin->component)
	{
		Steinberg::Vst::IAudioProcessor* vstEffect = {};
		plugin->component->queryInterface(IAudioProcessor::iid, (void**)&vstEffect);
		if (vstEffect)
		{
// TODO?			host_->setLatency(vstEffect->getLatencySamples()); // this should be supported on Waves.
			vstEffect->release();
		}

		open();
	}
	return plugin->controller != nullptr ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
}

gmpi::ReturnCode ControllerWrapper::open()
{
	isOpen = true;

	if (!plugin->controller) // VST not installed?
	{
		return gmpi::ReturnCode::Ok;
	}

	plugin->controller->setComponentHandler(componentHandler.get());

	// Pass pointer to 'this' to Process and GUI.
	const int controllerPtrParamId = chunkParamId + 1;
	const int voiceId = 0;
	auto me = this;

	int32_t parameterHandle{};
	host_->getParameterHandle(controllerPtrParamId, parameterHandle);
	// TODO host to just take parameter id directly, why convert to SE's handle at all here?
	host_->setParameter(parameterHandle, gmpi::Field::Value, voiceId, sizeof(me), (const uint8_t*) &me);

	{
		// always have to pass initial state from processor to controller.
		{
			assert(plugin->controller && plugin->component);

			// get processor state.
			MyBufferStream stream;
			plugin->component->getState(&stream);

			// pass to controller
			MyViewStream s3(stream.buffer_.data(), static_cast<int32_t>(stream.buffer_.size()));
			plugin->controller->setComponentState(&s3);
		}

#if 0 // TODO ??
		// Test if host has a valid chunk preset.
		isSynthEditPresetEmpty = false;
		host_->updateParameter(host_->getParameterHandle(handle_, chunkParamId), gmpi::Field::Value, voiceId);

		// In the case we have no preset stored yet (because user *just* inserted plugin), Copy init preset to SE patch memory.
		if (isSynthEditPresetEmpty)
		{
			preSaveState();
		}
#endif
	}

	startTimer(20);
	return gmpi::ReturnCode::Ok;
}

gmpi::ReturnCode ControllerWrapper::LoadPlugin(std::string path, std::string uuid)
{
	std::string error;
    dll = VST3::Hosting::Module::create(path, error);

	if(!dll)
	{
		// Could not create Module for file
#ifdef _WIN32
        _RPT1(0, "Failed to load VST3 child plugin. UUID:%s\n", uuid.c_str());
#endif
        return gmpi::ReturnCode::Fail;
	}

	const auto classID = VST3::UID::fromString(uuid);
	if(!classID)
	{
        return gmpi::ReturnCode::Fail;
	}

	auto factory = dll->getFactory();
	plugin->setup(factory, *classID);

	if (!plugin->controller)
	{
		// failed to load waves plugin
		return gmpi::ReturnCode::Fail;
	}

	const auto parameterCount = plugin->controller->getParameterCount();
	parametersToProcessor.reserve(parameterCount);
	for (int i = 0; i < parameterCount; ++i)
	{
		Steinberg::Vst::ParameterInfo info{};
		plugin->controller->getParameterInfo(i, info);
		parametersToProcessor.push_back(std::make_unique<ControllerWrapper::vstParameterVal>());
		parametersToProcessor.back()->id = info.id;
	}

	return gmpi::ReturnCode::Ok;
}

void ControllerWrapper::OpenGui()
{
	if (!plugin->controller) // VST not installed?
	{
		return;
	}

	auto view = owned(plugin->controller->createView(Steinberg::Vst::ViewType::kEditor));
	if (!view)
	{
		// EditController does not provide its own editor
		return;
	}

	windowController = std::make_shared<WindowController>(view);

	WindowController::createPlatformWindow(windowController);
}

bool ControllerWrapper::onTimer()
{
	serviceQueue();

	if( stateDirty )
	{
// TODO???		preSaveState();
		
		stateDirty = false;
	}
	return true;
}

tresult VstComponentHandler::beginEdit(ParamID paramId)
{
	/*
	const bool value = true;
	const int voiceId = 0;

	controller_->host_->setParameter(
		controller_->host_->getParameterHandle(controller_->handle_, paramId),
		MP_FT_GRAB,
		voiceId,
		(const void*)&value,
		(int32_t) sizeof(value)
	);
	*/
	return kResultOk;
}

tresult VstComponentHandler::performEdit (ParamID paramId, ParamValue valueNormalized)
{
	controller_->setParameterFromEditor(paramId, valueNormalized);
	controller_->stateDirty = true;
	return kResultOk;
}

tresult VstComponentHandler::endEdit (ParamID paramId)
{
	return kResultOk;
}

tresult VstComponentHandler::restartComponent (int32 flags)
{
	/*
	if (flags & Steinberg::Vst::kLatencyChanged && controller_->getProcessor())
	{
		const auto latency = controller_->getProcessor()->getLatencySamples();
		_RPT1(0, "LATENCY CHANGED: %d\n", latency);

		controller_->host_->setLatency(latency);

		return kResultOk;
	}
	*/

	if ((kIoChanged | kLatencyChanged | kReloadComponent) & flags)
	{
#ifdef _WIN32
		_RPT0(0, "restartComponent\n");
#endif
// TODO??		controller_->host_->setLatency(-1);
	}

	if (kParamValuesChanged & flags)
	{
		controller_->stateDirty = true;
	}

	return kResultOk;
}

tresult VstComponentHandler::setDirty(Steinberg::TBool state)
{
	controller_->stateDirty = true;
	return kResultOk;
}

gmpi::ReturnCode ControllerWrapper::registerProcessor(Steinberg::Vst::IComponent** component, Steinberg::Vst::IAudioProcessor** vstEffect)
{
	processor_component_ptr = component;
//	processor_vstEffect__ptr = vstEffect;

	if (processor_component_ptr) // && processor_vstEffect__ptr)
    {
        *component = plugin->component.get();
        (*component)->queryInterface(IAudioProcessor::iid, (void**)vstEffect);
    }

	return gmpi::ReturnCode::Ok;
}

void ControllerWrapper::setParameterFromEditor(uint32_t paramId, double valueNormalized)
{
	for (auto& p : parametersToProcessor)
	{
		if (p->id == paramId)
		{
			p->normalized.store(valueNormalized, std::memory_order_release);
			p->dirty.store(true, std::memory_order_release);

			parameters_dirty.store(true, std::memory_order_release);
			break;
		}
	}
}
