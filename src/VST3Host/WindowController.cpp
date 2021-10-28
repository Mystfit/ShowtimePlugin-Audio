#include "WindowController.h"
#include <showtime/ZstLogging.h>
#include <base/source/fdebug.h>

using namespace Steinberg;
using namespace Steinberg::Vst::EditorHost;
using namespace showtime;

WindowController::WindowController(const IPtr<IPlugView>& plugView) : plugView(plugView) {

}
WindowController::~WindowController() noexcept {

}

void WindowController::onShow(IWindow& w) {
	SMTG_DBPRT1("onShow called (%p)\n", (void*)&w);

	window = &w;
	if (!plugView)
		return;

	auto platformWindow = window->getNativePlatformWindow();
	if (plugView->isPlatformTypeSupported(platformWindow.type) != kResultTrue)
	{
		Log::app(Log::Level::error, "PlugView does not support platform type:{}", platformWindow.type);
		return;
	}

	plugView->setFrame(this);

	if (plugView->attached(platformWindow.ptr, platformWindow.type) != kResultTrue)
	{
		Log::app(Log::Level::error, "Attaching PlugView failed");
		return;
	}
}

void WindowController::onClose(Vst::EditorHost::IWindow& w) {
	SMTG_DBPRT1("onClose called (%p)\n", (void*)&w);

	closePlugView();
}

void WindowController::onResize(Vst::EditorHost::IWindow& w, Vst::EditorHost::Size newSize) {
	SMTG_DBPRT1("onResize called (%p)\n", (void*)&w);

	if (plugView)
	{
		ViewRect r{};
		r.right = newSize.width;
		r.bottom = newSize.height;
		ViewRect r2{};
		if (plugView->getSize(&r2) == kResultTrue && r != r2)
			plugView->onSize(&r);
	}
}

Size WindowController::constrainSize(Vst::EditorHost::IWindow& w, Vst::EditorHost::Size requestedSize) {
	SMTG_DBPRT1("constrainSize called (%p)\n", (void*)&w);

	ViewRect r{};
	r.right = requestedSize.width;
	r.bottom = requestedSize.height;
	if (plugView && plugView->checkSizeConstraint(&r) != kResultTrue)
	{
		plugView->getSize(&r);
	}
	requestedSize.width = r.right - r.left;
	requestedSize.height = r.bottom - r.top;
	return requestedSize;
}

void WindowController::onContentScaleFactorChanged(IWindow& window, float newScaleFactor) {
	SMTG_DBPRT1("onContentScaleFactorChanged called (%p)\n", (void*)&window);

	FUnknownPtr<IPlugViewContentScaleSupport> css(plugView);
	if (css)
	{
		css->setContentScaleFactor(newScaleFactor);
	}
}

// IPlugFrame
tresult PLUGIN_API WindowController::resizeView(IPlugView* view, ViewRect* newSize) {
	SMTG_DBPRT1("resizeView called (%p)\n", (void*)view);

	if (newSize == nullptr || view == nullptr || view != plugView)
		return kInvalidArgument;
	if (!window)
		return kInternalError;
	if (resizeViewRecursionGard)
		return kResultFalse;
	ViewRect r;
	if (plugView->getSize(&r) != kResultTrue)
		return kInternalError;
	if (r == *newSize)
		return kResultTrue;

	resizeViewRecursionGard = true;
	Size size{ newSize->right - newSize->left, newSize->bottom - newSize->top };
	window->resize(size);
	resizeViewRecursionGard = false;
	if (plugView->getSize(&r) != kResultTrue)
		return kInternalError;
	if (r != *newSize)
		plugView->onSize(newSize);
	return kResultTrue;
}

void WindowController::closePlugView() {
	if (plugView)
	{
		plugView->setFrame(nullptr);
		if (plugView->removed() != kResultTrue)
		{
			Log::app(Log::Level::error, "Removing PlugView failed");
		}
		plugView = nullptr;
	}
	window = nullptr;
}

tresult PLUGIN_API WindowController::queryInterface(const TUID _iid, void** obj){
	if (FUnknownPrivate::iidEqual(_iid, IPlugFrame::iid) ||
		FUnknownPrivate::iidEqual(_iid, FUnknown::iid))
	{
		*obj = this;
		addRef();
		return kResultTrue;
	}
	if (window)
		return window->queryInterface(_iid, obj);
	return kNoInterface;
}
