// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

QT_BEGIN_NAMESPACE
class QString;
QT_END_NAMESPACE

namespace Utils {

class QtcSettings;

class QTCREATOR_UTILS_EXPORT UnixUtils
{
public:
    static QString defaultFileBrowser();
    static QString fileBrowser(const QtcSettings *settings);
    static void setFileBrowser(QtcSettings *settings, const QString &term);
    static QString fileBrowserHelpText();
    static QString substituteFileBrowserParameters(const QString &command,
                                                   const QString &file);
};

} // Utils
