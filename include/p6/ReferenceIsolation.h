#pragma once

#include "p6/P6PatchState.h"

namespace p6 {

// Controls which unrelated hardware behaviours are neutralized when building
// a deterministic reference-render patch. This transformation is tooling-only:
// production patch loading and the realtime engine keep the original state.
struct ReferenceIsolationOptions {
    bool disableSync{true};
    bool disableGlide{true};
    bool disablePolyMod{true};
};

// Return a copy of `source` with only independently verified serialized
// parameters changed to deterministic dry-reference values. Unknown bytes in
// the canonical 1024-byte payload are preserved exactly.
P6PatchState makeReferenceIsolationPatch(
    const P6PatchState& source,
    const ReferenceIsolationOptions& options = {});

} // namespace p6
