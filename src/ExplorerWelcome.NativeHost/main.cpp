// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Lightweight Win32/C++/WinRT host for a system-themed XAML Island.
// The host owns only the shell-facing surface; heavy work stays out of process.
// -----------------------------------------------------------------------------
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <string>
#include <string_view>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "..\ExplorerWelcome.NativeUi\welcome_page.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Xaml::Media;

namespace
{
HWND g_xamlChild{};
DesktopWindowXamlSource g_xamlSource{ nullptr };
WindowsXamlManager g_xamlManager{ nullptr };

std::string EscapeJson(std::wstring_view value)
{
    std::string escaped;
    for (const wchar_t character : value)
    {
        switch (character)
        {
        case L'\\': escaped += "\\\\"; break;
        case L'"': escaped += "\\\""; break;
        case L'\r': escaped += "\\r"; break;
        case L'\n': escaped += "\\n"; break;
        case L'\t': escaped += "\\t"; break;
        default:
            if (character >= 0x20 && character <= 0x7e)
            {
                escaped.push_back(static_cast<char>(character));
            }
            else
            {
                escaped += "?";
            }
            break;
        }
    }
    return escaped;
}

bool SendBrokerRequest(std::string const& request, std::string& response)
{
    HANDLE pipe = CreateFileW(
        LR"(\\.\pipe\views-explorer-welcome-poc)",
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (pipe == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    const std::string line = request + "\n";
    DWORD written{};
    const bool sent = WriteFile(pipe, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) != FALSE;

    char responseBuffer[4096]{};
    DWORD read{};
    const bool received = sent && ReadFile(pipe, responseBuffer, sizeof(responseBuffer) - 1, &read, nullptr) != FALSE;
    CloseHandle(pipe);

    if (received)
    {
        response.assign(responseBuffer, read);
    }
    return received;
}

bool PingBroker()
{
    std::string response;
    return SendBrokerRequest(
        R"({"version":2,"type":"host.ping","correlationId":"native-host-ping"})",
        response) && response.find("host.pong") != std::string::npos;
}

void LaunchBrokerAction(std::wstring const& action, std::wstring const& target)
{
    const std::string request =
        "{\"version\":2,\"type\":\"action.request\",\"correlationId\":\"native-host-action\",\"action\":\"" +
        EscapeJson(action) + "\",\"target\":\"" + EscapeJson(target) + "\"}";
    std::string ignored;
    SendBrokerRequest(request, ignored);
}

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
    status.Text(L"Native XAML Island ready · broker not checked");
    status.FontSize(13);
    status.Opacity(0.78);

    auto action = Button();
    action.Content(box_value(L"Check heavy process"));
    action.HorizontalAlignment(HorizontalAlignment::Left);
    action.Click([status](Windows::Foundation::IInspectable const&, RoutedEventArgs const&)
    {
        status.Text(PingBroker() ? L"Named pipe connected · heavy process ready" : L"Named pipe unavailable · host remains responsive");
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

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (g_xamlChild != nullptr)
        {
            MoveWindow(g_xamlChild, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        }
        return 0;
    case WM_DESTROY:
        g_xamlSource.Close();
        g_xamlManager = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

HWND CreateHostWindow(HINSTANCE instance)
{
    const wchar_t className[] = L"ViewsExplorerWelcomeNativeHost";
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    winrt::check_bool(RegisterClassW(&windowClass) != 0);

    return CreateWindowExW(
        0,
        className,
        L"Views Explorer Welcome — feasibility study",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        720,
        480,
        nullptr,
        nullptr,
        instance,
        nullptr);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    try
    {
        winrt::init_apartment(apartment_type::single_threaded);
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        HWND window = CreateHostWindow(instance);
        winrt::check_bool(window != nullptr);

        g_xamlManager = WindowsXamlManager::InitializeForCurrentThread();
        g_xamlSource = DesktopWindowXamlSource();
        auto interop = g_xamlSource.as<IDesktopWindowXamlSourceNative>();
        winrt::check_hresult(interop->AttachToWindow(window));
        winrt::check_hresult(interop->get_WindowHandle(&g_xamlChild));
        g_xamlSource.Content(ExplorerWelcome::NativeUi::BuildWelcomePage(
            PingBroker,
            [](std::wstring const& action, std::wstring const& target)
            {
                LaunchBrokerAction(action, target);
            }));

        ShowWindow(window, showCommand);
        UpdateWindow(window);

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        winrt::uninit_apartment();
        return static_cast<int>(message.wParam);
    }
    catch (const winrt::hresult_error& error)
    {
        MessageBoxW(nullptr, error.message().c_str(), L"Views Explorer Welcome POC", MB_ICONERROR | MB_OK);
        return 1;
    }
}
