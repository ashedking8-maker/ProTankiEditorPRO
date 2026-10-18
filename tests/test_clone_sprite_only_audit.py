"""Regression for Canyon's sprite-only plants within opaque native clones."""
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class CloneSpriteOnlyAudit(unittest.TestCase):
    def test_only_real_source_sprites_are_certified(self):
        ui=(ROOT/'src/EditorUi.cpp').read_text()
        self.assertIn('asset->mesh.empty() && !asset->sprite.empty()',ui)
        self.assertIn('map.SetVerifiedCollisionFreeSprite(i,true)',ui)
        source=(ROOT/'src/MapDocument.h').read_text()
        self.assertIn('p.nativeWithCollision!=1',source)
        self.assertIn('!p.collisionOwnershipUnresolved',source)
        self.assertIn('!HasNativeCollisionForProp(index)',source)

    def test_certification_survives_copy_but_opaque_cannot_be_guessed(self):
        source=(ROOT/'src/MapDocument.cpp').read_text()
        self.assertIn('p.verifiedCollisionFreeSprite))',source)
        self.assertIn('verifiedCollisionFreeSprites=',source)
        self.assertIn('c.nativeCloneBatchId>=0 || !same(c.position)',source)
        self.assertIn('MayEraseOpaque(',source)
        self.assertIn('!props_[i].verifiedCollisionFreeSprite',source)

    def test_real_cpp_regression_includes_canyon_vegetation(self):
        native=(ROOT/'tests/native_clone_partial_edit_regression.cpp').read_text()
        for marker in ['"Tree 1"','"Bush 1"','sprites.DeletePropsWithCollision({3}',
                       'sprites.DeletePropsWithCollision({2}','sprites.CollisionTriangles().size()==1']:
            self.assertIn(marker,native)
        self.assertIn('NativeClonePartialEditRegression', (ROOT/'CMakeLists.txt').read_text())

if __name__=='__main__':
    unittest.main()
