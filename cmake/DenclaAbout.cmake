# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/DenclaLegal.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/DenclaLicenses.cmake")

# One entry point for the shared About UI, legal documents and license catalog.
function(dencla_install_about)
    cmake_parse_arguments(ABOUT "" "TARGET;APP_ID;APP_NAME;PROJECT_LICENSE;LICENSE_ID;COPYRIGHT;SOURCE_URL;MINIZ_SOURCE;NOTICES_FILE;LEGAL_DIRECTORY;PRIVACY_TEMPLATE;IMPRINT_TEMPLATE;PROFILE"
        "COMPONENTS;EXTRA_LICENSE_FILES" ${ARGN})
    if(ABOUT_UNPARSED_ARGUMENTS OR ABOUT_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid dencla_install_about arguments.")
    endif()
    if(NOT ABOUT_COPYRIGHT OR NOT ABOUT_LICENSE_ID)
        message(FATAL_ERROR "dencla_install_about requires COPYRIGHT and LICENSE_ID for the About notice.")
    endif()
    if(NOT ABOUT_APP_ID)
        set(ABOUT_APP_ID "${ABOUT_TARGET}")
    endif()
    set(legal_args "")
    foreach(field TARGET APP_ID APP_NAME LEGAL_DIRECTORY PRIVACY_TEMPLATE IMPRINT_TEMPLATE PROFILE)
        if(ABOUT_${field})
            list(APPEND legal_args ${field} "${ABOUT_${field}}")
        endif()
    endforeach()
    set(license_args "")
    foreach(field TARGET APP_ID APP_NAME PROJECT_LICENSE LICENSE_ID COPYRIGHT SOURCE_URL MINIZ_SOURCE NOTICES_FILE COMPONENTS EXTRA_LICENSE_FILES)
        if(ABOUT_${field})
            list(APPEND license_args ${field} "${ABOUT_${field}}")
        endif()
    endforeach()
    dencla_install_legal(${legal_args})
    dencla_install_licenses(${license_args})
    set(common "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    install(FILES "${common}/qml/AboutLegalSection.qml" "${common}/qml/AboutLicenseNotice.qml"
        "${common}/qml/AboutLinkItem.qml" "${common}/qml/AboutAcknowledgements.qml"
        DESTINATION "share/${ABOUT_APP_ID}/qml/components" COMPONENT Legal)
    install(FILES "${common}/qml/ThirdPartyLicensesPage.qml"
        DESTINATION "share/${ABOUT_APP_ID}/qml/pages" COMPONENT Legal)
endfunction()
