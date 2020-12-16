#pragma once
#include "opengl.hpp"

void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle);
bool LoadTextureFromPixArray(const char** arr, GLuint* out_texture,
                             int* out_width, int* out_height);
