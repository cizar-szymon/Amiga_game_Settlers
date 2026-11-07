/*
 * translation-po.cc - The translations of the texts for the Amiga port
 *
 * Instead of tinygettext (C++17, fetched at the build): the gettext .po
 * files of PROGDIR:po (pl.po, de.po ...) read here - msgctxt, msgid and
 * msgstr with their continuation lines and escapes, nothing else (no
 * plurals, no fuzzy handling). The texts are UTF-8; the game draws the
 * letters with Polish and German marks (gfx.cc).
 *
 * This file is part of the Settlers port of freeserf (GPL-3.0).
 */

#include "src/translation.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "src/log.h"

/* The dictionary of the language chosen: msgid (with "context\x04" in front
   when it has one) -> msgstr. */
namespace tinygettext {
class DictionaryManager {
 public:
  std::map<std::string, std::string> texts;
};
}

#ifdef __AMIGA__
static const char kPoDir[] = "PROGDIR:po/";
#else
static const char kPoDir[] = "po/";
#endif

/* The languages the port knows, in the order of the Language option. */
static const char *const kLanguages[][2] = {
  { "pl", "Polski" },
  { "de", "Deutsch" },
};

Translation::Translation()
  : manager(new tinygettext::DictionaryManager()) {
}

Translation::~Translation() {
}

void
Translation::add_directory(const std::string &path) {
}

Translation &
Translation::get_instance() {
  static Translation translation;
  return translation;
}

static bool
file_exists(const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  fclose(f);
  return true;
}

std::vector<std::string>
Translation::get_languages() {
  std::vector<std::string> result;
  for (const auto &l : kLanguages) {
    if (file_exists(std::string(kPoDir) + l[0] + ".po")) {
      result.push_back(l[0]);
    }
  }
  return result;
}

/* The text of a quoted .po string: the part between the quotes, escapes
   resolved. */
static std::string
po_string(const std::string &line, size_t from) {
  std::string out;
  size_t q = line.find('"', from);
  if (q == std::string::npos) return out;
  for (size_t i = q + 1; i < line.size(); i++) {
    char c = line[i];
    if (c == '"') break;
    if (c == '\\' && i + 1 < line.size()) {
      char e = line[++i];
      switch (e) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        default: out += e; break;
      }
      continue;
    }
    out += c;
  }
  return out;
}

static bool
starts(const std::string &s, const char *word) {
  size_t n = std::char_traits<char>::length(word);
  return s.compare(0, n, word) == 0 &&
         (s.size() == n || s[n] == ' ' || s[n] == '\t');
}

static void
read_po(const std::string &path, std::map<std::string, std::string> *texts) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    Log::Warn["translation"] << "No translation " << path;
    return;
  }
  std::string ctxt, id, str, line;
  int part = 0;  /* 1 msgctxt, 2 msgid, 3 msgstr */
  auto store = [&]() {
    if (!id.empty() && !str.empty()) {
      (*texts)[ctxt.empty() ? id : ctxt + '\x04' + id] = str;
    }
    ctxt.clear();
    id.clear();
    str.clear();
    part = 0;
  };
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') {
      if (part == 3) store();
      continue;
    }
    if (starts(line, "msgctxt")) {
      if (part == 3) store();
      ctxt = po_string(line, 7);
      part = 1;
    } else if (starts(line, "msgid")) {
      if (part == 3) store();
      id = po_string(line, 5);
      part = 2;
    } else if (starts(line, "msgstr")) {
      str = po_string(line, 6);
      part = 3;
    } else if (line[0] == '"') {
      std::string more = po_string(line, 0);
      if (part == 1) ctxt += more;
      if (part == 2) id += more;
      if (part == 3) str += more;
    }
  }
  if (part == 3) store();
}

void
Translation::set_language(const std::string &code) {
  manager->texts.clear();
  language.clear();
  if (code.empty()) return;
  read_po(std::string(kPoDir) + code + ".po", &manager->texts);
  if (!manager->texts.empty()) {
    language = code;
  }
}

std::string
Translation::get_language_name(const std::string &code) {
  if (code.empty()) return "English";
  for (const auto &l : kLanguages) {
    if (code == l[0]) return l[1];
  }
  return code;
}

std::string
Translation::translate(const char *msgid) {
  if (language.empty()) return msgid;
  auto it = manager->texts.find(msgid);
  return (it != manager->texts.end()) ? it->second : std::string(msgid);
}

std::string
Translation::translate(const char *context, const char *msgid) {
  if (language.empty()) return msgid;
  auto it = manager->texts.find(std::string(context) + '\x04' + msgid);
  return (it != manager->texts.end()) ? it->second : std::string(msgid);
}
