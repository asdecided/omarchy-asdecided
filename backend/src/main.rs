//! Native workspace operations. Writes are explicit and checked by the authoring module.
mod authoring;
use serde::Deserialize;
use serde_json::{json, Value};
use std::io::{self, Read};
use std::path::{Path, PathBuf};

const REQUEST_LIMIT: u64 = 8 * 1024 * 1024;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Request {
    protocol: u32,
    operation: Operation,
    project: String,
    #[serde(default)]
    query: String,
    #[serde(default)]
    content: String,
    #[serde(default)]
    expected_hash: String,
    #[serde(default)]
    new_file: bool,
    #[serde(default)]
    kind: String,
    #[serde(default)]
    title: String,
}

#[derive(Deserialize)]
#[serde(rename_all = "snake_case")]
enum Operation {
    Open,
    Search,
    Validate,
    Scope,
    Federation,
    Locate,
    Read,
    Template,
    Review,
    Save,
    Graph,
    Initialize,
}

struct Project {
    root: PathBuf,
    corpus: PathBuf,
}

impl Project {
    fn open(path: &str) -> Result<Self, String> {
        let root = Path::new(path)
            .canonicalize()
            .map_err(|e| format!("Cannot open project: {e}"))?;
        if !root.is_dir() {
            return Err("Choose a repository folder.".into());
        }
        // Selection is explicit. Never silently discover an ancestor project.
        let corpus = if root.join("decisions").is_dir() {
            root.join("decisions")
        } else if root.join("rac").is_dir() {
            root.join("rac")
        } else {
            return Err("No decisions/ or rac/ corpus in this folder. Choose the repository root; custom corpus layouts are not supported yet.".into());
        };
        let corpus = corpus.canonicalize().map_err(|e| e.to_string())?;
        if !corpus.starts_with(&root) {
            return Err("The corpus folder points outside the selected project.".into());
        }
        if !root.join(".decided").is_dir() && !root.join(".rac").is_dir() {
            return Err("This project has no .decided/ or .rac/ configuration directory. Initialise it with the decided CLI first.".into());
        }
        Ok(Self { root, corpus })
    }

    fn corpus_str(&self) -> Result<&str, String> {
        self.corpus
            .to_str()
            .ok_or_else(|| "The corpus path must be UTF-8.".into())
    }

    fn documents(&self) -> Result<Value, String> {
        let dir = self.corpus_str()?;
        let composed = rac_engine::federated_corpus::load_composed_corpus(dir, true)
            .map_err(|e| e.to_string())?;
        let export = match &composed {
            Some(corpus) => {
                rac_engine::export::build_documents_export_from_composed(dir, corpus, false)
                    .map_err(|e| e.to_string())?
            }
            None => rac_engine::export::build_documents_export(dir)
                .map_err(|e| e.message().to_string())?,
        };
        // Consume the engine's public export contract, preserving every source and override.
        let documents: Result<Vec<Value>, _> = rac_engine::output::render_documents_jsonl(&export)
            .lines()
            .map(serde_json::from_str)
            .collect();
        Ok(
            json!({"protocol": 1, "project": self.root, "corpus": self.corpus,
            "source": export.corpus_source, "core_version": rac_engine::output::rac_version(),
            "documents": documents.map_err(|e| e.to_string())?}),
        )
    }

    fn locate(&self, path: &str) -> Result<Value, String> {
        // Only current local export members may be handed to an external editor.
        let docs = self.documents()?;
        let _record = docs["documents"]
            .as_array()
            .unwrap()
            .iter()
            .find(|d| {
                d["metadata"]["path"].as_str() == Some(path)
                    && d["metadata"]["provenance"]["layer"]
                        .as_str()
                        .unwrap_or("local")
                        == "local"
            })
            .ok_or("This file is no longer a local member of the corpus. Refresh the project.")?;
        let candidate = Path::new(path);
        let candidate = if candidate.is_absolute() {
            candidate.to_path_buf()
        } else {
            self.root.join(candidate)
        };
        let canonical = candidate.canonicalize().map_err(|e| e.to_string())?;
        if !canonical.starts_with(&self.corpus) || !canonical.is_file() {
            return Err("The file is outside the local corpus.".into());
        }
        if rac_engine::federated_corpus::is_read_only_materialised_path(&canonical)
            .map_err(|e| e.to_string())?
        {
            return Err("Inherited sources are read-only.".into());
        }
        Ok(json!({"protocol": 1, "path": canonical}))
    }
}

fn run(request: Request) -> Result<u8, String> {
    if request.protocol != 1 {
        return Err("Unsupported desktop protocol version.".into());
    }
    if matches!(request.operation, Operation::Initialize) {
        println!(
            "{}",
            authoring::initialize(&request.project, &request.query)?
        );
        return Ok(0);
    }
    let project = Project::open(&request.project)?;
    std::env::set_current_dir(&project.root).map_err(|e| e.to_string())?;
    let dir = project.corpus_str()?.to_owned();
    let result = match request.operation {
        Operation::Read => Some(project.read_document(&request.query)?),
        Operation::Template => {
            Some(project.template(&request.kind, &request.title, &request.query)?)
        }
        Operation::Review => Some(project.review(
            &request.query,
            &request.content,
            &request.expected_hash,
            request.new_file,
        )?),
        Operation::Save => Some(project.save(
            &request.query,
            &request.content,
            &request.expected_hash,
            request.new_file,
        )?),
        Operation::Graph => Some(json!({"protocol":1,"graph":project.graph()?})),
        _ => None,
    };
    if let Some(result) = result {
        println!("{result}");
        return Ok(0);
    }
    let args = match request.operation {
        Operation::Open => {
            let mut data = project.documents()?;
            data["graph"] = project.graph()?;
            println!("{data}");
            return Ok(0);
        }
        Operation::Locate => {
            println!("{}", project.locate(&request.query)?);
            return Ok(0);
        }
        Operation::Search | Operation::Scope if request.query.trim().is_empty() => {
            return Err("Enter a search term or repository-relative code path.".into());
        }
        Operation::Search => vec![
            "find".into(),
            request.query,
            dir,
            "--json".into(),
            "--explain".into(),
        ],
        Operation::Validate => vec!["validate".into(), dir, "--json".into(), "--verify".into()],
        Operation::Scope => vec!["decisions-for".into(), request.query, dir, "--json".into()],
        Operation::Read
        | Operation::Template
        | Operation::Review
        | Operation::Save
        | Operation::Graph
        | Operation::Initialize => unreachable!(),
        Operation::Federation => vec!["corpus".into(), "status".into(), dir, "--json".into()],
    };
    // One engine implementation, statically linked at the reviewed commit. No PATH lookup.
    Ok(rac_engine::cli::run(&args))
}

fn main() {
    if std::env::args().nth(1).as_deref() == Some("--version") {
        println!(
            "asdecided-desktop-backend {} (core {})",
            env!("CARGO_PKG_VERSION"),
            rac_engine::output::rac_version()
        );
        return;
    }
    let result = (|| {
        let mut bytes = Vec::new();
        io::stdin()
            .take(REQUEST_LIMIT + 1)
            .read_to_end(&mut bytes)
            .map_err(|e| e.to_string())?;
        if bytes.len() as u64 > REQUEST_LIMIT {
            return Err("Desktop request exceeds 8 MiB.".into());
        }
        let request =
            serde_json::from_slice(&bytes).map_err(|e| format!("Invalid desktop request: {e}"))?;
        run(request)
    })();
    match result {
        Ok(code) => std::process::exit(code.into()),
        Err(error) => {
            eprintln!("{error}");
            std::process::exit(2);
        }
    }
}
