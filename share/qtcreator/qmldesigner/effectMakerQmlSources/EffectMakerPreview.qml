// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

import QtQuick
import QtQuick.Layouts
import QtQuickDesignerTheme
import HelperWidgets as HelperWidgets
import StudioControls as StudioControls
import StudioTheme 1.0 as StudioTheme
import EffectMakerBackend

Column {
    id: root

    width: parent.width

    Rectangle { // toolbar
        width: parent.width
        height: StudioTheme.Values.toolbarHeight
        color: StudioTheme.Values.themeToolbarBackground

        RowLayout {
            anchors.fill: parent
            spacing: 5
            anchors.rightMargin: 5
            anchors.leftMargin: 5

            Item {
                Layout.fillWidth: true
            }

            HelperWidgets.AbstractButton {
                enabled: previewImage.scale > .4
                style: StudioTheme.Values.viewBarButtonStyle
                buttonIcon: StudioTheme.Constants.zoomOut_medium
                tooltip: qsTr("Zoom out")

                onClicked: {
                    previewImage.scale -= .2
                }
            }

            HelperWidgets.AbstractButton {
                enabled: previewImage.scale < 2
                style: StudioTheme.Values.viewBarButtonStyle
                buttonIcon: StudioTheme.Constants.zoomIn_medium
                tooltip: qsTr("Zoom In")

                onClicked: {
                    previewImage.scale += .2
                }
            }

            HelperWidgets.AbstractButton {
                enabled: previewImage.scale !== 1
                style: StudioTheme.Values.viewBarButtonStyle
                buttonIcon: StudioTheme.Constants.fitAll_medium
                tooltip: qsTr("Zoom Fit")

                onClicked: {
                    previewImage.scale = 1
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Column {
                Text {
                    text: "0.000s"
                    color: StudioTheme.Values.themeTextColor
                    font.pixelSize: 10
                }

                Text {
                    text: "0000000"
                    color: StudioTheme.Values.themeTextColor
                    font.pixelSize: 10
                }
            }

            HelperWidgets.AbstractButton {
                style: StudioTheme.Values.viewBarButtonStyle
                buttonIcon: StudioTheme.Constants.toStartFrame_medium
                tooltip: qsTr("Restart Animation")

                onClicked: {} // TODO
            }

            HelperWidgets.AbstractButton {
                style: StudioTheme.Values.viewBarButtonStyle
                buttonIcon: StudioTheme.Constants.topToolbar_runProject
                tooltip: qsTr("Play Animation")

                onClicked: {} // TODO
            }
        }
    }

    Rectangle { // preview image
        id: previewImageBg

        color: "#dddddd"
        width: parent.width
        height: 200
        clip: true

        Image {
            id: previewImage

            anchors.margins: 5
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            smooth: true

            source: "images/qt_logo.png" // TODO: update image

            Behavior on scale {
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.OutQuad
                }
            }
        }
    }
}
