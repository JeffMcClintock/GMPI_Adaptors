#include "helpers/GmpiPluginEditor.h"
#include "Drawing.h"

using namespace gmpi;
using namespace gmpi::editor;
using namespace gmpi::drawing;

class VST3AdaptorGui final : public PluginEditor
{
 	void onSeteffectptr()
	{
		// pineffectptr changed
	}

 	Pin<blob> pineffectptr;

public:
	VST3AdaptorGui()
	{
	}

	ReturnCode render(gmpi::drawing::api::IDeviceContext *drawingContext) override
	{
		Graphics g(drawingContext);

		auto textFormat = g.getFactory().createTextFormat();
		auto brush = g.createSolidColorBrush(Colors::Red);

		g.drawTextU("Hello World!", textFormat, bounds, brush);

		return ReturnCode::Ok;
};

namespace
{
	auto r = sesdk::Register<VST3AdaptorGui>::withId(L"My VST3 Adaptor");
}
