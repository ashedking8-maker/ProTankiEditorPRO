"""Contract checks for the 0.5.17 source candidate, not a Windows runtime substitute."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parent.parent

def source(path):return (ROOT/path).read_text(encoding='utf-8')

class LosslessObject0517Audit(unittest.TestCase):
    def test_library_keeps_full_read_only_source_and_prop(self):
        h=source('src/AssetRegistry.h'); c=source('src/AssetRegistry.cpp')
        self.assertIn('originalLibraryXml',h)
        self.assertIn('originalPropXml',h)
        self.assertIn('std::make_shared<const std::string>(rawInput.str())',c)
        self.assertIn('prop.print(rawProp',c)
        self.assertIn('assets_=std::move(candidateAssets)',c)
        self.assertNotIn('save_file(',c)

    def test_instances_clone_full_subtree_and_refresh_on_save(self):
        h=source('src/MapDocument.h'); c=source('src/MapDocument.cpp')
        self.assertIn('std::shared_ptr<const std::string> originalPropXml',h)
        self.assertIn('p.print(originalNode',c)
        self.assertIn('geometry.append_copy(source)',c)
        self.assertIn('savedProp.print(raw',c)
        self.assertIn('props_[i].allowOpaqueMetadataCopy=false',c)
        self.assertIn('hasInvalidNativeMetadata',h)
        self.assertIn('malformed or repeated native prop fields',c)
        self.assertIn('no owned collision primitives',c)
        self.assertIn('geometry.remove_child(originalNodes[i])',c)

    def test_opaque_copy_requires_explicit_user_action(self):
        h=source('src/EditorUi.h'); c=source('src/EditorUi.cpp')
        self.assertIn('bool allowOpaqueMetadataCopy_{}',h)
        self.assertIn('Allow opaque XML copy for NEXT placement (advanced)',c)
        self.assertIn('Approve opaque XML copy for NEXT placement',c)
        self.assertIn('p.allowOpaqueMetadataCopy=allowOpaqueMetadataCopy_',c)
        self.assertIn('hasInvalidNativeMetadata',c)
        self.assertIn('allowOpaqueMetadataCopy_=false; // approval',c)
        self.assertIn('hasUncopyableMetadata && !pending.allowOpaqueMetadataCopy',c)

    def test_draft_template_stores_raw_source_without_native_export(self):
        draft=source('src/ObjectDraft.h'); ui=source('src/EditorUi.cpp')
        for name in ('library-source.xml','library-prop-template.xml','library-template-origin.txt'):
            self.assertIn(name,draft)
        self.assertIn('raw.write(d.libraryTemplateXml->data()',draft)
        self.assertIn('native_export false',draft)
        self.assertIn('Attach native library reference (read only)',ui)
        self.assertIn('objectDraft_.libraryTemplateXml=a.originalLibraryXml',ui)

    def test_windows_native_regression_registered(self):
        cmake=source('CMakeLists.txt')
        self.assertIn('LibraryReloadRegression',cmake)
        self.assertIn('LegacyFidelityRegression',cmake)
        self.assertIn('OriginalMetadataAndClipboardRoundTrip',cmake)
        self.assertIn('hasInvalidNativeMetadata',source('tests/legacy_fidelity_regression.cpp'))

if __name__=='__main__':unittest.main()
