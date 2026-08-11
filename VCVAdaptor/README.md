# VCVAdaptor

Run a VCV Rack module inside a GMPI plugin, generically.

A Rack module already declares its whole configuration in its constructor:
`config()` sizes the param/input/output tables, `configParam()` carries names,
ranges and defaults, `configInput()`/`configOutput()` carry port names. The
adaptor constructs one instance, reads that configuration back, and generates
the GMPI plugin XML to suit. Per-module code shrinks to nothing: compiling the
module's `.cpp` is what registers the plugin.

```cpp
#include "VcvModule.h"
#include "vcv/Fade.cpp"   // upstream, byte-for-byte
```

That is the whole per-module source file. The module's own registration line —
`Model* modelFade = createModel<Fade, FadeWidget>("Fade")` — runs at static
init, and `createModel()` turns it into a registered GMPI plugin: config()
becomes the XML, the ModuleWidget becomes the patch points and the editor's
knob layout, and the `createPanel("res/Fade.svg")` path resolves against the
art the build staged.

Set once per plugin, in CMake, never per module: `VCV_MODULE_ID_PREFIX`,
`VCV_MODULE_CATEGORY`, `VCV_MODULE_VENDOR`. Opt out with `VCVADAPTOR_NO_GUI`
(DSP only) or `VCV_NO_AUTO_REGISTER` (register by hand with
`vcv::registerModule()`).

## Pieces

- **`rack/plugin.hpp`** — a mock of Rack's `plugin.hpp`: the smallest set of
  declarations that lets Fundamental sources compile unmodified. Real where it
  matters (`simd::float_4`, `Param`/`Port` storage, the `config*()` metadata
  capture); inert stubs for everything host-side (widgets, menus, json), each
  marked `MOCK`. `createModel()` files every model in a `ModelRegistry`
  singleton at static-init time, so the adaptor can reach a module by slug
  without naming its C++ type.
- **`VcvPanelLayout.h`** — `readPanelLayout()`: builds the module's own
  `ModuleWidget` and walks it for every control's position, size, id and
  range. Feeds both the patch-point generator and the editor.
- **`VcvAdaptor.h`** — `generatePluginXml()` (config → GMPI XML) and
  `VcvProcessor` (the generic bridge: a pin per port, a parameter pin per
  param, one `process()` call per sample).
- **`VcvEditor.h`** — `VcvEditor`: draws the panel SVG and works the knobs.
  Fixed-size from the SVG's own `width`/`height`; hit-tests knobs by the
  layout's radii; vertical drag writes the parameter pin. It draws only the
  indicator line, because Fundamental's panel SVGs already carry the knob and
  jack artwork.

## Patch points come from the module too

`generatePatchPoints()` builds the module's own `ModuleWidget` and walks it.
A Rack module states its jack layout in that widget's constructor:

```cpp
addInput(createInputCentered<ThemedPJ301MPort>(
             mm2px(Vec(7.62, 67.53)), module, Fade::IN1_INPUT));
```

— position and port id together, in the module's own source. That is a better
source than the panel SVG: the SVG's `display:none` component layer is a
convention Fundamental happens to follow, whereas this is the code Rack itself
lays the panel out from, so it cannot drift from what the module really has.
(For Fade the two agree to the pixel, which is a useful cross-check: 156, 199,
244, 289, 334.)

The hit radius comes from each component's declared size — `ThemedPJ301MPort`
is an 8.7mm jack, so 13px. Override with `patchPointRadius`, or set
`patchPoints = false` to omit the section.

## Contracts worth knowing

- **Pin order**: inputs in VCV's order, then outputs, then parameter pins.
  `generatePluginXml()` and `VcvProcessor`'s pin construction both follow it;
  they must never diverge.
- **Patch-point `pinId` is NOT that order.** It indexes SynthEdit's *document*
  pin list, where a module's GUI pins are numbered ahead of its audio pins. So
  with N parameters (hence N `<GUI>` pins), audio pin *k* is document pin
  *N + k*. This bites silently: adding an editor renumbers every jack. To
  re-check, connect something and see which pin the host reports — for Fade
  (2 params, 3 inputs) `IN1_INPUT` is portId 1 and lands on document pin 3.
- **Ordering**: `VcvModule.h` must precede the module's `.cpp`, which is what
  instantiates `createModel()` and so needs the machinery declared. Anything
  written during static init and read later must live in a **function-local**
  static: an `inline static std::string` has its own dynamic initialization,
  unordered against that write, so the assignment lands first and the
  constructor then wipes it. That failure looks like an editor that cannot
  find its own model.
- **Include order**: GMPI and gmpi_ui headers come before `plugin.hpp`, which
  ends with `using namespace rack;` as Rack's own does. Afterwards, `rack::Rect`
  and `rack::Vec` make every `Rect` in `SvgParser.h` ambiguous. `VcvModule.h`
  handles this; it matters if you assemble the headers yourself.
- **Volts**: GMPI 1.0 = 10V. Audio pins scale ×10 in / ÷10 out so upstream
  idioms like `cv / 10.f` see real volts. Parameter pins are raw, in the
  module's declared range.
- **isConnected()**: the adaptor drives every port, so a module never sees an
  unconnected input; one that normalises an unconnected port to a fallback
  (`getNormalVoltage`) will read 0V instead. Revisit per-module if it matters.

First consumer: [VCV_Fundamental_gmpi](https://github.com/JeffMcClintock/VCV_Fundamental_gmpi).
