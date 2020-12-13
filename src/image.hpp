#pragma once
#include "opengl.hpp"

// Simple helper function to load an image into a OpenGL texture with common
// settings
// bool LoadTextureFromFile(const char* filename, GLuint* out_texture,
//                          int* out_width, int* out_height);
// bool LoadTextureFromPixArray(const char** arr, GLuint* out_texture,
//                              int* out_width, int* out_height);
void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle);
bool GetTextureFromCStructImage(GLuint* out_texture, int* out_width,
                                int* out_height);
