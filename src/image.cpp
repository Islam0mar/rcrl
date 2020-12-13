#include "image.hpp"

#include <cmath>

#include "image_source.h"
#include "opengl.hpp"

// Simple helper function to load an image into a OpenGL texture with common
// settings
// bool LoadTextureFromFile(const char* filename, GLuint* out_texture,
//                          int* out_width, int* out_height) {
//   // Load from file
//   int image_width = 0;
//   int image_height = 0;
//   unsigned char* image_data =
//       stbi_load(filename, &image_width, &image_height, NULL, 4);
//   if (image_data == NULL) return false;

//   // Create a OpenGL texture identifier
//   GLuint image_texture;
//   glGenTextures(1, &image_texture);
//   glBindTexture(GL_TEXTURE_2D, image_texture);

//   // Setup filtering parameters for display
//   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
//                   GL_CLAMP_TO_EDGE);  // This is required on WebGL for non
//                                       // power-of-two textures
//   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  //
//   Same

//   // Upload pixels into texture
// #if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
//   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
// #endif
//   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0,
//   GL_RGBA,
//                GL_UNSIGNED_BYTE, image_data);
//   stbi_image_free(image_data);

//   *out_texture = image_texture;
//   *out_width = image_width;
//   *out_height = image_height;

//   return true;
// }

// TODO: use XPM array
// FIXME: not working
// bool LoadTextureFromPixArray(const char** arr, GLuint* out_texture,
//                              int* out_width, int* out_height) {
//   // Load XPM array
//   SDL_Surface* surface = IMG_ReadXPMFromArray(const_cast<char**>(arr));

//   bool is_ok = true;
//   if (surface != nullptr) {
//     int w = pow(2, ceil(log(surface->w) /
//                         log(2)));  // Round up to the nearest power of two
//     SDL_Surface* new_surface = SDL_CreateRGBSurface(0, w, w, 24, 0xff000000,
//                                                     0x00ff0000, 0x0000ff00,
//                                                     0);
//     SDL_BlitSurface(surface, 0, new_surface,
//                     0);  // Blit onto a purely RGB Surface

//     // Create a OpenGL texture identifier
//     GLuint image_texture;

//     glGenTextures(1, &image_texture);
//     glBindTexture(GL_TEXTURE_2D, image_texture);

//     auto Mode = GL_RGB;

//     if (surface->format->BytesPerPixel == 4) {
//       Mode = GL_RGBA;
//     }

//     glTexImage2D(GL_TEXTURE_2D, 0, Mode, w, w, 0, Mode, GL_UNSIGNED_BYTE,
//                  new_surface->pixels);

//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//     *out_texture = image_texture;
//     *out_width = surface->w;
//     *out_height = surface->h;

//     SDL_FreeSurface(surface);
//   } else {
//     is_ok = false;
//   }
//   return is_ok;
// }

bool GetTextureFromCStructImage(GLuint* out_texture, int* out_width,
                                int* out_height) {
  // Load from file
  int image_width = loading.width;
  int image_height = loading.height;
  const unsigned char* image_data = loading.pixel_data;
  auto mode = GL_RGB;
  switch (loading.bytes_per_pixel) {
    case 2: {
      mode = GL_RGB16;
      break;
    }
    case 4: {
      mode = GL_RGBA;
      break;
    }
    case 3: {
      break;
    }
    default:
      return false;
      break;
  }
  if (image_data == NULL) return false;

  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  GL_CLAMP_TO_EDGE);  // This is required on WebGL for non
                                      // power-of-two textures
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  // Same

  // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
  glTexImage2D(GL_TEXTURE_2D, 0, mode, image_width, image_height, 0, mode,
               GL_UNSIGNED_BYTE, image_data);

  *out_texture = image_texture;
  *out_width = image_width;
  *out_height = image_height;

  return true;
}

static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
  return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
static inline ImVec2 ImRotate(const ImVec2& v, float cos_a, float sin_a) {
  return ImVec2(v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a);
}
void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

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
