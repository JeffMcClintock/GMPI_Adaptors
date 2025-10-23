#pragma once
#include <map>
#include <unordered_map>
#include <vector>

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting\hostclasses.h"

#include "GmpiApiCommon.h"
#include "RefCountMacros.h"

class VstFactory* GetVstFactory();

// VstFactory - a singleton object. The plugin registers it's ID with the factory.
class VstFactory : public gmpi::api::IPluginFactory
{
	struct pluginInfo
	{
		std::string uuid;
		std::string name;
		std::string shellPath;

		int numInputs{};
		int numOutputs{};
		int numMidiInputs{};
	};

	std::vector<pluginInfo> plugins;
	std::unordered_map<std::string, int> scannedFiles;

	bool scannedPlugins = {};
	static const char* pluginIdPrefix;
	Steinberg::Vst::HostApplication pluginContext;

public:
	virtual ~VstFactory() = default;

	gmpi::ReturnCode createInstance(const char* id, gmpi::api::PluginSubtype subtype, void** returnInterface) override;
	gmpi::ReturnCode getPluginInformation(int32_t index, gmpi::api::IString* returnXml) override;
	std::string uuidFromWrapperID(const char* uniqueId);

private:
	void ScanVsts();
	void ScanDll(const std::string& load_filename);
	void ScanFolder(const std::string searchPath);

	std::wstring getSettingFilePath();

	void savePluginInfo();
	void loadPluginInfo();

public:
	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IPluginFactory);
	GMPI_REFCOUNT_NO_DELETE;
};