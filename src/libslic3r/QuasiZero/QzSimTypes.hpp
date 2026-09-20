// Quasizero Slicer - plain data shared by the toolpath skeleton, the simulation engine and
// the front ends: a deposited segment and a point. GNU AGPLv3 inside the public Quasizero
// Slicer LITE; the Quasizero simulation engine carries an identical copy (dual-licensed).
#ifndef QZ_SIM_TYPES_HPP
#define QZ_SIM_TYPES_HPP

namespace Slic3r { namespace QuasiZero {

// One extrusion segment of the toolpath, in deposition order
struct QzSimSegment
{
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0; // [mm] plan endpoints
    float z = 0;                          // [mm] nominal bead top
    float w = 0, h = 0;                   // [mm] bead width / height
    int   layer = -1;                     // stability record index
    float t_end = 0;                      // [s] process time at the segment end
};

struct QzSimPoint { float x = 0, y = 0, z = 0; };

}} // namespace Slic3r::QuasiZero

#endif // QZ_SIM_TYPES_HPP
