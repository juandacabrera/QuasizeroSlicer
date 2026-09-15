// Quasizero Slicer - LITE edition: no PRO extension of the preview.
// GNU AGPLv3, part of the Quasizero fork of OrcaSlicer.
#include "QzProHooks.hpp"

namespace Slic3r {
namespace GUI {

std::unique_ptr<QzProHooks> qz_make_pro_hooks(GCodeViewer &) { return nullptr; }

} // namespace GUI
} // namespace Slic3r
