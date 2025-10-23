#include "VstFactory.h"
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <filesystem>
#include <assert.h>
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

gmpi::ReturnCode VstFactory::createInstance(const char* uniqueId, gmpi::api::PluginSubtype subtype, void** returnInterface)
{
	if (!scannedPlugins)
	{
		loadPluginInfo();
	}

	*returnInterface = nullptr; // if we fail for any reason, default return-val to NULL.

	if (strcmp(uniqueId, INFO_PLUGIN_ID) == 0)
	{
		if (gmpi::api::PluginSubtype::Editor == subtype)
		{
			auto wp = new AaVstWrapperDiagGui();

			*returnInterface = static_cast<gmpi::api::IEditor*>(wp);

			return gmpi::ReturnCode::Ok;
		}
		return gmpi::ReturnCode::Fail;
	}

	if (strcmp(uniqueId, PARAM_SET_PLUGIN_ID) == 0)
	{
		if (gmpi::api::PluginSubtype::Audio == subtype)
		{
			auto wp = new Vst3ParamSet();

			*returnInterface = static_cast<gmpi::api::IProcessor*>(wp);

			return gmpi::ReturnCode::Ok;
		}
		return gmpi::ReturnCode::Fail;
	}
	const auto vstUniqueId = uuidFromWrapperID(uniqueId);

	for (auto& pluginInfo : plugins)
	{
		if (pluginInfo.uuid_ == vstUniqueId)
		{
			switch ((int)subtype)
			{
			case (int) gmpi::api::PluginSubtype::Audio:
			{
				auto wp = new ProcessorWrapper();

				*returnInterface = static_cast<void*>(wp);

				return gmpi::ReturnCode::Ok;
			}
			break;

			case (int) gmpi::api::PluginSubtype::Controller:
			{
				auto wp = new ControllerWrapper(pluginInfo.shellPath_.c_str(), vstUniqueId);

				*returnInterface = static_cast<gmpi::api::IController_x*>(wp);

				return gmpi::ReturnCode::Ok;
			}
			break;

			case (int) gmpi::api::PluginSubtype::Editor:
			{
				auto wp = new EditButtonGui();

				*returnInterface = static_cast<gmpi::api::IEditor*>(wp);

				return gmpi::ReturnCode::Ok;
			}
			break;

			default:
				return gmpi::ReturnCode::Fail;
				break;
			}
		}
	}

	if (gmpi::api::PluginSubtype::Editor == subtype)
	{
		string err("Error");
		{
			err = "Can't find VST3 Plugin:" + vstUniqueId;
			err += "\n";
		}

		auto wp = new VstwrapperfailGui();
		wp->errorMsg = err;

		*returnInterface = static_cast<gmpi::api::IEditor*>(wp);

		return gmpi::ReturnCode::Ok;
	}

	return gmpi::ReturnCode::Fail;
}

gmpi::ReturnCode VstFactory::getPluginInformation(int32_t index, gmpi::api::IString* returnXml)
{
	if (!scannedPlugins)
	{
		loadPluginInfo();
	}

	if (index >= 0 && index < (int)plugins.size())
	{
		returnXml->setData(plugins[index].xml_.data(), (int32_t)plugins[index].xml_.size());
		return gmpi::ReturnCode::Ok;
	}

	return gmpi::ReturnCode::Fail;
}

#if 0

gmpi::IString* returnXml{};

if (MP_OK != iReturnXml->queryInterface(MP_IID_RETURNSTRING, reinterpret_cast<void**>(&returnXml)))
{
	return gmpi::MP_NOSUPPORT;
}

if (!scannedPlugins)
{
	loadPluginInfo();
}

const auto uuid = uuidFromWrapperID(uniqueId);

std::string path;
for (auto& p : plugins)
{
	if (p.uuid_ == uuid)
	{
		path = WStringToUtf8(p.shellPath_);
		break;
	}
}

std::string error;
auto module = VST3::Hosting::Module::create(path, error);
if (!module)
{
	// Could not create Module for file
	return gmpi::MP_FAIL;
}

auto classID = VST3::UID::fromString(uuid);
if (!classID)
{
	return gmpi::MP_FAIL;
}

auto factory = module->getFactory();
const std::string xmlFull = XmlFromPlugin(factory, *classID);
returnXml->setData(xmlFull.data(), (int32_t)xmlFull.size());
return xmlFull.empty() ? gmpi::MP_FAIL : gmpi::MP_OK;

return gmpi::MP_FAIL;

#endif

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

vector< std::string >getSearchPaths()
{
	vector< std::string >searchPaths;

#ifdef _WIN32
	// Use standard VST3 folder.
	char myPath[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, SHGFP_TYPE_CURRENT, myPath);
	std::string commonFilesFolder(myPath);

	// "C:\Program Files (x86)\Common Files\VST3
	searchPaths.push_back(commonFilesFolder + "\\VST3");
#else
    searchPaths.push_back(L"/Library/Audio/Plug-ins/VST3");
    searchPaths.push_back(L"~/Library/Audio/Plug-ins/VST3");
#endif
    
	return searchPaths;
}

void VstFactory::ShallowScanVsts()
{
//	allPluginsXml = "<?xml version=\"1.0\" encoding=\"utf - 8\" ?>\n<PluginList>\n";

	for (auto& itSp : getSearchPaths())
		RecursiveScanVsts(itSp);

//	allPluginsXml += "\n</PluginList>\n";
}

void VstFactory::ScanVsts()
{
	scannedPlugins = true;

	// Time to re-scan VSTs.
	plugins.clear();
	scannedFiles.clear();
	duplicates.clear();

	// Always add 'Info' plugin. xml must be continuous line, no line breaks.
	{
		ostringstream oss;
		oss << "<Plugin id=\"" << INFO_PLUGIN_ID << "\" name=\"Wrapper Info\" category=\"VST3 Plugins\" >"
			"<GUI graphicsApi=\"GmpiGui\"><Pin/></GUI>"
			"</Plugin>\n";

		plugins.push_back({ INFO_PLUGIN_ID, "Info", oss.str(), {} });
	}
	{
		const auto xml =
R"xml(
<Plugin id="GMPI: VST3 Param Set" name="VST3 Param Set" category="VST3 Plugins">
<Audio>
<Pin name="Signal In" datatype="float" />
<Pin name="Param Idx" datatype="int" default="0" />
<Pin name="ParamBuss" datatype="midi" direction="out" />
</Audio>
</Plugin>
)xml"; 
		plugins.push_back({ PARAM_SET_PLUGIN_ID, "Info", xml, {} });
	}

	ShallowScanVsts();

	savePluginInfo();
}

void VstFactory::RecursiveScanVsts(const std::string searchPath)
{
	for (auto& p : std::filesystem::recursive_directory_iterator(searchPath))
	{
		auto path = p.path();

		if (!p.is_directory() && path.extension() == ".vst3")
		{
			// handle universal bundles
			auto parentFolder = path.parent_path();
			if (parentFolder.parent_path().stem() == "Contents")
			{
				const bool isMacBinary = parentFolder.stem() == "MacOS";
#ifdef _WIN32
				const bool scanit = !isMacBinary; // scan anything not specifically mac
#else
				const bool isWinBinary = parentFolder.stem() == "x86_64-win";
				const bool scanit = isMacBinary && !isWinBinary; // scan only if specifically mac
#endif
				if(scanit)
					ScanDll(path.generic_string());
			}
		}
	}
}

struct backgroundData
{
	VstFactory* factory{};
	const std::wstring* full_path;
	const char* shellName;
};

void VstFactory::ScanDll(const std::string /*platform_string*/& path)
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

		const char* category = "VST3 Plugins";
		const bool isWavesShell = path.find("WaveShell") != std::string::npos;
		if(isWavesShell)
		{
			auto lastSlash = path.find_last_of('/');
			if(lastSlash == std::string::npos)
			{
				lastSlash = path.find_last_of('\\');
			}
			if(lastSlash == std::string::npos)
			{
				lastSlash = 0;
			}

			category = path.c_str() + lastSlash + 1;
		}

		auto factory = module->getFactory();
		for (auto& classInfo : factory.classInfos())
		{
			if (classInfo.category() == kVstAudioEffectClass)
			{
//				AddPluginName(category, classInfo.ID().toString(), classInfo.name(), path); // a quick scan of name-only.

				const auto uuid = classInfo.ID().toString();
				const auto name = classInfo.name();
				const auto xml = XmlFromPlugin(factory, classInfo.ID(), name);

#if 0
				// Plugin ID's must be unique. Skip multiple waveshells.
				for (auto& p : plugins)
				{
					if (p.uuid_ == uuid)
					{
						ostringstream oss2;
						oss2 << std::hex << uuid << ":" << p.name_ << ", " << name;

						duplicates.push_back(oss2.str());
						return;
					}
				}
				ostringstream oss;
				oss <<
					"<Plugin id=\"" << pluginIdPrefix << uuid << "\" name=\"" << name << "\" category=\"" << category << "/\" >"
					"<Audio/>"
					"<Controller/>"
					"<GUI graphicsApi=\"GmpiGui\" />"
					"</Plugin>\n";
#endif

				plugins.push_back({ uuid, name, xml, path });
			}
		}
	}
	catch (...) // PACE protected plugins will throw if dongle not present.
	{
		return;
	}
}

std::string VstFactory::XmlFromPlugin(VST3::Hosting::PluginFactory& factory, const VST3::UID& classId, std::string name)
{
	myPluginProvider plugProvider;
	plugProvider.setup(factory, classId);

	if (!plugProvider.controller)
	{
		//No EditController found (needed for allowing editor)
		return {};
	}

	// TODO!!!: Hide and handle MIDI CC dummy parameters
#if 0
	// Gather parameter names.
	vector<string> paramNames;
	const auto parameterCount = plugProvider.controller->getParameterCount();
	for(int i = 0; i < parameterCount; ++i)
	{
		Steinberg::Vst::ParameterInfo info{};
		plugProvider.controller->getParameterInfo(i, info);

		paramNames.push_back(WStringToUtf8(info.title));
	}
#endif

	auto classIdString = classId.toString();

	//std::string name;
	//for (auto& p : plugins)
	//{
	//	if (p.uuid_ == classIdString)
	//	{
	//		name = p.name_;
	//		break;
	//	}
	//}

	ostringstream oss;
	oss << "<Plugin id=\"" << pluginIdPrefix << classIdString << "\" name=\"" << name << "\" category=\"VST3 Plugins\" >";

	// Parameter to store state.
	oss << "<Parameters>";

	int i = 0;
#if 0
	for (auto name : paramNames)
	{
		oss << "<Parameter id=\"" << std::dec << i << "\" name=\"" << name << "\" datatype=\"float\" metadata=\",,1,0\" />";
		++i;
	}
#endif

	// next-to last parameter stores state from getChunk() / setChunk().
	oss << "<Parameter id=\"" << std::dec << i << "\" name=\"chunk\" ignorePatchChange=\"true\" datatype=\"blob\" />";
	++i;

	// Provide parameter to share pointer to plugin.
	oss << "<Parameter id=\"" << std::dec << i << "\" name=\"effectptr\" ignorePatchChange=\"true\" persistant=\"false\" private=\"true\" datatype=\"blob\" />";

	oss << "</Parameters>";

	// Controller.
	oss << "<Controller/>";

	// instansiate Processor
	if(!plugProvider.component)
	{
		// No EditController found (needed for allowing editor)
		return {};
	}

	int numInputs{};
	int numOutputs{};
	int numMidiInputs{};

	BusInfo busInfo = {};
	const int busIndex{};
	if (plugProvider.component->getBusInfo (kAudio, kInput, busIndex, busInfo) == kResultTrue)
	{
		numInputs = busInfo.channelCount;
	}

	if(plugProvider.component->getBusInfo(kAudio, kOutput, busIndex, busInfo) == kResultTrue)
	{
		numOutputs = busInfo.channelCount;
	}

	if(plugProvider.component->getBusInfo(kEvent, kInput, busIndex, busInfo) == kResultTrue)
	{
		numMidiInputs = busInfo.channelCount;
	}

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

	auto controllerPointerParamId = i;
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << controllerPointerParamId << "\" private=\"true\" />";

	if(numMidiInputs)
	{
		oss << "<Pin name=\"MIDI In\" direction=\"in\" datatype=\"midi\" />";
	}

	// Direct parameter access via MIDI
	oss << "<Pin name=\"ParamBuss\" direction=\"in\" datatype=\"midi\" />";

	for (int i = 0; i < numInputs; ++i)
		oss << "<Pin name=\"Signal In\" datatype=\"float\" rate=\"audio\" />";

	for (int i = 0; i < numOutputs; ++i)
		oss << "<Pin name=\"Signal Out\" direction=\"out\" datatype=\"float\" rate=\"audio\" />";

#if 0
	// DSP pins for receiving normalised parameter values from GUI.
	for(int i = 0; i < parameterCount; ++i)
	{
		oss << "<Pin datatype=\"float\" parameterId=\"" << i << "\" />";
	}
#endif

	oss << "</Audio>";

	// GUI.
	oss << "<GUI graphicsApi=\"GmpiGui\" >";

	// aeffect ptr first.
	oss << "<Pin name=\"effectptr\" datatype=\"blob\" parameterId=\"" << controllerPointerParamId << "\" private=\"true\" />";

	oss << "</GUI>";

	oss << "</Plugin>\n";

	return oss.str();
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

std::string VstFactory::getDiagnostics()
{
	std::ostringstream oss;
/* todo
	if (pluginIdMap.empty())
	{
		oss << "Can't locate *WaveShell*.*\nSearched:\n";

		auto searchPaths = getSearchPaths();
		for (auto path : searchPaths)
		{
			oss << WStringToUtf8(path) << "\n";
		}
	}
	else
	{
		oss << "WaveShells located:";
		bool first = true;
		for (auto& it : pluginIdMap)
		{
			if (!first)
			{
				oss << ", ";
			}
			oss << WStringToUtf8(it.second);
			first = false;
		}
		oss << "\n";

		oss << plugins.size() - 1 << " Waves plugins available.\n\n";

		if (!duplicates.empty())
		{
			oss << "Skiped Duplicates:\n";
			for (auto s : duplicates)
			{
				oss << s << "\n";
			}
		}
	}
*/
//	oss << "Can't open VST Plugin. (not a vst plugin). (";
	return oss.str();
}

void VstFactory::savePluginInfo()
{
#if defined(_WIN32)
	ofstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		for( auto& p : plugins )
		{
			if (p.shellPath_.empty())
				continue;

			myfile << p.name_ << "\n";
			myfile << p.uuid_ << "\n";
//			myfile << p.xmlBrief_ << "\n";
			myfile << p.shellPath_ << "\n";
		}
		myfile.close();
	}
#endif
}

void VstFactory::loadPluginInfo()
{
#if defined(_WIN32)
#if 0 // for now
	ifstream myfile(getSettingFilePath());
	if( myfile.is_open() )
	{
		string name, id, xml, path;
		std::getline(myfile, name);
		while( !myfile.eof() )
		{
			std::getline(myfile, id);
//			std::getline(myfile, xml);
			std::getline(myfile, path);
			plugins.push_back({ id, name, xml, path });

			// next plugin.
			std::getline(myfile, name);
		}
		myfile.close();
		scannedPlugins = true;
		ShallowScanVsts();
	}
	else
#endif
	{
		ScanVsts();
	}
#endif
}

#ifdef _WIN32

// Define the entry point for the DLL
#ifdef _MANAGED
#pragma managed(push, off)
#endif

// temp. testing with MFC included.
#ifndef __AFXWIN_H__

// store dll instance handle, needed when creating windows and window classes.
HMODULE dllInstanceHandle;

extern "C"
__declspec (dllexport)
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved)
{
	dllInstanceHandle = hModule;
	return TRUE;
}
#endif

#ifdef _MANAGED
#pragma managed(pop)
#endif

#endif // _WIN32

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


