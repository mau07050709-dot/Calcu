import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from unittest.mock import patch

from desktop_proxy import key_for_request, save_key

# Este token fijo solo se usa con procesos y datos de prueba.
TOKEN = "0123456789abcdef" * 4
HERE = Path(__file__).resolve().parent


def exchange(port, route, body=None, token=TOKEN):
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    payload = None if body is None else json.dumps(body).encode("ascii")
    request = urllib.request.Request(f"http://127.0.0.1:{port}{route}", payload, {
        "Authorization": "Bearer " + token, "Content-Type": "application/json",
    })
    try:
        response = opener.open(request, timeout=4)
    except urllib.error.HTTPError as failure:
        response = failure
    with response:
        return response.code, json.loads(response.read())


def subprocess_port(process):
    result = queue.Queue()
    threading.Thread(target=lambda: result.put(process.stdout.readline()), daemon=True).start()
    process.stdin.write(TOKEN + "\n")
    process.stdin.flush()
    line = result.get(timeout=10)
    return int(line)


class DesktopProxy(unittest.TestCase):
    def test_local_lifecycle_and_missing_credentials(self):
        with tempfile.TemporaryDirectory() as local:
            environment = dict(os.environ, LOCALAPPDATA=local, CALCU_BIND="0.0.0.0")
            environment.pop("OPENAI_API_KEY", None)
            with subprocess.Popen([sys.executable, "-B", "desktop_proxy.py", "--serve"], cwd=HERE,
                                  env=environment, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True) as server:
                try:
                    port = subprocess_port(server)
                    self.assertEqual(exchange(port, "/live", token=""), (200, {
                        "service": "calcu", "available": True,
                    }))
                    self.assertEqual(exchange(port, "/health", token="incorrecto")[0], 401)
                    self.assertEqual(exchange(port, "/health")[0], 200)
                    self.assertEqual(exchange(port, "/ask", {"question": "HOLA"})[0], 503)
                    self.assertEqual(exchange(port, "/verify", {})[0], 503)
                    server.stdin.close()
                    self.assertEqual(server.wait(timeout=5), 0)
                    self.assertEqual(server.stdout.read(), "")  # El canal solo anunció el puerto.
                finally:
                    if server.poll() is None:
                        server.kill()

    @unittest.skipUnless(os.name == "nt", "DPAPI solo está disponible en Windows")
    def test_key_round_trip_is_encrypted(self):
        fake_key = "clave-sintetica-para-prueba-dpapi"
        with tempfile.TemporaryDirectory() as local, patch.dict(os.environ, {
            "LOCALAPPDATA": local, "OPENAI_API_KEY": "",
        }):
            self.assertEqual(key_for_request(), "")
            save_key(fake_key)
            self.assertNotIn(fake_key.encode(), Path(local, "Calcu", "openai.dpapi").read_bytes())
            self.assertEqual(key_for_request(), fake_key)
            save_key(fake_key + "-reemplazo")
            self.assertEqual(key_for_request(), fake_key + "-reemplazo")

    def test_windows_loopback_two_turns_with_test_double(self):
        exe = os.environ.get("CALCU_TEST_WINHTTP_EXE")
        if not exe:
            self.skipTest("Define CALCU_TEST_WINHTTP_EXE para probar el cliente WinHTTP")
        command = """
import sys
from desktop_proxy import serve
def ask(question, history):
    if question == "HOLA" and not history:
        return "Primera respuesta de prueba con texto para varias paginas de la pantalla."
    if question == "QUE DIJE" and len(history) == 1 and history[0]["question"] == "HOLA":
        return "Recuerdo HOLA"
    return "Contexto incorrecto"
token = sys.stdin.readline().strip()
serve(token, ask, lambda: {"service": "calcu", "openai": True}, sys.stdin,
      lambda port: print(port, flush=True))
"""
        with subprocess.Popen([sys.executable, "-B", "-c", command], cwd=HERE,
                              stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as server:
            try:
                port = subprocess_port(server)
                subprocess.run([exe, "--loopback", f"http://127.0.0.1:{port}/ask", TOKEN],
                               check=True, timeout=25)
                server.stdin.close()
                self.assertEqual(server.wait(timeout=5), 0)
            finally:
                if server.poll() is None:
                    server.kill()


if __name__ == "__main__":
    unittest.main()
