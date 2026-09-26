# 0.5.5 Editing checkpoint — Windows verification plan

This source tree was reconstructed from 0.5.4. It has NOT been compiled here,
and static source preflight must not be mistaken for Windows verification.

## Build and install

1. Upload all files including `.github` and icon; run both existing Actions workflows.
2. Confirm source-preflight, MSVC x64 build and all CTest jobs are green.
3. Verify the version-specific installer and portable archive names; delete
   stale 0.5.4 installers so they cannot be launched by accident.
4. Install as a normal user. Confirm Desktop shortcut appears in the user's
   actual Desktop (possibly redirected to OneDrive), that no Start folder
   selection is displayed, and the checked Finish launch runs the EXE.
5. Uninstall; confirm only the app's own shortcut/installed files are removed.

## Functional editing / XML

1. Open a *copy* of Polygon or Sandbox. Disable gameplay overlays and zones;
   click or rectangle-select: hidden spawn/flag/zone must not intercept picking.
2. Shift-click two props, drag them together, Ctrl-click to toggle, Undo/Redo.
3. Enable Special kill/kick volumes. Click the wireframe **edge**, not only center;
   confirm Properties opens and moving zone translates both min/max.
4. Edit min/max in Properties, change Kill to Kick, toggle Free, duplicate,
   delete and Undo/Redo. Save As a copy. Reload and inspect `<special-geometry>`:
   each changed field must round-trip; untouched XML remains intact.
   Verify repeated zone dragging does not cause a large memory spike.
5. With a dirty map, exercise New, Open and window Close through Save / Don't
   save / Cancel. Verify Cancel retains the current map and Save As cancellation
   never silently discards the document. Test loading an invalid XML.

## Native test (approximate preview)

1. Click Test first time and choose the original local tank asset directory;
   verify the chooser is visible and the test splash is visible at least 0.5 s.
2. Drive via WASD and arrows; Q/E changes pitch; G/T cycle ALL spawn locations;
   R respawns; Esc returns without changing map XML.
3. Visually compare Wasp hull orientation, Smoky turret mount, chase view,
   terrain and collisions against the original tester at matching spawn headings.
   The +90° model alignment is a hypothesis based on the supplied user report;
   adjust only after comparison. No full ProTLVK physics or hull/turret chooser
   has been implemented.

The initial pre-main Windows loader delay cannot be removed by an in-process
splash alone; a separate launcher would need additional engineering and tests.

## Large-map memory protection

The original XML string is immutable and shared among document snapshots; after
a successful save the live document installs a fresh source buffer. Zone value
changes and mouse dragging use small `SpecialBox` history entries instead of
whole-map copies. Zone addition/deletion still requires a structural snapshot,
and all collision/prop arrays within those snapshots are still copied. Check
memory consumption against an actual large map in a Windows Release build.
