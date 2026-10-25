// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
// Runs the same ES5 helpers used by QML, without Qt or Sailfish dependencies.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const path = require("node:path");
const context = vm.createContext({});
vm.runInContext(fs.readFileSync(path.join(__dirname, "../qml/LegalFiles.js"), "utf8")
    .replace(/^\.pragma library\s*$/m, ""), context);
const fixture = {
    version: 1,
    application: {name: "<App>", license: "GPL-3.0-or-later", copyright: "© Test", url: ""},
    components: [{name: "Example", license: "MIT", url: "https://example.test", files: ["example.txt"]}]
};
const index = context.parseIndex(JSON.stringify(fixture));
assert.equal(context.documentPath("license", "", index), "licenses/APP-LICENSE.txt");
assert.equal(context.documentPath("third-party-license", "example.txt", index), "licenses/example.txt");
assert.equal(context.documentPath("impressum", "", null), "legal/impressum.html");
assert.equal(context.documentPath("datenschutz", "", null), "legal/datenschutz.html");
assert.equal(context.documentPath("unknown", "example.txt", index), "");
assert.equal(context.documentPath("third-party-license", "missing.txt", index), "");
assert.equal(context.documentPath("license", "", null), "");
for (const file of ["../secret.txt", "/etc/passwd.txt", "file:///tmp/a.txt", "https://example.test/a.txt",
                    "%2e%2e%2fsecret.txt", "a%2ftest.txt", "a\\b.txt", "a.txt?remote", "a.txt#fragment"]) {
    assert.equal(context.safeFile(file), false, file);
    const bad = JSON.parse(JSON.stringify(fixture));
    bad.components[0].files = [file];
    assert.throws(() => context.parseIndex(JSON.stringify(bad)));
}
for (const text of ["", "null", "{}", '{"version":2}', '{"version":1,"application":{},"components":[]}'])
    assert.throws(() => context.parseIndex(text));
assert.equal(context.externalUrl("https://example.test/project"), true);
for (const url of ["javascript:alert(1)", "file:///etc/passwd", "//example.test", "https://bad\nurl"])
    assert.equal(context.externalUrl(url), false);
console.log("Legal catalog parsing, file confinement and explicit links: PASS");

function words(value) { return value.replace(/\s+/g, " ").trim(); }
function reconstructed(blocks) {
    return blocks.map(block => (block.marker ? block.marker + " " : "") + block.text).join(" ");
}
const gpl = fs.readFileSync(path.join(__dirname, "../LICENSE"), "utf8");
const blocks = context.licenseBlocks(gpl);
assert.equal(blocks[0].kind, "title");
assert.equal(blocks[1].kind, "subtitle");
assert.ok(blocks.some(block => block.kind === "heading" && block.text === "Preamble"));
assert.ok(blocks.some(block => block.kind === "heading" && block.text === "0. Definitions."));
assert.ok(blocks.some(block => block.kind === "list" && block.marker === "a)"));
assert.ok(blocks.some(block => block.kind === "example" && block.text.includes("<one line")));
assert.equal(words(reconstructed(blocks)), words(gpl), "Formatting must preserve the complete license wording and markers");
assert.equal(context.licenseBlocks("One hard-wrapped\nline of prose.")[0].text, "One hard-wrapped line of prose.");
const bullets = "* First condition\ncontinued here\n* Second condition.";
const list = context.licenseBlocks(bullets);
assert.equal(list.length, 2);
assert.equal(words(reconstructed(list)), words(bullets));
const example = "    <script>literal text</script>\n      indented example";
assert.equal(context.licenseBlocks(example)[0].kind, "example");
assert.equal(words(reconstructed(context.licenseBlocks(example))), words(example));
for (const file of ["../LICENSES/BSD-3-Clause.txt", "../crypto/vendor/monocypher/LICENCE.md"]) {
    const text = fs.readFileSync(path.join(__dirname, file), "utf8");
    assert.equal(words(reconstructed(context.licenseBlocks(text))), words(text), file);
}
console.log("License typography: headings, reflow, hanging lists, examples and complete wording: PASS");
