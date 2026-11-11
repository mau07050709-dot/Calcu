"""Intermediario HTTP local para publicar detrás de un proxy TLS."""
import hmac
import json
import os
import socket
import unicodedata
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer

QUESTION_LIMIT = 240
ANSWER_LIMIT = 768
OPENAI_URL = "https://api.openai.com/v1/responses"


class ServiceError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status


def lcd_text(text):
    text = text.translate(str.maketrans({
        "“": '"', "”": '"', "‘": "'", "’": "'", "−": "-", "–": "-",
        "—": "-", "×": "*", "÷": "/", "π": "pi", "√": "sqrt",
        "°": " grados", "…": "...",
    }))
    text = unicodedata.normalize("NFKD", text)
    text = "".join(
        ch if ch.isascii() else "?"
        for ch in text if not unicodedata.combining(ch)
    )
    text = " ".join(text.split())
    text = "".join(ch if " " <= ch <= "~" else "?" for ch in text)
    return text if len(text) <= ANSWER_LIMIT else text[:ANSWER_LIMIT - 3].rstrip() + "..."


def openai_answer(question, api_key, model):
    payload = {
        "model": model,
        "input": question,
        "instructions": (
            "Responde en español de forma breve, en texto plano, sin Markdown. "
            "La respuesta se mostrará en una LCD de 16 columnas. "
            "En matemáticas usa notación de texto y explica los pasos esenciales."
        ),
        "max_output_tokens": 700,
        "store": False,
    }
    request = urllib.request.Request(
        OPENAI_URL,
        data=json.dumps(payload).encode(),
        headers={"Authorization": f"Bearer {api_key}", "Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=25) as response:
            raw = response.read(65537)
        if len(raw) > 65536:
            raise ServiceError(502, "Respuesta de OpenAI demasiado grande")
        data = json.loads(raw)
    except urllib.error.HTTPError as exc:
        status = 429 if exc.code == 429 else 502
        exc.close()
        raise ServiceError(status, "OpenAI rechazó la consulta") from None
    except urllib.error.URLError as exc:
        status = 504 if isinstance(exc.reason, TimeoutError) else 502
        raise ServiceError(status, "No se pudo conectar con OpenAI") from None
    except TimeoutError:
        raise ServiceError(504, "Tiempo de espera de OpenAI") from None
    except (OSError, ValueError):
        raise ServiceError(502, "Respuesta de OpenAI inválida") from None

    try:
        if data.get("status") not in ("completed", "incomplete"):
            raise ServiceError(502, "OpenAI no completó la consulta")
        parts = []
        for item in data["output"]:
            if item.get("type") != "message":
                continue
            for part in item.get("content", []):
                if part.get("type") == "output_text":
                    parts.append(part["text"])
                elif part.get("type") == "refusal":
                    parts.append(part["refusal"])
        answer = lcd_text(" ".join(parts))
        if data["status"] == "incomplete" and answer and not answer.endswith("..."):
            answer = lcd_text(answer + "...")
    except (AttributeError, KeyError, TypeError):
        raise ServiceError(502, "Formato de OpenAI inválido") from None
    if not answer:
        raise ServiceError(502, "OpenAI no devolvió texto")
    return answer


def handler_for(device_token, ask):
    expected = ("Bearer " + device_token).encode("ascii")

    class Handler(BaseHTTPRequestHandler):
        def setup(self):
            self.request.settimeout(5)
            super().setup()

        def send_json(self, status, payload):
            body = json.dumps(payload, ensure_ascii=True, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            try:
                if self.path != "/ask":
                    raise ServiceError(404, "Ruta desconocida")
                supplied = self.headers.get("Authorization", "").encode("latin1")
                if not hmac.compare_digest(supplied, expected):
                    raise ServiceError(401, "Token inválido")
                if self.headers.get_content_type() != "application/json":
                    raise ServiceError(415, "Se requiere JSON")
                try:
                    size = int(self.headers.get("Content-Length", "0"))
                    if not 1 <= size <= 2048:
                        raise ServiceError(413, "Longitud inválida")
                    payload = json.loads(self.rfile.read(size))
                    question = payload.get("question") if isinstance(payload, dict) else None
                except ValueError:
                    raise ServiceError(400, "JSON inválido") from None
                except socket.timeout:
                    raise ServiceError(408, "Tiempo de espera del cliente") from None
                if not isinstance(question, str) or not question.strip() or len(question) > QUESTION_LIMIT:
                    raise ServiceError(400, "Pregunta inválida")
                self.send_json(200, {"answer": ask(question.strip())})
            except ServiceError as exc:
                self.send_json(exc.status, {"error": str(exc)})
            except (BrokenPipeError, ConnectionResetError):
                pass

    return Handler


def main():
    key = os.environ.get("OPENAI_API_KEY", "")
    model = os.environ.get("OPENAI_MODEL", "")
    token = os.environ.get("CALCU_DEVICE_TOKEN", "")
    if not key or not model or len(token) < 32 or not all("!" <= c <= "~" for c in token):
        raise SystemExit(
            "Configura OPENAI_API_KEY, OPENAI_MODEL y CALCU_DEVICE_TOKEN "
            "(32 caracteres ASCII imprimibles como mínimo, sin espacios)."
        )
    port = int(os.environ.get("CALCU_PORT", "8080"))
    with HTTPServer(("127.0.0.1", port), handler_for(token, lambda q: openai_answer(q, key, model))) as server:
        print(f"Calcu escuchando en 127.0.0.1:{port}", flush=True)
        server.serve_forever()


if __name__ == "__main__":
    main()
