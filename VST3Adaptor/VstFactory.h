#pragma once
/*
#include "VstFactory.h"
*/

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

//	std::string allPluginsXml;


	bool scannedPlugins = {};
	static const char* pluginIdPrefix;
	Steinberg::Vst::HostApplication pluginContext;

public:
	virtual ~VstFactory() = default;

	gmpi::ReturnCode createInstance(const char* id, gmpi::api::PluginSubtype subtype, void** returnInterface) override;
	gmpi::ReturnCode getPluginInformation(int32_t index, gmpi::api::IString* returnXml) override;

#if 0
	/* IMpUnknown methods */
	GMPI_QUERYINTERFACE_METHOD(gmpi::api::IPluginFactory);
	GMPI_REFCOUNT_NO_DELETE

	/* IMpFactory methods */
	virtual int32_t MP_STDCALL createInstance(
		const wchar_t* uniqueId,
		int32_t subType,
		IMpUnknown* host,
		void** returnInterface);

	virtual int32_t MP_STDCALL createInstance2(
		const wchar_t* uniqueId,
		int32_t subType,
		void** returnInterface);

	virtual int32_t MP_STDCALL getSdkInformation(int32_t& returnSdkVersion, int32_t maxChars, wchar_t* returnCompilerInformation);

	// IMpShellFactory: Query a plugin's info.
	virtual int32_t MP_STDCALL getPluginIdentification(int32_t index, IMpUnknown* iReturnXml) override;	// ID and name only.
	std::string uuidFromWrapperID(const wchar_t* uniqueId);
	virtual int32_t MP_STDCALL getPluginInformation(const wchar_t* iid, IMpUnknown* iReturnXml) override;		// Full pin details.
#endif
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