// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
.pragma library

function safeFile(name) {
    // Basenames only: exclude URL syntax, encodings and path separators.
    return typeof name === "string" && /^[A-Za-z0-9][A-Za-z0-9_. -]*\.txt$/.test(name)
}

function externalUrl(url) {
    return typeof url === "string" && /^https?:\/\/[^\s]+$/i.test(url)
}

function parseIndex(text) {
    var index = JSON.parse(text)
    if (!index || index.version !== 1 || !index.application || !Array.isArray(index.components))
        throw new Error("Invalid license index")
    var app = index.application
    var keys = ["name", "license", "copyright", "url"]
    for (var k = 0; k < keys.length; ++k) {
        if (typeof app[keys[k]] !== "string") throw new Error("Invalid application metadata")
    }
    var files = ["APP-LICENSE.txt", "THIRD_PARTY_NOTICES.txt"]
    for (var i = 0; i < index.components.length; ++i) {
        var entry = index.components[i]
        if (!entry || typeof entry.name !== "string" || typeof entry.license !== "string"
                || typeof entry.url !== "string" || !Array.isArray(entry.files) || entry.files.length === 0)
            throw new Error("Invalid component metadata")
        for (var j = 0; j < entry.files.length; ++j) {
            if (!safeFile(entry.files[j])) throw new Error("Invalid license filename")
            if (files.indexOf(entry.files[j]) === -1) files.push(entry.files[j])
        }
    }
    index.files = files
    return index
}

function documentPath(documentId, filename, index) {
    if (documentId === "impressum") return "legal/impressum.html"
    if (documentId === "datenschutz") return "legal/datenschutz.html"
    if (documentId === "license") filename = "APP-LICENSE.txt"
    else if (documentId !== "third-party-license") return ""
    if (!index || !safeFile(filename) || index.files.indexOf(filename) === -1) return ""
    return "licenses/" + filename
}

// Presentation only: preserve every non-whitespace character of the original.
// Hard-wrapped prose is reflowed; examples retain their deliberate line breaks.
function licenseBlocks(text) {
    var paragraphs = text.replace(/\r\n?/g, "\n").split(/\n[ \t]*\n+/)
    var blocks = []
    function append(kind, value, marker) {
        if (value.length) blocks.push({kind: kind, text: value, marker: marker || ""})
    }
    for (var p = 0; p < paragraphs.length; ++p) {
        var lines = paragraphs[p].split("\n")
        while (lines.length && !lines[0].trim()) lines.shift()
        while (lines.length && !lines[lines.length - 1].trim()) lines.pop()
        var flat = paragraphs[p].replace(/\s+/g, " ").trim()
        if (!flat) continue
        if (blocks.length === 0 && lines[0].trim() === "GNU GENERAL PUBLIC LICENSE") {
            append("title", lines[0].trim())
            if (lines.length > 1) append("subtitle", lines.slice(1).join(" ").trim())
            continue
        }
        if (lines.length === 1 && (/^\s*\d+\.\s+\S/.test(lines[0])
                || /^(Preamble|TERMS AND CONDITIONS|END OF TERMS AND CONDITIONS|How to Apply These Terms to Your New Programs)$/.test(flat))) {
            append("heading", flat)
            continue
        }
        // Lettered/bulleted clauses have a hanging marker, separate from text.
        if (/^\s*(?:[a-z]\)|\*|[0-9]+\.)\s+/.test(lines[0])) {
            var marker = ""
            var body = []
            for (var l = 0; l < lines.length; ++l) {
                var match = lines[l].match(/^\s*([a-z]\)|\*|[0-9]+\.)\s+(.*)$/)
                if (match) {
                    append("list", body.join(" ").trim(), marker)
                    marker = match[1]
                    body = [match[2].trim()]
                } else body.push(lines[l].trim())
            }
            append("list", body.join(" ").trim(), marker)
            continue
        }
        var example = lines.length > 1 && lines.every(function(line) { return /^ {4}/.test(line) })
        append(example ? "example" : "paragraph", example ? lines.map(function(line) {
            return line.replace(/^ {4}/, "")
        }).join("\n") : flat)
    }
    return blocks
}
