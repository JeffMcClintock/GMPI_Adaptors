#pragma once
#include <map>
#include <unordered_map>
#include <vector>

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting\hostclasses.h"

#include "GmpiApiCommon.h"
#include "RefCountMacros.h"

typedef gmpi::api::IUnknown* (*MP_CreateFunc2)();

class VstFactory* GetVstFactory();

// VstFactory - a singleton object. The plugin registers it's ID with the factory.
class VstFactory : public gmpi::api::IPluginFactory
{
	struct pluginInfo
	{
		std::string uuid_;
		std::string name_;
		std::string xml_;
		std::string shellPath_;
	};

	std::vector< pluginInfo > plugins;
	std::unordered_map<std::string, int> scannedFiles;
	std::vector< std::string > duplicates;

	bool scannedPlugins = {};
	static const char* pluginIdPrefix;
	Steinberg::Vst::HostApplication pluginContext;

public:
	virtual ~VstFactory() = default;

	gmpi::ReturnCode createInstance(const char* id, gmpi::api::PluginSubtype subtype, void** returnInterface) override;
	gmpi::ReturnCode getPluginInformation(int32_t index, gmpi::api::IString* returnXml) override;
	std::string uuidFromWrapperID(const char* uniqueId);
	std::string XmlFromPlugin(VST3::Hosting::PluginFactory& factory, const VST3::UID& classId, std::string name);
	std::string getDiagnostics();

private:
	void ShallowScanVsts();
	void ScanVsts();
	void ScanDll(const std::string& load_filename);

	void RecursiveScanVsts(const std::string searchPath);

	std::wstring getSettingFilePath();

	void savePluginInfo();
	void loadPluginInfo();

public:
	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IPluginFactory);
	GMPI_REFCOUNT_NO_DELETE;
};