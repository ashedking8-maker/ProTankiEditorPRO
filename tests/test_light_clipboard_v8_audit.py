import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class LightClipboardV8Audit(unittest.TestCase):
    def test_light_is_first_class_gameplay_clipboard_item(self):
        ui=(ROOT/'src/EditorUi.cpp').read_text(encoding='utf-8')
        hdr=(ROOT/'src/EditorUi.h').read_text(encoding='utf-8')
        self.assertIn('LightMarker functionalClipboardLight_',hdr)
        self.assertIn('case FunctionalType::Light: functionalClipboardLight_=map.Lights()[index];break;',ui)
        self.assertIn('case FunctionalType::Light: type=FunctionalPlacement::Light;break;',ui)
        self.assertIn('auto item=functionalClipboardLight_;item.legacySourceIndex=-1;item.position=at;',ui)
        self.assertIn('functionalSelection_.push_back({FunctionalType::Light,index})',ui)
        self.assertIn('float distance2=24.f*24.f;',ui)

    def test_copied_light_keeps_detached_native_xml(self):
        header=(ROOT/'src/MapDocument.h').read_text(encoding='utf-8')
        cpp=(ROOT/'src/MapDocument.cpp').read_text(encoding='utf-8')
        self.assertIn('std::shared_ptr<const std::string> originalXml;',header)
        self.assertIn('marker.originalXml=std::make_shared<const std::string>(rawLight.str())',cpp)
        self.assertIn('node=section.append_copy(detached.child("light"))',cpp)

if __name__=='__main__':
    unittest.main()
