// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Graphics.Imaging.h>
using namespace winrt::Windows::Storage::Streams;

using namespace winrt;
using namespace Windows::Foundation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;

DWORD dwRes = 0, dwSize = sizeof(DWORD), dwOpacity = 0, dwLuminosity = 0, dwHide = 0;

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
template <typename T>
T convert_from_abi(com_ptr<::IInspectable> from)
{
	T to{ nullptr }; // `T` is a projected type.

	winrt::check_hresult(from->QueryInterface(winrt::guid_of<T>(),
		winrt::put_abi(to)));

	return to;
}
static com_ptr<IXamlDiagnostics> m_d;

DependencyObject FindDescendantByName(DependencyObject root, hstring name)
{
	if (root == nullptr)
	{
		return nullptr;
	}

	int count = VisualTreeHelper::GetChildrenCount(root);
	for (int i = 0; i < count; i++)
	{
		DependencyObject child = VisualTreeHelper::GetChild(root, i);
		if (child == nullptr)
		{
			continue;
		}

		hstring childName = child.GetValue(FrameworkElement::NameProperty()).as<hstring>();
		if (childName == name)
		{
			return child;
		}

		DependencyObject result = FindDescendantByName(child, name);
		if (result != nullptr)
		{
			return result;
		}
	}

	return nullptr;
}
struct VisualTreeWatcher : winrt::implements<VisualTreeWatcher, IVisualTreeServiceCallback2, winrt::non_agile>
{
public:
	VisualTreeWatcher(winrt::com_ptr<IUnknown> site);
	VisualTreeWatcher(const VisualTreeWatcher&) = delete;
	VisualTreeWatcher& operator=(const VisualTreeWatcher&) = delete;

	VisualTreeWatcher(VisualTreeWatcher&&) = delete;
	VisualTreeWatcher& operator=(VisualTreeWatcher&&) = delete;
private:
	HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation relation, VisualElement element, VisualMutationType mutationType) override;
	HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle element, VisualElementState elementState, LPCWSTR context) noexcept override;
	template<typename T>
	T FromHandle(InstanceHandle handle)
	{
		IInspectable obj;
		winrt::check_hresult(m_XamlDiagnostics->GetIInspectableFromHandle(handle, reinterpret_cast<::IInspectable**>(winrt::put_abi(obj))));

		return obj.as<T>();
	}

	winrt::com_ptr<IXamlDiagnostics> m_XamlDiagnostics;
};

VisualTreeWatcher::VisualTreeWatcher(winrt::com_ptr<IUnknown> site) :
	m_XamlDiagnostics(site.as<IXamlDiagnostics>())
{
	winrt::check_hresult(m_XamlDiagnostics.as<IVisualTreeService3>()->AdviseVisualTreeChange(this));
}
HRESULT VisualTreeWatcher::OnElementStateChanged(InstanceHandle, VisualElementState, LPCWSTR) noexcept
{
	return S_OK;
}
winrt::fire_and_forget Ball(ParentChildRelation relation, UIElement rootGrid)
{
	Grid gr = FindDescendantByName(rootGrid, L"SystemTrayFrameGrid").as<Grid>();

	// fix alignment
	auto NotifyIconStack = FindDescendantByName(gr, L"NotifyIconStack").as<Control>();
	Grid::SetColumn(NotifyIconStack, 1);
	auto NotificationAreaIcons = FindDescendantByName(gr, L"NotificationAreaIcons").as<Control>();
	Grid::SetColumn(NotificationAreaIcons, 2);

	// add button
	auto button = Button();
	button.Width(150);

	auto grid = Grid();
	static auto img = Image();
	img.Name(L"AlbumPfp");
	Thickness mar;
	mar.Left = 5;
	img.Margin(mar);
	img.Width(30);
	img.Height(30);
	grid.Children().Append(img);

	auto sp = StackPanel();
	static TextBlock songTitle;
	songTitle.Name(L"SongTitle");
	songTitle.Text(L"Unknown song");
	songTitle.FontSize(14);
	sp.Children().Append(songTitle);
	static TextBlock artistTitle;
	artistTitle.Name(L"ArtistTitle");
	artistTitle.Text(L"Unknown Artist");
	mar.Right = 5;
	mar.Left = 35;
	sp.Margin(mar);
	songTitle.MaxWidth(110);
	songTitle.TextTrimming(TextTrimming::CharacterEllipsis);
	artistTitle.MaxWidth(110);
	artistTitle.TextTrimming(TextTrimming::CharacterEllipsis);

	mar.Top = -3; mar.Left = mar.Right = mar.Bottom = 0;
	songTitle.Margin(mar);
	mar.Bottom = -5; mar.Top = -2;
	artistTitle.Margin(mar);
	artistTitle.FontSize(12);

	sp.Children().Append(artistTitle);
	grid.Children().Append(sp);

	button.Content(grid);
	gr.Children().InsertAt(0, button);
	button.Height(40);

	auto sessionManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
	auto session = sessionManager.GetCurrentSession();
	auto mediaprop = session.TryGetMediaPropertiesAsync().get();
	songTitle.Text(mediaprop.Title());
	artistTitle.Text(mediaprop.Artist());

	auto randomAccessStream = co_await mediaprop.Thumbnail().OpenReadAsync();
	auto decoder = co_await BitmapDecoder::CreateAsync(randomAccessStream);
	SoftwareBitmap softwareBitmap = co_await decoder.GetSoftwareBitmapAsync();
	Windows::UI::Xaml::Media::Imaging::WriteableBitmap writeableBitmap(softwareBitmap.PixelWidth(), softwareBitmap.PixelHeight());
	img.Source(writeableBitmap);
}
HRESULT VisualTreeWatcher::OnVisualTreeChange(ParentChildRelation relation, VisualElement element, VisualMutationType mutationType)
{
	if (mutationType == Add)
	{
		const std::wstring_view type{ element.Type, SysStringLen(element.Type) };
		if (type == L"Taskbar.TaskbarFrame")
		{
			const auto rootGrid = FromHandle<UIElement>(relation.Parent);
			Ball(relation, rootGrid);
			//winrt::Windows::UI::Xaml::Media::Imaging::BitmapImage bi;
			//bi.SetSourceAsync(mediaprop.Thumbnail().OpenReadAsync().get());
			//img.Source(bi);
		}
	}
	return S_OK;
}

struct ExplorerTAP : winrt::implements<ExplorerTAP, IObjectWithSite>
{
	HRESULT STDMETHODCALLTYPE SetSite(IUnknown* pUnkSite) noexcept override
	{
		site.copy_from(pUnkSite);
		com_ptr<IXamlDiagnostics> diag = site.as<IXamlDiagnostics>();
		com_ptr<VisualTreeWatcher> ok = winrt::make_self<VisualTreeWatcher>(site);

		/*
		com_ptr<::IInspectable> dispatcherPtr;
		diag->GetDispatcher(dispatcherPtr.put());
		CoreDispatcher dispatcher = convert_from_abi<CoreDispatcher>(dispatcherPtr);
		dispatcher.RunAsync(CoreDispatcherPriority::Normal, []()
			{



			});
		*/

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetSite(REFIID riid, void** ppvSite) noexcept override
	{
		return site.as(riid, ppvSite);
	}

private:
	winrt::com_ptr<IUnknown> site;
};

struct TAPFactory : winrt::implements<TAPFactory, IClassFactory>
{
	HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override try
	{
		*ppvObject = nullptr;

		if (pUnkOuter)
		{
			return CLASS_E_NOAGGREGATION;
		}

		return winrt::make<ExplorerTAP>().as(riid, ppvObject);
	}
	catch (...)
	{
		return winrt::to_hresult();
	}

	HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override
	{
		return S_OK;
	}
};
_Use_decl_annotations_ STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) try
{
	*ppv = nullptr;

	// TODO: move this somewhere common
	// {36162BD3-3531-4131-9B8B-7FB1A991EF51}
	static constexpr GUID temp =
	{ 0x36162bd3, 0x3531, 0x4131, { 0x9b, 0x8b, 0x7f, 0xb1, 0xa9, 0x91, 0xef, 0x51 } };
	if (rclsid == temp)
	{
		return winrt::make<TAPFactory>().as(riid, ppv);
	}
	else
	{
		return CLASS_E_CLASSNOTAVAILABLE;
	}
}
catch (...)
{
	return winrt::to_hresult();
}
_Use_decl_annotations_ STDAPI DllCanUnloadNow(void)
{
	if (winrt::get_module_lock())
	{
		return S_FALSE;
	}
	else
	{
		return S_OK;
	}
}
