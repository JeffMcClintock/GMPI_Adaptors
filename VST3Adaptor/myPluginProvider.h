#pragma once

#include <atomic>
#include <vector>

#include "pluginterfaces/base/smartpointer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"

class myPluginProvider
{
public:
	Steinberg::IPtr<Steinberg::Vst::IComponent> component;
	Steinberg::IPtr<Steinberg::Vst::IEditController> controller;
	Steinberg::Vst::HostApplication pluginContext;
	std::atomic<bool> isActive_;
	std::vector<int32_t> nativeParamIds;

	~myPluginProvider()
	{
		terminatePlugin();
	}

	void terminatePlugin()
	{
		disconnectComponents();

		bool controllerIsComponent = false;
		if (component)
		{
			controllerIsComponent = Steinberg::FUnknownPtr<Steinberg::Vst::IEditController>(component).getInterface() != nullptr;
			component->terminate();
		}

		if (controller && controllerIsComponent == false)
			controller->terminate();

		component = nullptr;
		controller = nullptr;
	}

	bool connectComponents()
	{
		Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint> compICP(component);
		Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint> contrICP(controller);

		return compICP
			&& contrICP
			&& Steinberg::kResultOk == compICP->connect(contrICP)
			&& Steinberg::kResultOk == contrICP->connect(compICP);
	}

	bool disconnectComponents()
	{
		Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint> compICP(component);
		Steinberg::FUnknownPtr<Steinberg::Vst::IConnectionPoint> contrICP(controller);

		return compICP
			&& contrICP
			&& Steinberg::kResultOk == compICP->disconnect(contrICP)
			&& Steinberg::kResultOk == contrICP->disconnect(compICP);
	}

	bool setup(const VST3::Hosting::PluginFactory& factory, const VST3::UID& classId)
	{
		bool res = false;

		//---create Plug-in here!--------------
		// create its component part
		component = factory.createInstance<Steinberg::Vst::IComponent>(classId);
		if (component)
		{
			// initialize the component with our context
			res = (component->initialize(&pluginContext) == Steinberg::kResultOk);

			// try to create the controller part from the component
			// (for Plug-ins which did not succeed to separate component from controller)
			if (component->queryInterface(Steinberg::Vst::IEditController::iid, (void**)&controller) != Steinberg::kResultTrue)
			{
				Steinberg::TUID controllerCID;

				// ask for the associated controller class ID
				if (component->getControllerClassId(controllerCID) == Steinberg::kResultTrue)
				{
					// create its controller part created from the factory
					controller = factory.createInstance<Steinberg::Vst::IEditController>(VST3::UID(controllerCID));
					if (controller)
					{
						// initialize the component with our context
						res = (controller->initialize(&pluginContext) == Steinberg::kResultOk);
					}
				}
			}
		}

		if (res)
		{
			connectComponents();

			{
				const auto parameterCount = controller->getParameterCount();
				nativeParamIds.reserve(parameterCount);
				for (int i = 0; i < parameterCount; ++i)
				{
					Steinberg::Vst::ParameterInfo info{};
					controller->getParameterInfo(i, info);
					nativeParamIds.push_back(info.id);
				}
			}
		}

		return res;
	}

	void setActive(bool isActive)
	{
		if (component)
		{
			isActive_ = isActive;
			component->setActive(isActive);
		}
		else
		{
			isActive_ = false;
		}
	}

	bool getActive() const
	{
		return isActive_;
	}

	int32_t toParamIndex(Steinberg::Vst::ParamID paramId)
	{
		auto it = std::find(nativeParamIds.begin(), nativeParamIds.end(), static_cast<int32_t>(paramId));
		if (it != nativeParamIds.end())
		{
			return (int32_t)(std::distance(nativeParamIds.begin(), it));
		}

		assert(false);
		return -1;
	}
};
