#pragma once
#include "helpers/GmpiPluginEditor.h"
#include "Drawing.h"
#include "VstFactory.h"

using namespace gmpi;
using namespace gmpi::editor;
using namespace gmpi::drawing;

struct AaVstWrapperDiagGui final : public PluginEditor
{

ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) override
{
	Graphics g(drawingContext);

	ClipDrawingToBounds x(g, bounds);

	g.clear(Colors::White);

	auto textFormat = g.getFactory().createTextFormat();
	auto brush = g.createSolidColorBrush(Colors::Black);

	g.drawTextU(GetVstFactory()->getDiagnostics(), textFormat, bounds, brush);

	return ReturnCode::Ok;
}

};