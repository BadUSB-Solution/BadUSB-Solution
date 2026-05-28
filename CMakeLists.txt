cmake_minimum_required(VERSION 3.21)

project(BadUSBSolution LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_QT_UI "Build the Qt Widgets user interface" OFF)

add_library(CapstoneCore STATIC
    Capstone/Logger/logger.cpp
    Capstone/Logger/json_log.cpp
    Capstone/Policy/policy.cpp
    Capstone/Policy/settings.cpp
    Capstone/Storage/app_paths.cpp
    Capstone/Hook/keyboard_hook.cpp
    Capstone/Detect/hid_scan.cpp
    Capstone/Detect/hid_fingerprint.cpp
    Capstone/Algorithm/PreAnalysis/KeyDecoder/key_decoder.cpp
    Capstone/Algorithm/PreAnalysis/KeyNormalizer/key_normalizer.cpp
    Capstone/Algorithm/PreAnalysis/UrlExtractor/url_extractor.cpp
    Capstone/Algorithm/PreAnalysis/ShellDetector/shell_detector.cpp
    Capstone/Algorithm/Analysis/TypingAnalyzer/typing_analyzer.cpp
    Capstone/Algorithm/Analysis/ThreatMatcher/threat_matcher.cpp
    Capstone/Algorithm/Analysis/ThreatMatcher/threat_command_analyzer.cpp
    Capstone/Algorithm/Analysis/ThreatMatcher/threat_executor_analyzer.cpp
    Capstone/Algorithm/Analysis/ThreatMatcher/threat_result_formatter.cpp
    Capstone/Algorithm/Analysis/ThreatMatcher/threat_db_connect.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/url_classifier.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/url_parser.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/ip_filter.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/shortener_filter.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/executable_filter.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/entropy_filter.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/scheme_filter.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/port_filter.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/hosting_filter.cpp
    Capstone/Algorithm/Analysis/CompoundScorer/compound_scorer.cpp
    Capstone/Algorithm/Analysis/CompoundScorer/hotkey_scorer.cpp
    Capstone/Policy/device_blocker.cpp
    Capstone/Policy/device_finder.cpp
    Capstone/ThirdParty/sqlite3.c
)

target_include_directories(CapstoneCore PUBLIC
    Capstone/Logger
    Capstone/Policy
    Capstone/Storage
    Capstone/Core
    Capstone/Hook
    Capstone/Detect
    Capstone/Algorithm/PreAnalysis
    Capstone/Algorithm/PreAnalysis/KeyDecoder
    Capstone/Algorithm/PreAnalysis/KeyNormalizer
    Capstone/Algorithm/PreAnalysis/UrlExtractor
    Capstone/Algorithm/PreAnalysis/ShellDetector
    Capstone/Algorithm/Analysis/TypingAnalyzer
    Capstone/Algorithm/Analysis/ThreatMatcher
    Capstone/Algorithm/Analysis/UrlClassifier
    Capstone/Algorithm/Analysis/CompoundScorer
    Capstone/ThirdParty
)

target_compile_definitions(CapstoneCore PUBLIC
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(CapstoneCore PRIVATE /utf-8 /W3)
endif()

target_link_libraries(CapstoneCore PUBLIC
    Cfgmgr32
    Hid
    Shell32
    Setupapi
    Crypt32
)

enable_testing()

if(BUILD_QT_UI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 REQUIRED COMPONENTS Widgets Svg LinguistTools)

    add_executable(CapstoneQtApp WIN32
        Capstone/UIQt/main_qt.cpp
        Capstone/UIQt/MainWindow.h
        Capstone/UIQt/MainWindow.cpp
        Capstone/UIQt/TrayWidget.h
        Capstone/UIQt/TrayWidget.cpp
        Capstone/UIQt/Theme.h
        Capstone/UIQt/Assets/ui.qrc
    )

    target_link_libraries(CapstoneQtApp PRIVATE
        CapstoneCore
        Qt6::Svg
        Qt6::Widgets
    )

    qt_add_translations(CapstoneQtApp
        TS_FILES Capstone/UIQt/Assets/i18n/en.ts
        RESOURCE_PREFIX "/i18n"
    )

    if(MSVC)
        target_link_options(CapstoneQtApp PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
    endif()

    target_include_directories(CapstoneQtApp PRIVATE
        Capstone/UIQt
    )

    add_custom_command(TARGET CapstoneQtApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/Capstone/Core/matchingDB.db
            $<TARGET_FILE_DIR:CapstoneQtApp>/matchingDB.db
    )
endif()
