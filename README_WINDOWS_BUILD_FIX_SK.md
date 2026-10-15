# Oprava Windows zostavenia – konflikt Polygon

Použi tento hotfix na projekt, do ktorého si už nahral predchádzajúci SourceCollision_System_fix. Nahraď tri priložené súbory na rovnakých cestách v GitHube. Ostatné súbory predchádzajúceho patchu ponechaj.

Priložený GitHub log ukazuje chybu C2872: Polygon je nejednoznačný názov. Windows SDK deklaruje funkciu Polygon v wingdi.h; nový kolízny kód používal rovnaký názov typu cez using namespace. Následné chyby vector, swap a chýbajúcich premenných boli dôsledkom nesprávne rozpoznaného typu.

Typ je teraz pomenovaný CollisionPolygon vo všetkých dotknutých deklaráciách a použitiach. Kolízny algoritmus, export a katalóg dát sa nemenia. Test na Linuxe navyše sprístupňuje globálnu funkciu Polygon podobne ako Windows, aby sa tento konflikt dal zachytiť aj bez Windows SDK.

Overenie: pôvodný kód s globálnou deklaráciou Polygon reprodukuje nejednoznačnosť; opravený test sa skompiloval a prešiel. Prešlo aj 81 Python auditov a kontrola konzistencie zdrojov. Kompletný MSVC/NSIS build sa lokálne nespúšťal; jeho výsledok musí potvrdiť nový GitHub Actions beh.
