"use strict";

/*
 * Лабораторная работа № 3, части III–V.
 * Чистые функции валидации плюс привязка к DOM, если страница загружена.
 */

const patterns = {
  // ФИО: 2–3 слова, только буквы (кириллица/латиница) и дефис.
  fio: /^[А-ЯЁA-Z][а-яёa-z-]+(?:\s+[А-ЯЁA-Z][а-яёa-z-]+){1,2}$/u,

  // Группа: М3О — именно кириллические М и О, затем три цифры, БВ, две цифры.
  group: /^М3О-\d{3}БВ-\d{2}$/u,

  // Один понятный учебный формат телефона.
  phone: /^\+7 \(\d{3}\) \d{3}-\d{2}-\d{2}$/,

  // Упрощённая проверка e-mail; не полная реализация RFC 5322.
  email: /^[^\s@]+@[^\s@]+\.[^\s@]+$/,

  // Проверяется только форма даты; существование даты — отдельно.
  date: /^\d{4}-\d{2}-\d{2}$/,
};

function isRealDate(value) {
  if (!patterns.date.test(value)) {
    return false;
  }

  const year = Number(value.slice(0, 4));
  const month = Number(value.slice(5, 7));
  const day = Number(value.slice(8, 10));
  const date = new Date(Date.UTC(year, month - 1, day));

  return (
    date.getUTCFullYear() === year &&
    date.getUTCMonth() === month - 1 &&
    date.getUTCDate() === day
  );
}

function isHttpUri(value) {
  try {
    const url = new URL(value);
    return url.protocol === "http:" || url.protocol === "https:";
  } catch {
    return false;
  }
}

const validators = {
  fio: {
    test: (value) => patterns.fio.test(value),
    message: "Введите 2–3 слова: только буквы русского или латинского алфавита и дефис.",
  },
  group: {
    test: (value) => patterns.group.test(value),
    message: "Формат: М3О-119БВ-26. Буквы М и О должны быть кириллическими.",
  },
  phone: {
    test: (value) => patterns.phone.test(value),
    message: "Формат: +7 (999) 123-45-67.",
  },
  email: {
    test: (value) => patterns.email.test(value),
    message: "Упрощённый формат: name@example.org (без пробелов, с точкой в домене).",
  },
  date: {
    test: (value) => isRealDate(value),
    message: "Нужна существующая дата в формате YYYY-MM-DD (например, 2026-11-16).",
  },
  uri: {
    test: (value) => isHttpUri(value),
    message: "Нужен абсолютный URI с протоколом http:// или https://.",
  },
};

function validateValue(name, value) {
  const rule = validators[name];
  if (!rule) {
    return { valid: true, message: "" };
  }

  if (value.trim() === "") {
    return { valid: false, message: "Заполните поле." };
  }

  return {
    valid: rule.test(value.trim()),
    message: rule.test(value.trim()) ? "" : rule.message,
  };
}

/* ---------- Разбор URI ---------- */

const URI_PATTERN =
  /^(?<scheme>[a-z][a-z0-9+.-]*):\/\/(?<authority>[^/?#]*)(?<path>[^?#]*)(?:\?(?<query>[^#]*))?(?:#(?<fragment>.*))?$/iu;

function parseUriWithRegex(value) {
  const match = URI_PATTERN.exec(value);
  if (!match) {
    return null;
  }

  const { scheme, authority, path, query = "", fragment = "" } = match.groups;

  let userinfo = "";
  let host = authority;
  let port = "";

  const at = authority.lastIndexOf("@");
  if (at !== -1) {
    userinfo = authority.slice(0, at);
    host = authority.slice(at + 1);
  }

  const hostPort = /^(\[[^\]]+\]|[^:]+):(\d+)$/.exec(host);
  if (hostPort) {
    host = hostPort[1];
    port = hostPort[2];
  }

  return { scheme, userinfo, host, port, path, query, fragment };
}

function parseUriWithUrl(value) {
  const url = new URL(value);
  const query = [...url.searchParams.entries()]
    .map(([key, val]) => `${key}=${val}`)
    .join("\n              ");

  return {
    protocol: url.protocol,
    hostname: url.hostname,
    port: url.port,
    pathname: url.pathname,
    search: url.search,
    hash: url.hash,
    query,
  };
}

/* ---------- Три механизма кодирования ---------- */

function encodeBase64Utf8(text) {
  const bytes = new TextEncoder().encode(text);
  let binary = "";
  for (const byte of bytes) {
    binary += String.fromCharCode(byte);
  }
  return btoa(binary);
}

function encodingComparison(group) {
  const encoded = encodeURIComponent(group);
  const decoded = decodeURIComponent(encoded);
  const sample = "大学";

  return [
    `URI component: ${group}`,
    `encodeURIComponent: ${encoded}`,
    `decodeURIComponent: ${decoded}`,
    `round-trip: ${group === decoded}`,
    "",
    `HTML: символ «ё» → сущность &#1105; (U+0451), это контекст HTML-текста.`,
    `Base64: ${sample} → UTF-8 ${new TextEncoder().encode(sample).length} байт → ${encodeBase64Utf8(sample)}`,
    `Percent-encoding: ${group} → ${encoded} (контекст URI, не кодировка Unicode).`,
  ].join("\n");
}

/* ---------- Привязка к DOM ---------- */

if (typeof document !== "undefined") {
  const form = document.querySelector("#validation-form");
  const formResult = document.querySelector("#form-result");

  function fieldElement(name) {
    return form.elements.namedItem(name);
  }

  function errorElement(name) {
    return document.querySelector(`[data-error-for="${name}"]`);
  }

  function renderField(name) {
    const field = fieldElement(name);
    const error = errorElement(name);
    const result = validateValue(name, field.value);

    field.setAttribute("aria-invalid", String(!result.valid));
    field.classList.toggle("invalid", !result.valid && field.value.trim() !== "");
    field.classList.toggle("valid", result.valid && field.value.trim() !== "");
    if (error) {
      error.textContent = result.message;
    }

    return result.valid;
  }

  function validateForm() {
    return ["fio", "group", "phone", "email", "date", "uri"]
      .map((name) => ({ name, valid: renderField(name) }))
      .every((item) => item.valid);
  }

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    const valid = validateForm();
    formResult.textContent = valid
      ? "Все поля прошли проверку."
      : "Есть ошибки: исправьте поля, отмеченные красным.";
    formResult.style.color = valid ? "#66bb6a" : "#ef5350";
  });

  for (const name of ["fio", "group", "phone", "email", "date", "uri"]) {
    fieldElement(name).addEventListener("input", () => renderField(name));
  }

  /* Демонстрация гомоглифов. */
  const cyrillicGroup = "М3О-119БВ-26";
  const latinGroup = "M3O-119БВ-26";

  function codePoints(value) {
    return [...value.slice(0, 3)]
      .map((ch) => `${ch}=U+${ch.codePointAt(0).toString(16).toUpperCase().padStart(4, "0")}`)
      .join(", ");
  }

  document.querySelector("#homoglyph-output").textContent = [
    `кириллица: ${codePoints(cyrillicGroup)}`,
    `латиница : ${codePoints(latinGroup)}`,
    `строки равны: ${cyrillicGroup === latinGroup}`,
    `regex для кириллической группы принимает кириллицу: ${patterns.group.test(cyrillicGroup)}`,
    `regex для кириллической группы принимает латиницу: ${patterns.group.test(latinGroup)}`,
  ].join("\n");

  /* Демонстрация разбора URI. */
  const uriSample =
    "https://example.org:8443/api/v1/student-profile?id=304&group=%D0%9C3%D0%9E-304%D0%91%D0%92-26#results";

  const regexParts = parseUriWithRegex(uriSample);
  const urlParts = parseUriWithUrl(uriSample);

  document.querySelector("#uri-output").textContent = [
    "Исходный URI:",
    uriSample,
    "",
    "Учебная регулярка:",
    `  scheme   = ${regexParts.scheme}`,
    `  host     = ${regexParts.host}`,
    `  port     = ${regexParts.port}`,
    `  path     = ${regexParts.path}`,
    `  query    = ${regexParts.query}`,
    `  fragment = ${regexParts.fragment}`,
    "",
    "Штатный new URL():",
    `  protocol = ${urlParts.protocol}`,
    `  hostname = ${urlParts.hostname}`,
    `  port     = ${urlParts.port}`,
    `  pathname = ${urlParts.pathname}`,
    `  search   = ${urlParts.search}`,
    `  hash     = ${urlParts.hash}`,
    `  query    = ${urlParts.query}`,
  ].join("\n");

  document.querySelector("#percent-output").textContent = encodingComparison(cyrillicGroup);
}

/* Для проверки чистых функций из Node.js без браузера. */
if (typeof module !== "undefined" && module.exports) {
  module.exports = {
    patterns,
    validators,
    validateValue,
    isRealDate,
    isHttpUri,
    parseUriWithRegex,
    parseUriWithUrl,
    encodingComparison,
  };
}
