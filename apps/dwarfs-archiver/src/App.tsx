import { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";

type Mode = "archive" | "unarchive" | "convert";

function App() {
  const [mode, setMode] = useState<Mode>("archive");
  const [sources, setSources] = useState<string[]>([]);
  const [output, setOutput] = useState("");
  const [level, setLevel] = useState(7);
  const [openViewer, setOpenViewer] = useState(true);
  const [log, setLog] = useState<string[]>([]);
  const [running, setRunning] = useState(false);
  const [exitCode, setExitCode] = useState<number | null>(null);

  useEffect(() => {
    let unsubs: Array<() => void> = [];
    (async () => {
      unsubs.push(
        await listen<string>("job-line", (ev) => {
          setLog((prev) => [...prev, ev.payload]);
        }),
      );
      unsubs.push(
        await listen<number>("job-done", (ev) => {
          setRunning(false);
          setExitCode(ev.payload);
        }),
      );
      unsubs.push(
        await listen<{ mode: string; sources: string[] }>("startup", (ev) => {
          const m = ev.payload.mode;
          if (m === "archive" || m === "unarchive" || m === "convert") setMode(m);
          if (ev.payload.sources?.length) {
            setSources(ev.payload.sources);
            if (m === "unarchive" || m === "convert") {
              // leave output blank for user
            } else if (ev.payload.sources.length === 1) {
              setOutput(`${ev.payload.sources[0].replace(/\/$/, "")}.dwarfs`);
            }
          }
        }),
      );
      const args = await invoke<string[]>("get_cli_args");
      let m: Mode = "archive";
      const srcs: string[] = [];
      for (let i = 0; i < args.length; i++) {
        if (args[i] === "--mode" && i + 1 < args.length) {
          const v = args[i + 1];
          if (v === "archive" || v === "unarchive" || v === "convert") m = v;
          i++;
          continue;
        }
        if (!args[i].startsWith("-")) srcs.push(args[i]);
      }
      setMode(m);
      if (srcs.length) setSources(srcs);
    })();
    return () => unsubs.forEach((u) => u());
  }, []);

  async function addSources(dirs: boolean) {
    const picked = await invoke<string[]>("pick_sources", { directories: dirs });
    if (picked.length) setSources((prev) => [...prev, ...picked]);
  }

  async function chooseOutput() {
    if (mode === "unarchive") {
      const d = await invoke<string | null>("pick_output_dir");
      if (d) setOutput(d);
    } else {
      const f = await invoke<string | null>("pick_output");
      if (f) setOutput(f);
    }
  }

  async function start() {
    setLog([]);
    setExitCode(null);
    setRunning(true);
    try {
      if (mode === "archive") {
        await invoke("start_archive", {
          sources,
          output,
          compressLevel: level,
          openViewer,
        });
      } else if (mode === "unarchive") {
        await invoke("start_unarchive", {
          image: sources[0] ?? "",
          outputDir: output,
        });
      } else {
        await invoke("start_convert", {
          archive: sources[0] ?? "",
          output,
          compressLevel: level,
          openViewer,
        });
      }
    } catch (e) {
      setLog([String(e)]);
      setRunning(false);
    }
  }

  return (
    <div className="flex h-full flex-col">
      <header className="flex items-center gap-3 border-b border-slate-800 bg-slate-950 px-4 py-3">
        <div className="text-lg font-semibold text-amber-300">DwarFS Archiver</div>
        <div className="ml-2 flex gap-1 rounded-md bg-slate-900 p-1">
          {(["archive", "unarchive", "convert"] as Mode[]).map((m) => (
            <button
              key={m}
              type="button"
              onClick={() => setMode(m)}
              className={`rounded px-3 py-1 text-sm capitalize ${
                mode === m ? "bg-amber-600 text-white" : "text-slate-300 hover:bg-slate-800"
              }`}
            >
              {m}
            </button>
          ))}
        </div>
      </header>

      <main className="grid min-h-0 flex-1 grid-rows-[auto_1fr] gap-4 p-4">
        <section className="space-y-3 rounded-lg border border-slate-800 bg-slate-950 p-4">
          <div className="flex flex-wrap gap-2">
            <button
              type="button"
              className="rounded bg-slate-800 px-3 py-1 text-sm hover:bg-slate-700"
              onClick={() => void addSources(true)}
            >
              Add folder…
            </button>
            <button
              type="button"
              className="rounded bg-slate-800 px-3 py-1 text-sm hover:bg-slate-700"
              onClick={() => void addSources(false)}
            >
              Add files…
            </button>
            <button
              type="button"
              className="rounded bg-slate-800 px-3 py-1 text-sm hover:bg-slate-700"
              onClick={() => setSources([])}
            >
              Clear
            </button>
          </div>
          <ul className="max-h-28 overflow-auto rounded border border-slate-800 bg-slate-900 p-2 font-mono text-xs">
            {sources.length === 0 && <li className="text-slate-500">No sources</li>}
            {sources.map((s) => (
              <li key={s}>{s}</li>
            ))}
          </ul>

          <div className="flex flex-wrap items-end gap-3">
            <label className="flex min-w-[16rem] flex-1 flex-col gap-1 text-sm">
              <span className="text-slate-400">
                {mode === "unarchive" ? "Output directory" : "Output image"}
              </span>
              <div className="flex gap-2">
                <input
                  className="flex-1 rounded border border-slate-700 bg-slate-900 px-2 py-1 font-mono text-xs"
                  value={output}
                  onChange={(e) => setOutput(e.target.value)}
                />
                <button
                  type="button"
                  className="rounded bg-slate-800 px-3 py-1 text-sm"
                  onClick={() => void chooseOutput()}
                >
                  Browse…
                </button>
              </div>
            </label>

            {(mode === "archive" || mode === "convert") && (
              <label className="flex flex-col gap-1 text-sm">
                <span className="text-slate-400">Compress level</span>
                <input
                  type="number"
                  min={0}
                  max={9}
                  value={level}
                  onChange={(e) => setLevel(Number(e.target.value))}
                  className="w-20 rounded border border-slate-700 bg-slate-900 px-2 py-1"
                />
              </label>
            )}

            {(mode === "archive" || mode === "convert") && (
              <label className="flex items-center gap-2 text-sm text-slate-300">
                <input
                  type="checkbox"
                  checked={openViewer}
                  onChange={(e) => setOpenViewer(e.target.checked)}
                />
                Open in Viewer when done
              </label>
            )}

            <button
              type="button"
              disabled={running || sources.length === 0 || !output}
              onClick={() => void start()}
              className="rounded bg-amber-600 px-4 py-2 text-sm font-medium text-white hover:bg-amber-500 disabled:opacity-40"
            >
              {running ? "Running…" : "Start"}
            </button>
            {exitCode !== null && (
              <span className={exitCode === 0 ? "text-emerald-400" : "text-rose-400"}>
                Exit {exitCode}
              </span>
            )}
          </div>
        </section>

        <pre className="overflow-auto rounded-lg border border-slate-800 bg-slate-950 p-3 font-mono text-xs text-slate-300">
          {log.join("\n") || "Progress and logs appear here."}
        </pre>
      </main>
    </div>
  );
}

export default App;
