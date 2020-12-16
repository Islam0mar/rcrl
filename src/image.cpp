#include "image.hpp"

#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "opengl.hpp"

using std::string;

static inline ImVec2 operator+(const ImVec2 &lhs, const ImVec2 &rhs) {
  return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
static inline ImVec2 ImRotate(const ImVec2 &v, float cos_a, float sin_a) {
  return ImVec2(v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a);
}
void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle) {
  ImDrawList *draw_list = ImGui::GetWindowDrawList();

  float cos_a = cosf(angle);
  float sin_a = sinf(angle);
  ImVec2 pos[4] = {
      center + ImRotate(ImVec2(-size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
      center + ImRotate(ImVec2(+size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
      center + ImRotate(ImVec2(+size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a),
      center + ImRotate(ImVec2(-size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a)};
  ImVec2 uvs[4] = {ImVec2(0.0f, 0.0f), ImVec2(1.0f, 0.0f), ImVec2(1.0f, 1.0f),
                   ImVec2(0.0f, 1.0f)};

  draw_list->AddImageQuad(tex_id, pos[0], pos[1], pos[2], pos[3], uvs[0],
                          uvs[1], uvs[2], uvs[3], IM_COL32_WHITE);
}

const char *kXpmColorKeys[] = {
    "s",  /* key #1: symbol */
    "m",  /* key #2: mono visual */
    "g4", /* key #3: 4 grays visual */
    "g",  /* key #4: gray visual */
    "c",  /* key #5: color visual */
};

bool XpmParseValues(const char **data, unsigned int *width,
                    unsigned int *height, unsigned int *ncolors,
                    unsigned int *cpp, unsigned int *x_hotspot,
                    unsigned int *y_hotspot, unsigned int *hotspot,
                    unsigned int *extensions) {
  bool is_ok = true;
  // xpm3 only
  /*
   * read values: width, height, ncolors, chars_per_pixel
   */
  std::stringstream ss;
  ss << string(data[0]);
  ss >> *width >> *height >> *ncolors >> *cpp;
  if (ss.fail()) {
    is_ok = false;
  }
  /*
   * read optional information (hotspot and/or XPMEXT) if any
   */
  *extensions = ss.str().find(" XPMEXT") != std::string::npos;
  if (ss >> *x_hotspot) {
    ss >> *y_hotspot;
    *hotspot = 1;
  } else {
    *hotspot = 0;
  }
  return is_ok;
}

bool XpmParseColors(const char **data, unsigned int ncolors,
                    std::unordered_map<string, std::uint32_t> *color_map) {
  bool is_ok = true;
  for (auto i = 0U; i < ncolors; ++i) {
    string str, key, color;
    std::stringstream ss;
    ss << data[i + 1];
    if (ss.str()[0] == ' ') {
      ss >> key >> color;
      str = " ";
    } else {
      ss >> str >> key >> color;
    }
    if (ss.fail()) {
      is_ok = false;
    }
    if (color[0] != '#') {
      // TODO: convert color text to uint32
    }
    color_map->insert({str, std::stol(color.substr(1), nullptr, 16)});
  }
  return is_ok;
}

static int XpmParsePixelsWithTransparentColor(
    const char **data, unsigned int width, unsigned int height,
    unsigned int ncolors, unsigned int cpp,
    const std::unordered_map<string, std::uint32_t> &color_map,
    std::vector<std::uint8_t> *pixels, std::uint32_t tarnsparent_color) {
  bool is_ok = true;
  if ((height > 0 &&
       width >= std::numeric_limits<std::uint32_t>::max() / height) ||
      width * height >=
          std::numeric_limits<std::uint32_t>::max() / sizeof(std::uint32_t)) {
    is_ok = false;
  }
  if (ncolors > 256) {
    is_ok = false;
  }
  pixels->reserve(width * height * 4);
  auto itr = pixels->begin();
  string line;
  for (auto i = 0U; i < height; ++i) {
    line = data[i + ncolors + 1];
    for (auto j = 0U; j < width; ++j) {
      auto color = color_map.at(line.substr(j, cpp));
      *itr++ = (color & 0xff0000) >> 16;
      *itr++ = (color & 0x00ff00) >> 8;
      *itr++ = (color & 0x0000ff);
      *itr++ = tarnsparent_color == color ? 0 : 255;
    }
  }
  return is_ok;
}

// load xpm array
bool LoadTextureFromPixArray(const char **arr, GLuint *out_texture,
                             int *out_width, int *out_height) {
  bool is_ok = true;

  unsigned int width, height, ncolors, cpp, x_hotspot, y_hotspot, hotspot,
      extensions;
  std::unordered_map<string, std::uint32_t> color_map;
  is_ok &= XpmParseValues(arr, &width, &height, &ncolors, &cpp, &x_hotspot,
                          &y_hotspot, &hotspot, &extensions);
  is_ok &= XpmParseColors(arr, ncolors, &color_map);
  std::vector<std::uint8_t> pixels;
  // Load XPM array
  is_ok &= XpmParsePixelsWithTransparentColor(arr, width, height, ncolors, cpp,
                                              color_map, &pixels, 0xffffff);
  if (is_ok) {
    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_BORDER);  // This is required on WebGL for non
                                          // power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_BORDER);  // Same
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels.data());
    *out_texture = image_texture;
    *out_width = width;
    *out_height = height;
  } else {
    is_ok = false;
  }
  return is_ok;
}
