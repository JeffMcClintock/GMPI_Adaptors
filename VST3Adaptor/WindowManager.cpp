#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "WindowManager.h"
#include "base/source/fdebug.h"

inline bool operator== (const ViewRect& r1, const ViewRect& r2)
{
	return memcmp (&r1, &r2, sizeof (ViewRect)) == 0;
}

inline bool operator!= (const ViewRect& r1, const ViewRect& r2)
{
	return !(r1 == r2);
}

//------------------------------------------------------------------------
WindowController::WindowController (const IPtr<IPlugView>& plugView) : plugView (plugView)
{
}

//------------------------------------------------------------------------
WindowController::~WindowController () noexcept
{
}

void WindowController::createPlatformWindow(std::shared_ptr<WindowController> windowController)
{
	auto& view = windowController->plugView;

	ViewRect plugViewSize{};
	auto result = view->getSize(&plugViewSize);
	if (result != kResultTrue)
	{
		// Could not get editor view size
		return;
	}

	const auto viewRect = ViewRectToRect(plugViewSize);

	auto window = IPlatform::instance().createWindow(
		"Editor", viewRect.size, view->canResize() == kResultTrue, windowController);

	if (!window)
	{
		// Could not create window
		return;
	}

	windowController->isInitialShow = true; // work arround resizing bug in JUCE plugins
	window->show();
	windowController->isInitialShow = false;
}


//------------------------------------------------------------------------
void WindowController::onShow (IWindow& w)
{
	SMTG_DBPRT1 ("onShow called (%p)\n", (void*)&w);

	window = &w;
	if (!plugView)
		return;

	auto platformWindow = window->getNativePlatformWindow ();
	if (plugView->isPlatformTypeSupported (platformWindow.type) != kResultTrue)
	{
		IPlatform::instance ().kill (-1, std::string ("PlugView does not support platform type:") +
		                                     platformWindow.type);
	}

	plugView->setFrame (this);

	if (plugView->attached (platformWindow.ptr, platformWindow.type) != kResultTrue)
	{
		IPlatform::instance ().kill (-1, "Attaching PlugView failed");
	}
}

//------------------------------------------------------------------------
void WindowController::closePlugView ()
{
	if (plugView)
	{
		plugView->setFrame (nullptr);
		if (plugView->removed () != kResultTrue)
		{
			IPlatform::instance ().kill (-1, "Removing PlugView failed");
		}
		plugView = nullptr;
	}
	window = nullptr;
}

void WindowController::closeWindow()
{
	if(window)
	{
		window->close();
	}
}

void WindowController::destroyView()
{
	closeWindow();
	closePlugView();
}

//------------------------------------------------------------------------
void WindowController::onClose (IWindow& w)
{
	SMTG_DBPRT1 ("onClose called (%p)\n", (void*)&w);

	closePlugView ();
}

//------------------------------------------------------------------------
void WindowController::onResize (IWindow& w, Size newSize)
{
	SMTG_DBPRT1 ("onResize called (%p)\n", (void*)&w);

	if (plugView)
	{
		ViewRect r {};
		r.right = newSize.width;
		r.bottom = newSize.height;
		ViewRect r2 {};
		if (plugView->getSize (&r2) == kResultTrue && r != r2)
			plugView->onSize (&r);
	}
}

//------------------------------------------------------------------------
Size WindowController::constrainSize (IWindow& w, Size requestedSize)
{
	SMTG_DBPRT1 ("constrainSize called (%p)\n", (void*)&w);

	ViewRect r {};
	r.right = requestedSize.width;
	r.bottom = requestedSize.height;
	if (plugView && plugView->checkSizeConstraint (&r) != kResultTrue)
	{
		plugView->getSize (&r);
	}
	requestedSize.width = r.right - r.left;
	requestedSize.height = r.bottom - r.top;
	return requestedSize;
}

//------------------------------------------------------------------------
void WindowController::onContentScaleFactorChanged (IWindow& w, float newScaleFactor)
{
	SMTG_DBPRT1 ("onContentScaleFactorChanged called (%p)\n", (void*)&w);

	FUnknownPtr<IPlugViewContentScaleSupport> css (plugView);
	if (css)
	{
		css->setContentScaleFactor (newScaleFactor);
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API WindowController::resizeView (IPlugView* view, ViewRect* newSize)
{
	SMTG_DBPRT1 ("resizeView called (%p)\n", (void*)view);

	if (newSize == nullptr || view == nullptr || view != plugView)
		return kInvalidArgument;
	if (!window)
		return kInternalError;
	if (resizeViewRecursionGard)
		return kResultFalse;
	ViewRect r;
	if (plugView->getSize (&r) != kResultTrue)
		return kInternalError;
	if (r == *newSize && !isInitialShow)
		return kResultTrue;

	resizeViewRecursionGard = true;
	Size size {newSize->right - newSize->left, newSize->bottom - newSize->top};
	window->resize (size);
	resizeViewRecursionGard = false;
	if (plugView->getSize (&r) != kResultTrue)
		return kInternalError;
	if (r != *newSize || isInitialShow)
		plugView->onSize (newSize);
	return kResultTrue;
}

