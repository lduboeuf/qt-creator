// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

import QtQuick
//import QtQuick.Controls as C
import StudioTheme 1.0 as StudioTheme

Flickable {
    id: flickable

    property alias horizontalThickness: horizontalScrollBar.height
    property alias verticalThickness: verticalScrollBar.width
    readonly property bool verticalScrollBarVisible: verticalScrollBar.scrollBarVisible
    readonly property bool horizontalScrollBarVisible: horizontalScrollBar.scrollBarVisible
    readonly property bool bothVisible: flickable.verticalScrollBarVisible
                                        && flickable.horizontalScrollBarVisible

    property real temporaryHeight: 0

    default property alias content: areaItem.children

    property bool adsFocus: false
    // objectName is used by the dock widget to find this particular ScrollView
    // and set the ads focus on it.
    objectName: "__mainSrollView"

    HoverHandler { id: hoverHandler }

    ScrollBar.horizontal: ScrollBar {
        id: horizontalScrollBar
        parent: flickable
        x: 0
        y: flickable.height - horizontalScrollBar.height
        width: flickable.availableWidth - (verticalScrollBar.isNeeded ? verticalScrollBar.thickness : 0)
        orientation: Qt.Horizontal

        show: (hoverHandler.hovered || flickable.focus || flickable.adsFocus
               || horizontalScrollBar.inUse || horizontalScrollBar.otherInUse)
              && horizontalScrollBar.isNeeded
        otherInUse: verticalScrollBar.inUse
    }

    ScrollBar.vertical: ScrollBar {
        id: verticalScrollBar
        parent: flickable
        x: flickable.width - verticalScrollBar.width
        y: 0
        height: flickable.availableHeight - (horizontalScrollBar.isNeeded ? horizontalScrollBar.thickness : 0)
        orientation: Qt.Vertical

        show: (hoverHandler.hovered || flickable.focus || flickable.adsFocus
               || horizontalScrollBar.inUse || horizontalScrollBar.otherInUse)
              && verticalScrollBar.isNeeded
        otherInUse: horizontalScrollBar.inUse
    }

    contentWidth: areaItem.childrenRect.width
    contentHeight: Math.max(areaItem.childrenRect.height, flickable.temporaryHeight)

    boundsMovement: Flickable.StopAtBounds
    boundsBehavior: Flickable.StopAtBounds

    Item { id: areaItem }

}
