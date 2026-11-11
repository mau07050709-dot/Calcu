import io
import json
import threading
import unittest
import urllib.error
import urllib.request
from http.server import HTTPServer
from unittest.mock import patch

from proxy import ANSWER_LIMIT, ServiceError, handler_for, lcd_text, openai_answer


class OpenAIContract(unittest.TestCase):
    def test_extracts_lcd_text_from_response_and_builds_request(self):
        result = {"status": "completed", "output": [
            {"type": "reasoning"},
            {"type": "message", "content": [
                {"type": "output_text", "text": "Ángulo: π × 2"},
                {"type": "output_text", "text": "y raíz: √9"},
            ]},
        ]}
        with patch("proxy.urllib.request.urlopen", return_value=io.BytesIO(json.dumps(result).encode())) as call:
            self.assertEqual(openai_answer("Prueba", "api-test", "test-model"), "Angulo: pi * 2 y raiz: sqrt9")
        req = call.call_args.args[0]
        self.assertEqual(req.full_url, "https://api.openai.com/v1/responses")
        self.assertEqual(req.get_header("Authorization"), "Bearer api-test")
        self.assertEqual(json.loads(req.data)["input"], "Prueba")
        self.assertFalse(json.loads(req.data)["store"])

    def test_timeout_and_empty_reply(self):
        with patch("proxy.urllib.request.urlopen", side_effect=TimeoutError):
            with self.assertRaises(ServiceError) as error:
                openai_answer("Prueba", "api-test", "test-model")
            self.assertEqual(error.exception.status, 504)
        with patch("proxy.urllib.request.urlopen", return_value=io.BytesIO(b'{"status":"completed","output":[]}')):
            with self.assertRaises(ServiceError) as error:
                openai_answer("Prueba", "api-test", "test-model")
            self.assertEqual(error.exception.status, 502)

    def test_limits_text(self):
        rendered = lcd_text("A" * 1000)
        self.assertEqual(len(rendered), ANSWER_LIMIT)
        self.assertTrue(rendered.endswith("..."))


class HttpContract(unittest.TestCase):
    def test_route_token_validation_and_response_length(self):
        token = "local-test-token"
        with HTTPServer(("127.0.0.1", 0), handler_for(token, lambda q: f"Eco {q}")) as server:
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


if __name__ == "__main__":
    unittest.main()
