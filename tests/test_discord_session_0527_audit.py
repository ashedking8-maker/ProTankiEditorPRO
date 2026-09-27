"""Read-only source invariants; Windows GPU/HTTP behavior still requires live testing."""
from pathlib import Path
import unittest

R=Path(__file__).resolve().parents[1]
def source(name):return (R/name).read_text(encoding='utf8')

class DiscordSession0527Audit(unittest.TestCase):
    def test_library_is_never_automatically_loaded(self):
        app=source('src/App.cpp');ui=source('src/EditorUi.cpp')
        self.assertNotIn('TryAutoLibraryForMap',app)
        self.assertNotIn('Auto-indexed local library root',app)
        self.assertNotIn('assets_.Scan(localLib',app)
        self.assertIn('assets_.Scan(ui_.PendingLibrary(),err)',app)
        self.assertNotIn('out<<"lastLibraryDirectory "',ui)
        self.assertIn('constexpr size_t budget=2048;',ui)
        self.assertIn('browseThumbnails_.clear()',ui)
        self.assertNotIn('thumbnail-cache',ui)

    def test_report_is_silent_and_persisted(self):
        cpp=source('src/BugReport.cpp');ui=source('src/EditorUi.cpp')
        self.assertIn('bug-report-last-success.txt',cpp)
        self.assertIn('std::lock_guard<std::mutex> lock(cooldownMutex);',cpp)
        self.assertIn('if(InCooldown())return {true,"",true};',cpp)
        self.assertIn('if(outcome.accepted)RecordCooldown();',cpp)
        self.assertIn('"client_id"',cpp.replace('\\"','"'))
        self.assertIn('!BugReport::CooldownActive()',ui)
        self.assertIn('if (!response.suppressed)',ui)
        self.assertIn('bool bugReportAttachLogs_{}',source('src/EditorUi.h'))

    def test_water_has_separate_sorted_alpha_pass(self):
        scene=source('src/SceneRenderer.cpp');header=source('src/SceneRenderer.h')
        self.assertIn('PSTransparent',scene)
        self.assertIn('fractionalAlpha',scene)
        self.assertIn('RenderMeshes(camera,false)',scene)
        self.assertIn('RenderMeshes(camera,true)',scene)
        self.assertIn('spriteDepthState_.Get():depthState_.Get()',scene)
        self.assertIn('std::stable_sort(draws.begin()',scene)
        self.assertIn('DrawIndexedInstanced(part.indexCount,1,part.firstIndex,0,static_cast<UINT>(draw.instance))',scene)
        self.assertIn('bool translucent{};',header)

    def test_worker_is_server_only_and_build_is_configurable(self):
        worker=source('report_gateway/discord/worker.mjs');app=source('src/BugReport.cpp')
        self.assertIn('REPORT_LIMITS',worker)
        self.assertIn('DISCORD_WEBHOOK_URL',worker)
        self.assertIn('!/^[a-f0-9]{32}$/.test(report.client_id)',worker)
        self.assertIn('expirationTtl:WINDOW_SECONDS',worker)
        self.assertIn("if(installation || address || legacy)return accepted()",worker)
        self.assertIn("form.append('files[0]'",worker)
        self.assertNotIn('discord.com/api/webhooks',app)
        self.assertIn('PTPRO_BUG_REPORT_ENDPOINT',source('CMakeLists.txt'))

if __name__=='__main__':unittest.main()
