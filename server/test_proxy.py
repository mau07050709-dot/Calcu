import io
import json
import os
import shutil
import ssl
import subprocess
import tempfile
from pathlib import Path
import threading
import unittest
import urllib.error
import urllib.request
from http.server import HTTPServer
from unittest.mock import patch

from proxy import ANSWER_LIMIT, ServiceError, TlsServer, handler_for, lcd_text, openai_answer, tls_context


class OpenAIContract(unittest.TestCase):
    def test_extracts_lcd_text_from_response_and_builds_request(self):
        result = {"status": "completed", "output": [
            {"type": "reasoning"},
            {"type": "message", "content": [
                {"type": "output_text", "text": "Ángulo: π × 2"},
                {"type": "output_text", "text": "y raíz: √9"},
            ]},
        ]}
        with patch("proxy.openai_http.open", return_value=io.BytesIO(json.dumps(result).encode())) as call:
            self.assertEqual(openai_answer("Prueba", "api-test", "test-model"), "Angulo: pi * 2 y raiz: sqrt9")
        req = call.call_args.args[0]
        self.assertEqual(req.full_url, "https://api.openai.com/v1/responses")
        self.assertEqual(req.get_header("Authorization"), "Bearer api-test")
        self.assertEqual(json.loads(req.data)["input"], [{"role": "user", "content": "Prueba"}])
        self.assertFalse(json.loads(req.data)["store"])

    def test_timeout_and_empty_reply(self):
        with patch("proxy.openai_http.open", side_effect=TimeoutError):
            with self.assertRaises(ServiceError) as error:
                openai_answer("Prueba", "api-test", "test-model")
            self.assertEqual(error.exception.status, 504)
        with patch("proxy.openai_http.open", return_value=io.BytesIO(b'{"status":"completed","output":[]}')):
            with self.assertRaises(ServiceError) as error:
                openai_answer("Prueba", "api-test", "test-model")
            self.assertEqual(error.exception.status, 502)

    def test_upstream_credentials_and_rate_limit(self):
        for upstream, expected in ((401, 424), (404, 424), (429, 429), (500, 502)):
            issue = urllib.error.HTTPError("https://example.invalid", upstream, "", {}, io.BytesIO())
            with patch("proxy.openai_http.open", side_effect=issue):
                with self.assertRaises(ServiceError) as failure:
                    openai_answer("Prueba", "api-test", "test-model")
            self.assertEqual(failure.exception.status, expected)

    def test_limits_text(self):
        rendered = lcd_text("A" * 1000)
        self.assertEqual(len(rendered), ANSWER_LIMIT)
        self.assertTrue(rendered.endswith("..."))


class HttpContract(unittest.TestCase):
    def test_route_token_validation_and_response_length(self):
        token = "local-test-token"
        with HTTPServer(("127.0.0.1", 0), handler_for(token, lambda q, history: f"Eco {q}")) as server:
            thread = threading.Thread(target=server.serve_forever)
            thread.start()
            try:
                url = f"http://127.0.0.1:{server.server_port}"

                def post(path, supplied, question):
                    request = urllib.request.Request(
                        url + path, json.dumps({"question": question}).encode(),
                        {"Authorization": "Bearer " + supplied, "Content-Type": "application/json"},
                    )
                    try:
                        response = urllib.request.urlopen(request, timeout=2)
                    except urllib.error.HTTPError as error:
                        response = error
                    with response:
                        body = response.read()
                        self.assertEqual(int(response.headers["Content-Length"]), len(body))
                        return response.status, json.loads(body)

                self.assertEqual(post("/ask", token, " hola "), (200, {"answer": "Eco hola"}))
                self.assertEqual(post("/ask", "wrong", "hola")[0], 401)
                self.assertEqual(post("/ask", token, " ")[0], 400)
                self.assertEqual(post("/other", token, "hola")[0], 404)
            finally:
                server.shutdown()
                thread.join()


class HttpsConversation(unittest.TestCase):
    def test_verified_tls_multi_turn_and_windows_certificate_rejection(self):
        openssl = shutil.which("openssl")
        if not openssl:
            self.skipTest("OpenSSL necesario para crear un certificado efímero de prueba")
        token = "only-local-integration-test-token-2026"
        with tempfile.TemporaryDirectory() as work:
            cert = Path(work, "local.crt")
            key = Path(work, "local.key")
            subprocess.run(
                [openssl, "req", "-x509", "-newkey", "rsa:2048", "-noenc", "-days", "1",
                 "-subj", "/CN=127.0.0.1", "-addext", "subjectAltName=IP:127.0.0.1",
                 "-keyout", str(key), "-out", str(cert)],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            )
            client = ssl.create_default_context(cafile=str(cert))
            upstream_calls = []

            def upstream(request, *, timeout):
                messages = json.loads(request.data)["input"]
                upstream_calls.append(messages)
                answer = "TEMA CALCU" if len(messages) == 1 else "RESPUESTA CON CONTEXTO"
                response = {"status": "completed", "output": [{"type": "message", "content": [
                    {"type": "output_text", "text": answer},
                ]}]}
                return io.BytesIO(json.dumps(response).encode())

            handler = handler_for(token, lambda q, history: openai_answer(q, "api-test", "model-test", history))
            with patch("proxy.openai_http.open", side_effect=upstream):
                with TlsServer(("127.0.0.1", 0), handler, tls_context(cert, key)) as server:
                    thread = threading.Thread(target=server.serve_forever)
                    thread.start()
                    try:
                        base = f"https://127.0.0.1:{server.server_port}"

                        def exchange(path, data=None, supplied=token):
                            request = urllib.request.Request(
                                base + path, json.dumps(data).encode() if data is not None else None,
                                {"Authorization": "Bearer " + supplied, "Content-Type": "application/json"},
                            )
                            try:
                                response = urllib.request.urlopen(request, context=client, timeout=3)
                            except urllib.error.HTTPError as issue:
                                response = issue
                            with response:
                                return response.status, json.loads(response.read())

                        self.assertEqual(exchange("/health"), (200, {"service": "calcu", "ready": True}))
                        self.assertEqual(exchange("/health", supplied="invalid")[0], 401)
                        self.assertEqual(len(upstream_calls), 0)
                        self.assertEqual(exchange("/ask", {"question": "RECUERDA CALCU"}),
                                         (200, {"answer": "TEMA CALCU"}))
                        history = [{"question": "RECUERDA CALCU", "answer": "TEMA CALCU"}]
                        self.assertEqual(exchange("/ask", {"question": "QUE RECUERDAS", "history": history}),
                                         (200, {"answer": "RESPUESTA CON CONTEXTO"}))
                        self.assertEqual(upstream_calls[-1], [
                            {"role": "user", "content": "RECUERDA CALCU"},
                            {"role": "assistant", "content": "TEMA CALCU"},
                            {"role": "user", "content": "QUE RECUERDAS"},
                        ])
                        self.assertEqual(exchange("/ask", {"question": "X", "history": history * 5})[0], 400)
                        self.assertEqual(exchange("/ask", {"question": "X", "history": [{"question": "X"}]})[0], 400)
                        self.assertEqual(len(upstream_calls), 2)
                        with self.assertRaises(urllib.error.URLError) as error:
                            urllib.request.urlopen(base + "/health", timeout=3)
                        self.assertIsInstance(error.exception.reason, ssl.SSLCertVerificationError)
                        if os.environ.get("CALCU_TEST_WINHTTP_EXE"):
                            subprocess.run([os.environ["CALCU_TEST_WINHTTP_EXE"], base + "/ask", token],
                                           check=True, timeout=25)
                    finally:
                        server.shutdown()
                        thread.join()


if __name__ == "__main__":
    unittest.main()
