# Visual / UX contract

The editor is a dense professional desktop tool, not a web dashboard.

- Base: charcoal/near-black, not pure black.
- Single restrained Windows-7-like blue accent.
- Corner radius: 2–3 px globally. Never pills.
- Transparency: panel/popup opacity where it helps viewport continuity; no glassmorphism blur.
- Separation: spacing first, then a subtle 1 px divider only when necessary. No colored "zones" or cards.
- Controls: quiet at rest; border/contrast appears on hover/focus. Active tool may use a thin accent indication rather than a filled bright button.
- Menus/toolbars: compact, desktop scale. No ribbon.
- Typography: Segoe UI/Tahoma-like metrics, information-dense.
- Motion: near-instant. Only short opacity transitions for transient notices.
- Notifications: compact rectangular dark toast, slight radius, high transparency, thin severity accent. Important persistent state belongs in the status/log area.
- Viewport is visually dominant. UI must recede when not being used.

## Default layout

Top: classic menu + compact tool row.  
Left: Scene / layers.  
Center: viewport.  
Right top: prop library/search/preview.  
Right bottom: properties.  
Bottom: thin status line with object counts, selected count, grid/snap, GPU/FPS/profiling data.

## Renderer target

D3D11 hardware path, x64. Static mesh geometry is uploaded once and shared across all instances. Instance transforms/material selections are dynamic buffers. Batches are keyed by mesh + texture/material. CPU work should scale primarily with visible batches rather than raw prop count.
