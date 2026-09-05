import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("onboarding", Path(__file__).parents[1] / "scripts/onboarding.py")
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class LifecycleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.env = patch.dict(os.environ, {"XDG_CONFIG_HOME": self.temp.name})
        self.env.start()
        self.addCleanup(self.env.stop)
        self.config = Path(self.temp.name) / "wayfinder/wayfinder-router.toml"
        self.config.parent.mkdir()
        self.config.write_text(setup.STARTER)
        self.subject = setup.Onboarding("router", str(self.config), "http://127.0.0.1:8088")
        self.keys = {}
        self.secret_patch = patch.object(self.subject, "secret", side_effect=self.secret)
        self.secret_patch.start()
        self.addCleanup(self.secret_patch.stop)
        self.commands = []
        self.runner = patch.object(setup, "run", side_effect=self.run_command)
        self.runner_mock = self.runner.start()
        self.addCleanup(self.runner.stop)
        self.http = patch.object(setup, "request", side_effect=self.request)
        self.http_mock = self.http.start()
        self.addCleanup(self.http.stop)

    def secret(self, action, item, value=None):
        if action == "store":
            self.keys[item] = value
        elif action == "lookup":
            return self.keys[item]
        elif action == "clear":
            self.keys.pop(item, None)
        elif action == "search":
            return item in self.keys
        return ""

    def run_command(self, args, **kwargs):
        self.commands.append(args)
        return json.dumps({"schema_version": "1", "destinations": 1,
                           "checks": [{"id": "config", "status": "pass"}]}).encode()

    def request(self, url, key=None, body=None):
        if url.endswith("/models") and "/router/" not in url:
            return {"data": [{"id": "chat-test"}, {"id": "embedding-test"}]}
        if url.endswith("/healthz"):
            return {"status": "ok", "offline": False}
        if url.endswith("/router/models"):
            return {"models": [{"name": "openai", "model": "chat-test", "key_ok": True, "endpoint": setup.API}]}
        if "/recent" in url:
            return {"recent": [{"request_id": "proof-123", "served_by": "openai", "http_status": 200, "outcome": "succeeded"}]}
        return {"choices": [{"message": {"content": "connected"}}], "_router_request_id": "proof-123"}

    def activate(self):
        self.subject.discover("private-key-never-log")
        self.subject.activate("chat-test")

    def test_full_lifecycle_preserves_key_boundary_and_restores_starter(self):
        self.assertEqual(self.subject.status()["stage"], "provider")
        self.subject.discover("private-key-never-log")
        self.assertEqual(self.config.read_text(), setup.STARTER)
        self.subject.activate("chat-test")
        self.assertIsNone(self.subject.status()["verifiedAt"])
        self.assertEqual(self.subject.test()["stage"], "verified")
        for path in self.config.parent.rglob("*"):
            if path.is_file():
                self.assertNotIn("private-key-never-log", path.read_text())
        self.assertNotIn("private-key-never-log", repr(self.commands))
        self.subject.disconnect()
        self.assertEqual(self.config.read_text(), setup.STARTER)
        self.assertEqual(self.keys, {})
        self.assertEqual(self.subject.status()["stage"], "provider")

    def test_custom_policy_and_symlinks_never_overwritten(self):
        custom = setup.STARTER + "# user edit\n"
        self.config.write_text(custom)
        with self.assertRaises(setup.SetupError):
            self.subject.discover("key")
        self.assertEqual(self.config.read_text(), custom)
        target = self.config.with_suffix(".original")
        self.config.rename(target)
        self.config.symlink_to(target)
        with self.assertRaises(setup.SetupError):
            self.subject.status()
        self.assertEqual(target.read_text(), custom)

    def test_failed_key_store_is_resumable(self):
        self.secret_patch.stop()
        with patch.object(self.subject, "secret", side_effect=setup.SetupError("locked")):
            with self.assertRaises(setup.SetupError):
                self.subject.discover("key")
        item = self.subject.load()["item"]
        with patch.object(self.subject, "secret", side_effect=self.secret):
            self.subject.discover("key-again")
        self.assertEqual(self.subject.load()["item"], item)
        self.assertEqual(self.config.read_text(), setup.STARTER)

    def test_failed_activation_can_repair_or_disconnect(self):
        self.subject.discover("key")
        with patch.object(self.subject, "restart", side_effect=setup.SetupError("stopped")):
            with self.assertRaises(setup.SetupError):
                self.subject.activate("chat-test")
        self.assertEqual(self.subject.status()["stage"], "activating")
        self.assertEqual(self.subject.repair()["stage"], "test")
        self.subject.disconnect()
        self.assertEqual(self.config.read_text(), setup.STARTER)

    def test_interrupted_before_policy_promotion_returns_to_model_step(self):
        self.subject.discover("key")
        state = self.subject.load()
        state.update(model="chat-test", stage="activating")
        self.subject.save(state)
        self.assertEqual(self.subject.repair()["stage"], "model")
        self.subject.activate("chat-test")

    def test_failed_test_clears_prior_proof_and_wrong_receipt_cannot_pass(self):
        self.activate()
        self.subject.test()
        original = self.request
        def wrong_receipt(url, *args):
            if "/recent" in url:
                return {"recent": [{"request_id": "someone-else", "outcome": "succeeded"}]}
            return original(url, *args)
        self.http_mock.side_effect = wrong_receipt
        with self.assertRaises(setup.SetupError):
            self.subject.test()
        self.assertIsNone(self.subject.status()["verifiedAt"])

    def test_failed_disconnect_retains_cleanup_identity(self):
        self.activate()
        with patch.object(self.subject, "secret", side_effect=setup.SetupError("locked")):
            with self.assertRaises(setup.SetupError):
                self.subject.disconnect()
        self.assertEqual(self.subject.status()["stage"], "disconnecting")
        self.assertEqual(self.config.read_text(), setup.STARTER)
        self.subject.disconnect()
        self.assertEqual(self.keys, {})

    def test_disconnect_after_interrupted_store_without_an_item(self):
        self.subject.save({"schema": 1, "stage": "saving-key", "item": "a" * 32})
        self.subject.disconnect()
        self.assertEqual(self.subject.status()["stage"], "provider")

    def test_model_and_doctor_validation_precede_policy_changes(self):
        self.subject.discover("key")
        with self.assertRaises(setup.SetupError):
            self.subject.activate('unknown"\n')
        self.runner_mock.side_effect = lambda *a, **k: b'{"schema_version":"1","destinations":1,"checks":[{"id":"config","status":"fail"}]}'
        with self.assertRaises(setup.SetupError):
            self.subject.activate("chat-test")
        self.assertEqual(self.config.read_text(), setup.STARTER)


class ProcessTests(unittest.TestCase):
    def test_bounds_and_error_redaction(self):
        for command, timeout in [(["python3", "-c", "import time; time.sleep(3)"], 0.05),
                                 (["python3", "-c", "print('x' * 2000000)"], 3),
                                 (["python3", "-c", "import sys; print('secret-value', file=sys.stderr); sys.exit(1)"], 3)]:
            with self.assertRaises(setup.SetupError) as error:
                setup.run(command, timeout=timeout)
            self.assertNotIn("secret-value", str(error.exception))

    @unittest.skipUnless(os.environ.get("WAYFINDER_ROUTER_BIN"), "set WAYFINDER_ROUTER_BIN for release contract")
    def test_generated_policy_passes_real_router_doctor(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.toml"
            path.write_text(setup.policy("chat-test", "a" * 32))
            result = setup.run([os.environ["WAYFINDER_ROUTER_BIN"], "doctor", "--config", str(path), "--json"], accepted=(0, 1))
            report = json.loads(result)
            self.assertEqual(report["destinations"], 1)
            self.assertIn({"id": "config", "status": "pass", "detail": "valid"}, report["checks"])


@unittest.skipUnless(os.environ.get("WAYFINDER_ROUTER_BIN"), "set WAYFINDER_ROUTER_BIN for live Router contract")
class RouterIntegrationTests(unittest.TestCase):
    def test_real_delivery_receipt_and_restart(self):
        class Provider(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                self.reply({"data": [{"id": "chat-test"}]})

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                if self.headers.get("Authorization") != "Bearer fixture-key":
                    self.send_error(401)
                    return
                self.reply({"id": "chatcmpl-fixture", "object": "chat.completion", "created": 1,
                            "model": body["model"], "choices": [{"index": 0, "message": {
                                "role": "assistant", "content": "connected"}, "finish_reason": "stop"}],
                            "usage": {"prompt_tokens": 10, "completion_tokens": 1, "total_tokens": 11}})

            def reply(self, body):
                data = json.dumps(body).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

        import socket
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        server = ThreadingHTTPServer(("127.0.0.1", 0), Provider)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        child = None
        router = os.environ["WAYFINDER_ROUTER_BIN"]
        real_run = setup.run
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "wayfinder/wayfinder-router.toml"
            config.parent.mkdir()
            config.write_text(setup.STARTER)
            secret_tool = Path(directory) / "secret-tool"
            secret_tool.write_text("#!/bin/sh\nstate=\"$0.stored\"\ncase $1 in lookup) printf fixture-key;; store) cat >/dev/null; touch \"$state\";; clear) rm -f \"$state\";; search) if test -f \"$state\"; then printf found; fi;; esac\n")
            secret_tool.chmod(0o700)
            endpoint = f"http://127.0.0.1:{port}"

            def managed_run(args, *positional, **kwargs):
                nonlocal child
                if len(args) > 1 and args[1] == "service":
                    return b""
                if args[0] == "systemctl":
                    if child:
                        child.terminate()
                        child.wait(timeout=5)
                        child = None
                    if "restart" in args:
                        child = subprocess.Popen([router, "serve", "--config", str(config), "--port", str(port)],
                                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        for _ in range(100):
                            try:
                                setup.request(endpoint + "/healthz")
                                return b""
                            except setup.SetupError:
                                time.sleep(0.02)
                        raise AssertionError("Router did not start")
                    return b""
                return real_run(args, *positional, **kwargs)

            try:
                with patch.dict(os.environ, {"XDG_CONFIG_HOME": directory, "HOME": directory,
                                             "WAYFINDER_ROUTER_SAVINGS_FILE": str(Path(directory) / "savings.json")}), \
                     patch.object(setup, "API", f"http://127.0.0.1:{server.server_port}/v1"), \
                     patch.object(setup, "SECRET_TOOL", str(secret_tool)), \
                     patch.object(setup, "run", side_effect=managed_run):
                    subject = setup.Onboarding(router, str(config), endpoint)
                    subject.discover("fixture-key")
                    subject.activate("chat-test")
                    self.assertEqual(subject.test()["stage"], "verified")
                    subject.repair()
                    self.assertIsNone(subject.status()["verifiedAt"])
                    self.assertEqual(subject.test()["stage"], "verified")
                    subject.disconnect()
                    self.assertEqual(config.read_text(), setup.STARTER)
            finally:
                if child:
                    child.terminate()
                    child.wait(timeout=5)
                server.shutdown()
                server.server_close()
                thread.join()


if __name__ == "__main__":
    unittest.main()
