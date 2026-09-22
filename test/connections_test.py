import importlib.machinery
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

loader = importlib.machinery.SourceFileLoader("connections", str(Path(__file__).resolve().parents[1] / "scripts/wayfinder-connections"))
spec = importlib.util.spec_from_loader(loader.name, loader)
helper = importlib.util.module_from_spec(spec)
loader.exec_module(helper)

BASE = '# Keep this comment\n[routing]\nthreshold = 0.5\n[gateway]\noffline = true\n[gateway.models.local]\nmodel = "llama"\nbase_url = "http://127.0.0.1:11434/v1"\n'

class ConnectionsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'router.toml'
        self.path.write_text(BASE)
        self.calls = []
    def invoke(self, args, payload=b''):
        self.calls.append((args,payload))
        class Result:
            returncode=0
            stdout=b'{"checks":[{"id":"config","status":"pass"}]}'
        return Result()
    def request(self, **changes):
        result=dict(action='save', id='hosted', provider='openai-compatible', endpoint='https://api.openai.com/v1', model='test-model', key='secret-example', revision=helper.revision(self.path.read_bytes()))
        result.update(changes)
        return result
    def test_preserves_policy_and_stores_no_plaintext_key(self):
        with patch.object(helper,'invoke',self.invoke):
            helper.handle(self.path,self.request())
        content=self.path.read_text()
        self.assertIn('# Keep this comment',content)
        self.assertIn('threshold = 0.5',content)
        self.assertNotIn('secret-example',content)
        self.assertIn('secret-tool lookup application',content)
        self.assertEqual(self.calls[-1][1],b'secret-example')
        self.assertNotIn('secret-example',' '.join(self.calls[-1][0]))
        self.assertEqual(self.path.stat().st_mode & 0o777,0o600)
    def test_revision_conflict_does_not_touch_keyring(self):
        with patch.object(helper,'invoke',self.invoke):
            with self.assertRaises(ValueError):helper.handle(self.path,self.request(revision='stale'))
        self.assertEqual(self.calls,[])
        self.assertEqual(self.path.read_text(),BASE)
    def test_rejects_redirecting_key_to_new_host(self):
        with patch.object(helper,'invoke',self.invoke):
            helper.handle(self.path,self.request())
            before=self.path.read_bytes()
            with self.assertRaises(ValueError):helper.handle(self.path,self.request(endpoint='https://other.example/v1',key=''))
            self.assertEqual(self.path.read_bytes(),before)
    def test_rejects_insecure_remote_and_shell_route(self):
        for changes in [dict(endpoint='http://remote.example/v1'),dict(id='x;cat'),dict(endpoint='https://key@api.example/v1')]:
            with self.assertRaises(ValueError):helper.handle(self.path,self.request(**changes))
        self.assertEqual(self.path.read_text(),BASE)
    def test_native_validation_failure_preserves_original(self):
        with patch.object(helper,'validate',side_effect=ValueError('Rejected')):
            with self.assertRaises(ValueError):helper.handle(self.path,self.request())
        self.assertEqual(self.path.read_text(),BASE)
        self.assertFalse(list(self.path.parent.glob('.wayfinder-connection-*')))
    def test_cannot_remove_route_referenced_by_policy(self):
        with self.assertRaises(ValueError):helper.handle(self.path,self.request(action='remove',id='local'))
    def test_offline_change_preserves_models_and_routing(self):
        with patch.object(helper,'invoke',self.invoke):
            helper.handle(self.path,self.request(action='offline',offline=False,key=''))
        content=self.path.read_text()
        self.assertIn('offline = false',content)
        self.assertIn('threshold = 0.5',content)
        self.assertIn('model = "llama"',content)
    def test_symlink_rejected(self):
        link=self.path.parent/'link.toml';link.symlink_to(self.path)
        with self.assertRaises(ValueError):helper.handle(link,{'action':'list'})
    def test_real_router_validation(self):
        import os
        router=os.environ.get('WAYFINDER_TEST_ROUTER')
        if not router:self.skipTest('Released Router supplied by Arch CI')
        with patch.object(helper,'ROUTER',router):
            helper.validate(self.path)
            self.path.write_text('invalid = [')
            with self.assertRaises(ValueError):helper.validate(self.path)

if __name__=='__main__':unittest.main()
