"""Intermediario de Calcu con TLS directo opcional y contexto de sesión."""
import argparse
import getpass
import hmac
import http.client
import json
import os
import socket
import ssl
import sys
import warnings
import unicodedata
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer

QUESTION_LIMIT = 240
ANSWER_LIMIT = 768
HISTORY_LIMIT = 4
REQUEST_LIMIT = 12288
OPENAI_URL = "https://api.openai.com/v1/responses"
BILLING_ERRORS = {
    "credit_balance_exhausted": "El saldo de créditos de OpenAI está agotado",
    "organization_spend_limit_exceeded": "Se alcanzó el límite de gasto de la organización",
    "project_spend_limit_exceeded": "Se alcanzó el límite de gasto del proyecto",
    "organization_usage_limit_exceeded": "Se alcanzó el límite de uso de la organización",
    "insufficient_quota": "Cuota insuficiente en el proyecto de OpenAI",
}


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


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


openai_http = urllib.request.build_opener(NoRedirect)


def openai_answer(question, api_key, model, history=(), *, verify=False):
    if not api_key or not model:
        raise ServiceError(503, "Configura OPENAI_API_KEY y OPENAI_MODEL en el servidor")
    messages = []
    for turn in history:
        messages.extend((
            {"role": "user", "content": turn["question"]},
            {"role": "assistant", "content": turn["answer"]},
        ))
    messages.append({"role": "user", "content": question})
    payload = {
        "model": model,
        "input": messages,
        "instructions": (
            "Esta es una comprobación breve de conectividad. Responde en texto plano." if verify else
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
        with openai_http.open(request, timeout=25) as response:
            raw = response.read(65537)
        if len(raw) > 65536:
            raise ServiceError(502, "Respuesta de OpenAI demasiado grande")
        data = json.loads(raw)
    except urllib.error.HTTPError as exc:
        status = 429 if exc.code == 429 else 424 if exc.code in (400, 401, 403, 404) else 502
        billing = None
        if status == 429:
            try:
                error = json.loads(exc.read(8192)).get("error", {})
                billing = BILLING_ERRORS.get(error.get("code"))
                if not billing and error.get("type") == "insufficient_quota":
                    billing = BILLING_ERRORS["insufficient_quota"]
            except (AttributeError, TypeError, ValueError, OSError, http.client.HTTPException):
                pass
        exc.close()
        if billing:
            raise ServiceError(402, billing) from None
        raise ServiceError(status, "OpenAI rechazó la consulta. Revisa el proyecto, modelo o cuota") from None
    except urllib.error.URLError as exc:
        status = 504 if isinstance(exc.reason, TimeoutError) else 502
        raise ServiceError(status, "No se pudo conectar con OpenAI") from None
    except TimeoutError:
        raise ServiceError(504, "Tiempo de espera de OpenAI") from None
    except (OSError, ValueError, http.client.HTTPException):
        raise ServiceError(502, "Respuesta de OpenAI inválida") from None

    try:
        if data.get("status") not in ("completed", "incomplete"):
            raise ServiceError(502, "OpenAI no completó la consulta")
        if verify and data["status"] != "completed":
            raise ServiceError(502, "OpenAI no completó la verificación")
        parts = []
        for item in data["output"]:
            if item.get("type") != "message":
                continue
            for part in item.get("content", []):
                if part.get("type") == "output_text":
                    parts.append(part["text"])
                elif part.get("type") == "refusal" and not verify:
                    parts.append(part["refusal"])
        answer = lcd_text(" ".join(parts))
        if data["status"] == "incomplete" and answer and not answer.endswith("..."):
            answer = lcd_text(answer + "...")
    except (AttributeError, KeyError, TypeError):
        raise ServiceError(502, "Formato de OpenAI inválido") from None
    if not answer:
        raise ServiceError(502, "OpenAI no devolvió texto")
    return answer


def verify_openai(api_key, model):
    openai_answer("Responde únicamente OK.", api_key, model, verify=True)
    return {"service": "calcu", "openai": True}


def bounded_text(value, limit):
    return isinstance(value, str) and 0 < len(value) <= limit and bool(value.strip())


def read_turns(payload):
    if not isinstance(payload, dict) or not bounded_text(payload.get("question"), QUESTION_LIMIT):
        raise ServiceError(400, "Pregunta inválida")
    history = payload.get("history", [])
    if not isinstance(history, list) or len(history) > HISTORY_LIMIT:
        raise ServiceError(400, "Historial inválido")
    for turn in history:
        if not isinstance(turn, dict) or not bounded_text(turn.get("question"), QUESTION_LIMIT) \
                or not bounded_text(turn.get("answer"), ANSWER_LIMIT):
            raise ServiceError(400, "Turno de historial inválido")
    return payload["question"].strip(), history


def handler_for(device_token, ask, verify):
    expected = ("Bearer " + device_token).encode("ascii")

    class Handler(BaseHTTPRequestHandler):
        def setup(self):
            self.request.settimeout(5)
            super().setup()

        def handle(self):
            try:
                super().handle()
            except (ConnectionResetError, ssl.SSLError):
                pass

        def send_json(self, status, payload):
            body = json.dumps(payload, ensure_ascii=True, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format, *args):
            # Las rutas y las cabeceras de clientes nunca van al registro.
            pass

        def authorize(self):
            supplied = self.headers.get("Authorization", "").encode("latin1")
            if not hmac.compare_digest(supplied, expected):
                raise ServiceError(401, "Token inválido")

        def health(self):
            if self.path == "/live":
                return {"service": "calcu", "available": True}
            if self.path != "/health":
                raise ServiceError(404, "Ruta desconocida")
            self.authorize()
            return {"service": "calcu", "ready": True}

        def question(self):
            if self.path == "/verify":
                self.authorize()
                return verify()
            if self.path != "/ask":
                raise ServiceError(404, "Ruta desconocida")
            self.authorize()
            if self.headers.get_content_type() != "application/json":
                raise ServiceError(415, "Se requiere JSON")
            try:
                size = int(self.headers.get("Content-Length", "0"))
                if not 1 <= size <= REQUEST_LIMIT:
                    raise ServiceError(413, "Longitud inválida")
                raw = self.rfile.read(size)
                if len(raw) != size:
                    raise ServiceError(400, "Cuerpo incompleto")
                payload = json.loads(raw)
            except (ValueError, RecursionError):
                raise ServiceError(400, "JSON inválido") from None
            except socket.timeout:
                raise ServiceError(408, "Tiempo de espera del cliente") from None
            question, history = read_turns(payload)
            return {"answer": ask(question, history)}

        def respond(self, method):
            try:
                try:
                    payload = method()
                    self.send_json(200, payload)
                except ServiceError as exc:
                    self.send_json(exc.status, {"error": str(exc)})
            except (BrokenPipeError, ConnectionResetError, TimeoutError):
                pass

        def do_GET(self):
            self.respond(self.health)

        def do_POST(self):
            self.respond(self.question)

    return Handler


class TlsServer(HTTPServer):
    def __init__(self, address, handler, tls):
        self.tls = tls
        super().__init__(address, handler)

    def get_request(self):
        sock, address = super().get_request()
        if self.tls is None:
            return sock, address
        sock.settimeout(5)
        try:
            return self.tls.wrap_socket(sock, server_side=True), address
        except OSError:
            sock.close()
            raise


def tls_context(cert, private_key):
    tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls.minimum_version = ssl.TLSVersion.TLSv1_2
    tls.load_cert_chain(cert, private_key, password=lambda: "")
    return tls


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prompt-key", action="store_true", help="Lee la clave API sin eco.")
    parser.add_argument("--check-openai", action="store_true", help="Realiza una consulta breve desde este equipo.")
    args = parser.parse_args()
    key = os.environ.get("OPENAI_API_KEY", "").strip()
    model = os.environ.get("OPENAI_MODEL", "").strip()
    token = os.environ.get("CALCU_DEVICE_TOKEN", "")
    if args.prompt_key:
        if not sys.stdin.isatty():
            raise SystemExit("Usa una terminal interactiva o configura OPENAI_API_KEY en el proceso servidor.")
        with warnings.catch_warnings():
            warnings.simplefilter("error", getpass.GetPassWarning)
            try:
                key = getpass.getpass("Clave API de OpenAI: ").strip()
            except (EOFError, getpass.GetPassWarning):
                raise SystemExit("No se pudo leer la clave sin eco. Usa OPENAI_API_KEY en el servidor.") from None
    if args.check_openai:
        try:
            verify_openai(key, model)
        except ServiceError as exc:
            raise SystemExit(f"OpenAI [{exc.status}]: {exc}") from None
        print("OpenAI respondió mediante Responses API.")
        return
    if not token:
        raise SystemExit("Falta CALCU_DEVICE_TOKEN. Genera un token independiente para Calcu.")
    if not key or not model:
        print("OpenAI pendiente: configura OPENAI_API_KEY y OPENAI_MODEL en el servidor.", flush=True)
    if not 32 <= len(token) <= 256 or not all("!" <= c <= "~" for c in token):
        raise SystemExit("CALCU_DEVICE_TOKEN debe tener de 32 a 256 caracteres ASCII sin espacios.")

    cert, private_key = (os.environ.get(name, "") for name in ("CALCU_TLS_CERT", "CALCU_TLS_KEY"))
    if bool(cert) != bool(private_key):
        raise SystemExit("Configura CALCU_TLS_CERT y CALCU_TLS_KEY juntos.")
    bind = os.environ.get("CALCU_BIND", "127.0.0.1")
    if not cert and bind != "127.0.0.1":
        raise SystemExit("El modo HTTP local solo escucha en 127.0.0.1. Configura TLS para otra interfaz.")
    try:
        tls = tls_context(cert, private_key) if cert else None
        port = int(os.environ.get("CALCU_PORT", "8443" if tls else "8080"))
        handler = handler_for(token, lambda q, turns: openai_answer(q, key, model, turns),
                              lambda: verify_openai(key, model))
        with TlsServer((bind, port), handler, tls) as server:
            print(f"Calcu escuchando en {bind}:{server.server_port} ({'HTTPS' if tls else 'HTTP local'})",
                  flush=True)
            server.serve_forever()
    except (ValueError, OverflowError, OSError):
        raise SystemExit("No se pudo iniciar Calcu. Revisa puerto, interfaz y archivos TLS.") from None


if __name__ == "__main__":
    main()
