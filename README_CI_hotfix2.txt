OPRAVA GITHUB CI – NativeLand01EndToEndExport

Pouzitie:
Rozbal ZIP do korena aktualneho repozitara a nahrad
 tests/native_object_export_integration.cpp
Potom commitni zmenu a spusti GitHub Actions znova.
Tento doplnkovy patch predpoklada uz aplikovany predchadzajuci CI hotfix.
Nahradza iba jeden testovaci subor; nemení aplikaciu ani workflow.

Pricina:
V dodanom logu presla kompilacia a 34/35 CTest testov.
Posledny test zlyhal pri bool(tex), este pocas vytvarania Fogtown fixture.
Originalny land01.3ds obsahuje nepouzity material Default bez textury
 a pouzity Material #1269 s texturou LAND03.JPG.
Test vytvaral subor aj pre prazdny nazov, teda sa pokusal zapisovat do adresara.

Oprava:
Obe fixture pouzivaju spolocny vyber materialov podla skutocnych faces.
Nepouzite materialy nevytvaraju texturove subory.
Pouzity material musi existovat a musi mat platny nazov textury.
Kontroly kolizii, exportu TARA a odmietnutia nepodporovanych zmien ostavaju.
Zlyhanie samotneho Fogtown exportu teraz vypise aj konkretnu chybu exportera.

Overenie:
- 81/81 Python auditov preslo; source consistency preslo.
- Skutocna opravena funkcia skompilovana cez g++ C++20 s NDEBUG.
- Priprava textur oboch realnych fixtures presla: LAND03.JPG a land24.jpg.
- Negativna skuska: pouzity material bez textury bol odmietnuty.
- Kompletny Windows CTest/Assimp export nebol lokalne spusteny.
  GitHub musi potvrdit aj zvysok doteraz blokovaneho testu.
