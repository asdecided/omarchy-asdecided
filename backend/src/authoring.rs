use super::Project;
use fs2::FileExt;
use rac_engine::{classify, commands, export, output, parse, scaffold, sha256};
use serde_json::{json, Value};
use similar::TextDiff;
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::{Component, Path, PathBuf};

pub const DOCUMENT_LIMIT: usize = 1024 * 1024;

fn read_text(path: &Path) -> Result<String, String> {
    let mut bytes = Vec::new();
    File::open(path)
        .map_err(|e| e.to_string())?
        .take((DOCUMENT_LIMIT + 1) as u64)
        .read_to_end(&mut bytes)
        .map_err(|e| e.to_string())?;
    if bytes.len() > DOCUMENT_LIMIT {
        return Err("Documents are limited to 1 MiB in the editor.".into());
    }
    String::from_utf8(bytes).map_err(|_| "The editor requires a UTF-8 Markdown file.".into())
}

impl Project {
    pub fn graph(&self) -> Result<Value, String> {
        let dir = self.corpus_str()?;
        let composed = rac_engine::federated_corpus::load_composed_corpus(dir, true)
            .map_err(|e| e.to_string())?;
        let graph = match composed {
            Some(corpus) => export::build_graph_export_from_composed(dir, &corpus, false)
                .map_err(|e| e.to_string())?,
            None => export::build_graph_export(dir).map_err(|e| e.message().to_string())?,
        };
        serde_json::from_str(&output::render_graph_json(&graph)).map_err(|e| e.to_string())
    }

    // Resolve every existing component without following symbolic links. New files
    // may have one missing final component, but never a missing parent directory.
    fn writable_path(&self, path: &str, new_file: bool) -> Result<PathBuf, String> {
        let candidate = Path::new(path);
        let full = if candidate.is_absolute() {
            candidate.to_owned()
        } else {
            self.corpus.join(candidate)
        };
        let relative = full
            .strip_prefix(&self.corpus)
            .map_err(|_| "Choose a path inside the local corpus.")?;
        if relative.as_os_str().is_empty()
            || relative
                .components()
                .any(|c| !matches!(c, Component::Normal(_)))
        {
            return Err("Parent traversal and empty file paths are not allowed.".into());
        }
        if full.extension().and_then(|s| s.to_str()) != Some("md") {
            return Err("Choose a .md filename.".into());
        }
        let mut current = self.corpus.clone();
        for component in relative.components() {
            current.push(component.as_os_str());
            match fs::symlink_metadata(&current) {
                Ok(meta) if meta.file_type().is_symlink() => {
                    return Err("Symbolic links cannot be edited through the app.".into())
                }
                Ok(meta) if current == full => {
                    if new_file {
                        return Err("That file already exists. Choose a new filename.".into());
                    }
                    if !meta.is_file() {
                        return Err("Choose a regular Markdown file.".into());
                    }
                }
                Ok(meta) if !meta.is_dir() => {
                    return Err("A parent path is not a directory.".into())
                }
                Ok(_) => (),
                Err(e)
                    if e.kind() == std::io::ErrorKind::NotFound && new_file && current == full => {}
                Err(e) => return Err(format!("Cannot access the file: {e}")),
            }
        }
        // Confirm the corpus itself has not moved through a symlink since selection.
        if self.corpus.canonicalize().map_err(|e| e.to_string())? != self.corpus {
            return Err("The corpus location changed. Reopen the project.".into());
        }
        if rac_engine::federated_corpus::is_read_only_materialised_path(&full)
            .map_err(|e| e.to_string())?
        {
            return Err("Inherited materialisations are read-only.".into());
        }
        Ok(full)
    }

    pub fn read_document(&self, path: &str) -> Result<Value, String> {
        let checked = self.writable_path(path, false)?;
        self.locate(checked.to_str().ok_or("The path must be UTF-8.")?)?;
        let text = read_text(&checked)?;
        Ok(
            json!({"protocol":1,"path":checked,"text":text,"hash":sha256::hexdigest(text.as_bytes())}),
        )
    }

    pub fn template(&self, kind: &str, title: &str, path: &str) -> Result<Value, String> {
        if title.trim().is_empty() || title.contains(['\n', '\r']) || title.len() > 200 {
            return Err("Enter a title of up to 200 characters on one line.".into());
        }
        let path = self.writable_path(path, true)?;
        let config = scaffold::load_repository_config(self.corpus_str()?)
            .map_err(|e| e.message().to_string())?
            .ok_or(
                "A repository_key is required in the project configuration to create artifacts.",
            )?;
        let body = scaffold::load_template(kind).map_err(|e| e.message().to_string())?;
        let id = scaffold::generate_id(&config.repository_key);
        let body = body.replacen("# Title", &format!("# {}", title.trim()), 1);
        let text = format!("{}{body}", scaffold::render_frontmatter(&id, kind));
        Ok(
            json!({"protocol":1,"path":path,"text":text,"hash":"","id":id,"type":kind,"title":title.trim(),"new_file":true}),
        )
    }

    fn proposal(
        &self,
        path: &str,
        text: &str,
        expected: &str,
        new_file: bool,
    ) -> Result<(PathBuf, String, Value), String> {
        if text.len() > DOCUMENT_LIMIT {
            return Err("Documents are limited to 1 MiB in the editor.".into());
        }
        let target = self.writable_path(path, new_file)?;
        let old = if new_file {
            String::new()
        } else {
            read_text(&target)?
        };
        if !new_file && sha256::hexdigest(old.as_bytes()) != expected {
            return Err("File changed on disk. Your draft is preserved. Copy it or reload the file before retrying; no changes were written.".into());
        }
        let path = target.to_str().ok_or("The path must be UTF-8.")?;
        let artifact = parse::parse_text(text, path);
        let docs = self.documents()?;
        if !new_file {
            self.locate(path)?;
            let original = parse::parse_text(&old, path);
            let identity = |a: &parse::Artifact| {
                a.metadata
                    .as_ref()
                    .map(|m| (m.id.clone(), m.artifact_type.clone()))
            };
            if identity(&original) != identity(&artifact)
                || classify::classify(&original).artifact_type
                    != classify::classify(&artifact).artifact_type
            {
                return Err("Keep the existing artifact ID and type. Create a new artifact for a different identity.".into());
            }
        } else if let Some(id) = artifact.metadata.as_ref().and_then(|m| m.id.as_ref()) {
            if docs["documents"]
                .as_array()
                .unwrap()
                .iter()
                .any(|d| d["id"].as_str().is_some_and(|x| x.eq_ignore_ascii_case(id)))
            {
                return Err("That artifact ID already exists. Start a fresh draft.".into());
            }
        }
        let dir = self.corpus_str()?;
        let composed = rac_engine::federated_corpus::load_composed_corpus(dir, true)
            .map_err(|e| e.to_string())?;
        let validation = match composed {
            Some(corpus) => commands::StdinCorpusValidation {
                source_path: path.into(),
                structural_issues: rac_engine::validate::validate_product(&artifact, dir),
                relationship_issues: corpus
                    .validate_proposed_document(&artifact, path, dir, true)
                    .issues,
            },
            None => commands::validate_stdin_against_corpus(&artifact, dir, path, true),
        };
        let report: Value = serde_json::from_str(&output::render_stdin_corpus_json(&validation))
            .map_err(|e| e.to_string())?;
        let diff = TextDiff::from_lines(old.as_str(), text)
            .unified_diff()
            .context_radius(3)
            .header("On disk", "Your draft")
            .to_string();
        let result = json!({"protocol":1,"valid":validation.ok(),"validation":report,"diff":diff,"path":target});
        Ok((target, old, result))
    }

    pub fn review(
        &self,
        path: &str,
        text: &str,
        expected: &str,
        new_file: bool,
    ) -> Result<Value, String> {
        self.proposal(path, text, expected, new_file)
            .map(|(_, _, review)| review)
    }

    pub fn save(
        &self,
        path: &str,
        text: &str,
        expected: &str,
        new_file: bool,
    ) -> Result<Value, String> {
        let lock_root = std::env::var_os("XDG_CACHE_HOME")
            .map(PathBuf::from)
            .filter(|p| p.is_absolute())
            .or_else(|| std::env::var_os("HOME").map(|p| PathBuf::from(p).join(".cache")))
            .ok_or("Cannot locate the user cache directory.")?
            .join("asdecided-desktop/locks");
        fs::create_dir_all(&lock_root).map_err(|e| e.to_string())?;
        let lock = OpenOptions::new()
            .create(true)
            .truncate(false)
            .read(true)
            .write(true)
            .open(lock_root.join(sha256::hexdigest(self.root.as_os_str().as_encoded_bytes())))
            .map_err(|e| e.to_string())?;
        lock.try_lock_exclusive()
            .map_err(|_| "Another AsDecided window is saving this project. Retry in a moment.")?;
        let (target, old, review) = self.proposal(path, text, expected, new_file)?;
        if review["valid"] != true {
            return Err(
                "Validation errors block this save. Review the findings and correct the draft."
                    .into(),
            );
        }
        let mut staged =
            tempfile::NamedTempFile::new_in(target.parent().unwrap()).map_err(|e| e.to_string())?;
        if !new_file {
            staged
                .as_file()
                .set_permissions(
                    fs::metadata(&target)
                        .map_err(|e| e.to_string())?
                        .permissions(),
                )
                .map_err(|e| e.to_string())?;
        }
        staged
            .write_all(text.as_bytes())
            .map_err(|e| e.to_string())?;
        staged.as_file().sync_all().map_err(|e| e.to_string())?;
        self.writable_path(path, new_file)?;
        if new_file {
            staged
                .persist_noclobber(&target)
                .map_err(|e| e.to_string())?;
        } else {
            if read_text(&target)? != old {
                return Err(
                    "File changed during save. Your draft is preserved; no changes were written."
                        .into(),
                );
            }
            staged.persist(&target).map_err(|e| e.to_string())?;
        }
        // A post-rename durability failure must not be reported as 'nothing saved'.
        let warning = File::open(target.parent().unwrap())
            .and_then(|f| f.sync_all())
            .err()
            .map(|e| format!("Saved, but the directory could not be synced: {e}"));
        Ok(
            json!({"protocol":1,"path":target,"hash":sha256::hexdigest(text.as_bytes()),"text":text,"warning":warning}),
        )
    }
}

pub fn initialize(path: &str, key: &str) -> Result<Value, String> {
    let root = Path::new(path).canonicalize().map_err(|e| e.to_string())?;
    if !root.is_dir() {
        return Err("Choose an existing project folder.".into());
    }
    for name in [".decided", ".rac", "decisions", "rac"] {
        if fs::symlink_metadata(root.join(name)).is_ok() {
            return Err("This folder already contains an AsDecided layout. Open it instead; initialization never replaces existing knowledge.".into());
        }
    }
    // Ask core to produce the config in a staging directory before touching the
    // actual layout. A bad key cannot leave a half-initialized repository.
    let staging = tempfile::tempdir_in(&root).map_err(|e| e.to_string())?;
    scaffold::init_repository(
        staging.path().to_str().ok_or("The path must be UTF-8.")?,
        key,
        None,
        None,
        None,
    )
    .map_err(|e| e.message().to_string())?;
    let config = root.join(".decided");
    let corpus = root.join("decisions");
    fs::create_dir(&config).map_err(|e| e.to_string())?;
    if let Err(e) = fs::create_dir(&corpus) {
        let _ = fs::remove_dir(&config);
        return Err(e.to_string());
    }
    if let Err(e) = fs::hard_link(
        staging.path().join(".decided/config.yaml"),
        config.join("config.yaml"),
    ) {
        let _ = fs::remove_dir(&corpus);
        let _ = fs::remove_dir(&config);
        return Err(e.to_string());
    }
    Ok(json!({"protocol":1,"project":root,"created":true}))
}
