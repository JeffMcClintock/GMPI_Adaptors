#pragma once

#include <memory>
#include <atomic>
#include "WindowManager.h"
#include "Core/GmpiApiEditor.h"
#include "helpers/Timer.h"
#include "Hosting/message_queues.h"
#include "RefCountMacros.h"

namespace VST3
{
	namespace Hosting
	{
		class Module;
	}
}
namespace Steinberg
{
namespace Vst
{
	class IComponent;
	class IAudioProcessor;
}
}

class VstComponentHandler;
class myPluginProvider;

class ControllerWrapper : public gmpi::api::IController, public gmpi::TimerClient
{
protected:
	static const int chunkParamId = 0;

	std::string filename_;
	std::string shellPluginId_;
	std::shared_ptr<VST3::Hosting::Module> dll;
	std::shared_ptr<WindowController> windowController;
	std::unique_ptr<VstComponentHandler> componentHandler;

	bool inhibitFeedback = {};
	bool isSynthEditPresetEmpty = {};
	bool isOpen = {};
	// we need a way of informing SE processor when plugin is unloaded.
	// we do so by nulling it's pointers to the VST3s processor.
	Steinberg::Vst::IComponent** processor_component_ptr = {};
	gmpi::hosting::lock_free_fifo m_message_que_dsp_to_ui;

	void serviceQueue();

public:
	std::atomic<bool> parameters_dirty;
	struct vstParameterVal
	{
		uint32_t id = {};
		std::atomic<bool> dirty;
		std::atomic<float> normalized = {};
	};

	std::vector<std::unique_ptr<vstParameterVal>> parametersToProcessor; // communicated parameter changes from editor to processor

	std::unique_ptr<myPluginProvider> plugin;
	int32_t handle_ = -1;
	bool stateDirty = {};
	gmpi::api::IControllerHost* host_ = {};

	ControllerWrapper(const char* filename, const std::string& uuid);
	~ControllerWrapper();
	
	void setParameterFromProcessorUnsafe(uint32_t paramId, double valueNormalized);

	// IParameterObserver
	gmpi::ReturnCode setParameter(int32_t parameterIndex, gmpi::Field fieldId, int32_t voice, int32_t size, const uint8_t* data) override;

	// IController_x
	gmpi::ReturnCode initialize(gmpi::api::IUnknown* host, int32_t handle) override;
	gmpi::ReturnCode syncState() override;

	gmpi::ReturnCode open();

#if 0
	virtual int32_t MP_STDCALL setHost(gmpi::IMpUnknown* host) override;
	virtual int32_t MP_STDCALL setParameter(int32_t parameterHandle, int32_t fieldId, int32_t voice, const void* data, int32_t size) override;
	virtual int32_t MP_STDCALL preSaveState() override;
	virtual int32_t MP_STDCALL open() override;
	virtual int32_t MP_STDCALL setPinDefault(int32_t, int32_t, const char*) override
	{
		return gmpi::MP_OK;
	}

	virtual int32_t MP_STDCALL setPin(int32_t, int32_t, int64_t, const void*) override
	{
		return gmpi::MP_OK;
	}
	virtual int32_t MP_STDCALL notifyPin(int32_t, int32_t) override
	{
		return gmpi::MP_OK;
	}
	int32_t MP_STDCALL onDelete() override
	{
		return gmpi::MP_OK;
	}
#endif

	gmpi::ReturnCode LoadPlugin(std::string path, std::string uuid);
	bool onTimer() override;
	void OpenGui();

	gmpi::ReturnCode registerProcessor(Steinberg::Vst::IComponent**, Steinberg::Vst::IAudioProcessor**);
	void setParameterFromEditor(uint32_t paramId, double valueNormalized);

	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IController)
	GMPI_REFCOUNT;
};


