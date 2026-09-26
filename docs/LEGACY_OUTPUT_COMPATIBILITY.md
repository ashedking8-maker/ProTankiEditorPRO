# Legacy map output compatibility

This is a hard requirement, not a best-effort feature.

The new editor may replace the renderer, UI, scene cache and editing implementation, but maps must remain consumable by the same ProTanki pipeline that accepts maps produced by the original AIR/Alternativa editor.

## Milestone 3 strategy

`MapDocument` keeps the complete loaded XML as the **legacy master document**. The editor also builds an in-memory representation for rendering/editing, but it does not regenerate the whole map from that reduced model.

When saving in Milestone 3:

1. Reparse the original legacy master XML.
2. Require the same static-prop count.
3. Update only static props that were actually edited; untouched numeric text is not regenerated through `float`.
4. Keep prop identity (`library-name`, `group-name`, `name`) and `texture-name` from the legacy master.
5. Keep collision geometry, spawn points, bonus regions, special geometry, lights, waypoints, domination keypoints, CTF flags and any currently unsupported XML nodes in the document.
6. Write legacy position values with 3 decimal places and static-prop rotation values with 6 decimal places.
7. Reparse the serialized output and run a structural compatibility guard before replacing the destination file.
8. Replace the destination atomically through a temporary file.

The writer intentionally does **not** normalize rotations to 0..2π. Old maps contain negative angles and angles above 2π, so preserving that convention avoids needless data changes.

## Why the editor does not generate XML from scratch yet

The loaded map can contain gameplay or collision data that the current milestone can read/count but cannot edit. Generating a new document only from the data model would silently discard those sections. Keeping the legacy master makes transform-only editing safe while support for the remaining entities is implemented.

## Verification tool

`tools/verify_legacy_xml.py ORIGINAL.xml EXPORTED.xml`

The developer-side verifier allows static-prop position/rotation values to differ, but checks that everything else remains semantically equivalent.

Future milestones that add/delete props or edit gameplay/collision entities must extend both the serializer and this verifier before those operations are considered complete.
