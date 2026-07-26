// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Minimal native Shell Namespace Extension with a system-themed XAML Island.
// No context-menu handler is implemented. Slow work remains out of process.
// -----------------------------------------------------------------------------
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <strsafe.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <atomic>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Text.h>

#include "..\ExplorerWelcome.NativeUi\broker_client.h"
#include "..\ExplorerWelcome.NativeUi\welcome_page.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Xaml::Media;

namespace
{
const CLSID kNamespaceClsid =
{ 0xa714cffa, 0xa7b2, 0x49fd, { 0x9f, 0x15, 0xe4, 0x2b, 0x1a, 0xef, 0xbc, 0xa5 } };

std::atomic<long> g_objectCount{ 0 };
std::atomic<long> g_serverLocks{ 0 };
extern HINSTANCE g_instance;

UIElement BuildWelcomeContent()
{
    auto root = Grid();
    root.RequestedTheme(ElementTheme::Default);
    root.Padding(Thickness{ 28, 24, 28, 24 });
    root.Background(SolidColorBrush(ColorHelper::FromArgb(255, 28, 28, 32)));

    auto card = Border();
    card.CornerRadius(CornerRadius{ 18, 18, 18, 18 });
    card.Padding(Thickness{ 24, 22, 24, 22 });
    card.Background(SolidColorBrush(ColorHelper::FromArgb(255, 43, 43, 49)));

    auto content = StackPanel();
    content.Spacing(10);

    auto eyebrow = TextBlock();
    eyebrow.Text(L"VIEWS · EXPLORER WELCOME");
    eyebrow.FontSize(12);
    eyebrow.Opacity(0.78);

    auto title = TextBlock();
    title.Text(L"Welcome back");
    title.FontSize(30);
    title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());

    auto subtitle = TextBlock();
    subtitle.Text(L"A calm, useful starting point for your files.");
    subtitle.FontSize(16);
    subtitle.TextWrapping(TextWrapping::Wrap);
    subtitle.Opacity(0.86);

    auto status = TextBlock();
    status.Text(L"Namespace Extension loaded · heavy work stays external");
    status.FontSize(13);
    status.Opacity(0.78);

    auto action = Button();
    action.Content(box_value(L"Check broker"));
    action.HorizontalAlignment(HorizontalAlignment::Left);
    action.Click([status](Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        status.Text(ExplorerWelcome::NativeUi::BrokerClient::Ping() ? L"Named pipe connected · heavy process ready" : L"Named pipe unavailable · Explorer view remains responsive");
    });

    content.Children().Append(eyebrow);
    content.Children().Append(title);
    content.Children().Append(subtitle);
    content.Children().Append(status);
    content.Children().Append(action);
    card.Child(content);
    root.Children().Append(card);
    return root;
}

class EmptyEnum final : public IEnumIDList
{
public:
    EmptyEnum() { ++g_objectCount; }
    ~EmptyEnum() { --g_objectCount; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid == IID_IUnknown || riid == IID_IEnumIDList)
        {
            *object = static_cast<IEnumIDList*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --m_refCount;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Next(ULONG count, PITEMID_CHILD* items, ULONG* fetched) override
    {
        if (!items || (count != 1 && !fetched)) return E_INVALIDARG;
        if (fetched) *fetched = 0;
        return S_FALSE;
    }

    HRESULT STDMETHODCALLTYPE Skip(ULONG) override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE Reset() override { return S_OK; }

    HRESULT STDMETHODCALLTYPE Clone(IEnumIDList** result) override
    {
        if (!result) return E_POINTER;
        *result = new EmptyEnum();
        return S_OK;
    }

private:
    std::atomic<ULONG> m_refCount{ 1 };
};

class WelcomeShellView final : public IShellView
{
public:
    WelcomeShellView() { ++g_objectCount; }
    ~WelcomeShellView()
    {
        DestroyViewWindow();
        --g_objectCount;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid == IID_IUnknown || riid == IID_IOleWindow || riid == IID_IShellView)
        {
            *object = static_cast<IShellView*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --m_refCount;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE GetWindow(HWND* window) override
    {
        if (!window) return E_POINTER;
        *window = m_viewWindow;
        return m_viewWindow ? S_OK : E_FAIL;
    }

    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(MSG*) override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UIActivate(UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Refresh() override { return S_OK; }

    HRESULT STDMETHODCALLTYPE CreateViewWindow(IShellView*, LPCFOLDERSETTINGS settings, IShellBrowser* browser, RECT* viewRect, HWND* window) override
    {
        if (!browser || !viewRect || !window) return E_POINTER;
        *window = nullptr;
        if (m_viewWindow) return E_UNEXPECTED;

        m_browser = browser;
        m_browser->AddRef();

        HWND parent{};
        HRESULT hr = m_browser->GetWindow(&parent);
        if (FAILED(hr)) return hr;

        WNDCLASSW windowClass{};
        windowClass.hInstance = g_instance;
        windowClass.lpfnWndProc = ViewWindowProc;
        windowClass.lpszClassName = L"ViewsExplorerWelcomeNamespaceView";
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&windowClass);

        m_viewWindow = CreateWindowExW(
            0,
            windowClass.lpszClassName,
            L"Views Explorer Welcome",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            viewRect->left,
            viewRect->top,
            viewRect->right - viewRect->left,
            viewRect->bottom - viewRect->top,
            parent,
            nullptr,
            g_instance,
            this);

        if (!m_viewWindow)
        {
            m_browser->Release();
            m_browser = nullptr;
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (settings)
        {
            m_folderSettings = *settings;
        }

        try
        {
            m_xamlManager = WindowsXamlManager::InitializeForCurrentThread();
            m_xamlSource = DesktopWindowXamlSource();
            auto interop = m_xamlSource.as<IDesktopWindowXamlSourceNative>();
            winrt::check_hresult(interop->AttachToWindow(m_viewWindow));
            winrt::check_hresult(interop->get_WindowHandle(&m_xamlChild));
            m_xamlSource.Content(ExplorerWelcome::NativeUi::BuildWelcomePage(
                ExplorerWelcome::NativeUi::BrokerClient::Ping,
                [this](std::wstring const& action, std::wstring const& target)
                {
                    if (action == L"folder.open" && NavigateFolderInCurrentView(target))
                    {
                        return;
                    }
                    ExplorerWelcome::NativeUi::BrokerClient::LaunchActionAsync(action, target);
                },
                {},
                ExplorerWelcome::NativeUi::BrokerClient::RequestSnapshot,
                ExplorerWelcome::NativeUi::BrokerClient::LoadCachedSnapshot()));
            ResizeXamlChild();
        }
        catch (const winrt::hresult_error& error)
        {
            SetWindowTextW(m_viewWindow, error.message().c_str());
        }

        *window = m_viewWindow;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DestroyViewWindow() override
    {
        if (m_viewWindow)
        {
            DestroyWindow(m_viewWindow);
            m_viewWindow = nullptr;
        }
        CloseXaml();
        if (m_browser)
        {
            m_browser->Release();
            m_browser = nullptr;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurrentInfo(LPFOLDERSETTINGS settings) override
    {
        if (!settings) return E_POINTER;
        *settings = m_folderSettings;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE AddPropertySheetPages(DWORD, LPFNADDPROPSHEETPAGE, LPARAM) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SaveViewState() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SelectItem(PCUITEMID_CHILD, SVSIF) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetItemObject(UINT, REFIID, void**) override { return E_NOINTERFACE; }

private:
    static LRESULT CALLBACK ViewWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* view = reinterpret_cast<WelcomeShellView*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            view = static_cast<WelcomeShellView*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        }

        if (view)
        {
            switch (message)
            {
            case WM_SIZE:
                view->ResizeXamlChild();
                return 0;
            case WM_DESTROY:
                view->CloseXaml();
                return 0;
            default:
                break;
            }
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    void ResizeXamlChild()
    {
        if (m_xamlChild && m_viewWindow)
        {
            RECT rect{};
            GetClientRect(m_viewWindow, &rect);
            SetWindowPos(
                m_xamlChild,
                nullptr,
                0,
                0,
                rect.right - rect.left,
                rect.bottom - rect.top,
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }

    bool NavigateFolderInCurrentView(std::wstring const& target)
    {
        if (!m_browser || target.empty() || target.size() > 4096 || target.find(L'\0') != std::wstring::npos)
        {
            return false;
        }

        PIDLIST_ABSOLUTE item{};
        SFGAOF attributes = SFGAO_FOLDER;
        const HRESULT parseResult = SHParseDisplayName(
            target.c_str(),
            nullptr,
            &item,
            SFGAO_FOLDER,
            &attributes);
        if (FAILED(parseResult) || !item || (attributes & SFGAO_FOLDER) == 0)
        {
            if (item)
            {
                CoTaskMemFree(item);
            }
            return false;
        }

        // IShellBrowser is the documented public route for same-view navigation.
        const HRESULT browseResult = m_browser->BrowseObject(
            item,
            SBSP_ABSOLUTE | SBSP_SAMEBROWSER);
        CoTaskMemFree(item);
        return SUCCEEDED(browseResult);
    }

    void CloseXaml()
    {
        if (m_xamlSource)
        {
            m_xamlSource.Close();
            m_xamlSource = nullptr;
        }
        m_xamlChild = nullptr;
        m_xamlManager = nullptr;
    }

    std::atomic<ULONG> m_refCount{ 1 };
    IShellBrowser* m_browser{};
    HWND m_viewWindow{};
    HWND m_xamlChild{};
    FOLDERSETTINGS m_folderSettings{ FVM_ICON, FWF_NONE };
    DesktopWindowXamlSource m_xamlSource{ nullptr };
    WindowsXamlManager m_xamlManager{ nullptr };
};

class WelcomeFolder final : public IShellFolder, public IPersistFolder2
{
public:
    WelcomeFolder() { ++g_objectCount; }
    ~WelcomeFolder()
    {
        if (m_rootPidl) CoTaskMemFree(m_rootPidl);
        --g_objectCount;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid == IID_IUnknown || riid == IID_IShellFolder)
        {
            *object = static_cast<IShellFolder*>(this);
        }
        else if (riid == IID_IPersist || riid == IID_IPersistFolder || riid == IID_IPersistFolder2)
        {
            *object = static_cast<IPersistFolder2*>(this);
        }
        else
        {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --m_refCount;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE ParseDisplayName(HWND, LPBC, LPOLESTR, ULONG*, PIDLIST_RELATIVE*, ULONG*) override { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE EnumObjects(HWND, SHCONTF, IEnumIDList** result) override
    {
        if (!result) return E_POINTER;
        *result = new EmptyEnum();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE BindToObject(PCUIDLIST_RELATIVE, LPBC, REFIID, void**) override { return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE BindToStorage(PCUIDLIST_RELATIVE, LPBC, REFIID, void**) override { return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE CompareIDs(LPARAM, PCUIDLIST_RELATIVE, PCUIDLIST_RELATIVE) override { return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0); }

    HRESULT STDMETHODCALLTYPE CreateViewObject(HWND, REFIID riid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid == IID_IShellView)
        {
            auto* view = new WelcomeShellView();
            *object = static_cast<IShellView*>(view);
            return S_OK;
        }
        // Deliberately no context-menu surface.
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetAttributesOf(UINT count, PCUITEMID_CHILD_ARRAY, SFGAOF* attributes) override
    {
        if (!attributes) return E_POINTER;
        if (count == 0)
        {
            *attributes = SFGAO_FOLDER | SFGAO_HASSUBFOLDER;
        }
        else
        {
            *attributes &= SFGAO_FOLDER;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetUIObjectOf(HWND, UINT, PCUITEMID_CHILD_ARRAY, REFIID, UINT*, void**) override
    {
        // No context-menu, drag-drop, property-sheet, or file-item UI object.
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetDisplayNameOf(PCUITEMID_CHILD, SHGDNF, STRRET* name) override
    {
        if (!name) return E_POINTER;
        name->uType = STRRET_WSTR;
        const wchar_t text[] = L"Views Explorer Welcome";
        const size_t bytes = (wcslen(text) + 1) * sizeof(wchar_t);
        name->pOleStr = static_cast<wchar_t*>(CoTaskMemAlloc(bytes));
        if (!name->pOleStr) return E_OUTOFMEMORY;
        memcpy(name->pOleStr, text, bytes);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetNameOf(HWND, PCUITEMID_CHILD, LPCOLESTR, SHGDNF, PITEMID_CHILD*) override { return E_ACCESSDENIED; }

    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* classId) override
    {
        if (!classId) return E_POINTER;
        *classId = kNamespaceClsid;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Initialize(PCIDLIST_ABSOLUTE pidl) override
    {
        if (m_rootPidl) CoTaskMemFree(m_rootPidl);
        m_rootPidl = pidl ? ILCloneFull(pidl) : nullptr;
        return pidl && !m_rootPidl ? E_OUTOFMEMORY : S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetCurFolder(PIDLIST_ABSOLUTE* pidl) override
    {
        if (!pidl) return E_POINTER;
        *pidl = m_rootPidl ? ILCloneFull(m_rootPidl) : nullptr;
        return m_rootPidl && !*pidl ? E_OUTOFMEMORY : S_OK;
    }

private:
    std::atomic<ULONG> m_refCount{ 1 };
    PIDLIST_ABSOLUTE m_rootPidl{};
};

class ShellClassFactory final : public IClassFactory
{
public:
    ShellClassFactory() { ++g_objectCount; }
    ~ShellClassFactory() { --g_objectCount; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid == IID_IUnknown || riid == IID_IClassFactory)
        {
            *object = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --m_refCount;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** object) override
    {
        if (outer) return CLASS_E_NOAGGREGATION;
        if (!object) return E_POINTER;
        *object = nullptr;
        auto* folder = new WelcomeFolder();
        const HRESULT hr = folder->QueryInterface(riid, object);
        folder->Release();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override
    {
        if (lock) ++g_serverLocks;
        else --g_serverLocks;
        return S_OK;
    }

private:
    std::atomic<ULONG> m_refCount{ 1 };
};

HINSTANCE g_instance{};
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** object)
{
    if (!object) return E_POINTER;
    *object = nullptr;
    if (!IsEqualCLSID(rclsid, kNamespaceClsid)) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new ShellClassFactory();
    const HRESULT hr = factory->QueryInterface(riid, object);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return g_objectCount.load() == 0 && g_serverLocks.load() == 0 ? S_OK : S_FALSE;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_instance = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
