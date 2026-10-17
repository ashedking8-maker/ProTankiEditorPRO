# ProTanki Editor PRO 0.5.28 — Geometry View / G Live Refresh (2026-10-17)

OBSAH: IBA opravene a pridane subory. Skopirujte ich so zachovanim adresarov
`src/` a `tests/` do zdrojoveho projektu, s ktorym uz boli aplikovane
Fogtown Mesh Import FIX a AX Library FIX. Zostavte novu Windows aplikaciu.

### Presna pricina
`SceneRenderer::BuildScene()` vytvaral GPU buffers pre Geometry View jedenkrat.
`EditorUi::ApplyLiveTransform()` pri posune zavolal `scene.UpdatePropTransform()`,
ktory aktualizuje LEN vizualnu instanciu, nie GPU kolizne buffer-y.
Klaves G prepla len rezim vykreslovania. Vysledok: kolizne plochy ostavali na
starych vizualizovanych miestach, aj ked sa native XML kolizia uz presunula.

### Oprava
- `SceneRenderer::RefreshCollisionGeometry(map)` prebuduje IBA diagnosticke
  GPU buffery z `CollisionPreview::Build(map)` — bez resetu kamery, bez zmeny mapy.
- Vsetky tri sposoby vstupu do Geometry view (G, menu, tlacidlo) obnovia obraz.
- Ak sa meni transformacia objektu pocas Geometry view, obnovi sa aj obraz.
- Undo / Redo transformacii obnovuju Geometry view.
- Regresny test preveruje, ze viazana kolizna plocha/trojuholnik pri presune
  meni polohu a neviazany (neznamy) trojuholnik sa nesmie automaticky hybat.

### Bezpecnost
- Nemenili sa algoritmy importu a exportu map, XML serializacia ani definicie
  hernych kolizii. Oprava synchronizuje diagnosticky vykreslovaci buffer.
- Tento FIX NESLUBUJE obnovu neznamej kolizie bez overeneho priradenia k objektu.
  Ak po fixnuti G zostanu kolizie mimo presunuteho objektu, porovnat danu
  testovaciu mapu a originalnu kniznicu/model: pravdepodobne chyba priradenie
  nativnych colliderov, nie ich GPU prekreslenie.
- Povodne projekty a mapy si pred skusanim zalohujte. Najprv testujte Save As.

### Overenie
- 87 Python source/unit kontrol preslo.
- Kontrola konzistencie projektu cez tools/check_source_consistency.py presla.
- Rozsireny C++ CollisionPreviewRegression je dodany pre Windows CI, ale nie je
  spusteny v tomto Linux prostredi. Windows D3D11 build/run nie je overeny.

### Test v aplikacii
1. Otvorte testovaciu mapu a presunte Land04/06 vizualny objekt.
2. Stlacte G. Porovnajte aktualnu koliznu polohu s novou vizualnou polohou.
3. Stlacte G znova, urobte dalsi posun, Undo a Redo. Overte koliziu.
4. Ulozte mapu cez Save As, znovu otvorte, skontrolujte G a otestujte v hre.
5. Ak polohy nesedia, poslite testovacie map XML, presny model .3ds a novy log
   k overeniu kolizneho vlastnictva. Samotny prechod tankom po casti terenu
   nepotvrdzuje koliziu vsetkych presunutych objektov.
