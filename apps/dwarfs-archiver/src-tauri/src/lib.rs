use std::fs;
use std::io::{BufRead, BufReader};
use std::path::Path;
use std::process::{Command, Stdio};
use std::thread;
use tauri::{AppHandle, Emitter, Manager, State};
use tauri_plugin_dialog::DialogExt;

#[derive(Default)]
struct AppState {
    mode: std::sync::Mutex<String>,
    sources: std::sync::Mutex<Vec<String>>,
}

fn emit_line(app: &AppHandle, line: String) {
    let _ = app.emit("job-line", line);
}

fn run_streaming(app: AppHandle, program: &str, args: Vec<String>) -> i32 {
    let mut child = match Command::new(program)
        .args(&args)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
    {
        Ok(c) => c,
        Err(e) => {
            emit_line(&app, format!("failed to spawn {program}: {e}"));
            let _ = app.emit("job-done", -1);
            return -1;
        }
    };

    let app_err = app.clone();
    let err_handle = child.stderr.take().map(|stderr| {
        thread::spawn(move || {
            for line in BufReader::new(stderr).lines().flatten() {
                emit_line(&app_err, line);
            }
        })
    });

    let app_out = app.clone();
    let out_handle = child.stdout.take().map(|stdout| {
        thread::spawn(move || {
            for line in BufReader::new(stdout).lines().flatten() {
                emit_line(&app_out, line);
            }
        })
    });

    let code = child.wait().ok().and_then(|s| s.code()).unwrap_or(-1);
    if let Some(h) = err_handle {
        let _ = h.join();
    }
    if let Some(h) = out_handle {
        let _ = h.join();
    }
    let _ = app.emit("job-done", code);
    code
}

fn is_squashfs(path: &Path) -> bool {
    let n = path
        .file_name()
        .map(|s| s.to_string_lossy().to_ascii_lowercase())
        .unwrap_or_default();
    n.ends_with(".sqfs") || n.ends_with(".sqxz") || n.ends_with(".squashfs")
}

fn extract_other_archive(app: &AppHandle, archive: &Path, dest: &Path) -> Result<(), String> {
    fs::create_dir_all(dest).map_err(|e| e.to_string())?;
    if is_squashfs(archive) {
        emit_line(app, format!("unsquashfs -f -d {} {}", dest.display(), archive.display()));
        let st = Command::new("unsquashfs")
            .args(["-f", "-d", dest.to_str().unwrap_or(""), archive.to_str().unwrap_or("")])
            .status()
            .map_err(|e| e.to_string())?;
        if !st.success() {
            return Err("unsquashfs failed".into());
        }
        return Ok(());
    }
    emit_line(
        app,
        format!("7z x -y -o{} {}", dest.display(), archive.display()),
    );
    let st = Command::new("7z")
        .args([
            "x",
            "-y",
            &format!("-o{}", dest.display()),
            archive.to_str().unwrap_or(""),
        ])
        .status()
        .map_err(|e| e.to_string())?;
    if !st.success() {
        return Err("7z extract failed".into());
    }
    Ok(())
}

#[tauri::command]
fn get_cli_args() -> Vec<String> {
    std::env::args().skip(1).collect()
}

#[tauri::command]
fn set_mode(state: State<AppState>, mode: String, sources: Vec<String>) -> Result<(), String> {
    *state.mode.lock().map_err(|e| e.to_string())? = mode;
    *state.sources.lock().map_err(|e| e.to_string())? = sources;
    Ok(())
}

#[tauri::command]
fn pick_sources(app: AppHandle, directories: bool) -> Result<Vec<String>, String> {
    if directories {
        let dirs = app.dialog().file().blocking_pick_folders();
        Ok(dirs
            .unwrap_or_default()
            .into_iter()
            .filter_map(|p| p.into_path().ok())
            .map(|p| p.to_string_lossy().into_owned())
            .collect())
    } else {
        let files = app.dialog().file().blocking_pick_files();
        Ok(files
            .unwrap_or_default()
            .into_iter()
            .filter_map(|p| p.into_path().ok())
            .map(|p| p.to_string_lossy().into_owned())
            .collect())
    }
}

#[tauri::command]
fn pick_output(app: AppHandle) -> Result<Option<String>, String> {
    let f = app
        .dialog()
        .file()
        .add_filter("DwarFS", &["dwarfs", "dwfs", "dw"])
        .set_file_name("archive.dwarfs")
        .blocking_save_file();
    Ok(f.and_then(|p| p.into_path().ok()).map(|p| p.to_string_lossy().into_owned()))
}

#[tauri::command]
fn pick_output_dir(app: AppHandle) -> Result<Option<String>, String> {
    let d = app.dialog().file().blocking_pick_folder();
    Ok(d.and_then(|p| p.into_path().ok()).map(|p| p.to_string_lossy().into_owned()))
}

#[tauri::command]
fn start_archive(
    app: AppHandle,
    sources: Vec<String>,
    output: String,
    compress_level: u32,
    open_viewer: bool,
) -> Result<(), String> {
    if sources.is_empty() {
        return Err("no sources".into());
    }
    if output.is_empty() {
        return Err("no output".into());
    }
    let input = if sources.len() == 1 {
        sources[0].clone()
    } else {
        // mkdwarfs wants a root dir; for multiple files create a temp list.
        return Err("select a single directory (or use convert/unarchive modes)".into());
    };

    thread::spawn(move || {
        let args = vec![
            "-i".into(),
            input,
            "-o".into(),
            output.clone(),
            "-l".into(),
            compress_level.to_string(),
            "--log-level".into(),
            "info".into(),
            "-f".into(),
        ];
        emit_line(&app, format!("mkdwarfs {}", args.join(" ")));
        let code = run_streaming(app.clone(), "mkdwarfs", args);
        if code == 0 && open_viewer {
            let _ = Command::new("dwarfs-viewer").arg(&output).spawn();
        }
    });
    Ok(())
}

#[tauri::command]
fn start_unarchive(app: AppHandle, image: String, output_dir: String) -> Result<(), String> {
    if image.is_empty() || output_dir.is_empty() {
        return Err("image and output required".into());
    }
    thread::spawn(move || {
        let args = vec![
            "-i".into(),
            image,
            "-o".into(),
            output_dir,
        ];
        emit_line(&app, format!("dwarfsextract {}", args.join(" ")));
        run_streaming(app, "dwarfsextract", args);
    });
    Ok(())
}

#[tauri::command]
fn start_convert(
    app: AppHandle,
    archive: String,
    output: String,
    compress_level: u32,
    open_viewer: bool,
) -> Result<(), String> {
    if archive.is_empty() || output.is_empty() {
        return Err("archive and output required".into());
    }
    thread::spawn(move || {
        let tmp = std::env::temp_dir().join(format!(
            "dwarfs-convert-{}",
            std::process::id()
        ));
        if let Err(e) = extract_other_archive(&app, Path::new(&archive), &tmp) {
            emit_line(&app, e);
            let _ = app.emit("job-done", -1);
            let _ = fs::remove_dir_all(&tmp);
            return;
        }
        // Prefer single top-level dir if archive extracted with one root.
        let input = match fs::read_dir(&tmp) {
            Ok(rd) => {
                let ents: Vec<_> = rd.filter_map(|e| e.ok()).collect();
                if ents.len() == 1 && ents[0].path().is_dir() {
                    ents[0].path()
                } else {
                    tmp.clone()
                }
            }
            Err(_) => tmp.clone(),
        };
        let args = vec![
            "-i".into(),
            input.to_string_lossy().into_owned(),
            "-o".into(),
            output.clone(),
            "-l".into(),
            compress_level.to_string(),
            "--log-level".into(),
            "info".into(),
            "-f".into(),
        ];
        emit_line(&app, format!("mkdwarfs {}", args.join(" ")));
        let code = run_streaming(app.clone(), "mkdwarfs", args);
        let _ = fs::remove_dir_all(&tmp);
        if code == 0 && open_viewer {
            let _ = Command::new("dwarfs-viewer").arg(&output).spawn();
        }
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
            set_mode,
            pick_sources,
            pick_output,
            pick_output_dir,
            start_archive,
            start_unarchive,
            start_convert
        ])
        .setup(|app| {
            let args: Vec<String> = std::env::args().skip(1).collect();
            let mut mode = "archive".to_string();
            let mut sources = Vec::new();
            let mut i = 0;
            while i < args.len() {
                if args[i] == "--mode" && i + 1 < args.len() {
                    mode = args[i + 1].clone();
                    i += 2;
                    continue;
                }
                if let Some(rest) = args[i].strip_prefix("--mode=") {
                    mode = rest.to_string();
                    i += 1;
                    continue;
                }
                if !args[i].starts_with('-') {
                    sources.push(args[i].clone());
                }
                i += 1;
            }
            let state = app.state::<AppState>();
            let _ = set_mode(state, mode.clone(), sources.clone());
            let _ = app.emit("startup", serde_json::json!({ "mode": mode, "sources": sources }));
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
