// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2026 Marcel Petrick
// Platform differences the Qt Test suites must isolate or normalize.
#pragma once

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QtGlobal>

namespace test_environment {

// Conf keeps its current configuration in an INI file on Windows and natively elsewhere.
inline constexpr QSettings::Format currentFormat =
#ifdef Q_OS_WIN32
    QSettings::IniFormat;
#else
    QSettings::NativeFormat;
#endif

// Redirect every per-user and system settings location the product uses into home.
// XDG variables cover Linux; Windows resolves INI locations from known folders, so
// they are set explicitly (home/qtscrob/qtscrob.ini, like XDG_CONFIG_HOME).
inline void isolate(const QString& home) {
    qputenv("HOME", home.toUtf8());
    qputenv("XDG_CONFIG_HOME", home.toUtf8());
    qputenv("XDG_CONFIG_DIRS", (home + "/system").toUtf8());
    qputenv("XDG_CACHE_HOME", home.toUtf8());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, home);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, home + "/system");
#ifdef Q_OS_WIN32
    // The offscreen platform looks for fonts next to Qt, which ships none.
    if (qEnvironmentVariableIsEmpty("QT_QPA_FONTDIR"))
        qputenv("QT_QPA_FONTDIR", (qEnvironmentVariable("SystemRoot", "C:/Windows") + "/Fonts").toUtf8());
#endif
}

// Legacy 0.10/0.11 settings are NativeFormat, which is the registry on Windows and cannot be
// redirected. Tests that create them may only run where clearing them is explicitly allowed.
inline bool legacyRegistryWritable() {
#ifdef Q_OS_WIN32
    return qEnvironmentVariable("QTSCROBBLER_TEST_WINDOWS_REGISTRY") == "1";
#else
    return true;
#endif
}

inline void clearLegacyRegistry() {
#ifdef Q_OS_WIN32
    for (const auto scope : {QSettings::UserScope, QSettings::SystemScope}) {
        QSettings legacy(QSettings::NativeFormat, scope, "qtscrob");
        legacy.clear();
        legacy.sync();
    }
#endif
}

// Child processes write text-mode stdout/stderr, which ends lines in \r\n on Windows.
inline QByteArray lines(QByteArray output) {
#ifdef Q_OS_WIN32
    output.replace("\r\n", "\n");
#endif
    return output;
}

}  // namespace test_environment
