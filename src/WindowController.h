#pragma once

#include "platform/iwindow.h"
#include "platform/iplatform.h"
#ifdef WIN32
#include "platform/win32/window.h"
#endif
#include <pluginterfaces/gui/iplugview.h>
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"


namespace Steinberg {
	inline bool operator== (const ViewRect& r1, const ViewRect& r2)
	{
		return memcmp(&r1, &r2, sizeof(ViewRect)) == 0;
	}

	//------------------------------------------------------------------------
	inline bool operator!= (const ViewRect& r1, const ViewRect& r2)
	{
		return !(r1 == r2);
	}

	namespace Vst {
		namespace EditorHost {
			class WindowController : public IWindowController, public IPlugFrame
			{
			public:
				WindowController(const IPtr<IPlugView>& plugView);
				~WindowController() noexcept override;

				void onShow(IWindow& w) override;

				void onClose(Vst::EditorHost::IWindow& w) override;

				void onResize(Vst::EditorHost::IWindow& w, Vst::EditorHost::Size newSize) override;

				Size constrainSize(Vst::EditorHost::IWindow& w, Vst::EditorHost::Size requestedSize) override;

				void onContentScaleFactorChanged(IWindow& window, float newScaleFactor) override;

				// IPlugFrame
				tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* newSize) override;

				void closePlugView();

			private:
				tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) override;
				uint32 PLUGIN_API addRef() override { return 1000; }
				uint32 PLUGIN_API release() override { return 1000; }

				IPtr<IPlugView> plugView;
				Vst::EditorHost::IWindow* window{ nullptr };
				bool resizeViewRecursionGard{ false };
			};

		} //namespace EditorHost
	} //namespace Vst
} // namespace Steinberg