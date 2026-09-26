# Source of truth and serialization boundaries (0.5.17)

`library.xml` is an asset **definition** source; an individual map `<prop>` is
an **instance**, and `<collision-geometry>` stores separate native primitives.
The binary `3DS` helper and visible mesh are a fourth data source. Treating a
library `<prop>` as if it were a map `<prop>` is not a safe export strategy.

Known projections: name/group/library, model/sprite path, chosen texture,
transform, observed optional `with_collision` and `free`. Unknown definition
fields are kept in the original library XML payload; unknown instance fields
stay with the detached original prop subtree. No metadata is silently turned
into a guessed native material, particle, collision or game behavior flag.

An original map with no modifications is written from unchanged source bytes.
A modified map is parsed and serialized via pugixml, preserving XML nodes and
attributes semantically but not promising byte-identical whitespace. Copying
an instance uses a full subtree with selected known fields patched. Unknown
unique IDs, references and location-dependent data still require investigation.
Opting into one opaque duplication transfers data, not verified gameplay
semantics. Malformed known flags and incomplete native collision are blocked.

Object-draft library sidecars are provenance only: they do not add to original
library.xml, transform GLB into native 3DS, or give a draft executable collision
or effect behavior. The original library must remain untouched.
