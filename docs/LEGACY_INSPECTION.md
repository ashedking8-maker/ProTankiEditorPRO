# Legacy package inspection

The uploaded editor package confirms that `GTanksEditor.exe` is only a small Adobe AIR launcher/wrapper. The actual application is `GTanksEditor.swf` and the package includes Adobe AIR runtime files. SWF symbols/strings identify Alternativa3D-era classes and Flash/AIR drawing APIs.

## Asset corpus

The extracted package contains 118 parseable prop libraries and 1,491 indexed prop definitions. Each definition is addressed by the same identity used in map XML:

`library-name + group-name + prop name`

A definition then resolves to either a `.3ds` mesh or sprite and zero or more named diffuse texture variants.

## Skyscrapers map compatibility test

The supplied `map_skyscrapers.xml` contains 3,737 static prop instances. All **3,737 / 3,737** resolve successfully against the supplied legacy libraries: zero unresolved props.

Those instances reduce to only:

- 19 unique source meshes/sprites
- 37 unique mesh/sprite + texture combinations

This is extremely favorable for a modern GPU renderer. Instead of treating 3,737 props as unrelated scene objects, the renderer can upload each mesh once and issue a small number of instanced batches containing many transforms.

Largest observed batches include roughly 892 instances of one `Com Build` mesh/material combination and 502 instances of one `Passage` bridge mesh/material combination.

## Consequence for the rewrite

The first real renderer should be structured around shared immutable GPU mesh buffers and per-batch instance buffers. This avoids reproducing the legacy engine's per-object CPU-heavy rendering path.
