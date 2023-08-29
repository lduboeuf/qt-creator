// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#ifdef FAKEVIM_STANDALONE

#include <utils/store.h>

namespace Utils { class FilePath {}; }

#else

#include <coreplugin/dialogs/ioptionspage.h>

#endif

#include <QCoreApplication>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace FakeVim::Internal {

#ifdef FAKEVIM_STANDALONE

#ifdef QTC_USE_STORE
using Key = QByteArray;
#else
using Key = QString;
#endif

class FvBaseAspect
{
public:
    FvBaseAspect() = default;
    virtual ~FvBaseAspect() = default;

    virtual void setVariantValue(const QVariant &) {}
    virtual void setDefaultVariantValue(const QVariant &) {}
    virtual QVariant variantValue() const { return {}; }
    virtual QVariant defaultVariantValue() const { return {}; }
    void setSettingsKey(const Key &group, const Key &key);
    Key settingsKey() const;
    void setCheckable(bool) {}
    void setDisplayName(const QString &) {}
    void setToolTip(const QString &) {}

private:
    Key m_settingsGroup;
    Key m_settingsKey;
};

template <class ValueType>
class FvTypedAspect : public FvBaseAspect
{
public:
    void setVariantValue(const QVariant &value) override
    {
        m_value = value.value<ValueType>();
    }
    void setDefaultVariantValue(const QVariant &value) override
    {
        m_defaultValue = value.value<ValueType>();
    }
    QVariant variantValue() const override
    {
        return QVariant::fromValue<ValueType>(m_value);
    }
    QVariant defaultVariantValue() const override
    {
        return QVariant::fromValue<ValueType>(m_defaultValue);
    }

    ValueType value() const { return m_value; }
    ValueType operator()() const { return m_value; }

    ValueType m_value;
    ValueType m_defaultValue;
};

using FvBoolAspect = FvTypedAspect<bool>;
using FvIntegerAspect = FvTypedAspect<qint64>;
using FvStringAspect = FvTypedAspect<QString>;
using FvFilePathAspect = FvTypedAspect<Utils::FilePath>;

class FvAspectContainer : public FvBaseAspect
{
public:
};

#else

using FvAspectContainer = Utils::AspectContainer;
using FvBaseAspect = Utils::BaseAspect;
using FvBoolAspect = Utils::BoolAspect;
using FvIntegerAspect = Utils::IntegerAspect;
using FvStringAspect = Utils::StringAspect;
using FvFilePathAspect = Utils::FilePathAspect;

#endif

class FakeVimSettings final : public FvAspectContainer
{
public:
    FakeVimSettings();
    ~FakeVimSettings();

    FvBaseAspect *item(const Utils::Key &name);
    QString trySetValue(const QString &name, const QString &value);

    FvBoolAspect useFakeVim;
    FvBoolAspect readVimRc;
    FvFilePathAspect vimRcPath;

    FvBoolAspect startOfLine;
    FvIntegerAspect tabStop;
    FvBoolAspect hlSearch;
    FvBoolAspect smartTab;
    FvIntegerAspect shiftWidth;
    FvBoolAspect expandTab;
    FvBoolAspect autoIndent;
    FvBoolAspect smartIndent;

    FvBoolAspect incSearch;
    FvBoolAspect useCoreSearch;
    FvBoolAspect smartCase;
    FvBoolAspect ignoreCase;
    FvBoolAspect wrapScan;

    // command ~ behaves as g~
    FvBoolAspect tildeOp;

    // indent  allow backspacing over autoindent
    // eol     allow backspacing over line breaks (join lines)
    // start   allow backspacing over the start of insert; CTRL-W and CTRL-U
    //         stop once at the start of insert.
    FvStringAspect backspace;

    // @,48-57,_,192-255
    FvStringAspect isKeyword;

    // other actions
    FvBoolAspect showMarks;
    FvBoolAspect passControlKey;
    FvBoolAspect passKeys;
    FvStringAspect clipboard;
    FvBoolAspect showCmd;
    FvIntegerAspect scrollOff;
    FvBoolAspect relativeNumber;
    FvStringAspect formatOptions;

    // Plugin emulation
    FvBoolAspect emulateVimCommentary;
    FvBoolAspect emulateReplaceWithRegister;
    FvBoolAspect emulateExchange;
    FvBoolAspect emulateArgTextObj;
    FvBoolAspect emulateSurround;

    FvBoolAspect blinkingCursor;
    FvBoolAspect systemEncoding;

private:
    void setup(FvBaseAspect *aspect, const QVariant &value,
               const Utils::Key &settingsKey,
               const Utils::Key &shortName,
               const QString &label);

    QHash<Utils::Key, FvBaseAspect *> m_nameToAspect;
    QHash<FvBaseAspect *, Utils::Key> m_aspectToName;
};

FakeVimSettings &settings();

} // FakeVim::Internal
