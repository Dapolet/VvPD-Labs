"use strict";

/*
 * Проверка чистых функций из validation.js без браузера:
 *     node web/selftest.js
 */

const {
  validateValue,
  parseUriWithRegex,
  parseUriWithUrl,
  encodingComparison,
} = require("./validation.js");

const cases = [
  ["fio", "Полетаев Дмитрий", true],
  ["fio", "Полетаев", false],
  ["group", "М3О-119БВ-26", true],
  ["group", "M3O-119БВ-26", false],
  ["group", "М3О-11БВ-26", false],
  ["phone", "+7 (999) 123-45-67", true],
  ["phone", "8 999 123 45 67", false],
  ["email", "student@example.org", true],
  ["email", "student@", false],
  ["email", "student@example", false],
  ["date", "2026-11-16", true],
  ["date", "2026-02-29", false],
  ["date", "2026-02-31", false],
  ["date", "16.11.2026", false],
  ["uri", "https://example.org:8443/api/v1/student-profile?id=304", true],
  ["uri", "example.org/api", false],
  ["uri", "ftp://example.org/file", false],
];

let failed = 0;

for (const [field, value, expected] of cases) {
  const actual = validateValue(field, value).valid;
  const status = actual === expected ? "PASS" : "FAIL";
  if (actual !== expected) {
    failed += 1;
  }
  console.log(`${status} ${field.padEnd(6)} ${JSON.stringify(value)} → ${actual}`);
}

const uri =
  "https://example.org:8443/api/v1/student-profile?id=304&group=%D0%9C3%D0%9E-304%D0%91%D0%92-26#results";

const regexParts = parseUriWithRegex(uri);
const urlParts = parseUriWithUrl(uri);

console.log("\nregex:", JSON.stringify(regexParts));
console.log("URL  :", JSON.stringify(urlParts));

if (
  regexParts.scheme !== "https" ||
  regexParts.host !== "example.org" ||
  regexParts.port !== "8443" ||
  regexParts.path !== "/api/v1/student-profile" ||
  regexParts.fragment !== "results"
) {
  failed += 1;
  console.log("FAIL regex URI breakdown");
} else {
  console.log("PASS regex URI breakdown");
}

console.log("\npercent-encoding demo:\n" + encodingComparison("М3О-119БВ-26"));

if (failed > 0) {
  console.error(`\n${failed} test(s) failed`);
  process.exit(1);
}

console.log("\nAll tests passed");
