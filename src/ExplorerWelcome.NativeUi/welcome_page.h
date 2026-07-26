// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Shared WinRT XAML visual shell for the standalone host and Shell view.
// -----------------------------------------------------------------------------
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>

namespace ExplorerWelcome::NativeUi
{
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Shapes;
using namespace winrt::Windows::UI::Xaml::Media::Imaging;

using ActionCallback = std::function<void(std::wstring const& action, std::wstring const& target)>;

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

inline winrt::Windows::UI::Xaml::Controls::Border MakeMetricCard(
    winrt::hstring const& label,
    winrt::hstring const& value,
    winrt::hstring const& detail,
    double progress)
{
    auto card = MakeCard(Thickness{ 14, 12, 14, 12 });
    auto panel = StackPanel();
    panel.Spacing(6);
    panel.Children().Append(MakeText(label, 12, 0.78));
    panel.Children().Append(MakeText(value, 22));
    auto bar = ProgressBar();
    bar.Minimum(0);
    bar.Maximum(100);
    bar.Value(progress);
    bar.Height(4);
    bar.Foreground(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(255, 91, 174, 255)));
    panel.Children().Append(bar);
    panel.Children().Append(MakeText(detail, 11, 0.72));
    card.Child(panel);
    return card;
}

inline winrt::Windows::UI::Xaml::Controls::Border MakeStorageCard(
    winrt::hstring const& title,
    winrt::hstring const& used,
    winrt::hstring const& format,
    double progress,
    std::wstring const& target,
    winrt::Windows::UI::Xaml::Controls::TextBlock const& status,
    ActionCallback const& action)
{
    auto card = MakeCard(Thickness{ 16, 14, 16, 14 });
    auto panel = StackPanel();
    panel.Spacing(7);
    auto header = Grid();
    header.ColumnDefinitions().Append(ColumnDefinition());
    header.ColumnDefinitions().Append(ColumnDefinition());
    header.ColumnDefinitions().GetAt(0).Width(Star());
    header.ColumnDefinitions().GetAt(1).Width(GridLength{ 86, GridUnitType::Pixel });
    header.Children().Append(MakeText(title, 14));
    auto health = MakeText(L"●  Disponibile", 11, 0.78);
    health.HorizontalAlignment(HorizontalAlignment::Right);
    Grid::SetColumn(health, 1);
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

inline winrt::Windows::UI::Xaml::UIElement BuildWelcomePage(
    std::function<bool()> const& pingBroker,
    ActionCallback const& action,
    std::wstring const& wallpaperPath = {})
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
    hero.Background(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(255, 53, 51, 53)));
    if (!wallpaperPath.empty())
    {
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
            // Keep the system-color fallback when the wallpaper cannot load.
        }
    }

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
    identity.Children().Append(MakeText(L"WORKSTATION", 27));
    identity.Children().Append(MakeText(L"Views Explorer Home V2", 13, 0.82));
    identity.Children().Append(MakeText(L"● 192.168.1.25", 13, 0.86));
    identity.Children().Append(MakeText(L"⚙ Ryzen 9 5950X", 13, 0.86));
    identity.Children().Append(MakeText(L"▣ RTX 4060", 13, 0.86));
    identity.Children().Append(MakeText(L"▤ RAM 12.3 di 31.9 GB", 13, 0.86));
    heroGrid.Children().Append(identity);

    auto cpu = MakeMetricCard(L"CPU", L"12%", L"mock sample", 12);
    Grid::SetColumn(cpu, 1);
    heroGrid.Children().Append(cpu);
    auto memory = MakeMetricCard(L"Memoria", L"38%", L"12.3/31.9 GB", 38);
    Grid::SetColumn(memory, 2);
    heroGrid.Children().Append(memory);
    auto storage = MakeMetricCard(L"Archiviazione", L"52%", L"554 GB/1.0 TB", 52);
    Grid::SetColumn(storage, 3);
    heroGrid.Children().Append(storage);
    auto network = MakeMetricCard(L"Rete", L"88 Mbps", L"Invio/Ricezione", 68);
    Grid::SetColumn(network, 4);
    heroGrid.Children().Append(network);
    hero.Child(heroGrid);
    page.Children().Append(hero);

    auto storageTitle = MakeText(L"Archiviazione", 18);
    page.Children().Append(storageTitle);
    auto storageWrap = VariableSizedWrapGrid();
    storageWrap.Orientation(Orientation::Horizontal);
    storageWrap.MaximumRowsOrColumns(4);
    storageWrap.HorizontalAlignment(HorizontalAlignment::Stretch);
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
    page.Children().Append(settings);
    page.Children().Append(status);

    auto retry = Button();
    retry.Content(winrt::box_value(L"Aggiorna dati"));
    retry.HorizontalAlignment(HorizontalAlignment::Left);
    retry.MinHeight(40);
    retry.Click([status, pingBroker](auto const&, auto const&)
    {
        status.Text(pingBroker && pingBroker() ? L"Broker raggiungibile · refresh richiesto" : L"Broker non disponibile · dati locali mantenuti");
    });
    page.Children().Append(retry);

    scroll.Content(page);
    root.Children().Append(scroll);
    return root;
}
}
