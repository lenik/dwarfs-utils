import { useCallback, useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";

type Tab = "metadata" | "content" | "integrity";

type DirEntry = {
  name: string;
  path: string;
  is_dir: boolean;
  size: number;
};

const tabs: { id: Tab; label: string }[] = [
  { id: "metadata", label: "Metadata" },
  { id: "content", label: "Content" },
  { id: "integrity", label: "Integrity" },
];

function App() {
  const [tab, setTab] = useState<Tab>("metadata");
  const [image, setImage] = useState<string>("");
  const [meta, setMeta] = useState<string>("");
  const [metaError, setMetaError] = useState<string>("");
  const [cwd, setCwd] = useState<string>("");
  const [entries, setEntries] = useState<DirEntry[]>([]);
  const [mountPath, setMountPath] = useState<string>("");
  const [integrityLog, setIntegrityLog] = useState<string[]>([]);
  const [integrityRunning, setIntegrityRunning] = useState(false);
  const [integrityCode, setIntegrityCode] = useState<number | null>(null);
  const [busy, setBusy] = useState(false);

  const loadMetadata = useCallback(async () => {
    try {
      setMetaError("");
      const json = await invoke<string>("metadata_json");
      try {
        setMeta(JSON.stringify(JSON.parse(json), null, 2));
      } catch {
        setMeta(json);
      }
    } catch (e) {
      setMetaError(String(e));
      setMeta("");
    }
  }, []);

  const loadDir = useCallback(async (rel?: string) => {
    setBusy(true);
    try {
      const mount = await invoke<string>("ensure_mounted");
      setMountPath(mount);
      const list = await invoke<DirEntry[]>("list_dir", { rel: rel ?? cwd });
      setEntries(list);
      if (rel !== undefined) setCwd(rel);
    } catch (e) {
      setMetaError(String(e));
    } finally {
      setBusy(false);
    }
  }, [cwd]);

  useEffect(() => {
    let unsubs: Array<() => void> = [];
    (async () => {
      unsubs.push(
        await listen<string>("startup-tab", (ev) => {
          const t = ev.payload;
          if (t === "metadata" || t === "content" || t === "integrity") setTab(t);
          if (t === "navigator") setTab("content");
          if (t === "fsck") setTab("integrity");
        }),
      );
      unsubs.push(
        await listen<string>("integrity-line", (ev) => {
          setIntegrityLog((prev) => [...prev, ev.payload]);
        }),
      );
      unsubs.push(
        await listen<number>("integrity-done", (ev) => {
          setIntegrityRunning(false);
          setIntegrityCode(ev.payload);
        }),
      );
      const args = await invoke<string[]>("get_cli_args");
      let path = "";
      for (let i = 0; i < args.length; i++) {
        if (args[i] === "--tab" && i + 1 < args.length) {
          const t = args[i + 1];
          if (t === "metadata" || t === "content" || t === "integrity") setTab(t);
          i++;
          continue;
        }
        if (args[i].startsWith("--tab=")) {
          const t = args[i].slice(6);
          if (t === "metadata" || t === "content" || t === "integrity") setTab(t);
          continue;
        }
        if (!args[i].startsWith("-")) path = args[i];
      }
      if (path) {
        await invoke("set_image", { path });
        setImage(path);
      }
    })();
    return () => unsubs.forEach((u) => u());
  }, []);

  useEffect(() => {
    if (!image) return;
    if (tab === "metadata") void loadMetadata();
    if (tab === "content") void loadDir(cwd || "");
    if (tab === "integrity" && integrityLog.length === 0 && !integrityRunning) {
      // wait for user to start
    }
  }, [tab, image]);

  async function pickImage() {
    const path = await invoke<string | null>("open_image_dialog");
    if (path) {
      setImage(path);
      setCwd("");
      setIntegrityLog([]);
      setIntegrityCode(null);
    }
  }

  async function startIntegrity() {
    setIntegrityLog([]);
    setIntegrityCode(null);
    setIntegrityRunning(true);
    try {
      await invoke("run_integrity");
    } catch (e) {
      setIntegrityLog([String(e)]);
      setIntegrityRunning(false);
    }
  }

  function enterDir(ent: DirEntry) {
    if (ent.is_dir) void loadDir(ent.path);
  }

  function goUp() {
    if (!cwd) return;
    const parts = cwd.split("/").filter(Boolean);
    parts.pop();
    void loadDir(parts.join("/"));
  }

  return (
    <div className="flex h-full flex-col">
      <header className="flex items-center gap-3 border-b border-slate-700 bg-slate-900 px-4 py-3">
        <div className="text-lg font-semibold tracking-wide text-sky-300">DwarFS Viewer</div>
        <div className="ml-2 flex gap-1 rounded-md bg-slate-800 p-1">
          {tabs.map((t) => (
            <button
              key={t.id}
              type="button"
              onClick={() => setTab(t.id)}
              className={`rounded px-3 py-1 text-sm ${
                tab === t.id ? "bg-sky-600 text-white" : "text-slate-300 hover:bg-slate-700"
              }`}
            >
              {t.label}
            </button>
          ))}
        </div>
        <div className="ml-auto flex min-w-0 max-w-[50%] items-center gap-2">
          <span className="truncate text-xs text-slate-400" title={image}>
            {image || "No image open"}
          </span>
          <button
            type="button"
            onClick={() => void pickImage()}
            className="shrink-0 rounded bg-slate-700 px-3 py-1 text-sm hover:bg-slate-600"
          >
            Open…
          </button>
        </div>
      </header>

      <main className="min-h-0 flex-1 overflow-auto p-4">
        {!image && (
          <div className="flex h-full flex-col items-center justify-center gap-3 text-slate-400">
            <p>Open a `.dwarfs` / `.dwfs` / `.dw` image to begin.</p>
            <button
              type="button"
              onClick={() => void pickImage()}
              className="rounded bg-sky-600 px-4 py-2 text-white hover:bg-sky-500"
            >
              Open image
            </button>
          </div>
        )}

        {image && tab === "metadata" && (
          <div className="space-y-2">
            {metaError && <p className="text-rose-400">{metaError}</p>}
            <pre className="overflow-auto rounded-lg bg-slate-900 p-4 text-sm text-slate-200">
              {meta || "Loading…"}
            </pre>
          </div>
        )}

        {image && tab === "content" && (
          <div className="space-y-3">
            <div className="flex items-center gap-2 text-sm text-slate-400">
              <button
                type="button"
                onClick={goUp}
                disabled={!cwd}
                className="rounded bg-slate-800 px-2 py-1 disabled:opacity-40"
              >
                Up
              </button>
              <span className="font-mono">
                {mountPath}/{cwd}
              </span>
              {busy && <span>…</span>}
            </div>
            <ul className="divide-y divide-slate-800 rounded-lg border border-slate-800 bg-slate-900">
              {entries.map((ent) => (
                <li key={ent.path}>
                  <button
                    type="button"
                    className="flex w-full items-center gap-3 px-3 py-2 text-left hover:bg-slate-800"
                    onClick={() => enterDir(ent)}
                  >
                    <span className="w-8 text-sky-400">{ent.is_dir ? "DIR" : "FILE"}</span>
                    <span className="flex-1 font-mono text-sm">{ent.name}</span>
                    {!ent.is_dir && (
                      <span className="text-xs text-slate-500">{ent.size.toLocaleString()} B</span>
                    )}
                  </button>
                </li>
              ))}
              {entries.length === 0 && (
                <li className="px-3 py-6 text-center text-slate-500">Empty directory</li>
              )}
            </ul>
          </div>
        )}

        {image && tab === "integrity" && (
          <div className="flex h-full flex-col gap-3">
            <div className="flex items-center gap-3">
              <button
                type="button"
                disabled={integrityRunning}
                onClick={() => void startIntegrity()}
                className="rounded bg-sky-600 px-4 py-2 text-sm text-white hover:bg-sky-500 disabled:opacity-50"
              >
                {integrityRunning ? "Checking…" : "Run dwarfsck --check-integrity"}
              </button>
              {integrityCode !== null && (
                <span
                  className={
                    integrityCode === 0 ? "text-emerald-400" : "text-rose-400"
                  }
                >
                  Exit code: {integrityCode}
                </span>
              )}
            </div>
            <pre className="min-h-0 flex-1 overflow-auto rounded-lg bg-slate-900 p-4 font-mono text-xs text-slate-300">
              {integrityLog.join("\n") || "Integrity report will appear here."}
            </pre>
          </div>
        )}
      </main>
    </div>
  );
}

export default App;
