"""Proceso local que Calcu inicia en Windows mediante tuberías anónimas."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import re
import sys
import tempfile
import threading

from proxy import TlsServer, handler_for, openai_answer, verify_openai

DEFAULT_MODEL = "gpt-6-luna"


class Blob(ctypes.Structure):
    _fields_ = [("size", wintypes.DWORD), ("data", ctypes.c_void_p)]


def protect(data, encrypt):
    crypt = ctypes.WinDLL("crypt32", use_last_error=True)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.LocalFree.restype = ctypes.c_void_p
    function = crypt.CryptProtectData if encrypt else crypt.CryptUnprotectData
    function.argtypes = [ctypes.POINTER(Blob), ctypes.c_void_p, ctypes.c_void_p,
                         ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(Blob)]
    function.restype = wintypes.BOOL
    source = ctypes.create_string_buffer(data)
    input_blob = Blob(len(data), ctypes.addressof(source))
    output = Blob()
    try:
        # CRYPTPROTECT_UI_FORBIDDEN. Sin alcance de máquina, DPAPI usa la cuenta actual.
        if not function(ctypes.byref(input_blob), None, None, None, None, 1, ctypes.byref(output)):
            raise ctypes.WinError(ctypes.get_last_error())
        return ctypes.string_at(output.data, output.size)
    finally:
        ctypes.memset(source, 0, len(source))
        if output.data:
            ctypes.memset(output.data, 0, output.size)
            kernel.LocalFree(output.data)


def credential_path():
    return Path(os.environ["LOCALAPPDATA"], "Calcu", "openai.dpapi")


def key_for_request():
    configured = os.environ.get("OPENAI_API_KEY", "").strip()
    if configured:
        return configured
    try:
        with credential_path().open("rb") as source:
            encrypted = source.read(32769)
        if not encrypted or len(encrypted) > 32768:
            return ""
        return protect(encrypted, False).decode("ascii")
    except (OSError, KeyError, UnicodeError):
        return ""


def save_key(key):
    destination = credential_path()
    destination.parent.mkdir(parents=True, exist_ok=True)
    encrypted = protect(key.encode("ascii"), True)
    with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as target:
        temporary = Path(target.name)
        try:
            target.write(encrypted)
            target.close()
            os.replace(temporary, destination)
        finally:
            target.close()
            temporary.unlink(missing_ok=True)


def configure_key():
    import tkinter as tk
    from tkinter import ttk
    import webbrowser

    window = tk.Tk()
    window.title("Calcu · Configurar OpenAI")
    window.resizable(False, False)
    content = ttk.Frame(window, padding=22)
    content.grid()
    ttk.Label(content, text="Conectar Calcu a OpenAI", font=("Segoe UI", 14, "bold")).grid(
        row=0, column=0, columnspan=2, sticky="w")
    ttk.Label(content, text="Pega una clave API de un proyecto de OpenAI con acceso al modelo.\n"
                           "Windows la cifra para tu cuenta. Calcu reutilizará la clave al abrir.").grid(
        row=1, column=0, columnspan=2, sticky="w", pady=(10, 14))
    ttk.Label(content, text="Clave API").grid(row=2, column=0, sticky="w")
    field = ttk.Entry(content, show="*", width=57)
    field.grid(row=3, column=0, columnspan=2, sticky="ew", pady=(4, 8))
    status = ttk.Label(content, text="", wraplength=420)
    status.grid(row=4, column=0, columnspan=2, sticky="w", pady=(0, 8))

    def store(_event=None):
        value = field.get().strip()
        if not re.fullmatch(r"sk-[A-Za-z0-9_-]{16,509}", value):
            status.configure(text="Introduce una clave API completa, sin espacios.")
            return
        try:
            save_key(value)
        except (OSError, KeyError):
            status.configure(text="Windows no pudo guardar la clave de forma cifrada.")
            return
        field.delete(0, "end")
        field.state(["disabled"])
        save.state(["disabled"])
        status.configure(text="Clave guardada. Cierra esta ventana y vuelve a enviar tu pregunta en Calcu.")

    ttk.Button(content, text="Abrir claves de OpenAI", command=lambda: webbrowser.open(
        "https://platform.openai.com/api-keys")).grid(row=5, column=0, sticky="w")
    save = ttk.Button(content, text="Guardar clave", command=store)
    save.grid(row=5, column=1, sticky="e")
    window.bind("<Return>", store)
    window.bind("<Escape>", lambda _event: window.destroy())
    field.focus_set()
    window.mainloop()


def serve(token, ask, verify, control, announce):
    # El proceso local nunca escucha en una interfaz física, aunque existan variables de despliegue.
    with TlsServer(("127.0.0.1", 0), handler_for(token, ask, verify), None) as server:
        def wait_for_parent():
            control.read(1)
            server.shutdown()

        threading.Thread(target=wait_for_parent, daemon=True).start()
        announce(server.server_port)
        server.serve_forever()


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "--configure":
        configure_key()
        return
    if sys.argv[1:] != ["--serve"] or sys.stdin.isatty() or sys.stdout.isatty():
        raise SystemExit("Inicia este servidor desde Calcu.")
    control = sys.stdin.buffer
    raw = control.readline(66)
    if not re.fullmatch(rb"[0-9a-f]{64}\r?\n", raw):
        raise SystemExit("No se pudo iniciar la sesión local.")
    model = os.environ.get("OPENAI_MODEL", "").strip() or DEFAULT_MODEL
    serve(raw.rstrip(b"\r\n").decode("ascii"),
          lambda question, history: openai_answer(question, key_for_request(), model, history),
          lambda: verify_openai(key_for_request(), model),
          control, lambda port: print(port, flush=True))


if __name__ == "__main__":
    main()
