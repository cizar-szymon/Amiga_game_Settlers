/*
 * sprite-file-amiga.cc - SpriteFile without SDL3_image (replaces
 * src/sprite-file-dummy.cc): the game's own resource images are built in
 * (port/gen/resource_images.h, made by tools/embed_images.py).
 */

#include "src/sprite-file.h"

#include <cstring>

#include "port/gen/resource_images.h"

SpriteFile::SpriteFile() {
}

bool
SpriteFile::load(const std::string &path) {
  return false;
}

bool
SpriteFile::load_resource(const std::string &name) {
  for (const ResourceImage &img : resource_images) {
    if (name == img.name) {
      create(img.width, img.height);
      std::memcpy(data, img.bgra, img.width * img.height * 4);
      return true;
    }
  }
  return false;
}
