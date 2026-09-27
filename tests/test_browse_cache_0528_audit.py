"""Source checks for lifecycle, in-memory PNG and conservative native export.
Live GPU allocation/rendering and ProTLVK compatibility require Windows tests.
"""
from pathlib import Path
import unittest
R=Path(__file__).resolve().parents[1]
def src(file):return (R/file).read_text(encoding='utf-8')
class BrowseCache0528Audit(unittest.TestCase):
    def test_session_only_two_tier_cache(self):
        ui=src('src/EditorUi.cpp');hdr=src('src/EditorUi.h');codec=src('src/PreviewThumbnailCodec.h')
        self.assertIn('browseCpuThumbnails_.clear(); browseFrame_=0;',ui)
        self.assertIn('if (browseWasOpen_ && !browseLibraryOpen_)',ui)
        self.assertIn('browseThumbnails_.clear();',ui)
        self.assertIn('browseWasOpen_=true; DrawBrowseLibrary',ui)
        self.assertIn('CapturePng(thumb.srv.Get()',ui)
        self.assertIn('RestorePng(cached->second.png',ui)
        self.assertIn('TrimBrowseGpuCache(PreviewThumbnailCodec::GpuBudget',ui)
        self.assertIn('TrimBrowseCpuCache();',ui)
        self.assertIn('browseCpuThumbnails_.clear(); recentAssets_.clear();',hdr)
        self.assertIn('CreateStreamOnHGlobal(nullptr,TRUE',codec)
        self.assertIn('SHCreateMemStream(png.data()',codec)
        self.assertNotIn('std::ofstream',codec)
        self.assertNotIn('constexpr size_t budget=2048;',ui)
        self.assertNotIn('TryAutoLibraryForMap',src('src/App.cpp'))
    def test_native_export_retains_verified_land01_and_rejects_guesses(self):
        export=src('src/NativeObjectExport.h');plan=src('src/NativeExportCollisionPolicy.h')
        self.assertIn('NativeTerrainDelta::Build',export)
        self.assertIn('NativeExportCollisionPolicy::Validate',export)
        self.assertIn('NativeCollisionImport::ReadNodes(modelFile',export)
        self.assertIn('Exported terrain helper vertex changed during 3DS round-trip',export)
        self.assertIn('Mixed triangle/plane/box collision cannot be converted safely',plan)
        self.assertIn('Original plane collision needs a separately verified native exporter',plan)
    def test_worker_accepts_new_version(self):
        self.assertIn("'0.5.28'",src('report_gateway/discord/worker.mjs'))
        self.assertIn('"version":"0.5.28"',src('src/BugReport.cpp').replace('\\"','"'))
if __name__=='__main__':unittest.main()
