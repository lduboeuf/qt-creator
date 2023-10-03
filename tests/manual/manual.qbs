Project {
    name: "QtcManualTests"

    qbsSearchPaths: "qbs"

    references: [
        "debugger/gui/gui.qbs",
        "debugger/simple/simple.qbs",
        "deviceshell/deviceshell.qbs",
        "fakevim/fakevim.qbs",
        "pluginview/pluginview.qbs",
        "proparser/testreader.qbs",
        "shootout/shootout.qbs",
        "spinner/spinner.qbs",
        "subdirfilecontainer/subdirfilecontainer.qbs",
        "tasking/demo/demo.qbs",
        "tasking/imagescaling/imagescaling.qbs",
        "widgets/widgets.qbs",
    ]
}
