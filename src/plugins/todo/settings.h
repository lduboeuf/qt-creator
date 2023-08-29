// Copyright (C) 2016 Dmitry Savchenko
// Copyright (C) 2016 Vasiliy Sorokin
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "keyword.h"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Todo {
namespace Internal {

enum ScanningScope {
    ScanningScopeCurrentFile,
    ScanningScopeProject,
    ScanningScopeSubProject,
    ScanningScopeMax
};

class Settings {
public:
    KeywordList keywords;
    ScanningScope scanningScope = ScanningScopeCurrentFile;
    bool keywordsEdited = false;

    void save(QSettings *settings) const;
    void load(QSettings *settings);
    void setDefault();
    bool equals(const Settings &other) const;
};

bool operator ==(const Settings &s1, const Settings &s2);
bool operator !=(const Settings &s1, const Settings &s2);

} // namespace Internal
} // namespace Todo

Q_DECLARE_METATYPE(Todo::Internal::ScanningScope)
