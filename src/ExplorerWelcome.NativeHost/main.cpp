// =============================================================================
// Views Explorer Welcome POC
// File: src/ExplorerWelcome.NativeHost/main.cpp
// Purpose: Lightweight Win32/C++/WinRT host for a system-themed XAML Island. The host owns only the shell-facing surface; heavy work stays out of process.
//
// Copyright (c) 2026 Umberto Giacobbi
// Author: Umberto Giacobbi
// Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
// License: PolyForm Noncommercial License 1.0.0
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Open source: https://umbertogiacobbi.biz/opensource
// =============================================================================

#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

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
HWND g_xamlChild{};
DesktopWindowXamlSource g_xamlSource{ nullptr };
WindowsXamlManager g_xamlManager{ nullptr };

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
        status.Text(ExplorerWelcome::NativeUi::BrokerClient::Ping() ? L"Named pipe connected · heavy process ready" : L"Named pipe unavailable · host remains responsive");
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
            SetWindowPos(
                g_xamlChild,
                nullptr,
                0,
                0,
                LOWORD(lParam),
                HIWORD(lParam),
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
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
        L"Views Explorer Welcome - feasibility study",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1440,
        900,
        nullptr,
        nullptr,
        instance,
        nullptr);
}

void PumpPendingMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand)
{
    const bool lifetimeSmoke =
        commandLine && _wcsicmp(commandLine, L"--lifetime-smoke") == 0;
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

        if (lifetimeSmoke)
        {
            RECT clientRect{};
            winrt::check_bool(GetClientRect(window, &clientRect) != FALSE);
            winrt::check_bool(SetWindowPos(
                g_xamlChild,
                nullptr,
                0,
                0,
                clientRect.right - clientRect.left,
                clientRect.bottom - clientRect.top,
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW) != FALSE);

            for (int iteration = 0; iteration < 50; ++iteration)
            {
                g_xamlSource.Content(ExplorerWelcome::NativeUi::BuildWelcomePage(
                    {},
                    {},
                    {},
                    {},
                    {}));
                PumpPendingMessages();
                g_xamlSource.Content(nullptr);
                PumpPendingMessages();
            }

            DestroyWindow(window);
            winrt::uninit_apartment();
            return 0;
        }

        g_xamlSource.Content(ExplorerWelcome::NativeUi::BuildWelcomePage(
            ExplorerWelcome::NativeUi::BrokerClient::Ping,
            [](std::wstring const& action, std::wstring const& target)
            {
                ExplorerWelcome::NativeUi::BrokerClient::LaunchActionAsync(action, target);
            },
            {},
            ExplorerWelcome::NativeUi::BrokerClient::RequestSnapshot,
            ExplorerWelcome::NativeUi::BrokerClient::LoadCachedSnapshot()));

        ShowWindow(window, showCommand);
        UpdateWindow(window);

        RECT clientRect{};
        winrt::check_bool(GetClientRect(window, &clientRect) != FALSE);
        winrt::check_bool(SetWindowPos(
            g_xamlChild,
            nullptr,
            0,
            0,
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW) != FALSE);
        UpdateWindow(g_xamlChild);

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
        if (!lifetimeSmoke)
        {
            MessageBoxW(nullptr, error.message().c_str(), L"Views Explorer Welcome POC", MB_ICONERROR | MB_OK);
        }
        return 1;
    }
}
