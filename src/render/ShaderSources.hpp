#pragma once

namespace coomer {

inline constexpr unsigned char kVertexShaderSourceData[] = {
#embed "quad.vert" suffix(, 0)
};

inline const char* kVertexShaderSource =
    reinterpret_cast<const char*>(kVertexShaderSourceData);

inline constexpr unsigned char kFragmentShaderSourceData[] = {
#embed "quad.frag" suffix(, 0)
};

inline const char* kFragmentShaderSource =
    reinterpret_cast<const char*>(kFragmentShaderSourceData);

}  // namespace coomer
