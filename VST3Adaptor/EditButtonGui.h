#pragma once

#include "helpers/GmpiPluginEditor.h"
#include "Drawing.h"
#include "ControllerWrapper.h"
#include "myPluginProvider.h"

using namespace gmpi;
using namespace gmpi::editor;
using namespace gmpi::drawing;

struct EditButtonGui final : public PluginEditor
{
	static const int border = 2;
	std::string shellPluginId_;
	std::string filename_;

	ControllerWrapper* controller_ = {};
	static const int controllertPtrPinId = 0;

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

	ReturnCode setPin(int32_t PinIndex, int32_t voice, int32_t size, const uint8_t* data) override
	{
		if (controllertPtrPinId == PinIndex && size == sizeof(void*))
		{
			controller_ = *(ControllerWrapper**)data;
		}

		return PluginEditor::setPin(PinIndex, voice, size, data);
	}

	gmpi::ReturnCode onPointerDown(gmpi::drawing::Point point, int32_t flags) override
	{
		// Let host handle right-clicks.
		if (!controller_ || (flags & gmpi::api::GG_POINTER_FLAG_FIRSTBUTTON) == 0)
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
