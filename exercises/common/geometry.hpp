#pragma once

#include <vector>

namespace codea {

// Interleaved position.xyz and normal.xyz. Each face owns four vertices so its
// normal stays sharp instead of being averaged across cube corners.
inline std::vector<float> cubeVertices() {
    return {
        -1,-1, 1, 0,0,1,   1,-1, 1, 0,0,1,   1, 1, 1, 0,0,1,  -1, 1, 1, 0,0,1,
         1,-1,-1, 0,0,-1, -1,-1,-1, 0,0,-1, -1, 1,-1, 0,0,-1,  1, 1,-1, 0,0,-1,
        -1,-1,-1,-1,0,0,  -1,-1, 1,-1,0,0,  -1, 1, 1,-1,0,0,  -1, 1,-1,-1,0,0,
         1,-1, 1, 1,0,0,   1,-1,-1, 1,0,0,   1, 1,-1, 1,0,0,   1, 1, 1, 1,0,0,
        -1, 1, 1, 0,1,0,   1, 1, 1, 0,1,0,   1, 1,-1, 0,1,0,  -1, 1,-1, 0,1,0,
        -1,-1,-1, 0,-1,0,  1,-1,-1, 0,-1,0,  1,-1, 1, 0,-1,0, -1,-1, 1, 0,-1,0};
}

inline std::vector<unsigned int> cubeIndices() {
    std::vector<unsigned int> indices;
    for (unsigned int face = 0; face < 6; ++face) {
        const unsigned int base = face * 4;
        indices.insert(indices.end(), {base, base + 1, base + 2,
                                       base, base + 2, base + 3});
    }
    return indices;
}

} // namespace codea
