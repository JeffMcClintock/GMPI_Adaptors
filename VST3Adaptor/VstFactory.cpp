#include "VstFactory.h"
#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include "myPluginProvider.h"
#include "helpers/GmpiPluginEditor.h"
#include "ProcessorWrapper.h"
#include "EditButtonGui.h"
#include "AaVstWrapperDiagGui.h"
#include "VstwrapperfailGui.h"

#if defined(_WIN32)
#include "shlobj.h"
#endif

#define INFO_PLUGIN_ID "GMPI: VST3 ADAPTOR"
#define PARAM_SET_PLUGIN_ID "GMPI: VST3 Param Set"

using namespace std;
using namespace gmpi;
using namespace Steinberg;
using namespace Steinberg::Vst;

const char* VstFactory::pluginIdPrefix = "gmpiVST3ADAPTOR:";

// The Controller registers itself once it knows its handle.
// The API contract guarantees the Controller outlives the Processor, so it always registers first.
void VstFactory::registerWrapper(int32_t handle, ControllerWrapper* controller)
{
	std::lock_guard<std::mutex> lock(registryMutex);
	auto& entry = registry[handle];
	assert(!entry.processor); // Controller should always register before its Processor.
	entry.controller = controller;
}

// The Processor registers itself once it knows its handle. Returns the Controller, which registered already.
ControllerWrapper* VstFactory::registerWrapper(int32_t handle, ProcessorWrapper* processor)
{
	std::lock_guard<std::mutex> lock(registryMutex);
	auto& entry = registry[handle];
	entry.processor = processor;
	return entry.controller;
}

void VstFactory::unregisterWrapper(int32_t handle, ControllerWrapper* controller)
{
	std::lock_guard<std::mutex> lock(registryMutex);
	if (auto it = registry.find(handle); it != registry.end() && it->second.controller == controller)
	{
		assert(!it->second.processor); // Controller should always outlive its Processor.
		registry.erase(it);
	}
}

void VstFactory::unregisterWrapper(int32_t handle, ProcessorWrapper* processor)
{
	std::lock_guard<std::mutex> lock(registryMutex);
	if (auto it = registry.find(handle); it != registry.end() && it->second.processor == processor)
	{
		// prevent the controller from writing through its pointers into the (deceased) processor.
		if (it->second.controller)
			it->second.controller->onProcessorRemoved();

		it->second.processor = nullptr;
		if (!it->second.controller)
			registry.erase(it);
	}
}

ControllerWrapper* VstFactory::getController(int32_t handle)
{
	std::lock_guard<std::mutex> lock(registryMutex);
	auto it = registry.find(handle);
	return it == registry.end() ? nullptr : it->second.controller;
}

gmpi::ReturnCode VstFactory::createInstance(const char* uniqueId, gmpi::api::PluginSubtype subtype, void** returnInterface)
{
	loadPluginInfo();

	*returnInterface = nullptr; // if we fail for any reason, default return-val to NULL.

	if (strcmp(uniqueId, INFO_PLUGIN_ID) == 0)
	{
		if (gmpi::api::PluginSubtype::Editor == subtype)
			*returnInterface = static_cast<gmpi::api::IEditor*>(new AaVstWrapperDiagGui());

		return *returnInterface ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	if (strcmp(uniqueId, PARAM_SET_PLUGIN_ID) == 0)
	{
		if (gmpi::api::PluginSubtype::Audio == subtype)
			*returnInterface = static_cast<gmpi::api::IProcessor*>(new Vst3ParamSet());

		return *returnInterface ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
	}

	const auto vstUniqueId = uuidFromWrapperID(uniqueId);

	for (auto& pluginInfo : plugins)
	{
		if (pluginInfo.uuid == vstUniqueId)
		{
			switch ((int)subtype)
			{
			case (int) gmpi::api::PluginSubtype::Audio:
				*returnInterface = static_cast<void*>(new ProcessorWrapper());
				break;

			case (int) gmpi::api::PluginSubtype::Controller:
				*returnInterface = static_cast<gmpi::api::IController*>(new ControllerWrapper(pluginInfo.shellPath.c_str(), vstUniqueId));
				break;

			case (int) gmpi::api::PluginSubtype::Editor:
				*returnInterface = static_cast<gmpi::api::IEditor*>(new EditButtonGui());
				break;

			default:
				break;
			}

			return *returnInterface ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
		}
	}

	if (gmpi::api::PluginSubtype::Editor == subtype)
		*returnInterface = static_cast<gmpi::api::IEditor*>(new VstwrapperfailGui("Can't find VST3 Plugin:" + vstUniqueId));

	return *returnInterface ? gmpi::ReturnCode::Ok : gmpi::ReturnCode::Fail;
}

gmpi::ReturnCode VstFactory::getPluginInformation(int32_t index, gmpi::api::IString* returnXml)
{
	loadPluginInfo();

	if (0 == index)
	{
		ScanVsts();

		ostringstream oss;
		oss << "<Plugin id=\"" << INFO_PLUGIN_ID << "\" name=\"Wrapper Info\" category=\"VST3 Plugins\" >"
			"<GUI graphicsApi=\"GmpiGui\"><Pin/></GUI>"
			"</Plugin>\n";

		const auto xml = oss.str();

		returnXml->setData(xml.data(), (int32_t)xml.size());
		return gmpi::ReturnCode::Ok;
	}
	else if (1 == index)
	{
		const std::string xml =
R"xml(
<Plugin id="GMPI: VST3 Param Set" name="VST3 Param Set" category="VST3 Plugins">
<Audio>
<Pin name="Signal In" datatype="float" />
<Pin name="Param Idx" datatype="int" default="0" />
<Pin name="ParamBuss" datatype="midi" direction="out" />
</Audio>
</Plugin>
)xml";
		returnXml->setData(xml.data(), (int32_t)xml.size());
		return gmpi::ReturnCode::Ok;
	}

	index -= 2; // adjust for info and param-set plugins.

	if (index >= 0 && index < (int)plugins.size())
	{
		const auto& info = plugins[index];

		ostringstream oss;
		oss << "<Plugin id=\"" << pluginIdPrefix << info.uuid << "\" name=\"" << info.name << "\" category=\"VST3 Plugins\" >";

		// Parameter to store state.
		oss << "<Parameters>";

		// parameter stores state from getChunk() / setChunk().
		oss << "<Parameter id=\"0\" name=\"chunk\" ignorePatchChange=\"true\" datatype=\"blob\" />";

		oss << "</Parameters>";

		// Controller.
		oss << "<Controller/>";

		// Audio.
		oss << "<Audio>";

		// Add Power and Tempo pins.
		// TODO !!! revise these, need auto sleep? it was a bit buggy i think
	//		<Pin name = "Auto Sleep" datatype = "bool" isMinimised="true" />

		oss << R"XML(
	<Pin name = "Power/Bypass" datatype = "bool" default = "1" />
	<Pin name = "Host BPM" datatype = "float" hostConnect = "Time/BPM" />
	<Pin name = "Host SP" datatype = "float" hostConnect = "Time/SongPosition" />
	<Pin name = "Host Transport" datatype = "bool" hostConnect = "Time/TransportPlaying" />
	<Pin name = "Numerator" datatype = "int" hostConnect = "Time/Timesignature/Numerator" />
	<Pin name = "Denominator" datatype = "int" hostConnect = "Time/Timesignature/Denominator" />
	<Pin name = "Host Bar Start" datatype = "float" hostConnect = "Time/BarStartPosition" />
	<Pin datatype = "int" hostConnect = "Processor/OfflineRenderMode" />
)XML";

//		if (info.numMidiInputs)
		{
			oss << "<Pin name=\"MIDI In\" direction=\"in\" datatype=\"midi\" />";
		}

		// Direct parameter access via MIDI
		oss << "<Pin name=\"ParamBuss\" direction=\"in\" datatype=\"midi\" />";

		for (int i = 0; i < info.numInputs; ++i)
			oss << "<Pin name=\"Signal In\" datatype=\"float\" rate=\"audio\" />";

		for (int i = 0; i < info.numOutputs; ++i)
			oss << "<Pin name=\"Signal Out\" direction=\"out\" datatype=\"float\" rate=\"audio\" />";

		oss << "</Audio>";

		// GUI.
		oss << "<GUI graphicsApi=\"GmpiGui\" />\n</Plugin>\n";

		const auto xml = oss.str();

		returnXml->setData(xml.data(), (int32_t)xml.size());
		return gmpi::ReturnCode::Ok;
	}

	return gmpi::ReturnCode::Fail;
}

std::string VstFactory::uuidFromWrapperID(const char* uniqueId)
{
	const auto prefixLength = strlen(pluginIdPrefix);
	assert(strlen(pluginIdPrefix) >= prefixLength);

	if(strlen(uniqueId) <= prefixLength)
	{
		return {};
	}

	return uniqueId + prefixLength;
}

vector<std::string> getSearchPaths()
{
	vector<std::string> searchPaths;

#ifdef _WIN32
	// Use standard VST3 folder.
	char myPath[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, SHGFP_TYPE_CURRENT, myPath);
	std::string commonFilesFolder(myPath);

	// "C:\Program Files (x86)\Common Files\VST3
	searchPaths.push_back(commonFilesFolder + "\\VST3");
#else
    searchPaths.push_back("/Library/Audio/Plug-ins/VST3");
    searchPaths.push_back("~/Library/Audio/Plug-ins/VST3");
#endif
    
	return searchPaths;
}

void VstFactory::ScanVsts()
{
	scannedPlugins = true;

	// Time to re-scan VSTs.
	plugins.clear();
	scannedFiles.clear();

	for (auto& itSp : getSearchPaths())
		ScanFolder(itSp);

	savePluginInfo();
}

void VstFactory::ScanFolder(const std::string searchPath)
{
	for (auto& p : std::filesystem::recursive_directory_iterator(searchPath))
	{
		auto path = p.path();

		if(path.extension().string() == ".vst3")
		{
			if(p.is_directory()) // handle bundles.
			{
				path = path / "Contents" /
#ifdef _WIN32
					"x86_64-win";
#else
					"MacOS";
#endif

				// scan fist file in there.
				for(auto& exe_path : std::filesystem::directory_iterator(path))
				{
					const auto executablePath = exe_path.path();
					if(executablePath.filename() == "." || executablePath.filename() == "..")
						continue;

					ScanDll(executablePath.string());
					break;
				}
			}
#ifdef _WIN32
			else // handle standalone dlls on Windows.
			{
				ScanDll(path.string());
			}
#endif
		}
	}
}

struct backgroundData
{
	VstFactory* factory{};
	const std::wstring* full_path;
	const char* shellName;
};

void VstFactory::ScanDll(const std::string& path)
{
	if (scannedFiles[path] == 1)
		return;

	scannedFiles[path] = 1;

	VST3::Hosting::Module::Ptr module;
	try
	{
		std::string error;
		module = VST3::Hosting::Module::create(path, error);
		if (!module)
		{
			// Could not create Module for file
			return;
		}

		auto factory = module->getFactory();
		for (auto& classInfo : factory.classInfos())
		{
			if (classInfo.category() == kVstAudioEffectClass)
			{
				const auto uuid = classInfo.ID().toString();
				const auto name = classInfo.name();

				myPluginProvider plugProvider;
				plugProvider.setup(factory, classInfo.ID());

				// No EditController found? (needed for allowing editor)
				if (!plugProvider.controller)
					return;

				int numInputs{};
				int numOutputs{};
				int numMidiInputs{};

				BusInfo busInfo = {};
				const int busIndex{};
				if (plugProvider.component->getBusInfo(kAudio, kInput, busIndex, busInfo) == kResultTrue)
					numInputs = busInfo.channelCount;

				if (plugProvider.component->getBusInfo(kAudio, kOutput, busIndex, busInfo) == kResultTrue)
					numOutputs = busInfo.channelCount;

				if (plugProvider.component->getBusInfo(kEvent, kInput, busIndex, busInfo) == kResultTrue)
					numMidiInputs = busInfo.channelCount;

				plugins.push_back({ uuid, name, path, numInputs, numOutputs, numMidiInputs });
			}
		}
	}
	catch (...) // PACE protected plugins will throw if dongle not present.
	{
		return;
	}
}

// Determine settings file: C:\Users\Jeff\AppData\Local\SeVst3Wrapper\ScannedPlugins.xml
std::wstring VstFactory::getSettingFilePath()
{
#if defined(_WIN32)
	wchar_t mySettingsPath[MAX_PATH]{};
	SHGetFolderPathW( NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, mySettingsPath );
	std::wstring meSettingsFile( mySettingsPath );
	meSettingsFile += L"\\GmpiVst3Adaptor";

	// Create folder if not already.
	const auto res = _wmkdir(meSettingsFile.c_str());
	if (res && EEXIST != errno)
	{
		// 13  EACCES            /* Permission denied
		_RPT2(0, "mkdir FAIL %d (%S)\n", errno, meSettingsFile.c_str());
		return {};
	}
	meSettingsFile += L"\\ScannedPlugins.xml";

	return meSettingsFile;
#else
	return {};
#endif
}

void VstFactory::savePluginInfo()
{
#if defined(_WIN32)
	ofstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		for( auto& p : plugins )
		{
			myfile << p.name << "\n";
			myfile << p.uuid << "\n";
			myfile << p.shellPath << "\n";
			myfile << p.numInputs << "\n";
			myfile << p.numOutputs << "\n";
			myfile << p.numMidiInputs << "\n";
		}
		myfile.close();
	}
#endif
}

void VstFactory::loadPluginInfo()
{
	if (scannedPlugins)
		return;

#if defined(_WIN32)
	ifstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		string name, id, path;
		std::getline(myfile, name);
		while( !myfile.eof() )
		{
			std::getline(myfile, id);
			std::getline(myfile, path);

			string temp;
			char* end{};
			std::getline(myfile, temp);
			const int numInputs = strtol(temp.c_str(), &end, 10);

			std::getline(myfile, temp);
			const int numOutputs = strtol(temp.c_str(), &end, 10);

			std::getline(myfile, temp);
			const int numMidiInputs = strtol(temp.c_str(), &end, 10);

			plugins.push_back({ id, name, path, numInputs, numOutputs, numMidiInputs });

			// next plugin.
			std::getline(myfile, name);
		}
		myfile.close();
		scannedPlugins = true;
	}
	else
	{
		ScanVsts();
	}
#endif
}

//---------------FACTORY --------------------

VstFactory* GetVstFactory()
{
	// Initialize on first use.  Ensures the factory is alive before any other object
	// including other global static objects (allows plugins to auto-register).
	static VstFactory theFactory;

	return &theFactory;
}

// This is the DLL's main entry point.  It returns the factory.
extern "C"

#ifdef _WIN32
__declspec (dllexport)
#else
#if defined (__GNUC__)
__attribute__((visibility("default")))
#endif
#endif

ReturnCode MP_GetFactory(void** returnInterface)
{
	if (!returnInterface)
		return ReturnCode::Fail;

	return GetVstFactory()->queryInterface(&gmpi::api::IUnknown::guid, returnInterface);
}


