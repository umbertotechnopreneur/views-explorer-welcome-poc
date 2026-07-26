// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Shared WinRT XAML visual shell for the standalone host and Shell view.
// -----------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>

#include "dashboard_snapshot.h"

namespace ExplorerWelcome::NativeUi
{
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Shapes;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;

using ActionCallback = std::function<void(std::wstring const& action, std::wstring const& target)>;
using SnapshotCallback = std::function<bool(NativeSnapshot& snapshot, std::wstring& summary)>;
using ApplySnapshotCallback = std::function<void(NativeSnapshot const& snapshot)>;

inline void RunSnapshotRefreshAsync(
    winrt::Windows::UI::Xaml::Controls::TextBlock const& status,
    SnapshotCallback const& snapshot,
    ApplySnapshotCallback const& apply)
{
    auto dispatcher = status.Dispatcher();
    std::thread([status, dispatcher, snapshot, apply]
    {
        NativeSnapshot result;
        std::wstring summary;
        const bool available = snapshot && snapshot(result, summary);
        if (summary.empty())
        {
            summary = available ? L"Snapshot aggiornato" : L"Broker non disponibile · dati locali mantenuti";
        }

        dispatcher.RunAsync(
            winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
            [status, available, result = std::move(result), summary, apply]
            {
                if (available && apply)
                {
                    apply(result);
                }
                status.Text(winrt::hstring(summary));
            });
    }).detach();
}

inline winrt::Windows::UI::Xaml::Controls::TextBlock MakeText(
    winrt::hstring const& value,
    double size,
    double opacity = 1.0)
{
    using namespace winrt::Windows::UI::Xaml::Controls;
    auto text = TextBlock();
    text.Text(value);
    text.FontSize(size);
    text.Opacity(opacity);
    return text;
}

inline winrt::Windows::UI::Xaml::Controls::Border MakeCard(
    Thickness const& padding = Thickness{ 18, 16, 18, 16 })
{
    auto card = Border();
    card.CornerRadius(CornerRadius{ 14, 14, 14, 14 });
    card.Padding(padding);
    card.BorderThickness(Thickness{ 1, 1, 1, 1 });
    card.BorderBrush(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(92, 255, 255, 255)));
    card.Background(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(164, 35, 35, 39)));
    return card;
}

inline winrt::Windows::UI::Xaml::GridLength Star(double value = 1.0)
{
    return GridLength{ value, GridUnitType::Star };
}

struct MetricCardElements
{
    Border Card{ nullptr };
    TextBlock Value{ nullptr };
    TextBlock Detail{ nullptr };
    ProgressBar Progress{ nullptr };
};

inline MetricCardElements MakeMetricCard(
    winrt::hstring const& label,
    winrt::hstring const& value,
    winrt::hstring const& detail,
    double progress)
{
    auto card = MakeCard(Thickness{ 14, 12, 14, 12 });
    auto panel = StackPanel();
    panel.Spacing(6);
    panel.Children().Append(MakeText(label, 12, 0.78));
    auto valueText = MakeText(value, 22);
    panel.Children().Append(valueText);
    auto bar = ProgressBar();
    bar.Minimum(0);
    bar.Maximum(100);
    bar.Value(progress);
    bar.Height(4);
    bar.Foreground(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(255, 91, 174, 255)));
    panel.Children().Append(bar);
    auto detailText = MakeText(detail, 11, 0.72);
    panel.Children().Append(detailText);
    card.Child(panel);
    return MetricCardElements{ card, valueText, detailText, bar };
}

inline winrt::Windows::UI::Xaml::Controls::Border MakeStorageCard(
    winrt::hstring const& title,
    winrt::hstring const& used,
    winrt::hstring const& format,
    double progress,
    std::wstring const& target,
    winrt::Windows::UI::Xaml::Controls::TextBlock const& status,
    ActionCallback const& action,
    winrt::hstring const& healthText = L"Stato sconosciuto")
{
    auto card = MakeCard(Thickness{ 16, 14, 16, 14 });
    auto panel = StackPanel();
    panel.Spacing(7);
    auto header = StackPanel();
    header.Spacing(2);
    auto titleText = MakeText(title, 14);
    titleText.TextTrimming(TextTrimming::CharacterEllipsis);
    header.Children().Append(titleText);
    auto health = MakeText(healthText, 11, 0.78);
    header.Children().Append(health);
    panel.Children().Append(header);
    auto bar = ProgressBar();
    bar.Minimum(0);
    bar.Maximum(100);
    bar.Value(progress);
    bar.Height(5);
    bar.Foreground(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(255, 71, 170, 255)));
    panel.Children().Append(bar);
    auto footer = Grid();
    footer.ColumnDefinitions().Append(ColumnDefinition());
    footer.ColumnDefinitions().Append(ColumnDefinition());
    footer.ColumnDefinitions().GetAt(0).Width(Star());
    footer.ColumnDefinitions().GetAt(1).Width(Star());
    footer.Children().Append(MakeText(used, 11, 0.78));
    auto fs = MakeText(format, 11, 0.72);
    fs.HorizontalAlignment(HorizontalAlignment::Right);
    Grid::SetColumn(fs, 1);
    footer.Children().Append(fs);
    panel.Children().Append(footer);
    auto open = Button();
    open.Content(winrt::box_value(L"Apri in Explorer"));
    open.HorizontalAlignment(HorizontalAlignment::Left);
    open.MinHeight(40);
    open.Click([target, status, action](auto const&, auto const&)
    {
        if (action) action(L"folder.open", target);
        status.Text(L"Richiesta inviata al broker");
    });
    panel.Children().Append(open);
    card.Child(panel);
    return card;
}

inline winrt::Windows::UI::Xaml::Controls::Border MakeListCard(
    winrt::hstring const& title,
    std::vector<std::pair<winrt::hstring, winrt::hstring>> const& rows,
    winrt::Windows::UI::Xaml::Controls::TextBlock const& status,
    ActionCallback const& action)
{
    auto card = MakeCard();
    auto panel = StackPanel();
    panel.Spacing(10);
    panel.Children().Append(MakeText(title, 16));
    for (auto const& row : rows)
    {
        auto button = Button();
        button.HorizontalContentAlignment(HorizontalAlignment::Left);
        button.HorizontalAlignment(HorizontalAlignment::Stretch);
        button.MinHeight(40);
        auto rowGrid = Grid();
        rowGrid.ColumnDefinitions().Append(ColumnDefinition());
        rowGrid.ColumnDefinitions().Append(ColumnDefinition());
        rowGrid.ColumnDefinitions().GetAt(0).Width(Star());
        rowGrid.ColumnDefinitions().GetAt(1).Width(GridLength{ 90, GridUnitType::Pixel });
        rowGrid.Children().Append(MakeText(row.first, 13));
        auto detail = MakeText(row.second, 11, 0.68);
        detail.HorizontalAlignment(HorizontalAlignment::Right);
        Grid::SetColumn(detail, 1);
        rowGrid.Children().Append(detail);
        button.Content(rowGrid);
        button.Click([status, action, title, row](auto const&, auto const&)
        {
            if (action) action(L"item.open", std::wstring(row.first.c_str()));
            status.Text(winrt::hstring(L"Azione richiesta: ") + title);
        });
        panel.Children().Append(button);
    }
    card.Child(panel);
    return card;
}

inline std::wstring FormatBytes(std::uint64_t value)
{
    constexpr double gibibyte = 1024.0 * 1024.0 * 1024.0;
    constexpr double mebibyte = 1024.0 * 1024.0;
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(1);
    if (value >= static_cast<std::uint64_t>(gibibyte))
    {
        stream << value / gibibyte << L" GB";
    }
    else
    {
        stream << value / mebibyte << L" MB";
    }
    return stream.str();
}

inline std::wstring FormatPercent(std::optional<double> value)
{
    if (!value)
    {
        return L"—";
    }
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(0) << *value << L"%";
    return stream.str();
}

inline std::wstring FormatRate(
    std::optional<std::uint64_t> receive,
    std::optional<std::uint64_t> send)
{
    if (!receive || !send)
    {
        return L"—";
    }
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(1)
        << (*receive + *send) / (1024.0 * 1024.0) << L" MB/s";
    return stream.str();
}

inline double ProgressValue(std::optional<double> value)
{
    return value ? (std::max)(0.0, (std::min)(100.0, *value)) : 0.0;
}

inline void ApplyWallpaper(
    Border const& hero,
    std::wstring const& wallpaperPath)
{
    hero.Background(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(255, 53, 51, 53)));
    if (wallpaperPath.empty())
    {
        return;
    }

    try
    {
        auto imageBrush = ImageBrush();
        auto bitmap = BitmapImage(winrt::Windows::Foundation::Uri(winrt::hstring(wallpaperPath)));
        imageBrush.ImageSource(bitmap);
        imageBrush.Stretch(Stretch::UniformToFill);
        imageBrush.Opacity(0.62);
        hero.Background(imageBrush);
    }
    catch (...)
    {
        // Preserve the system-color fallback for missing or inaccessible images.
    }
}

inline Border MakeNativeListCard(
    winrt::hstring const& title,
    std::vector<NativeListItem> const& rows,
    winrt::hstring const& emptyMessage,
    TextBlock const& status,
    ActionCallback const& action,
    std::wstring const& actionName)
{
    auto card = MakeCard();
    auto panel = StackPanel();
    panel.Spacing(10);
    panel.Children().Append(MakeText(title, 16));

    if (rows.empty())
    {
        auto empty = MakeText(emptyMessage, 12, 0.72);
        empty.TextWrapping(TextWrapping::Wrap);
        panel.Children().Append(empty);
    }
    else
    {
        for (auto const& row : rows)
        {
            auto button = Button();
            button.HorizontalContentAlignment(HorizontalAlignment::Left);
            button.HorizontalAlignment(HorizontalAlignment::Stretch);
            button.MinHeight(40);
            button.IsEnabled(row.IsAvailable && !row.Target.empty());

            auto rowGrid = Grid();
            rowGrid.ColumnDefinitions().Append(ColumnDefinition());
            rowGrid.ColumnDefinitions().Append(ColumnDefinition());
            rowGrid.ColumnDefinitions().GetAt(0).Width(Star());
            rowGrid.ColumnDefinitions().GetAt(1).Width(GridLength{ 96, GridUnitType::Pixel });
            rowGrid.Children().Append(MakeText(winrt::hstring(row.DisplayName), 13));

            auto detail = MakeText(winrt::hstring(row.Detail), 11, 0.68);
            detail.HorizontalAlignment(HorizontalAlignment::Right);
            Grid::SetColumn(detail, 1);
            rowGrid.Children().Append(detail);
            button.Content(rowGrid);

            const auto target = row.Target;
            button.Click([status, action, actionName, target](auto const&, auto const&)
            {
                if (action)
                {
                    action(actionName, target);
                    status.Text(L"Richiesta inviata al broker");
                }
            });
            panel.Children().Append(button);
        }
    }

    card.Child(panel);
    return card;
}

inline void PopulateStorageCards(
    VariableSizedWrapGrid const& storageWrap,
    NativeSnapshot const& snapshot,
    TextBlock const& status,
    ActionCallback const& action)
{
    storageWrap.Children().Clear();
    if (snapshot.Storage.empty())
    {
        auto empty = MakeCard();
        empty.Width(290);
        empty.Child(MakeText(L"Nessun volume disponibile", 12, 0.72));
        storageWrap.Children().Append(empty);
        return;
    }

    for (auto const& item : snapshot.Storage)
    {
        const auto total = item.TotalBytes.value_or(0);
        const auto free = item.FreeBytes.value_or(0);
        const auto used = total >= free ? total - free : 0;
        const auto percent = total > 0 ? 100.0 * used / total : 0.0;
        const auto title = item.Label.empty() ? item.Path : item.Path + L"  " + item.Label;
        const auto usage = total > 0
            ? FormatBytes(used) + L" usati di " + FormatBytes(total)
            : L"Capacità non disponibile";
        auto disk = MakeStorageCard(
            winrt::hstring(title),
            winrt::hstring(usage),
            winrt::hstring(item.FileSystem.empty() ? item.Kind : item.FileSystem),
            percent,
            item.Path,
            status,
            action,
            winrt::hstring(item.Health == L"Unknown" ? L"Stato sconosciuto" : item.Health));
        disk.Width(290);
        storageWrap.Children().Append(disk);
    }
}

inline void PopulateLowerCards(
    VariableSizedWrapGrid const& lower,
    NativeSnapshot const& snapshot,
    TextBlock const& status,
    ActionCallback const& action)
{
    lower.Children().Clear();

    auto network = MakeNativeListCard(
        L"Rete",
        snapshot.NetworkLocations,
        L"Nessuna posizione di rete connessa",
        status,
        action,
        L"folder.open");
    network.Width(290);
    lower.Children().Append(network);

    auto recent = MakeNativeListCard(
        L"Recenti",
        snapshot.RecentItems,
        L"Windows non espone documenti recenti",
        status,
        action,
        L"item.open");
    recent.Width(290);
    lower.Children().Append(recent);

    const auto highlightedMessage = snapshot.HighlightedUnavailableReason.empty()
        ? winrt::hstring(L"Nessun elemento Windows in evidenza")
        : winrt::hstring(snapshot.HighlightedUnavailableReason);
    auto highlighted = MakeNativeListCard(
        L"In evidenza",
        snapshot.HighlightedItems,
        highlightedMessage,
        status,
        action,
        L"folder.open");
    highlighted.Width(290);
    lower.Children().Append(highlighted);

    auto terminal = MakeNativeListCard(
        L"Terminale",
        snapshot.TerminalProfiles,
        L"Nessun profilo terminale disponibile",
        status,
        action,
        L"terminal.open");
    terminal.Width(290);
    lower.Children().Append(terminal);
}

inline void PopulateQuickSettings(
    VariableSizedWrapGrid const& settings,
    std::vector<NativeQuickSetting> const& items,
    TextBlock const& status,
    ActionCallback const& action)
{
    settings.Children().Clear();
    for (auto const& item : items)
    {
        auto button = Button();
        button.Content(winrt::box_value(winrt::hstring(item.DisplayName)));
        button.MinHeight(40);
        button.MinWidth(120);
        const auto uri = item.Uri;
        button.Click([status, action, uri](auto const&, auto const&)
        {
            if (action)
            {
                action(L"settings.open", uri);
                status.Text(L"Impostazione richiesta al broker");
            }
        });
        settings.Children().Append(button);
    }
}

inline winrt::Windows::UI::Xaml::UIElement BuildWelcomePage(
    std::function<bool()> const& pingBroker,
    ActionCallback const& action,
    std::wstring const& wallpaperPath = {},
    SnapshotCallback const& snapshot = {},
    NativeSnapshot const& initialSnapshot = {})
{
    auto root = Grid();
    root.RequestedTheme(ElementTheme::Default);
    root.Background(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(255, 23, 23, 26)));

    auto scroll = ScrollViewer();
    scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    scroll.Padding(Thickness{ 22, 18, 22, 22 });
    auto page = StackPanel();
    page.Spacing(14);

    auto status = MakeText(L"Snapshot mock pronto · broker non interrogato", 12, 0.78);

    auto hero = Border();
    hero.CornerRadius(CornerRadius{ 18, 18, 18, 18 });
    hero.MinHeight(250);
    hero.Padding(Thickness{ 28, 24, 28, 24 });
    ApplyWallpaper(
        hero,
        initialSnapshot.WallpaperPath.empty() ? wallpaperPath : initialSnapshot.WallpaperPath);

    auto heroGrid = Grid();
    heroGrid.ColumnDefinitions().Append(ColumnDefinition());
    heroGrid.ColumnDefinitions().Append(ColumnDefinition());
    heroGrid.ColumnDefinitions().Append(ColumnDefinition());
    heroGrid.ColumnDefinitions().Append(ColumnDefinition());
    heroGrid.ColumnDefinitions().Append(ColumnDefinition());
    heroGrid.ColumnDefinitions().GetAt(0).Width(Star(2.2));
    for (uint32_t i = 1; i < 5; ++i) heroGrid.ColumnDefinitions().GetAt(i).Width(Star());

    auto identity = StackPanel();
    identity.Spacing(8);
    auto machineName = MakeText(
        winrt::hstring(initialSnapshot.IsLoaded ? initialSnapshot.MachineName : L"WORKSTATION"),
        27);
    auto machineModel = MakeText(
        winrt::hstring(initialSnapshot.IsLoaded ? initialSnapshot.MachineModel : L"Views Explorer Home V2"),
        13,
        0.82);
    auto networkIdentity = MakeText(
        winrt::hstring(initialSnapshot.IsLoaded && !initialSnapshot.PrimaryNetworkIdentity.empty()
            ? L"● " + initialSnapshot.PrimaryNetworkIdentity
            : L"● Rete in attesa"),
        13,
        0.86);
    auto cpuIdentity = MakeText(
        winrt::hstring(initialSnapshot.IsLoaded ? L"⚙ " + initialSnapshot.CpuModel : L"⚙ CPU in attesa"),
        13,
        0.86);
    auto gpuIdentity = MakeText(
        winrt::hstring(initialSnapshot.IsLoaded ? L"▣ " + initialSnapshot.GpuModel : L"▣ GPU in attesa"),
        13,
        0.86);
    auto memoryIdentity = MakeText(
        initialSnapshot.MemoryTotalBytes > 0
            ? winrt::hstring(L"▤ RAM " + FormatBytes(initialSnapshot.MemoryUsedBytes) + L" di " +
                FormatBytes(initialSnapshot.MemoryTotalBytes))
            : winrt::hstring(L"▤ RAM in attesa"),
        13,
        0.86);
    identity.Children().Append(machineName);
    identity.Children().Append(machineModel);
    identity.Children().Append(networkIdentity);
    identity.Children().Append(cpuIdentity);
    identity.Children().Append(gpuIdentity);
    identity.Children().Append(memoryIdentity);
    heroGrid.Children().Append(identity);

    auto cpu = MakeMetricCard(
        L"CPU",
        winrt::hstring(initialSnapshot.IsLoaded ? FormatPercent(initialSnapshot.CpuPercent) : L"12%"),
        initialSnapshot.IsLoaded ? L"Dato locale" : L"mock sample",
        initialSnapshot.IsLoaded ? ProgressValue(initialSnapshot.CpuPercent) : 12);
    Grid::SetColumn(cpu.Card, 1);
    heroGrid.Children().Append(cpu.Card);
    auto memory = MakeMetricCard(
        L"Memoria",
        winrt::hstring(initialSnapshot.IsLoaded ? FormatPercent(initialSnapshot.MemoryPercent) : L"38%"),
        initialSnapshot.MemoryTotalBytes > 0
            ? winrt::hstring(FormatBytes(initialSnapshot.MemoryUsedBytes) + L" / " + FormatBytes(initialSnapshot.MemoryTotalBytes))
            : winrt::hstring(L"In attesa"),
        initialSnapshot.IsLoaded ? ProgressValue(initialSnapshot.MemoryPercent) : 38);
    Grid::SetColumn(memory.Card, 2);
    heroGrid.Children().Append(memory.Card);
    auto storage = MakeMetricCard(
        L"Archiviazione",
        winrt::hstring(initialSnapshot.IsLoaded ? FormatPercent(initialSnapshot.StoragePercent) : L"52%"),
        initialSnapshot.IsLoaded ? L"Volume principale" : L"554 GB/1.0 TB",
        initialSnapshot.IsLoaded ? ProgressValue(initialSnapshot.StoragePercent) : 52);
    Grid::SetColumn(storage.Card, 3);
    heroGrid.Children().Append(storage.Card);
    auto network = MakeMetricCard(
        L"Rete",
        winrt::hstring(initialSnapshot.IsLoaded
            ? FormatRate(initialSnapshot.NetworkReceiveBytesPerSecond, initialSnapshot.NetworkSendBytesPerSecond)
            : L"88 Mbps"),
        L"Invio/Ricezione",
        initialSnapshot.IsLoaded
            ? ProgressValue(initialSnapshot.NetworkReceiveBytesPerSecond || initialSnapshot.NetworkSendBytesPerSecond
                ? std::optional<double>(50.0)
                : std::nullopt)
            : 68);
    Grid::SetColumn(network.Card, 4);
    heroGrid.Children().Append(network.Card);
    hero.Child(heroGrid);
    page.Children().Append(hero);

    auto storageTitle = MakeText(L"Archiviazione", 18);
    page.Children().Append(storageTitle);
    auto storageWrap = VariableSizedWrapGrid();
    storageWrap.Orientation(Orientation::Horizontal);
    storageWrap.MaximumRowsOrColumns(4);
    storageWrap.HorizontalAlignment(HorizontalAlignment::Stretch);
    if (initialSnapshot.IsLoaded)
    {
        for (auto const& item : initialSnapshot.Storage)
        {
            const auto total = item.TotalBytes.value_or(0);
            const auto free = item.FreeBytes.value_or(0);
            const auto used = total >= free ? total - free : 0;
            const auto percent = total > 0 ? 100.0 * used / total : 0.0;
            const auto title = item.Label.empty()
                ? item.Path
                : item.Path + L"  " + item.Label;
            const auto usage = total > 0
                ? FormatBytes(used) + L" usati di " + FormatBytes(total)
                : L"Capacità non disponibile";
            auto disk = MakeStorageCard(
                winrt::hstring(title),
                winrt::hstring(usage),
                winrt::hstring(item.FileSystem.empty() ? item.Kind : item.FileSystem),
                percent,
                item.Path,
                status,
                action,
                winrt::hstring(item.Health == L"Unknown" ? L"Stato sconosciuto" : item.Health));
            disk.Width(290);
            storageWrap.Children().Append(disk);
        }
    }
    else
    {
        auto diskC = MakeStorageCard(L"C  System disk", L"196 GB usati di 418 GB", L"NTFS", 47, L"C:\\", status, action);
        diskC.Width(290);
        storageWrap.Children().Append(diskC);
        auto diskD = MakeStorageCard(L"D  VMX SDD 1.0TB INTERNAL", L"693 GB usati di 931 GB", L"NTFS", 74, L"D:\\", status, action);
        diskD.Width(290);
        storageWrap.Children().Append(diskD);
        auto diskE = MakeStorageCard(L"E  Git Temp & Swap", L"320 GB usati di 465 GB", L"NTFS", 69, L"E:\\", status, action);
        diskE.Width(290);
        storageWrap.Children().Append(diskE);
        auto diskS = MakeStorageCard(L"S  System part", L"46.4 GB usati di 46.5 GB", L"NTFS", 99, L"S:\\", status, action);
        diskS.Width(290);
        storageWrap.Children().Append(diskS);
    }
    page.Children().Append(storageWrap);

    auto lower = VariableSizedWrapGrid();
    lower.Orientation(Orientation::Horizontal);
    lower.MaximumRowsOrColumns(4);
    lower.HorizontalAlignment(HorizontalAlignment::Stretch);
    auto networkCard = MakeListCard(L"Rete", { { L"Spacey (\\\\HCMC-SERVER)", L"88% · Connesso" } }, status, action);
    networkCard.Width(290);
    lower.Children().Append(networkCard);
    auto recentCard = MakeListCard(L"Recenti", {
        { L"quarterly_report.pptx", L"Desktop · 09:14" },
        { L"project_plan.xlsx", L"30_Engineering · ieri" },
        { L"notes.md", L"Obsidian · ieri" },
        { L"architecture.drawio", L"30_Engineering · ieri" },
        { L"invoice_0425.pdf", L"Invoices · 2 giorni fa" }
    }, status, action);
    recentCard.Width(290);
    lower.Children().Append(recentCard);
    auto highlightedCard = MakeListCard(L"In evidenza", {
        { L"30_Engineering", L"Questo PC" },
        { L"Obsidian", L"Questo PC" },
        { L"70_Assets", L"Questo PC" },
        { L"Invoices", L"Questo PC" },
        { L"Windows Terminal", L"URL" }
    }, status, action);
    highlightedCard.Width(290);
    lower.Children().Append(highlightedCard);
    auto terminalCard = MakeListCard(L"Terminale", {
        { L"PowerShell 7", L"Apri nella posizione corrente" },
        { L"Prompt dei comandi", L"Apri nella posizione corrente" },
        { L"Ubuntu (WSL)", L"Apri shell" },
        { L"Impostazioni terminale", L"Configura profili" }
    }, status, action);
    terminalCard.Width(290);
    lower.Children().Append(terminalCard);
    page.Children().Append(lower);

    auto settingsTitle = MakeText(L"Impostazioni rapide", 18);
    page.Children().Append(settingsTitle);
    auto settings = VariableSizedWrapGrid();
    settings.Orientation(Orientation::Horizontal);
    settings.MaximumRowsOrColumns(8);
    const std::vector<std::pair<winrt::hstring, std::wstring>> quickSettings = {
        { L"Display", L"ms-settings:display" },
        { L"Audio", L"ms-settings:sound" },
        { L"Rete", L"ms-settings:network" },
        { L"Bluetooth", L"ms-settings:bluetooth" },
        { L"Archiviazione", L"ms-settings:storagesense" },
        { L"Windows Update", L"ms-settings:windowsupdate" }
    };
    for (auto const& setting : quickSettings)
    {
        auto button = Button();
        button.Content(winrt::box_value(winrt::hstring(setting.first)));
        button.MinHeight(40);
        button.MinWidth(120);
        button.Click([status, action, uri = std::wstring(setting.second)](auto const&, auto const&)
        {
            if (action) action(L"settings.open", uri);
            status.Text(L"Impostazione richiesta al broker");
        });
        settings.Children().Append(button);
    }
    if (initialSnapshot.IsLoaded)
    {
        PopulateLowerCards(lower, initialSnapshot, status, action);
        PopulateQuickSettings(settings, initialSnapshot.QuickSettings, status, action);
    }
    page.Children().Append(settings);
    page.Children().Append(status);

    const ApplySnapshotCallback applySnapshot =
        [hero,
         machineName,
         machineModel,
         networkIdentity,
         cpuIdentity,
         gpuIdentity,
         memoryIdentity,
         cpu,
         memory,
         storage,
         network,
         storageWrap,
         lower,
         settings,
         status,
         action](NativeSnapshot const& data)
    {
        ApplyWallpaper(hero, data.WallpaperPath);
        machineName.Text(winrt::hstring(data.MachineName));
        machineModel.Text(winrt::hstring(
            data.OsDisplayVersion.empty()
                ? data.MachineModel
                : data.MachineModel + L" · Windows " + data.OsDisplayVersion));
        networkIdentity.Text(winrt::hstring(
            data.PrimaryNetworkIdentity.empty()
                ? L"● Rete non disponibile"
                : L"● " + data.PrimaryNetworkIdentity));
        cpuIdentity.Text(winrt::hstring(L"⚙ " + data.CpuModel));
        gpuIdentity.Text(winrt::hstring(L"▣ " + data.GpuModel));
        memoryIdentity.Text(data.MemoryTotalBytes > 0
            ? winrt::hstring(L"▤ RAM " + FormatBytes(data.MemoryUsedBytes) + L" di " + FormatBytes(data.MemoryTotalBytes))
            : winrt::hstring(L"▤ RAM non disponibile"));

        cpu.Value.Text(winrt::hstring(FormatPercent(data.CpuPercent)));
        cpu.Detail.Text(L"Dato locale");
        cpu.Progress.Value(ProgressValue(data.CpuPercent));
        memory.Value.Text(winrt::hstring(FormatPercent(data.MemoryPercent)));
        memory.Detail.Text(data.MemoryTotalBytes > 0
            ? winrt::hstring(FormatBytes(data.MemoryUsedBytes) + L" / " + FormatBytes(data.MemoryTotalBytes))
            : winrt::hstring(L"Non disponibile"));
        memory.Progress.Value(ProgressValue(data.MemoryPercent));
        storage.Value.Text(winrt::hstring(FormatPercent(data.StoragePercent)));
        storage.Detail.Text(L"Volume principale");
        storage.Progress.Value(ProgressValue(data.StoragePercent));
        network.Value.Text(winrt::hstring(
            FormatRate(data.NetworkReceiveBytesPerSecond, data.NetworkSendBytesPerSecond)));
        network.Detail.Text(L"Invio/Ricezione");
        network.Progress.Value(
            data.NetworkReceiveBytesPerSecond || data.NetworkSendBytesPerSecond ? 50.0 : 0.0);

        PopulateStorageCards(storageWrap, data, status, action);
        PopulateLowerCards(lower, data, status, action);
        PopulateQuickSettings(settings, data.QuickSettings, status, action);
    };

    auto retry = Button();
    retry.Content(winrt::box_value(L"Aggiorna dati"));
    retry.HorizontalAlignment(HorizontalAlignment::Left);
    retry.MinHeight(40);
    retry.Click([status, pingBroker, snapshot, applySnapshot](auto const&, auto const&)
    {
        status.Text(L"Aggiornamento in corso…");
        if (snapshot)
        {
            RunSnapshotRefreshAsync(status, snapshot, applySnapshot);
        }
        else
        {
            status.Text(pingBroker && pingBroker()
                ? L"Broker raggiungibile · snapshot non configurato"
                : L"Broker non disponibile · dati locali mantenuti");
        }
    });
    page.Children().Append(retry);

    scroll.Content(page);
    root.Children().Append(scroll);
    root.Loaded([status, snapshot, applySnapshot](auto const&, auto const&)
    {
        if (snapshot)
        {
            status.Text(L"Caricamento dati locali…");
            RunSnapshotRefreshAsync(status, snapshot, applySnapshot);
        }
    });
    return root;
}
}
