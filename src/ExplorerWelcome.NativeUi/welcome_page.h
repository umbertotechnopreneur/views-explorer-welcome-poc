/* =============================================================================
 * Views Explorer Welcome POC
 * File: src/ExplorerWelcome.NativeUi/welcome_page.h
 * Purpose: Shared WinRT XAML visual shell for the standalone host and Shell view.
 *
 * Copyright (c) 2026 Umberto Giacobbi
 * Author: Umberto Giacobbi
 * Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
 * License: PolyForm Noncommercial License 1.0.0
 * SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * Open source: https://umbertogiacobbi.biz/opensource
 * =============================================================================
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
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
using XamlPolyline = winrt::Windows::UI::Xaml::Shapes::Polyline;

// Refreshes the broker snapshot off the UI thread and applies it only while the page is alive.
inline void RunSnapshotRefreshAsync(
    winrt::Windows::UI::Xaml::Controls::TextBlock const& status,
    SnapshotCallback const& snapshot,
    ApplySnapshotCallback const& apply,
    std::shared_ptr<std::atomic_bool> const& pageAlive,
    std::shared_ptr<std::atomic_bool> const& refreshRunning,
    bool announceStatus = true)
{
    if (!pageAlive || !pageAlive->load() || !refreshRunning || refreshRunning->exchange(true))
    {
        return;
    }

    auto dispatcher = status.Dispatcher();
    std::thread([status, dispatcher, snapshot, apply, pageAlive, refreshRunning, announceStatus]
    {
        NativeSnapshot result;
        std::wstring summary;
        bool available = false;
        try
        {
            available = snapshot && snapshot(result, summary);
        }
        catch (...)
        {
            summary = L"Aggiornamento non disponibile · dati locali mantenuti";
        }

        if (summary.empty())
        {
            summary = available ? L"Snapshot aggiornato" : L"Broker non disponibile · dati locali mantenuti";
        }

        if (!pageAlive->load())
        {
            refreshRunning->store(false);
            return;
        }

        try
        {
            dispatcher.RunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                [status,
                 available,
                 result = std::move(result),
                 summary,
                 apply,
                 pageAlive,
                 refreshRunning,
                 announceStatus]
                {
                    try
                    {
                        if (pageAlive->load())
                        {
                            if (available && apply)
                            {
                                apply(result);
                            }
                            if (announceStatus)
                            {
                                status.Text(winrt::hstring(summary));
                            }
                        }
                    }
                    catch (...)
                    {
                        // A closing XAML Island may reject late UI work.
                    }
                    refreshRunning->store(false);
                });
        }
        catch (...)
        {
            refreshRunning->store(false);
        }
    }).detach();
}

// Creates a text block with the default Windows typography and a reusable opacity.
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

struct HeroTextElements
{
    Grid Root{ nullptr };
    TextBlock Text{ nullptr };
};

// Creates hero-only text with a small glyph-shaped shadow for changing wallpaper contrast.
inline HeroTextElements MakeHeroText(
    winrt::hstring const& value,
    double size,
    double opacity = 1.0)
{
    auto root = Grid();
    auto shadowHost = Canvas();
    shadowHost.IsHitTestVisible(false);
    root.Children().Append(shadowHost);

    auto text = MakeText(value, size, opacity);
    root.Children().Append(text);
    shadowHost.Loaded([shadowHost, text](auto const&, auto const&)
    {
        using winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview;
        if (ElementCompositionPreview::GetElementChildVisual(shadowHost))
        {
            return;
        }

        // The separate host stays behind the TextBlock while its alpha mask tracks live text changes.
        auto hostVisual = ElementCompositionPreview::GetElementVisual(shadowHost);
        auto compositor = hostVisual.Compositor();
        auto shadow = compositor.CreateDropShadow();
        shadow.Color(winrt::Windows::UI::ColorHelper::FromArgb(255, 0, 0, 0));
        shadow.BlurRadius(1.5f);
        shadow.Offset(winrt::Windows::Foundation::Numerics::float3{ 0.0f, 0.75f, 0.0f });
        shadow.Opacity(0.28f);
        shadow.Mask(text.GetAlphaMask());

        auto shadowVisual = compositor.CreateSpriteVisual();
        shadowVisual.Shadow(shadow);
        auto bindSize = compositor.CreateExpressionAnimation(L"hostVisual.Size");
        bindSize.SetReferenceParameter(L"hostVisual", hostVisual);
        shadowVisual.StartAnimation(L"Size", bindSize);
        ElementCompositionPreview::SetElementChildVisual(shadowHost, shadowVisual);
    });
    return HeroTextElements{ root, text };
}

// Reads a Windows theme brush and falls back only when a XAML Island resource is unavailable.
inline winrt::Windows::UI::Xaml::Media::Brush ThemeBrush(
    winrt::hstring const& key,
    winrt::Windows::UI::Color const& fallback)
{
    try
    {
        auto resources = Application::Current().Resources();
        if (resources.HasKey(winrt::box_value(key)))
        {
            return resources.Lookup(winrt::box_value(key)).as<Brush>();
        }
    }
    catch (...)
    {
        // Early host startup can miss app-level resources, so keep a readable fallback.
    }

    return SolidColorBrush(fallback);
}

// Creates one XAML gradient stop with the verbose C++/WinRT property setters.
inline winrt::Windows::UI::Xaml::Media::GradientStop MakeGradientStop(
    winrt::Windows::UI::Color const& color,
    double offset)
{
    auto stop = GradientStop();
    stop.Color(color);
    stop.Offset(offset);
    return stop;
}

// Creates one shared, native backdrop material for the four hero metric cards.
inline winrt::Windows::UI::Xaml::Media::Brush MakeMetricCardMaterial()
{
    const auto fallback = winrt::Windows::UI::ColorHelper::FromArgb(255, 52, 48, 50);
    try
    {
        auto material = AcrylicBrush();
        material.BackgroundSource(AcrylicBackgroundSource::Backdrop);
        material.TintColor(winrt::Windows::UI::ColorHelper::FromArgb(255, 32, 29, 30));
        material.TintOpacity(0.54);
        material.TintLuminosityOpacity(
            winrt::box_value(0.22)
                .as<winrt::Windows::Foundation::IReference<double>>());
        material.FallbackColor(fallback);
        material.AlwaysUseFallback(false);
        return material;
    }
    catch (...)
    {
        // Policy, battery saver, or an early XAML Island can disable backdrop effects.
        return SolidColorBrush(fallback);
    }
}

// Creates the single flat accent used by memory and storage progress fills.
inline winrt::Windows::UI::Xaml::Media::Brush MakeMetricAccentBrush()
{
    return SolidColorBrush(
        winrt::Windows::UI::ColorHelper::FromArgb(255, 143, 183, 238));
}

// Creates the muted solid stroke used by real CPU and network histories.
inline winrt::Windows::UI::Xaml::Media::Brush MakeSparklineBrush()
{
    return SolidColorBrush(
        winrt::Windows::UI::ColorHelper::FromArgb(232, 188, 197, 207));
}

// Creates a native-feeling card frame close to Explorer's default dark theme metrics.
inline winrt::Windows::UI::Xaml::Controls::Border MakeCard(
    Thickness const& padding = Thickness{ 16, 14, 16, 14 })
{
    auto card = Border();
    card.CornerRadius(CornerRadius{ 7, 7, 7, 7 });
    card.Padding(padding);
    card.BorderThickness(Thickness{ 1, 1, 1, 1 });
    card.BorderBrush(ThemeBrush(
        L"SystemControlForegroundBaseLowBrush",
        winrt::Windows::UI::ColorHelper::FromArgb(72, 255, 255, 255)));
    card.Background(ThemeBrush(
        L"SystemControlBackgroundChromeMediumLowBrush",
        winrt::Windows::UI::ColorHelper::FromArgb(176, 37, 37, 39)));
    return card;
}

// Keeps column sizing concise at call sites.
inline winrt::Windows::UI::Xaml::GridLength Star(double value = 1.0)
{
    return GridLength{ value, GridUnitType::Star };
}

// Builds a small Segoe MDL2 icon next to text so rows read like Explorer items.
inline winrt::Windows::UI::Xaml::Controls::Grid MakeIconText(
    winrt::hstring const& glyph,
    winrt::hstring const& value,
    double size = 12,
    double opacity = 0.84)
{
    auto row = Grid();
    row.ColumnDefinitions().Append(ColumnDefinition());
    row.ColumnDefinitions().Append(ColumnDefinition());
    row.ColumnDefinitions().GetAt(0).Width(GridLength{ 22, GridUnitType::Pixel });
    row.ColumnDefinitions().GetAt(1).Width(Star());

    auto icon = FontIcon();
    icon.Glyph(glyph);
    icon.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    icon.FontSize(size + 1);
    icon.Opacity(opacity);
    row.Children().Append(icon);

    auto text = MakeText(value, size, opacity);
    text.TextTrimming(TextTrimming::CharacterEllipsis);
    Grid::SetColumn(text, 1);
    row.Children().Append(text);
    return row;
}

// Wraps an existing text block with a native glyph while preserving live updates to that text.
inline winrt::Windows::UI::Xaml::Controls::Grid MakeIconTextBlock(
    winrt::hstring const& glyph,
    winrt::Windows::UI::Xaml::FrameworkElement const& content,
    double size = 12,
    double opacity = 0.84)
{
    auto row = Grid();
    row.ColumnDefinitions().Append(ColumnDefinition());
    row.ColumnDefinitions().Append(ColumnDefinition());
    row.ColumnDefinitions().GetAt(0).Width(GridLength{ 22, GridUnitType::Pixel });
    row.ColumnDefinitions().GetAt(1).Width(Star());

    auto icon = FontIcon();
    icon.Glyph(glyph);
    icon.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    icon.FontSize(size + 1);
    icon.Opacity(opacity);
    row.Children().Append(icon);

    Grid::SetColumn(content, 1);
    row.Children().Append(content);
    return row;
}

enum class SparklineKind
{
    CpuPercent,
    NetworkMegabytesPerSecond
};

// Draws only finite broker samples, using an adaptive range so small real changes stay legible.
inline void SetSparkline(
    XamlPolyline const& line,
    std::vector<double> const& samples,
    std::optional<double> current,
    SparklineKind kind)
{
    line.Points().Clear();
    std::vector<double> bounded;
    const auto first = samples.size() > 24 ? samples.size() - 24 : 0;
    bounded.reserve((std::min)(samples.size(), static_cast<std::size_t>(24)));
    for (auto index = first; index < samples.size(); ++index)
    {
        if (!std::isfinite(samples[index]))
        {
            continue;
        }
        bounded.push_back(kind == SparklineKind::CpuPercent
            ? (std::max)(0.0, (std::min)(100.0, samples[index]))
            : (std::max)(0.0, samples[index]));
    }
    if (bounded.empty() && current && std::isfinite(*current))
    {
        bounded.push_back(kind == SparklineKind::CpuPercent
            ? (std::max)(0.0, (std::min)(100.0, *current))
            : (std::max)(0.0, *current));
    }
    if (bounded.empty())
    {
        return;
    }

    auto minimum = *std::min_element(bounded.begin(), bounded.end());
    auto maximum = *std::max_element(bounded.begin(), bounded.end());
    const auto minimumSpan = kind == SparklineKind::CpuPercent
        ? 18.0
        : (std::max)(0.05, maximum * 0.25);
    if (maximum - minimum < minimumSpan)
    {
        const auto midpoint = (maximum + minimum) / 2.0;
        minimum = midpoint - minimumSpan / 2.0;
        maximum = midpoint + minimumSpan / 2.0;
    }
    if (kind == SparklineKind::CpuPercent)
    {
        minimum = (std::max)(0.0, minimum);
        maximum = (std::min)(100.0, maximum);
    }
    else
    {
        minimum = (std::max)(0.0, minimum);
    }
    if (maximum <= minimum)
    {
        maximum = minimum + 1.0;
    }

    const auto width = line.Width();
    const auto height = line.Height();
    const auto pointCount = (std::max)(static_cast<std::size_t>(2), bounded.size());
    for (std::size_t index = 0; index < pointCount; ++index)
    {
        // A single real observation is repeated only to render a full-width flat segment.
        const auto sample = bounded[bounded.size() == 1 ? 0 : index];
        const auto x = static_cast<float>(index * width / (pointCount - 1));
        const auto normalized = (sample - minimum) / (maximum - minimum);
        const auto y = static_cast<float>(height - normalized * height);
        line.Points().Append(winrt::Windows::Foundation::Point{ x, y });
    }
}

// Creates a subtle Explorer-style sparkline used by live resource cards.
inline XamlPolyline MakeSparkline(
    double width = 80,
    double height = 22,
    double opacity = 1.0)
{
    auto line = XamlPolyline();
    line.Width(width);
    line.Height(height);
    line.StrokeThickness(1.4);
    line.Stroke(MakeSparklineBrush());
    line.Opacity(opacity);
    return line;
}

struct MetricCardElements
{
    Border Card{ nullptr };
    TextBlock Value{ nullptr };
    TextBlock Detail{ nullptr };
    ProgressBar Progress{ nullptr };
    XamlPolyline Sparkline{ nullptr };
    Button Action{ nullptr };
};

enum class MetricCardLayout
{
    Sparkline,
    InlineDetailWithProgress,
    SparklineWithDetail
};

// Builds one mockup-aligned metric card and keeps its live-updated elements reachable.
inline MetricCardElements MakeMetricCard(
    winrt::Windows::UI::Xaml::Media::Brush const& material,
    winrt::hstring const& label,
    winrt::hstring const& value,
    winrt::hstring const& detail,
    double progress,
    MetricCardLayout layout,
    bool emphasized = false,
    std::shared_ptr<std::wstring> actionTarget = {},
    TextBlock status = nullptr,
    ActionCallback action = {})
{
    auto card = MakeCard(Thickness{ 0, 0, 0, 0 });
    card.MinHeight(82);
    card.Background(material);
    card.BorderBrush(SolidColorBrush(winrt::Windows::UI::ColorHelper::FromArgb(
        emphasized ? 176 : 30,
        255,
        255,
        255)));

    auto shell = StackPanel();
    auto panel = StackPanel();
    panel.Margin(Thickness{ 14, 9, 14, 8 });
    panel.Spacing(5);

    auto labelText = MakeHeroText(label, 10, 0.82);
    labelText.Text.TextTrimming(TextTrimming::CharacterEllipsis);
    panel.Children().Append(labelText.Root);

    auto valueGrid = Grid();
    valueGrid.ColumnDefinitions().Append(ColumnDefinition());
    valueGrid.ColumnDefinitions().Append(ColumnDefinition());
    valueGrid.ColumnDefinitions().GetAt(0).Width(Star());
    valueGrid.ColumnDefinitions().GetAt(1).Width(GridLength{ 1, GridUnitType::Auto });
    auto valueText = MakeHeroText(value, 14.5);
    valueText.Text.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    valueText.Text.TextTrimming(TextTrimming::CharacterEllipsis);
    valueGrid.Children().Append(valueText.Root);

    auto detailText = MakeHeroText(detail, 8.5, 0.76);
    detailText.Text.TextTrimming(TextTrimming::CharacterEllipsis);
    auto sparkline = MakeSparkline();
    if (layout == MetricCardLayout::InlineDetailWithProgress)
    {
        detailText.Root.Margin(Thickness{ 7, 0, 0, 0 });
        detailText.Root.HorizontalAlignment(HorizontalAlignment::Right);
        detailText.Root.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(detailText.Root, 1);
        valueGrid.Children().Append(detailText.Root);
    }
    else
    {
        sparkline.Margin(Thickness{ 7, 0, 0, 0 });
        sparkline.HorizontalAlignment(HorizontalAlignment::Right);
        sparkline.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(sparkline, 1);
        valueGrid.Children().Append(sparkline);
    }
    panel.Children().Append(valueGrid);

    auto bar = ProgressBar();
    bar.Minimum(0);
    bar.Maximum(100);
    bar.Value(progress);
    bar.Height(5);
    bar.Foreground(MakeMetricAccentBrush());
    bar.Visibility(layout == MetricCardLayout::InlineDetailWithProgress
        ? Visibility::Visible
        : Visibility::Collapsed);
    winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetName(
        bar,
        winrt::hstring(std::wstring(label.c_str()) + L": " + value.c_str()));
    if (layout == MetricCardLayout::InlineDetailWithProgress)
    {
        panel.Children().Append(bar);
    }
    else if (layout == MetricCardLayout::SparklineWithDetail)
    {
        panel.Children().Append(detailText.Root);
    }
    shell.Children().Append(panel);

    auto actionButton = Button{ nullptr };
    if (actionTarget)
    {
        auto separator = Border();
        separator.Height(1);
        separator.Background(SolidColorBrush(
            winrt::Windows::UI::ColorHelper::FromArgb(54, 255, 255, 255)));
        shell.Children().Append(separator);

        actionButton = Button();
        actionButton.Height(28);
        actionButton.Padding(Thickness{ 10, 0, 8, 0 });
        actionButton.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        actionButton.Background(SolidColorBrush(
            winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0)));
        actionButton.BorderBrush(SolidColorBrush(
            winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0)));
        actionButton.BorderThickness(Thickness{ 0, 0, 0, 0 });
        auto actionContent = Grid();
        actionContent.ColumnDefinitions().Append(ColumnDefinition());
        actionContent.ColumnDefinitions().Append(ColumnDefinition());
        actionContent.ColumnDefinitions().GetAt(0).Width(Star());
        actionContent.ColumnDefinitions().GetAt(1).Width(GridLength{ 20, GridUnitType::Pixel });
        auto actionText = MakeHeroText(L"Apri in Explorer", 9, 0.88);
        actionText.Root.VerticalAlignment(VerticalAlignment::Center);
        actionContent.Children().Append(actionText.Root);
        auto actionIcon = FontIcon();
        actionIcon.Glyph(L"\xE8A7");
        actionIcon.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
        actionIcon.FontSize(10);
        actionIcon.Opacity(0.82);
        actionIcon.HorizontalAlignment(HorizontalAlignment::Right);
        Grid::SetColumn(actionIcon, 1);
        actionContent.Children().Append(actionIcon);
        actionButton.Content(actionContent);
        actionButton.Click([actionTarget, status, action](auto const&, auto const&)
        {
            if (actionTarget->empty())
            {
                if (status) status.Text(L"Volume principale non disponibile");
                return;
            }
            if (action) action(L"folder.open", *actionTarget);
            if (status) status.Text(L"Apertura del volume principale richiesta");
        });
        shell.Children().Append(actionButton);
    }

    card.Child(shell);
    return MetricCardElements{ card, valueText.Text, detailText.Text, bar, sparkline, actionButton };
}

// Adds a metric card to the hero grid while leaving its width to the responsive layout.
inline void AppendHeroMetric(
    winrt::Windows::UI::Xaml::Controls::Grid const& grid,
    Border const& card,
    std::uint32_t column,
    std::uint32_t row)
{
    card.Margin(Thickness{ 3, 3, 3, 3 });
    card.HorizontalAlignment(HorizontalAlignment::Stretch);
    card.VerticalAlignment(VerticalAlignment::Top);
    Grid::SetColumn(card, column);
    Grid::SetRow(card, row);
    grid.Children().Append(card);
}

// Reflows the four hero metrics without rebuilding their live-bound controls.
inline void ConfigureMetricGrid(
    Grid const& metrics,
    std::array<Border, 4> const& cards,
    std::uint32_t columnCount)
{
    metrics.ColumnDefinitions().Clear();
    metrics.RowDefinitions().Clear();
    const auto safeColumnCount = (std::max)(1u, columnCount);
    const auto rowCount = static_cast<std::uint32_t>((cards.size() + safeColumnCount - 1) / safeColumnCount);
    for (std::uint32_t column = 0; column < safeColumnCount; ++column)
    {
        metrics.ColumnDefinitions().Append(ColumnDefinition());
        const std::array<double, 4> wideWeights{ 0.96, 1.02, 1.12, 1.0 };
        metrics.ColumnDefinitions().GetAt(column).Width(
            Star(safeColumnCount == 4 ? wideWeights[column] : 1.0));
    }
    for (std::uint32_t row = 0; row < rowCount; ++row)
    {
        metrics.RowDefinitions().Append(RowDefinition());
    }

    // Attached grid coordinates can be changed in place, preserving metric animations and values.
    for (std::uint32_t index = 0; index < cards.size(); ++index)
    {
        Grid::SetColumn(cards[index], index % safeColumnCount);
        Grid::SetRow(cards[index], index / safeColumnCount);
    }
}

// Distributes wrap-grid cards into the largest readable column count for the available width.
inline void ApplyWrapCardWidths(
    VariableSizedWrapGrid const& wrap,
    double availableWidth,
    std::uint32_t maximumColumns = 4)
{
    const auto itemCount = wrap.Children().Size();
    if (itemCount == 0)
    {
        return;
    }

    constexpr double preferredWidth = 260.0;
    constexpr double outerGap = 8.0;
    const auto fittedColumns = static_cast<std::uint32_t>(
        (std::max)(1.0, std::floor((availableWidth + outerGap) / (preferredWidth + outerGap))));
    const auto columnCount = (std::min)((std::min)(maximumColumns, itemCount), fittedColumns);
    const auto itemWidth = (std::max)(
        210.0,
        (availableWidth - outerGap * columnCount) / columnCount);

    // Width includes a small external margin so native cards keep an even visual rhythm while wrapping.
    wrap.MaximumRowsOrColumns(columnCount);
    for (auto const& child : wrap.Children())
    {
        if (auto element = child.try_as<FrameworkElement>())
        {
            element.Width(itemWidth);
            element.Margin(Thickness{ 4, 4, 4, 4 });
        }
    }
}

// Applies wide, medium, and compact arrangements from the XAML root width in device-independent pixels.
inline void ApplyResponsiveLayout(
    double rootWidth,
    Border const& hero,
    Grid const& heroGrid,
    StackPanel const& identity,
    Grid const& metrics,
    std::array<Border, 4> const& metricCards,
    VariableSizedWrapGrid const& storageWrap,
    VariableSizedWrapGrid const& lower)
{
    const auto safeRootWidth = (std::max)(320.0, rootWidth);
    const auto contentWidth = safeRootWidth - 36.0;
    heroGrid.ColumnDefinitions().Clear();
    heroGrid.RowDefinitions().Clear();
    identity.Margin(Thickness{ 0, 0, 0, 0 });
    metrics.HorizontalAlignment(HorizontalAlignment::Right);
    metrics.VerticalAlignment(VerticalAlignment::Center);

    if (safeRootWidth >= 900.0)
    {
        hero.Padding(Thickness{ 18, 18, 18, 18 });
        hero.MinHeight(170);
        heroGrid.ColumnDefinitions().Append(ColumnDefinition());
        heroGrid.ColumnDefinitions().Append(ColumnDefinition());
        heroGrid.ColumnDefinitions().GetAt(0).Width(GridLength{ 230, GridUnitType::Pixel });
        heroGrid.ColumnDefinitions().GetAt(1).Width(Star());
        heroGrid.RowDefinitions().Append(RowDefinition());
        Grid::SetColumn(identity, 0);
        Grid::SetRow(identity, 0);
        Grid::SetColumn(metrics, 1);
        Grid::SetRow(metrics, 0);
        metrics.Margin(Thickness{ 35, 0, 0, 0 });
        const auto desiredMetricsWidth =
            (std::max)(760.0, (std::min)(900.0, safeRootWidth * 0.58));
        metrics.Width((std::min)(desiredMetricsWidth, safeRootWidth - 310.0));
        metrics.HorizontalAlignment(HorizontalAlignment::Left);
        metrics.VerticalAlignment(VerticalAlignment::Bottom);
        ConfigureMetricGrid(metrics, metricCards, 4);
    }
    else if (safeRootWidth >= 600.0)
    {
        hero.Padding(Thickness{ 20, 18, 20, 18 });
        hero.MinHeight(190);
        heroGrid.ColumnDefinitions().Append(ColumnDefinition());
        heroGrid.ColumnDefinitions().Append(ColumnDefinition());
        heroGrid.ColumnDefinitions().GetAt(0).Width(GridLength{ 210, GridUnitType::Pixel });
        heroGrid.ColumnDefinitions().GetAt(1).Width(Star());
        heroGrid.RowDefinitions().Append(RowDefinition());
        Grid::SetColumn(identity, 0);
        Grid::SetRow(identity, 0);
        Grid::SetColumn(metrics, 1);
        Grid::SetRow(metrics, 0);
        metrics.Margin(Thickness{ 12, 0, 0, 0 });
        metrics.Width((std::max)(300.0, safeRootWidth - 290.0));
        ConfigureMetricGrid(metrics, metricCards, 2);
    }
    else
    {
        hero.Padding(Thickness{ 18, 18, 18, 18 });
        hero.MinHeight(0);
        heroGrid.ColumnDefinitions().Append(ColumnDefinition());
        heroGrid.ColumnDefinitions().GetAt(0).Width(Star());
        heroGrid.RowDefinitions().Append(RowDefinition());
        heroGrid.RowDefinitions().Append(RowDefinition());
        Grid::SetColumn(identity, 0);
        Grid::SetRow(identity, 0);
        Grid::SetColumn(metrics, 0);
        Grid::SetRow(metrics, 1);
        metrics.Margin(Thickness{ 0, 14, 0, 0 });
        metrics.Width(std::numeric_limits<double>::quiet_NaN());
        metrics.HorizontalAlignment(HorizontalAlignment::Stretch);
        ConfigureMetricGrid(metrics, metricCards, safeRootWidth >= 410.0 ? 2 : 1);
    }

    const auto wrapWidth = (std::max)(280.0, contentWidth);
    ApplyWrapCardWidths(storageWrap, wrapWidth);
    ApplyWrapCardWidths(lower, wrapWidth);
}

// Removes the filled-button surface so list actions read as native rows inside their parent card.
inline void StyleTransparentActionButton(Button const& button)
{
    const auto transparent = ThemeBrush(
        L"SystemControlTransparentBrush",
        winrt::Windows::UI::ColorHelper::FromArgb(0, 0, 0, 0));
    button.Background(transparent);
    button.BorderBrush(transparent);
    button.BorderThickness(Thickness{ 0, 0, 0, 0 });
    button.Padding(Thickness{ 6, 3, 6, 3 });
}

// Builds a drive tile that matches the canonical storage row: icon, health, usage, and overflow.
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
    auto card = MakeCard(Thickness{ 12, 10, 12, 10 });
    card.MinHeight(88);
    auto panel = StackPanel();
    panel.Spacing(6);

    auto header = Grid();
    header.ColumnDefinitions().Append(ColumnDefinition());
    header.ColumnDefinitions().Append(ColumnDefinition());
    header.ColumnDefinitions().Append(ColumnDefinition());
    header.ColumnDefinitions().Append(ColumnDefinition());
    header.ColumnDefinitions().GetAt(0).Width(GridLength{ 32, GridUnitType::Pixel });
    header.ColumnDefinitions().GetAt(1).Width(Star());
    header.ColumnDefinitions().GetAt(2).Width(GridLength{ 84, GridUnitType::Pixel });
    header.ColumnDefinitions().GetAt(3).Width(GridLength{ 30, GridUnitType::Pixel });

    auto driveIcon = FontIcon();
    driveIcon.Glyph(L"\xE958");
    driveIcon.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    driveIcon.FontSize(22);
    driveIcon.Opacity(0.9);
    header.Children().Append(driveIcon);

    auto titleText = MakeText(title, 12);
    titleText.TextTrimming(TextTrimming::CharacterEllipsis);
    Grid::SetColumn(titleText, 1);
    header.Children().Append(titleText);

    const auto compactHealth = healthText == L"Stato sconosciuto"
        ? winrt::hstring(L"Sconosciuto")
        : healthText;
    auto health = MakeText(compactHealth, 10, 0.78);
    health.HorizontalAlignment(HorizontalAlignment::Right);
    Grid::SetColumn(health, 2);
    header.Children().Append(health);

    auto more = Button();
    more.Width(28);
    more.Height(28);
    more.Padding(Thickness{ 0, 0, 0, 0 });
    StyleTransparentActionButton(more);
    more.Padding(Thickness{ 0, 0, 0, 0 });
    auto moreIcon = FontIcon();
    moreIcon.Glyph(L"\xE712");
    moreIcon.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    moreIcon.FontSize(12);
    more.Content(moreIcon);
    ToolTipService::SetToolTip(more, winrt::box_value(L"Apri in Explorer"));
    more.Click([target, status, action](auto const&, auto const&)
    {
        if (action) action(L"folder.open", target);
        status.Text(L"Richiesta inviata al broker");
    });
    Grid::SetColumn(more, 3);
    header.Children().Append(more);
    panel.Children().Append(header);

    auto bar = ProgressBar();
    bar.Minimum(0);
    bar.Maximum(100);
    bar.Value(progress);
    bar.Height(5);
    bar.Foreground(ThemeBrush(
        L"SystemControlHighlightAccentBrush",
        winrt::Windows::UI::ColorHelper::FromArgb(255, 71, 170, 255)));
    winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetName(
        bar,
        winrt::hstring(
            std::wstring(title.c_str()) + L": " + used.c_str() + L"; " + healthText.c_str()));
    panel.Children().Append(bar);
    auto footer = Grid();
    footer.ColumnDefinitions().Append(ColumnDefinition());
    footer.ColumnDefinitions().Append(ColumnDefinition());
    footer.ColumnDefinitions().GetAt(0).Width(Star());
    footer.ColumnDefinitions().GetAt(1).Width(Star());
    auto usedText = MakeText(used, 11, 0.78);
    usedText.TextTrimming(TextTrimming::CharacterEllipsis);
    footer.Children().Append(usedText);
    auto fs = MakeText(format, 11, 0.72);
    fs.TextTrimming(TextTrimming::CharacterEllipsis);
    fs.HorizontalAlignment(HorizontalAlignment::Right);
    Grid::SetColumn(fs, 1);
    footer.Children().Append(fs);
    panel.Children().Append(footer);

    card.Child(panel);
    return card;
}

// Chooses a standard Windows glyph for the kind of list action being displayed.
inline winrt::hstring GlyphForAction(std::wstring const& actionName)
{
    if (actionName == L"terminal.open") return L"\xE756";
    if (actionName == L"folder.open") return L"\xE8B7";
    if (actionName == L"item.open") return L"\xE8A5";
    return L"\xE7C5";
}

// Chooses a standard Windows glyph for a quick Settings URI.
inline winrt::hstring GlyphForSetting(std::wstring const& uri)
{
    if (uri.find(L"display") != std::wstring::npos) return L"\xE7F4";
    if (uri.find(L"sound") != std::wstring::npos) return L"\xE767";
    if (uri.find(L"network") != std::wstring::npos) return L"\xE968";
    if (uri.find(L"bluetooth") != std::wstring::npos) return L"\xE702";
    if (uri.find(L"storage") != std::wstring::npos) return L"\xE958";
    if (uri.find(L"windowsupdate") != std::wstring::npos) return L"\xE895";
    return L"\xE713";
}

// Builds compact icon-and-text content for a native Windows button.
inline StackPanel MakeButtonContent(
    winrt::hstring const& glyph,
    winrt::hstring const& label)
{
    auto content = StackPanel();
    content.Orientation(Orientation::Horizontal);
    content.Spacing(8);
    auto icon = FontIcon();
    icon.Glyph(glyph);
    icon.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    icon.FontSize(13);
    content.Children().Append(icon);
    content.Children().Append(MakeText(label, 12));
    return content;
}

// Builds a simple action list card for static rows.
inline winrt::Windows::UI::Xaml::Controls::Border MakeListCard(
    winrt::hstring const& title,
    std::vector<std::pair<winrt::hstring, winrt::hstring>> const& rows,
    winrt::Windows::UI::Xaml::Controls::TextBlock const& status,
    ActionCallback const& action,
    std::wstring const& actionName = L"item.open")
{
    auto card = MakeCard();
    card.Height(272);
    auto layout = Grid();
    layout.RowDefinitions().Append(RowDefinition());
    layout.RowDefinitions().Append(RowDefinition());
    layout.RowDefinitions().GetAt(0).Height(GridLength{ 1, GridUnitType::Auto });
    layout.RowDefinitions().GetAt(1).Height(Star());
    layout.Children().Append(MakeText(title, 16));

    auto rowsScroll = ScrollViewer();
    rowsScroll.Margin(Thickness{ 0, 8, 0, 0 });
    rowsScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    rowsScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    auto rowsPanel = StackPanel();
    rowsPanel.Spacing(8);
    for (auto const& row : rows)
    {
        auto button = Button();
        StyleTransparentActionButton(button);
        button.HorizontalContentAlignment(HorizontalAlignment::Left);
        button.HorizontalAlignment(HorizontalAlignment::Stretch);
        button.MinHeight(34);
        auto rowGrid = Grid();
        rowGrid.ColumnDefinitions().Append(ColumnDefinition());
        rowGrid.ColumnDefinitions().Append(ColumnDefinition());
        rowGrid.ColumnDefinitions().GetAt(0).Width(Star());
        rowGrid.ColumnDefinitions().GetAt(1).Width(GridLength{ 90, GridUnitType::Pixel });
        rowGrid.Children().Append(MakeIconText(GlyphForAction(actionName), row.first, 12, 0.86));
        auto detail = MakeText(row.second, 11, 0.68);
        detail.HorizontalAlignment(HorizontalAlignment::Right);
        detail.TextTrimming(TextTrimming::CharacterEllipsis);
        Grid::SetColumn(detail, 1);
        rowGrid.Children().Append(detail);
        button.Content(rowGrid);
        button.Click([status, action, title, row, actionName](auto const&, auto const&)
        {
            if (action) action(actionName, std::wstring(row.first.c_str()));
            status.Text(winrt::hstring(L"Azione richiesta: ") + title);
        });
        rowsPanel.Children().Append(button);
    }
    rowsScroll.Content(rowsPanel);
    Grid::SetRow(rowsScroll, 1);
    layout.Children().Append(rowsScroll);
    card.Child(layout);
    return card;
}

// Formats byte values for compact card labels.
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

// Returns the first ready local volume used by the compact system-storage metric.
inline NativeStorageItem const* PrimaryStorage(NativeSnapshot const& snapshot)
{
    for (auto const& item : snapshot.Storage)
    {
        if (!item.IsNetwork && item.IsReady && item.TotalBytes && item.FreeBytes)
        {
            return &item;
        }
    }
    return nullptr;
}

// Formats the primary volume usage without replacing unavailable data with a fake value.
inline std::wstring PrimaryStorageUsage(NativeSnapshot const& snapshot)
{
    const auto item = PrimaryStorage(snapshot);
    if (!item)
    {
        return L"Non disponibile";
    }

    const auto total = item->TotalBytes.value_or(0);
    const auto free = item->FreeBytes.value_or(0);
    const auto used = total >= free ? total - free : 0;
    return FormatBytes(used) + L"/" + FormatBytes(total);
}

// Formats optional utilization as a percent without pretending unknown data is zero.
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

// Formats aggregate receive and send rates for the network card.
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

// Converts the aggregate network rate to the same decimal megabytes used by broker history.
inline std::optional<double> NetworkMegabytesPerSecond(
    std::optional<std::uint64_t> receive,
    std::optional<std::uint64_t> send)
{
    if (!receive || !send)
    {
        return std::nullopt;
    }
    return (*receive + *send) / 1'000'000.0;
}

// Converts optional utilization into a bounded progress value.
inline double ProgressValue(std::optional<double> value)
{
    return value ? (std::max)(0.0, (std::min)(100.0, *value)) : 0.0;
}

// Applies the current wallpaper with a readable fallback material.
inline void ApplyWallpaper(
    Border const& hero,
    std::wstring const& wallpaperPath)
{
    hero.Background(ThemeBrush(
        L"SystemControlBackgroundChromeMediumBrush",
        winrt::Windows::UI::ColorHelper::FromArgb(255, 53, 51, 53)));
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
        imageBrush.Opacity(0.78);
        hero.Background(imageBrush);
    }
    catch (...)
    {
        // Preserve the system-color fallback for missing or inaccessible images.
    }
}

// Builds a populated list card from broker-backed Windows items.
inline Border MakeNativeListCard(
    winrt::hstring const& title,
    std::vector<NativeListItem> const& rows,
    winrt::hstring const& emptyMessage,
    TextBlock const& status,
    ActionCallback const& action,
    std::wstring const& actionName)
{
    auto card = MakeCard();
    card.Height(272);
    auto layout = Grid();
    layout.RowDefinitions().Append(RowDefinition());
    layout.RowDefinitions().Append(RowDefinition());
    layout.RowDefinitions().GetAt(0).Height(GridLength{ 1, GridUnitType::Auto });
    layout.RowDefinitions().GetAt(1).Height(Star());
    layout.Children().Append(MakeText(title, 16));

    auto rowsScroll = ScrollViewer();
    rowsScroll.Margin(Thickness{ 0, 8, 0, 0 });
    rowsScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    rowsScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    auto rowsPanel = StackPanel();
    rowsPanel.Spacing(8);

    if (rows.empty())
    {
        auto empty = MakeText(emptyMessage, 12, 0.72);
        empty.TextWrapping(TextWrapping::Wrap);
        rowsPanel.Children().Append(empty);
    }
    else
    {
        for (auto const& row : rows)
        {
            auto button = Button();
            StyleTransparentActionButton(button);
            button.HorizontalContentAlignment(HorizontalAlignment::Left);
            button.HorizontalAlignment(HorizontalAlignment::Stretch);
            button.MinHeight(34);
            button.IsEnabled(row.IsAvailable && !row.Target.empty());

            auto rowGrid = Grid();
            rowGrid.ColumnDefinitions().Append(ColumnDefinition());
            rowGrid.ColumnDefinitions().Append(ColumnDefinition());
            rowGrid.ColumnDefinitions().GetAt(0).Width(Star());
            rowGrid.ColumnDefinitions().GetAt(1).Width(GridLength{ 96, GridUnitType::Pixel });
            rowGrid.Children().Append(MakeIconText(
                GlyphForAction(actionName),
                winrt::hstring(row.DisplayName),
                12,
                0.86));

            auto detail = MakeText(winrt::hstring(row.Detail), 11, 0.68);
            detail.HorizontalAlignment(HorizontalAlignment::Right);
            detail.TextTrimming(TextTrimming::CharacterEllipsis);
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
            rowsPanel.Children().Append(button);
        }
    }

    rowsScroll.Content(rowsPanel);
    Grid::SetRow(rowsScroll, 1);
    layout.Children().Append(rowsScroll);
    card.Child(layout);
    return card;
}

// Rebuilds the storage section; the page-level responsive pass assigns final card widths.
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
        empty.Width(220);
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
        disk.Width(220);
        storageWrap.Children().Append(disk);
    }
}

// Rebuilds the four lower dashboard cards before the responsive width pass.
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
    network.Width(220);
    lower.Children().Append(network);

    auto recent = MakeNativeListCard(
        L"Recenti",
        snapshot.RecentItems,
        L"Windows non espone documenti recenti",
        status,
        action,
        L"item.open");
    recent.Width(220);
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
    highlighted.Width(220);
    lower.Children().Append(highlighted);

    auto terminal = MakeNativeListCard(
        L"Terminale",
        snapshot.TerminalProfiles,
        L"Nessun profilo terminale disponibile",
        status,
        action,
        L"terminal.open");
    terminal.Width(220);
    lower.Children().Append(terminal);
}

// Rebuilds documented Windows Settings shortcuts from snapshot preferences.
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
        button.Content(MakeButtonContent(
            GlyphForSetting(item.Uri),
            winrt::hstring(item.DisplayName)));
        button.MinHeight(40);
        button.MinWidth(120);
        button.Margin(Thickness{ 4, 4, 4, 4 });
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

// Builds the complete Explorer Home V2 page shared by the standalone host and namespace view.
inline winrt::Windows::UI::Xaml::UIElement BuildWelcomePage(
    std::function<bool()> const& pingBroker,
    ActionCallback const& action,
    std::wstring const& wallpaperPath = {},
    SnapshotCallback const& snapshot = {},
    NativeSnapshot const& initialSnapshot = {},
    SnapshotCallback const& metrics = {})
{
    auto root = Grid();
    auto pageAlive = std::make_shared<std::atomic_bool>(true);
    auto refreshRunning = std::make_shared<std::atomic_bool>(false);
    root.RequestedTheme(ElementTheme::Default);
    root.Background(ThemeBrush(
        L"SystemControlBackgroundChromeMediumBrush",
        winrt::Windows::UI::ColorHelper::FromArgb(255, 31, 31, 32)));

    auto scroll = ScrollViewer();
    scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    scroll.Padding(Thickness{ 10, 8, 10, 12 });
    auto page = StackPanel();
    page.Spacing(10);

    auto status = MakeText(L"Snapshot mock pronto · broker non interrogato", 12, 0.78);
    winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetName(
        status,
        L"Stato dashboard");
    winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetLiveSetting(
        status,
        winrt::Windows::UI::Xaml::Automation::Peers::AutomationLiveSetting::Polite);

    auto hero = Border();
    hero.CornerRadius(CornerRadius{ 7, 7, 7, 7 });
    hero.MinHeight(170);
    hero.Padding(Thickness{ 24, 18, 24, 18 });
    ApplyWallpaper(
        hero,
        initialSnapshot.WallpaperPath.empty() ? wallpaperPath : initialSnapshot.WallpaperPath);

    auto heroGrid = Grid();

    auto identity = StackPanel();
    identity.Spacing(6);
    auto machineNameElements = MakeHeroText(
        winrt::hstring(initialSnapshot.IsLoaded ? initialSnapshot.MachineName : L"WORKSTATION"),
        17);
    auto machineName = machineNameElements.Text;
    machineName.TextTrimming(TextTrimming::CharacterEllipsis);
    auto machineModelElements = MakeHeroText(
        winrt::hstring(initialSnapshot.IsLoaded ? initialSnapshot.MachineModel : L"Views Explorer Home V2"),
        9,
        0.82);
    auto machineModel = machineModelElements.Text;
    machineModel.TextTrimming(TextTrimming::CharacterEllipsis);
    auto networkIdentityElements = MakeHeroText(
        winrt::hstring(initialSnapshot.IsLoaded && !initialSnapshot.PrimaryNetworkIdentity.empty()
            ? initialSnapshot.PrimaryNetworkIdentity
            : L"Rete in attesa"),
        9,
        0.86);
    auto networkIdentity = networkIdentityElements.Text;
    networkIdentity.TextTrimming(TextTrimming::CharacterEllipsis);
    auto cpuIdentityElements = MakeHeroText(
        winrt::hstring(initialSnapshot.IsLoaded ? initialSnapshot.CpuModel : L"CPU in attesa"),
        9,
        0.86);
    auto cpuIdentity = cpuIdentityElements.Text;
    cpuIdentity.TextTrimming(TextTrimming::CharacterEllipsis);
    auto gpuIdentityElements = MakeHeroText(
        winrt::hstring(initialSnapshot.IsLoaded ? initialSnapshot.GpuModel : L"GPU in attesa"),
        9,
        0.86);
    auto gpuIdentity = gpuIdentityElements.Text;
    gpuIdentity.TextTrimming(TextTrimming::CharacterEllipsis);
    auto memoryIdentityElements = MakeHeroText(
        initialSnapshot.MemoryTotalBytes > 0
            ? winrt::hstring(L"RAM " + FormatBytes(initialSnapshot.MemoryUsedBytes) + L" di " +
                FormatBytes(initialSnapshot.MemoryTotalBytes))
            : winrt::hstring(L"RAM in attesa"),
        9,
        0.86);
    auto memoryIdentity = memoryIdentityElements.Text;
    memoryIdentity.TextTrimming(TextTrimming::CharacterEllipsis);
    identity.Children().Append(machineNameElements.Root);
    identity.Children().Append(machineModelElements.Root);
    identity.Children().Append(MakeIconTextBlock(L"\xE774", networkIdentityElements.Root, 9, 0.86));
    identity.Children().Append(MakeIconTextBlock(L"\xE950", cpuIdentityElements.Root, 9, 0.86));
    identity.Children().Append(MakeIconTextBlock(L"\xE7F4", gpuIdentityElements.Root, 9, 0.86));
    identity.Children().Append(MakeIconTextBlock(L"\xE8B9", memoryIdentityElements.Root, 9, 0.86));
    heroGrid.Children().Append(identity);

    auto heroMetrics = Grid();
    heroMetrics.HorizontalAlignment(HorizontalAlignment::Right);
    heroMetrics.VerticalAlignment(VerticalAlignment::Bottom);
    heroGrid.Children().Append(heroMetrics);

    auto metricMaterial = MakeMetricCardMaterial();
    auto cpu = MakeMetricCard(
        metricMaterial,
        L"CPU",
        winrt::hstring(initialSnapshot.IsLoaded ? FormatPercent(initialSnapshot.CpuPercent) : L"12%"),
        L"",
        initialSnapshot.IsLoaded ? ProgressValue(initialSnapshot.CpuPercent) : 12,
        MetricCardLayout::Sparkline);
    SetSparkline(
        cpu.Sparkline,
        initialSnapshot.CpuSparkline,
        initialSnapshot.IsLoaded ? initialSnapshot.CpuPercent : std::nullopt,
        SparklineKind::CpuPercent);
    AppendHeroMetric(heroMetrics, cpu.Card, 0, 0);
    auto memory = MakeMetricCard(
        metricMaterial,
        L"Memoria",
        winrt::hstring(initialSnapshot.IsLoaded ? FormatPercent(initialSnapshot.MemoryPercent) : L"38%"),
        initialSnapshot.MemoryTotalBytes > 0
            ? winrt::hstring(FormatBytes(initialSnapshot.MemoryUsedBytes) + L"/" + FormatBytes(initialSnapshot.MemoryTotalBytes))
            : winrt::hstring(L"In attesa"),
        initialSnapshot.IsLoaded ? ProgressValue(initialSnapshot.MemoryPercent) : 38,
        MetricCardLayout::InlineDetailWithProgress);
    AppendHeroMetric(heroMetrics, memory.Card, 1, 0);
    auto primaryStorageTarget = std::make_shared<std::wstring>();
    if (const auto item = PrimaryStorage(initialSnapshot))
    {
        *primaryStorageTarget = item->Path;
    }
    auto storage = MakeMetricCard(
        metricMaterial,
        L"Archiviazione",
        winrt::hstring(initialSnapshot.IsLoaded ? FormatPercent(initialSnapshot.StoragePercent) : L"52%"),
        initialSnapshot.IsLoaded
            ? winrt::hstring(PrimaryStorageUsage(initialSnapshot))
            : winrt::hstring(L"554 GB/1.0 TB"),
        initialSnapshot.IsLoaded ? ProgressValue(initialSnapshot.StoragePercent) : 52,
        MetricCardLayout::InlineDetailWithProgress,
        true,
        primaryStorageTarget,
        status,
        action);
    AppendHeroMetric(heroMetrics, storage.Card, 0, 1);
    auto network = MakeMetricCard(
        metricMaterial,
        L"Rete",
        winrt::hstring(initialSnapshot.IsLoaded
            ? FormatRate(initialSnapshot.NetworkReceiveBytesPerSecond, initialSnapshot.NetworkSendBytesPerSecond)
            : L"88 Mbps"),
        L"Invio/Ricezione",
        0,
        MetricCardLayout::SparklineWithDetail);
    SetSparkline(
        network.Sparkline,
        initialSnapshot.NetworkSparkline,
        initialSnapshot.IsLoaded
            ? NetworkMegabytesPerSecond(
                initialSnapshot.NetworkReceiveBytesPerSecond,
                initialSnapshot.NetworkSendBytesPerSecond)
            : std::nullopt,
        SparklineKind::NetworkMegabytesPerSecond);
    AppendHeroMetric(heroMetrics, network.Card, 1, 1);
    const std::array<Border, 4> metricCards{ cpu.Card, memory.Card, storage.Card, network.Card };
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
            disk.Width(220);
            storageWrap.Children().Append(disk);
        }
    }
    else
    {
        auto diskC = MakeStorageCard(L"C  System disk", L"196 GB usati di 418 GB", L"NTFS", 47, L"C:\\", status, action);
        diskC.Width(220);
        storageWrap.Children().Append(diskC);
        auto diskD = MakeStorageCard(L"D  VMX SDD 1.0TB INTERNAL", L"693 GB usati di 931 GB", L"NTFS", 74, L"D:\\", status, action);
        diskD.Width(220);
        storageWrap.Children().Append(diskD);
        auto diskE = MakeStorageCard(L"E  Git Temp & Swap", L"320 GB usati di 465 GB", L"NTFS", 69, L"E:\\", status, action);
        diskE.Width(220);
        storageWrap.Children().Append(diskE);
        auto diskS = MakeStorageCard(L"S  System part", L"46.4 GB usati di 46.5 GB", L"NTFS", 99, L"S:\\", status, action);
        diskS.Width(220);
        storageWrap.Children().Append(diskS);
    }
    page.Children().Append(storageWrap);

    auto lower = VariableSizedWrapGrid();
    lower.Orientation(Orientation::Horizontal);
    lower.MaximumRowsOrColumns(4);
    lower.ItemHeight(280);
    lower.HorizontalAlignment(HorizontalAlignment::Stretch);
    auto networkCard = MakeListCard(
        L"Rete",
        { { L"Spacey (\\\\HCMC-SERVER)", L"88% · Connesso" } },
        status,
        action,
        L"folder.open");
    networkCard.Width(220);
    lower.Children().Append(networkCard);
    auto recentCard = MakeListCard(L"Recenti", {
        { L"quarterly_report.pptx", L"Desktop · 09:14" },
        { L"project_plan.xlsx", L"30_Engineering · ieri" },
        { L"notes.md", L"Obsidian · ieri" },
        { L"architecture.drawio", L"30_Engineering · ieri" },
        { L"invoice_0425.pdf", L"Invoices · 2 giorni fa" }
    }, status, action, L"item.open");
    recentCard.Width(220);
    lower.Children().Append(recentCard);
    auto highlightedCard = MakeListCard(L"In evidenza", {
        { L"30_Engineering", L"Questo PC" },
        { L"Obsidian", L"Questo PC" },
        { L"70_Assets", L"Questo PC" },
        { L"Invoices", L"Questo PC" },
        { L"Windows Terminal", L"URL" }
    }, status, action, L"folder.open");
    highlightedCard.Width(220);
    lower.Children().Append(highlightedCard);
    auto terminalCard = MakeListCard(L"Terminale", {
        { L"PowerShell 7", L"Apri nella posizione corrente" },
        { L"Prompt dei comandi", L"Apri nella posizione corrente" },
        { L"Ubuntu (WSL)", L"Apri shell" },
        { L"Impostazioni terminale", L"Configura profili" }
    }, status, action, L"terminal.open");
    terminalCard.Width(220);
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
        button.Content(MakeButtonContent(
            GlyphForSetting(setting.second),
            winrt::hstring(setting.first)));
        button.MinHeight(40);
        button.MinWidth(120);
        button.Margin(Thickness{ 4, 4, 4, 4 });
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

    // Applies only the inexpensive fields delivered by the one-second metrics endpoint.
    const ApplySnapshotCallback applyLiveMetrics =
        [cpu, memory, network](NativeSnapshot const& data)
    {
        cpu.Value.Text(winrt::hstring(FormatPercent(data.CpuPercent)));
        SetSparkline(
            cpu.Sparkline,
            data.CpuSparkline,
            data.CpuPercent,
            SparklineKind::CpuPercent);
        winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetName(
            cpu.Card,
            winrt::hstring(L"CPU: " + FormatPercent(data.CpuPercent)));

        memory.Value.Text(winrt::hstring(FormatPercent(data.MemoryPercent)));
        memory.Progress.Value(ProgressValue(data.MemoryPercent));
        winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetName(
            memory.Progress,
            winrt::hstring(L"Memoria: " + FormatPercent(data.MemoryPercent)));

        network.Value.Text(winrt::hstring(
            FormatRate(data.NetworkReceiveBytesPerSecond, data.NetworkSendBytesPerSecond)));
        network.Detail.Text(L"Invio/Ricezione");
        SetSparkline(
            network.Sparkline,
            data.NetworkSparkline,
            NetworkMegabytesPerSecond(
                data.NetworkReceiveBytesPerSecond,
                data.NetworkSendBytesPerSecond),
            SparklineKind::NetworkMegabytesPerSecond);
        winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetName(
            network.Card,
            winrt::hstring(
                L"Rete: " +
                FormatRate(data.NetworkReceiveBytesPerSecond, data.NetworkSendBytesPerSecond)));
    };

    // Applies the full snapshot only on load or manual refresh, then rebuilds slower sections once.
    const ApplySnapshotCallback applySnapshot =
        [root,
         hero,
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
         primaryStorageTarget,
         storageWrap,
         lower,
         settings,
         status,
         action,
         applyLiveMetrics](NativeSnapshot const& data)
    {
        ApplyWallpaper(hero, data.WallpaperPath);
        machineName.Text(winrt::hstring(data.MachineName));
        machineModel.Text(winrt::hstring(
            data.OsDisplayVersion.empty()
                ? data.MachineModel
                : data.MachineModel + L" · Windows " + data.OsDisplayVersion));
        networkIdentity.Text(winrt::hstring(
            data.PrimaryNetworkIdentity.empty()
                ? L"Rete non disponibile"
                : data.PrimaryNetworkIdentity));
        cpuIdentity.Text(winrt::hstring(data.CpuModel));
        gpuIdentity.Text(winrt::hstring(data.GpuModel));
        memoryIdentity.Text(data.MemoryTotalBytes > 0
            ? winrt::hstring(L"RAM " + FormatBytes(data.MemoryUsedBytes) + L" di " + FormatBytes(data.MemoryTotalBytes))
            : winrt::hstring(L"RAM non disponibile"));

        memory.Detail.Text(data.MemoryTotalBytes > 0
            ? winrt::hstring(FormatBytes(data.MemoryUsedBytes) + L"/" + FormatBytes(data.MemoryTotalBytes))
            : winrt::hstring(L"Non disponibile"));
        storage.Value.Text(winrt::hstring(FormatPercent(data.StoragePercent)));
        storage.Detail.Text(winrt::hstring(PrimaryStorageUsage(data)));
        storage.Progress.Value(ProgressValue(data.StoragePercent));
        primaryStorageTarget->clear();
        if (const auto item = PrimaryStorage(data))
        {
            *primaryStorageTarget = item->Path;
        }
        winrt::Windows::UI::Xaml::Automation::AutomationProperties::SetName(
            storage.Progress,
            winrt::hstring(L"Archiviazione: " + FormatPercent(data.StoragePercent)));
        applyLiveMetrics(data);

        PopulateStorageCards(storageWrap, data, status, action);
        PopulateLowerCards(lower, data, status, action);
        PopulateQuickSettings(settings, data.QuickSettings, status, action);
        const auto availableWidth = (std::max)(280.0, root.ActualWidth() - 36.0);
        ApplyWrapCardWidths(storageWrap, availableWidth);
        ApplyWrapCardWidths(lower, availableWidth);
    };

    auto metricsTimer = DispatcherTimer();
    metricsTimer.Interval(std::chrono::seconds(1));
    auto metricsTickToken = std::make_shared<winrt::event_token>();
    auto metricsTickAttached = std::make_shared<std::atomic_bool>(false);

    auto retry = Button();
    retry.Content(winrt::box_value(L"Aggiorna dati"));
    retry.HorizontalAlignment(HorizontalAlignment::Left);
    retry.MinHeight(40);
    retry.Click([status, pingBroker, snapshot, applySnapshot, pageAlive, refreshRunning](auto const&, auto const&)
    {
        status.Text(L"Aggiornamento in corso…");
        if (snapshot)
        {
            RunSnapshotRefreshAsync(status, snapshot, applySnapshot, pageAlive, refreshRunning);
        }
        else
        {
            status.Text(pingBroker && pingBroker()
                ? L"Broker raggiungibile · snapshot non configurato"
                : L"Broker non disponibile · dati locali mantenuti");
        }
    });
    page.Children().Append(retry);

    root.SizeChanged([
        hero,
        heroGrid,
        identity,
        heroMetrics,
        metricCards,
        storageWrap,
        lower](auto const&, SizeChangedEventArgs const& args)
    {
        ApplyResponsiveLayout(
            args.NewSize().Width,
            hero,
            heroGrid,
            identity,
            heroMetrics,
            metricCards,
            storageWrap,
            lower);
    });

    scroll.Content(page);
    root.Children().Append(scroll);
    root.Loaded([
        root,
        hero,
        heroGrid,
        identity,
        heroMetrics,
        metricCards,
        storageWrap,
        lower,
        status,
        snapshot,
        metrics,
        applySnapshot,
        applyLiveMetrics,
        pageAlive,
        refreshRunning,
        metricsTimer,
        metricsTickToken,
        metricsTickAttached](auto const&, auto const&)
    {
        pageAlive->store(true);
        ApplyResponsiveLayout(
            root.ActualWidth(),
            hero,
            heroGrid,
            identity,
            heroMetrics,
            metricCards,
            storageWrap,
            lower);
        if (snapshot)
        {
            status.Text(L"Caricamento dati locali…");
            RunSnapshotRefreshAsync(status, snapshot, applySnapshot, pageAlive, refreshRunning);
        }
        if (metrics)
        {
            if (!metricsTickAttached->exchange(true))
            {
                *metricsTickToken = metricsTimer.Tick(
                    [status, metrics, applyLiveMetrics, pageAlive, refreshRunning](auto const&, auto const&)
                {
                    // Live ticks never rewrite the status live-region or rebuild dashboard sections.
                    RunSnapshotRefreshAsync(
                        status,
                        metrics,
                        applyLiveMetrics,
                        pageAlive,
                        refreshRunning,
                        false);
                });
            }
            metricsTimer.Start();
        }
    });
    root.Unloaded([
        pageAlive,
        metricsTimer,
        metricsTickToken,
        metricsTickAttached](auto const&, auto const&)
    {
        pageAlive->store(false);
        metricsTimer.Stop();
        if (metricsTickAttached->exchange(false))
        {
            try
            {
                metricsTimer.Tick(*metricsTickToken);
            }
            catch (...)
            {
                // The DispatcherTimer can already be torn down with its XAML Island.
            }
        }
    });
    return root;
}
}
