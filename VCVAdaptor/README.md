# VCVAdaptor

Run a VCV Rack module inside a GMPI plugin, generically.

A Rack module already declares its whole configuration in its constructor:
`config()` sizes the param/input/output tables, `configParam()` carries names,
ranges and defaults, `configInput()`/`configOutput()` carry port names. The
adaptor constructs one instance, reads that configuration back, and generates
the GMPI plugin XML to suit. Per-module code shrinks to: compile upstream's
`.cpp` against the mock, plus one `registerModule()` call.

```cpp
#include "VcvAdaptor.h"
#include "vcv/Fade.cpp"   // upstream, byte-for-byte

namespace {
auto r = vcv::registerModule("Fade",
    { .id = "VCV: Fade", .category = "VCV Fundamental", .vendor = "VCV (ported)" },
    [] { return vcv::createProcessor("Fade"); });
}
```

## Pieces

- **`rack/plugin.hpp`** — a mock of Rack's `plugin.hpp`: the smallest set of
  declarations that lets Fundamental sources compile unmodified. Real where it
  matters (`simd::float_4`, `Param`/`Port` storage, the `config*()` metadata
  capture); inert stubs for everything host-side (widgets, menus, json), each
  marked `MOCK`. `createModel()` files every model in a `ModelRegistry`
  singleton at static-init time, so the adaptor can reach a module by slug
  without naming its C++ type.
- **`VcvAdaptor.h`** — `generatePluginXml()` (config → GMPI XML) and
  `VcvProcessor` (the generic bridge: a pin per port, a parameter pin per
  param, one `process()` call per sample).

## Contracts worth knowing

- **Pin order**: inputs in VCV's order, then outputs, then parameter pins.
  `generatePluginXml()` and `VcvProcessor`'s pin construction both follow it;
  they must never diverge. Patch-point XML indexes into this order.
- **Ordering**: `registerModule()` resolves the slug at static-init, so
  `#include` the upstream `.cpp` *above* the call, in the same translation
  unit — top-to-bottom initialization within one TU is the guarantee.
- **Volts**: GMPI 1.0 = 10V. Audio pins scale ×10 in / ÷10 out so upstream
  idioms like `cv / 10.f` see real volts. Parameter pins are raw, in the
  module's declared range.
- **isConnected()**: the adaptor drives every port, so a module never sees an
  unconnected input; one that normalises an unconnected port to a fallback
  (`getNormalVoltage`) will read 0V instead. Revisit per-module if it matters.

First consumer: [VCV_Fundamental_gmpi](https://github.com/JeffMcClintock/VCV_Fundamental_gmpi).
