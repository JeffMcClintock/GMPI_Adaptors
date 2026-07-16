#pragma once

#include "helpers/GmpiPluginEditor.h"
#include "Drawing.h"
#include "ControllerWrapper.h"
#include "myPluginProvider.h"
#include "VstFactory.h"

using namespace gmpi;
using namespace gmpi::editor;
using namespace gmpi::drawing;

struct EditButtonGui final : public PluginEditor
{
	static const int border = 2;
	std::string shellPluginId_;
	std::string filename_;

	ControllerWrapper* controller_ = {};

	ReturnCode initialize() override
	{
		// obtain our other half (the Controller) from the factory. We share the same host-assigned handle.
		controller_ = GetVstFactory()->getController(editorHost->getHandle());

		return PluginEditor::initialize();
	}

	ReturnCode render(gmpi::drawing::api::IDeviceContext* drawingContext) override
	{
		Graphics g(drawingContext);

		ClipDrawingToBounds x(g, bounds);

		// Background Fill.
		bool isCaptured{};
		inputHost->getCapture(isCaptured);

		g.clear(isCaptured ? 0xE8E8Eff : 0xE8E8E8u);

		// Outline.
		auto brush = g.createSolidColorBrush(0x969696u);
		g.drawRectangle(bounds, brush);

		brush.setColor(0x000032u);

		std::string txt = "EDIT";

		if (!controller_ || !controller_->plugin->controller)
		{
			txt = "LOADFAIL";
			brush.setColor(Colors::Red);
		}

		const auto textRect = inflateRect(bounds, -2);
		auto textFormat = g.getFactory().createTextFormat();
		g.drawTextU(txt, textFormat, textRect, brush);

		return ReturnCode::Ok;
	}

	gmpi::ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t flags) override
	{
		// Let host handle right-clicks.
		if (!controller_ || (flags & static_cast<int32_t>(gmpi::api::PointerFlags::FirstButton)) == 0)
			return ReturnCode::Unhandled;

		inputHost->setCapture();
		drawingHost->invalidateRect({});

		return ReturnCode::Ok;
	}

	gmpi::ReturnCode onPointerUp(gmpi::drawing::Point point, int32_t flags) override
	{
		bool isCaptured{};
		inputHost->getCapture(isCaptured);

		if (!isCaptured)
			return ReturnCode::Unhandled;

		inputHost->releaseCapture();
		drawingHost->invalidateRect({});

		controller_->OpenGui();
		return ReturnCode::Ok;
	}

	ReturnCode measure(const gmpi::drawing::Size* availableSize, gmpi::drawing::Size* returnDesiredSize) override
	{
		returnDesiredSize->width = 100;
		returnDesiredSize->height = 30;
		return ReturnCode::Ok;
	}
};
