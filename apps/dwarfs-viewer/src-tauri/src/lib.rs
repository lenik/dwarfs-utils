use serde::Serialize;
use std::fs;
use std::io::{BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::thread;
use tauri::{AppHandle, Emitter, Manager, State}; // Manager: app.state
use tauri_plugin_dialog::DialogExt;

#[derive(Default)]
struct AppState {
    image: std::sync::Mutex<Option<PathBuf>>,
    mount: std::sync::Mutex<Option<PathBuf>>,
}

#[derive(Serialize)]
struct DirEntry {
    name: String,
    path: String,
    is_dir: bool,
    size: u64,
}

fn dwarfs_suffixes() -> &'static [&'static str] {
    &[".dwarfs", ".dwfs", ".dw"]
}

fn sibling_mount(image: &Path) -> Option<PathBuf> {
    let name = image.file_name()?.to_string_lossy();
    let lower = name.to_ascii_lowercase();
    for suf in dwarfs_suffixes() {
        if lower.ends_with(suf) {
            let stem = &name[..name.len() - suf.len()];
            return Some(image.with_file_name(stem));
        }
    }
    None
}

fn run_capture(cmd: &str, args: &[&str]) -> Result<String, String> {
    let out = Command::new(cmd)
        .args(args)
        .output()
        .map_err(|e| format!("failed to run {cmd}: {e}"))?;
    if !out.status.success() {
        let err = String::from_utf8_lossy(&out.stderr);
        return Err(format!("{cmd} failed: {err}"));
    }
    Ok(String::from_utf8_lossy(&out.stdout).to_string())
}

#[tauri::command]
fn get_cli_args() -> Vec<String> {
    std::env::args().skip(1).collect()
}

#[tauri::command]
fn set_image(state: State<AppState>, path: String) -> Result<String, String> {
    let p = PathBuf::from(&path);
    if !p.is_file() {
        return Err(format!("not a file: {path}"));
    }
    *state.image.lock().map_err(|e| e.to_string())? = Some(p.clone());
    Ok(p.to_string_lossy().into_owned())
}

#[tauri::command]
fn open_image_dialog(app: AppHandle, state: State<AppState>) -> Result<Option<String>, String> {
    let file = app
        .dialog()
        .file()
        .add_filter("DwarFS", &["dwarfs", "dwfs", "dw"])
        .blocking_pick_file();
    match file {
        Some(f) => {
            let path = f
                .into_path()
                .map_err(|e| format!("path: {e}"))?
                .to_string_lossy()
                .into_owned();
            set_image(state, path.clone())?;
            Ok(Some(path))
        }
        None => Ok(None),
    }
}

#[tauri::command]
fn metadata_json(state: State<AppState>) -> Result<String, String> {
    let image = state
        .image
        .lock()
        .map_err(|e| e.to_string())?
        .clone()
        .ok_or_else(|| "no image open".to_string())?;
    run_capture(
        "dwarfsck",
        &[
            "-i",
            image.to_str().ok_or("bad path")?,
            "-j",
            "-d",
            "version,history,metadata_summary",
        ],
    )
}

#[tauri::command]
fn ensure_mounted(state: State<AppState>) -> Result<String, String> {
    let image = state
        .image
        .lock()
        .map_err(|e| e.to_string())?
        .clone()
        .ok_or_else(|| "no image open".to_string())?;
    let mount = sibling_mount(&image).ok_or_else(|| "cannot derive mount path".to_string())?;

    if mount.exists() {
        // Already a directory; may already be mounted.
        let mounts = fs::read_to_string("/proc/self/mountinfo").unwrap_or_default();
        let mount_s = mount.to_string_lossy();
        if mounts.lines().any(|l| l.contains(&*mount_s) && l.contains("dwarfs")) {
            *state.mount.lock().map_err(|e| e.to_string())? = Some(mount.clone());
            return Ok(mount_s.into_owned());
        }
    } else {
        fs::create_dir_all(&mount).map_err(|e| e.to_string())?;
    }

    let status = Command::new("dwarfs")
        .arg(&image)
        .arg(&mount)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .map_err(|e| format!("dwarfs: {e}"))?;
    drop(status);

    // Brief wait for FUSE to come up.
    for _ in 0..20 {
        thread::sleep(std::time::Duration::from_millis(100));
        if mount.read_dir().is_ok() {
            break;
        }
    }

    *state.mount.lock().map_err(|e| e.to_string())? = Some(mount.clone());
    Ok(mount.to_string_lossy().into_owned())
}

#[tauri::command]
fn list_dir(state: State<AppState>, rel: Option<String>) -> Result<Vec<DirEntry>, String> {
    let mount = state
        .mount
        .lock()
        .map_err(|e| e.to_string())?
        .clone()
        .ok_or_else(|| "not mounted".to_string())?;
    let path = match rel {
        Some(r) if !r.is_empty() && r != "/" => mount.join(r.trim_start_matches('/')),
        _ => mount.clone(),
    };
    let mut entries = Vec::new();
    for ent in fs::read_dir(&path).map_err(|e| e.to_string())? {
        let ent = ent.map_err(|e| e.to_string())?;
        let meta = ent.metadata().map_err(|e| e.to_string())?;
        let name = ent.file_name().to_string_lossy().into_owned();
        let full = ent.path();
        let rel_path = full
            .strip_prefix(&mount)
            .unwrap_or(&full)
            .to_string_lossy()
            .into_owned();
        entries.push(DirEntry {
            name,
            path: rel_path,
            is_dir: meta.is_dir(),
            size: meta.len(),
        });
    }
    entries.sort_by(|a, b| b.is_dir.cmp(&a.is_dir).then(a.name.cmp(&b.name)));
    Ok(entries)
}

#[tauri::command]
fn unmount_image(state: State<AppState>) -> Result<(), String> {
    let mount = state
        .mount
        .lock()
        .map_err(|e| e.to_string())?
        .clone()
        .ok_or_else(|| "not mounted".to_string())?;
    let _ = Command::new("fusermount3")
        .args(["-u", mount.to_str().unwrap_or("")])
        .status();
    let _ = fs::remove_dir(&mount);
    *state.mount.lock().map_err(|e| e.to_string())? = None;
    Ok(())
}

#[tauri::command]
fn run_integrity(app: AppHandle, state: State<AppState>) -> Result<(), String> {
    let image = state
        .image
        .lock()
        .map_err(|e| e.to_string())?
        .clone()
        .ok_or_else(|| "no image open".to_string())?;

    let app2 = app.clone();
    thread::spawn(move || {
        let mut child = match Command::new("dwarfsck")
            .args([
                "-i",
                image.to_str().unwrap_or(""),
                "--check-integrity",
                "-v",
            ])
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
        {
            Ok(c) => c,
            Err(e) => {
                let _ = app2.emit("integrity-line", format!("error: {e}"));
                let _ = app2.emit("integrity-done", -1);
                return;
            }
        };

        if let Some(stderr) = child.stderr.take() {
            let reader = BufReader::new(stderr);
            for line in reader.lines().flatten() {
                let _ = app2.emit("integrity-line", line);
            }
        }
        if let Some(stdout) = child.stdout.take() {
            let reader = BufReader::new(stdout);
            for line in reader.lines().flatten() {
                let _ = app2.emit("integrity-line", line);
            }
        }
        let code = child.wait().ok().and_then(|s| s.code()).unwrap_or(-1);
        let _ = app2.emit("integrity-done", code);
    });

    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState::default())
        .invoke_handler(tauri::generate_handler![
            get_cli_args,
            set_image,
            open_image_dialog,
            metadata_json,
            ensure_mounted,
            list_dir,
            unmount_image,
            run_integrity
        ])
        .setup(|app| {
            // Parse argv for --tab= and image path.
            let args: Vec<String> = std::env::args().skip(1).collect();
            let mut tab = "metadata".to_string();
            let mut image: Option<String> = None;
            let mut i = 0;
            while i < args.len() {
                if args[i] == "--tab" && i + 1 < args.len() {
                    tab = args[i + 1].clone();
                    i += 2;
                    continue;
                }
                if let Some(rest) = args[i].strip_prefix("--tab=") {
                    tab = rest.to_string();
                    i += 1;
                    continue;
                }
                if !args[i].starts_with('-') {
                    image = Some(args[i].clone());
                }
                i += 1;
            }
            if let Some(path) = image {
                let state = app.state::<AppState>();
                let _ = set_image(state, path);
            }
            let _ = app.emit("startup-tab", tab);
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
