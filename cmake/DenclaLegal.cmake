# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

include_guard(GLOBAL)
option(DENCLA_REQUIRE_LEGAL_DETAILS "Reject incomplete publisher/privacy details in distribution builds" OFF)
set(DENCLA_LEGAL_CONFIG "" CACHE FILEPATH "Optional private publisher profile shared by this build")

function(dencla_install_legal)
    cmake_parse_arguments(LEGAL "" "TARGET;APP_ID;APP_NAME;LEGAL_DIRECTORY;PRIVACY_TEMPLATE;IMPRINT_TEMPLATE;PROFILE" "" ${ARGN})
    if(LEGAL_UNPARSED_ARGUMENTS OR LEGAL_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid dencla_install_legal arguments.")
    endif()
    if(NOT LEGAL_TARGET OR NOT TARGET "${LEGAL_TARGET}")
        message(FATAL_ERROR "dencla_install_legal needs an existing TARGET.")
    endif()
    if(NOT LEGAL_APP_ID)
        set(LEGAL_APP_ID "${LEGAL_TARGET}")
    endif()
    if(NOT LEGAL_APP_ID MATCHES "^[A-Za-z0-9][A-Za-z0-9_.-]*$")
        message(FATAL_ERROR "Legal APP_ID must be a single package directory name.")
    endif()
    if(NOT LEGAL_APP_NAME)
        set(LEGAL_APP_NAME "${LEGAL_APP_ID}")
    endif()
    if(NOT LEGAL_LEGAL_DIRECTORY)
        set(LEGAL_LEGAL_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/legal")
    endif()
    if(NOT LEGAL_PRIVACY_TEMPLATE)
        message(FATAL_ERROR "Provide this application's PRIVACY_TEMPLATE; do not reuse another app's privacy claims.")
    endif()
    get_filename_component(common "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    get_filename_component(directory "${LEGAL_LEGAL_DIRECTORY}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(privacy "${LEGAL_PRIVACY_TEMPLATE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(LEGAL_IMPRINT_TEMPLATE)
        get_filename_component(imprint "${LEGAL_IMPRINT_TEMPLATE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        set(imprint "${common}/legal/impressum.html.in")
    endif()
    if(LEGAL_PROFILE)
        get_filename_component(profile "${LEGAL_PROFILE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        set(profile "${DENCLA_LEGAL_CONFIG}")
    endif()
    # Never use the phony target's CMakeFiles/<target> path as a directory:
    # Make would see an existing file and skip the supposedly always-run command.
    set(output "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/dencla-legal-${LEGAL_TARGET}")
    set(command "${CMAKE_COMMAND}"
        "-DDENCLA_LEGAL_DIRECTORY=${directory}"
        "-DDENCLA_LEGAL_PRIVACY_TEMPLATE=${privacy}"
        "-DDENCLA_LEGAL_IMPRINT_TEMPLATE=${imprint}"
        "-DDENCLA_LEGAL_APP_NAME=${LEGAL_APP_NAME}"
        "-DDENCLA_LEGAL_OUTPUT_DIR=${output}"
        "-DDENCLA_LEGAL_CONFIG=${profile}"
        "-DDENCLA_REQUIRE_LEGAL_DETAILS=${DENCLA_REQUIRE_LEGAL_DETAILS}"
        -P "${common}/cmake/GenerateLegal.cmake")
    execute_process(COMMAND ${command} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not generate legal documents for ${LEGAL_APP_ID}.")
    endif()
    add_custom_target(${LEGAL_TARGET}-legal ALL COMMAND ${command}
        COMMENT "Generating legal documents for ${LEGAL_APP_ID}" VERBATIM)
    add_dependencies(${LEGAL_TARGET} ${LEGAL_TARGET}-legal)
    add_custom_target(${LEGAL_TARGET}-legal-check
        COMMAND "${CMAKE_COMMAND}"
            "-DDENCLA_LEGAL_DIRECTORY=${directory}"
            "-DDENCLA_LEGAL_PRIVACY_TEMPLATE=${privacy}"
            "-DDENCLA_LEGAL_CONFIG=${profile}"
            -DDENCLA_REQUIRE_LEGAL_DETAILS=ON -DDENCLA_LEGAL_VALIDATE_ONLY=ON
            -P "${common}/cmake/GenerateLegal.cmake"
        VERBATIM)
    set_property(TARGET ${LEGAL_TARGET} PROPERTY DENCLA_LEGAL_OUTPUT_DIRECTORY "${output}")

    # Bracket arguments preserve quotes, spaces and dollar signs in public
    # application names/paths when this command is embedded in the install script.
    set(code "execute_process(COMMAND")
    foreach(argument IN LISTS command)
        set(equals "=")
        string(FIND "${argument}" "]${equals}]" position)
        while(NOT position EQUAL -1)
            string(APPEND equals "=")
            string(FIND "${argument}" "]${equals}]" position)
        endwhile()
        string(APPEND code " [${equals}[${argument}]${equals}]")
    endforeach()
    string(APPEND code " RESULT_VARIABLE legal_result)\nif(NOT legal_result EQUAL 0)\nmessage(FATAL_ERROR \"Could not regenerate legal documents.\")\nendif()")
    install(CODE "${code}" COMPONENT Legal)
    install(FILES "${output}/impressum.html" "${output}/datenschutz.html"
        DESTINATION "share/${LEGAL_APP_ID}/legal" COMPONENT Legal)
    install(FILES "${common}/qml/ImprintPage.qml" "${common}/qml/LegalDocumentPage.qml"
        "${common}/qml/LicenseCatalog.qml" "${common}/qml/LegalFiles.js"
        DESTINATION "share/${LEGAL_APP_ID}/qml/pages" COMPONENT Legal)
    install(FILES "${common}/qml/LicenseText.qml"
        DESTINATION "share/${LEGAL_APP_ID}/qml/components" COMPONENT Legal)
endfunction()
