// Copyright (C) 2020 Alexis Jeandet.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mesonprojectplugin.h"

#include "mesonactionsmanager.h"
#include "mesonbuildconfiguration.h"
#include "mesonbuildsystem.h"
#include "mesonproject.h"
#include "mesonrunconfiguration.h"
#include "ninjabuildstep.h"
#include "toolssettingsaccessor.h"
#include "toolssettingspage.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/projectmanager.h>
#include <projectexplorer/runcontrol.h>

#include <utils/fsengine/fileiconprovider.h>

using namespace ProjectExplorer;
using namespace Utils;

namespace MesonProjectManager::Internal {

class MesonProjectPluginPrivate
{
public:
    ToolsSettingsPage m_toolslSettingsPage;
    ToolsSettingsAccessor m_toolsSettings;
    MesonBuildStepFactory m_buildStepFactory;
    MesonBuildConfigurationFactory m_buildConfigurationFactory;
    MesonRunConfigurationFactory m_runConfigurationFactory;
    MesonActionsManager m_actions;
    MachineFileManager m_machineFilesManager;
    SimpleTargetRunnerFactory m_mesonRunWorkerFactory{{m_runConfigurationFactory.runConfigurationId()}};
};

MesonProjectPlugin::~MesonProjectPlugin()
{
    delete d;
}

void MesonProjectPlugin::initialize()
{
    d = new MesonProjectPluginPrivate;

    ProjectManager::registerProjectType<MesonProject>(Constants::Project::MIMETYPE);
    FileIconProvider::registerIconOverlayForFilename(Constants::Icons::MESON, "meson.build");
    FileIconProvider::registerIconOverlayForFilename(Constants::Icons::MESON, "meson_options.txt");
}

} // MesonProjectManager::Internal
