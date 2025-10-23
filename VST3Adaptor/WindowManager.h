#pragma once

#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/samples/vst-hosting/editorhost/source/platform/iwindow.h"
#include "public.sdk/samples/vst-hosting/editorhost/source/platform/iplatform.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"

using namespace Steinberg;
using namespace Steinberg::Vst::EditorHost;

class WindowController : public IWindowController, public IPlugFrame
{
public:
	WindowController (const IPtr<IPlugView>& plugView);
	~WindowController () noexcept override;

	void onShow (IWindow& w) override;
	void onClose (IWindow& w) override;
	void onResize (IWindow& w, Size newSize) override;
	Size constrainSize (IWindow& w, Size requestedSize) override;
	void onContentScaleFactorChanged (IWindow& window, float newScaleFactor) override;

	// IPlugFrame
	tresult PLUGIN_API resizeView (IPlugView* view, ViewRect* newSize) override;

	void closePlugView ();
	void closeWindow();

	void destroyView();

	static void createPlatformWindow(std::shared_ptr<WindowController> windowController);

private:
	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) override
	{
		if (FUnknownPrivate::iidEqual (_iid, IPlugFrame::iid) ||
		    FUnknownPrivate::iidEqual (_iid, FUnknown::iid))
		{
			*obj = this;
			addRef ();
			return kResultTrue;
		}
		if (window)
			return window->queryInterface (_iid, obj);
		return kNoInterface;
	}
	uint32 PLUGIN_API addRef () override { return 1000; }
	uint32 PLUGIN_API release () override { return 1000; }

	IPtr<IPlugView> plugView;
	IWindow* window {nullptr};
	bool resizeViewRecursionGard {false};
	bool isInitialShow{ false };
};

