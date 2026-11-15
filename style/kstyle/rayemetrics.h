/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2020 Noah Davis <noahadvs@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtGlobal>

namespace Raye
{
//* standard pen widths
struct PenWidth {
    /* Using 1 instead of slightly more than 1 causes symbols drawn with
     * pen strokes to look skewed. The exact amount added does not matter
     * as long as it isn't too visible. Even with QPen::setCosmetic(true),
     * 1px pen widths still look slightly worse.
     */
    // The standard pen stroke width for symbols.
    static constexpr qreal Symbol = 1.001;

    // The standard pen stroke width for frames.
    static constexpr qreal Frame = 1.001;

    // The standard pen stroke width for shadows.
    static constexpr qreal Shadow = 1.001;

    // A value for pen width arguments to make it clear that there is no pen stroke
    static constexpr int NoPen = 0;
};

//* metrics, in pixels at 100 %. Raye follows the Windows 11 control
//* sizes (theming spec 1.9 and 3.3): 32 px controls, 4 px corners on
//* controls and 8 px on menus, thin scroll bars, 3 px progress bars.
struct Metrics {
    // general
    static constexpr int ArrowSize = 10;
    static constexpr int SmallArrowSize = 5;

    // frames
    static constexpr int Frame_FrameWidth = 2;
    static constexpr int Frame_FrameRadius = 4;

    // controls: buttons, fields and combo boxes are this tall
    static constexpr int Control_MinHeight = 32;

    // layout
    static constexpr int Layout_TopLevelMarginWidth = 12;
    static constexpr int Layout_ChildMarginWidth = 8;
    static constexpr int Layout_DefaultSpacing = 8;

    // line editors
    static constexpr int LineEdit_FrameWidth = 6;

    // menu items
    static constexpr int Menu_FrameWidth = 0;
    static constexpr int MenuItem_MarginWidth = 4;
    static constexpr int MenuItem_HighlightGap = 4;
    static constexpr int MenuItem_ExtraLeftMargin = 4;
    static constexpr int MenuItem_MarginHeight = 4;
    static constexpr int MenuItem_MinHeight = 32;
    // 8 px inside the item (+ 4 px menu padding = 12 px from the menu edge)
    static constexpr int MenuItem_ContentMargin = 8;
    static constexpr int MenuItem_ColumnSpacing = 12;
    static constexpr int MenuItem_GlyphSize = 16;
    static constexpr int Menu_Radius = 8;
    static constexpr int MenuItem_ItemSpacing = 2;
    static constexpr int MenuItem_AcceleratorSpace = 16;
    static constexpr int MenuItem_TextLeftMargin = 8;

    // combobox
    static constexpr int ComboBox_FrameWidth = 6;

    // spinbox
    static constexpr int SpinBox_FrameWidth = LineEdit_FrameWidth;
    static constexpr int SpinBox_ArrowButtonWidth = 20;

    // groupbox title margin
    static constexpr int GroupBox_TitleMarginWidth = 4;

    // buttons
    static constexpr int Button_MinWidth = 64;
    static constexpr int Button_MarginWidth = 6;
    // text padding left and right of a push button (11 px, Windows)
    static constexpr int Button_TextMarginWidth = 9;
    static constexpr int Button_TextMarginHeight = 4;
    static constexpr int Button_ItemSpacing = 4;

    // tool buttons
    static constexpr int ToolButton_MarginWidth = 6;
    static constexpr int ToolButton_ItemSpacing = 4;
    static constexpr int ToolButton_InlineIndicatorWidth = 12;

    // menu button indicator
    static constexpr int MenuButton_IndicatorWidth = 20;

    // checkboxes and radio buttons
    static constexpr int CheckBox_Size = 20;
    static constexpr int CheckBox_FocusMarginWidth = 2;
    static constexpr int CheckBox_ItemSpacing = 8;
    static constexpr int CheckBox_Radius = 4;

    // menubar items
    static constexpr int MenuBarItem_MarginWidth = 10;
    static constexpr int MenuBarItem_MarginHeight = 6;

    // scrollbars
    static constexpr int ScrollBar_Extend = 12;
    static constexpr int ScrollBar_SliderWidth = 6;
    // the scroll bar at rest: a thin line, 2 px from the outer edge
    static constexpr int ScrollBar_IdleWidth = 2;
    static constexpr int ScrollBar_IdleMargin = 2;
    static constexpr int ScrollBar_MinSliderHeight = 20;
    static constexpr int ScrollBar_NoButtonHeight = 3;
    static constexpr int ScrollBar_SingleButtonHeight = ScrollBar_Extend;
    static constexpr int ScrollBar_DoubleButtonHeight = 2 * ScrollBar_Extend;

    // toolbars
    static constexpr int ToolBar_FrameWidth = 0;
    static constexpr int ToolBar_HandleExtent = 10;
    static constexpr int ToolBar_HandleWidth = 6;
    static constexpr int ToolBar_SeparatorWidth = 8;
    static constexpr int ToolBar_ExtensionWidth = 20;
    static constexpr int ToolBar_ItemMargin = 6;
    static constexpr int ToolBar_ItemSpacing = 0;
    static constexpr int ToolBar_SeparatorVerticalMargin = 2;

    // progressbars
    static constexpr int ProgressBar_BusyIndicatorSize = 14;
    static constexpr int ProgressBar_Thickness = 3;
    static constexpr int ProgressBar_ItemSpacing = 4;

    // mdi title bar
    static constexpr int TitleBar_MarginWidth = 4;

    // sliders
    static constexpr int Slider_TickLength = 8;
    static constexpr int Slider_TickMarginWidth = 2;
    static constexpr int Slider_GrooveThickness = 4;
    static constexpr int Slider_ControlThickness = 20;

    // tabbar
    static constexpr int TabBar_TabMarginHeight = 4;
    static constexpr int TabBar_TabMarginWidth = 8;
    static constexpr int TabBar_TabMinWidth = 80;
    static constexpr int TabBar_TabMinHeight = 32;
    static constexpr int TabBar_StaticTabMinHeight = 34;
    static constexpr int TabBar_TabItemSpacing = 8;
    static constexpr int TabBar_TabOverlap = 1;
    static constexpr int TabBar_BaseOverlap = 2;
    static constexpr int TabBar_ActiveEffectSize = 3;

    // tab widget
    static constexpr int TabWidget_MarginWidth = 3;

    // toolbox
    static constexpr int ToolBox_TabMinWidth = 80;
    static constexpr int ToolBox_TabItemSpacing = 4;
    static constexpr int ToolBox_TabMarginWidth = 8;

    // tooltips
    // padding 8,5,8,7: the frame gives 5 all round, polish() adds the rest
    static constexpr int ToolTip_FrameWidth = 5;
    static constexpr int ToolTip_Radius = 4;

    // list headers
    static constexpr int Header_MarginWidth = 6;
    static constexpr int Header_ItemSpacing = 4;
    static constexpr int Header_ArrowSize = ArrowSize;

    // tree view
    static constexpr int ItemView_ArrowSize = ArrowSize;
    static constexpr int ItemView_ItemMarginWidth = 4;
    // list rows and side panels: 32 px rows, 4 px inset, a 3x16 accent pill on the selected one
    static constexpr int ItemView_MinHeight = 32;
    static constexpr int ItemView_PillWidth = 3;
    static constexpr int ItemView_PillHeight = 16;
    // the first cell's contents start after the pill: 4 px inset + 8 = 12 px from the row edge
    static constexpr int ItemView_TextIndent = 8;
    static constexpr int SidePanel_ItemMarginWidth = 4;

    // splitter
    static constexpr int Splitter_SplitterWidth = 1;

    // shadow dimensions
    static constexpr int Shadow_Overlap = 2;
};
}
