# V7 — globálna oprava starých 3DS s veľkým world-origin

Tento patch nadväzuje na V6 a rieši chýbajúce objekty v mapách ako Rio. V dodanom logu mala mapa 3 600 props a presne 52 chýbajúcich mesh inštancií. Všetkých 52 boli výskyty `Com Build/complex build/porch1`, ktorých `porch1.3ds` editor odmietal ako `Malformed 3DS mesh/keyframe chunks.`

## Skutočná príčina

`porch1.3ds` nie je poškodený. Jeho vizuálny mesh je uložený vo veľmi vzdialených pôvodných súradniciach približne X=11 417 447 a 3DS lokálna matica/keyframe túto artist-scene transláciu odčíta. Po normalizácii sú vrcholy opäť bežné hodnoty v stovkách jednotiek.

`Native3DSScene` však ešte pred aplikovaním 0x4160 matice odmietal každý surový vrchol alebo keyframe position s absolútnou hodnotou >= 10 000 000. Tým sa legitímny legacy asset nesprávne označil za malformed.

## Oprava

Parser teraz na surových 3DS world-space vrcholoch a absolútnej keyframe position kontroluje iba to, že sú čísla konečné (žiadne NaN/Inf). Bezpečnostný rozsah sa uplatňuje až po prevedení do normalizovaného prop-local priestoru. Toto zodpovedá tomu, ako staré 3DS assety používajú object matrix/pivot a ako pôvodný editor následne vizuálny root vkladá do prop obalu.

Nejde o výnimku pre názov `porch1`. Pravidlo je globálne pre všetky 3DS. Kolízny importér zostáva fail-closed: výsledný normalizovaný helper offset/rotation/size musí byť stále v bezpečnom rozsahu. Veľký surový artist origin teda už nevadí, ale absurdne vzdialený výsledný collider sa neakceptuje.

## Overenie

- Audit celej dodanej knižnice: 1 432 `.3ds` súborov. Starý limit odmietal presne jeden — `ComBuild/porch1.3ds`. Po novej raw/world-space validácii prešlo všetkých 1 432 štrukturálnym parserom.
- Skutočný `porch1.3ds` po oprave: 9 mesh nodes, 9 keyframe nodes, visual anchor `porch1`, 8 natívnych collision planes.
- 79 Python auditov: PASS.
- 7 scalar C++ regresných programov: PASS. Nový regresný prípad vytvára syntetický 3DS s world-origin > 11 miliónov a overuje správnu lokalizáciu aj fail-closed ochranu pre obrovský výsledný collider.
- `tools/check_source_consistency.py`: PASS.

Windows/MSVC + Assimp renderer a ProTLVK hra tu neboli spustené. Po GitHub build-e otvor `map_rio.xml`. Očakávanie: `missing=0` namiesto `missing=52` a všetky porch1 diely sa majú zobraziť.

Log marker opraveného build-u: `0.5.28-large-world-3ds-v7`.
