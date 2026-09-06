use serde_json::{json, Value};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Output, Stdio};

fn fixture() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../tests/fixture")
        .canonicalize()
        .unwrap()
}
fn call_bytes(bytes: &[u8]) -> Output {
    let mut child = Command::new(env!("CARGO_BIN_EXE_asdecided-desktop-backend"))
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .env("DECIDED_NO_CACHE", "1")
        .spawn()
        .unwrap();
    child.stdin.take().unwrap().write_all(bytes).unwrap();
    child.wait_with_output().unwrap()
}
fn call(root: &Path, operation: &str, query: &str) -> Output {
    call_bytes(
        &serde_json::to_vec(
            &json!({"protocol":1,"operation":operation,"project":root,"query":query}),
        )
        .unwrap(),
    )
}
fn value(output: &Output) -> Value {
    serde_json::from_slice(&output.stdout)
        .unwrap_or_else(|e| panic!("{e}: {}", String::from_utf8_lossy(&output.stderr)))
}
fn copy_tree(from: &Path, to: &Path) {
    fs::create_dir_all(to).unwrap();
    for entry in fs::read_dir(from).unwrap() {
        let entry = entry.unwrap();
        let target = to.join(entry.file_name());
        if entry.file_type().unwrap().is_dir() {
            copy_tree(&entry.path(), &target);
        } else {
            fs::copy(entry.path(), target).unwrap();
        }
    }
}
#[test]
fn open_search_scope_validate_and_locate_use_the_real_engine() {
    let root = fixture();
    let opened = call(&root, "open", "");
    assert!(opened.status.success());
    let docs = value(&opened);
    assert_eq!(docs["documents"].as_array().unwrap().len(), 2);
    let search = call(&root, "search", "native");
    assert!(search.status.success());
    assert_eq!(value(&search)["matches"][0]["id"], "APP-KTQ63DPSMF19");
    let scoped = call(&root, "scope", "src/example.rs");
    assert!(scoped.status.success());
    assert_eq!(value(&scoped)["decisions"][0]["id"], "APP-KTQ63DPSMF19");
    assert_eq!(value(&scoped)["decisions"][0]["matching_entry"], "src/");
    let validation = call(&root, "validate", "");
    assert!(validation.status.success());
    assert_eq!(value(&validation)["summary"]["checked"], 2);
    let file = docs["documents"][0]["metadata"]["path"].as_str().unwrap();
    assert_eq!(value(&call(&root, "locate", file))["path"], file);
    assert!(value(&call(&root, "search", "no-such-term-xyz"))["matches"]
        .as_array()
        .unwrap()
        .is_empty());
}
#[test]
fn failed_open_does_not_discover_an_ancestor_project() {
    let result = call(&fixture().join("decisions"), "open", "");
    assert!(!result.status.success());
    assert!(result.stdout.is_empty());
}
#[test]
fn protocol_is_bounded_and_has_no_arbitrary_command_passthrough() {
    for bytes in [
        b"{}".to_vec(),
        serde_json::to_vec(&json!({"protocol":2,"operation":"open","project":fixture()})).unwrap(),
        serde_json::to_vec(&json!({"protocol":1,"operation":"delete","project":fixture()}))
            .unwrap(),
        vec![b' '; 8 * 1024 * 1024 + 1],
    ] {
        let result = call_bytes(&bytes);
        assert_eq!(result.status.code(), Some(2));
        assert!(result.stdout.is_empty());
    }
}
#[test]
fn validation_errors_are_structured_findings_and_refresh_observes_edits() {
    let temp = tempfile::tempdir().unwrap();
    copy_tree(&fixture(), temp.path());
    let file = temp.path().join("decisions/native.md");
    let before = fs::read_to_string(&file).unwrap();
    fs::write(&file, before.replace("## Decision", "## Removed")).unwrap();
    let result = call(temp.path(), "validate", "");
    assert_eq!(result.status.code(), Some(1));
    assert_eq!(value(&result)["valid"], false);
    let result = call(temp.path(), "open", "");
    assert!(value(&result)["documents"][0]["text"]
        .as_str()
        .unwrap()
        .contains("## Removed"));
}
#[test]
fn editor_handoff_refuses_nonmembers_and_symlink_escape() {
    let temp = tempfile::tempdir().unwrap();
    copy_tree(&fixture(), temp.path());
    let outside = tempfile::tempdir().unwrap();
    let target = outside.path().join("secret.md");
    fs::write(&target, "private").unwrap();
    let file = temp.path().join("decisions/native.md");
    fs::remove_file(&file).unwrap();
    #[cfg(unix)]
    std::os::unix::fs::symlink(&target, &file).unwrap();
    assert!(!call(temp.path(), "locate", file.to_str().unwrap())
        .status
        .success());
    assert!(!call(temp.path(), "locate", target.to_str().unwrap())
        .status
        .success());
}
#[test]
fn verified_federation_preserves_history_and_refuses_inherited_editor_handoff() {
    let temp = tempfile::tempdir().unwrap();
    copy_tree(
        &Path::new(env!("CARGO_MANIFEST_DIR")).join("../tests/federation"),
        temp.path(),
    );
    let result = call(temp.path(), "open", "");
    assert!(
        result.status.success(),
        "{}",
        String::from_utf8_lossy(&result.stderr)
    );
    let opened = value(&result);
    let docs = opened["documents"].as_array().unwrap();
    assert!(docs.iter().any(|d| d["metadata"]["provenance"]["overrides"]
        .as_array()
        .is_some_and(|a| !a.is_empty())));
    let inherited = docs
        .iter()
        .find(|d| d["metadata"]["provenance"]["layer"] == "inherited")
        .unwrap();
    assert!(!call(
        temp.path(),
        "locate",
        inherited["metadata"]["path"].as_str().unwrap()
    )
    .status
    .success());
    assert_eq!(
        value(&call(temp.path(), "federation", ""))["status"],
        "verified"
    );
    fs::write(
        temp.path()
            .join("vendor/platform/decisions/platform-policy.md"),
        "changed",
    )
    .unwrap();
    let broken = call(temp.path(), "open", "");
    assert!(!broken.status.success());
    assert!(
        broken.stdout.is_empty(),
        "Never show a partial unverified inherited corpus"
    );
}

fn edit_call(
    root: &Path,
    operation: &str,
    path: &str,
    text: &str,
    hash: &str,
    new_file: bool,
) -> Output {
    call_bytes(
        &serde_json::to_vec(
            &json!({"protocol":1,"operation":operation,"project":root,"query":path,
        "content":text,"expected_hash":hash,"new_file":new_file}),
        )
        .unwrap(),
    )
}
#[test]
fn authoring_review_save_reopen_and_external_conflict() {
    let temp = tempfile::tempdir().unwrap();
    copy_tree(&fixture(), temp.path());
    let path = temp.path().join("decisions/native.md");
    let path = path.to_str().unwrap();
    let original = value(&call(temp.path(), "read", path));
    let hash = original["hash"].as_str().unwrap();
    let text = original["text"]
        .as_str()
        .unwrap()
        .replace("Qt for presentation", "native Qt for presentation");
    let review = edit_call(temp.path(), "review", path, &text, hash, false);
    assert!(review.status.success());
    assert_eq!(value(&review)["valid"], true);
    assert!(value(&review)["diff"]
        .as_str()
        .unwrap()
        .contains("+Use native Qt"));
    assert_eq!(
        fs::read_to_string(path).unwrap(),
        original["text"].as_str().unwrap(),
        "Review must not write"
    );
    let saved = edit_call(temp.path(), "save", path, &text, hash, false);
    assert!(
        saved.status.success(),
        "{}",
        String::from_utf8_lossy(&saved.stderr)
    );
    assert_eq!(fs::read_to_string(path).unwrap(), text);
    let new_hash = value(&saved)["hash"].as_str().unwrap().to_string();
    assert_eq!(value(&call(temp.path(), "read", path))["hash"], new_hash);
    fs::write(path, format!("{text}\nExternal change\n")).unwrap();
    let conflict = edit_call(temp.path(), "save", path, &text, &new_hash, false);
    assert!(!conflict.status.success());
    assert!(String::from_utf8_lossy(&conflict.stderr).contains("changed on disk"));
    assert!(fs::read_to_string(path)
        .unwrap()
        .contains("External change"));
}
#[test]
fn invalid_drafts_identity_changes_and_path_escapes_never_write() {
    let temp = tempfile::tempdir().unwrap();
    copy_tree(&fixture(), temp.path());
    let path = temp.path().join("decisions/native.md");
    let path = path.to_str().unwrap();
    let original = value(&call(temp.path(), "read", path));
    let text = original["text"].as_str().unwrap();
    let hash = original["hash"].as_str().unwrap();
    let invalid = text.replace("## Decision", "## Missing");
    let review = edit_call(temp.path(), "review", path, &invalid, hash, false);
    assert_eq!(value(&review)["valid"], false);
    assert!(!edit_call(temp.path(), "save", path, &invalid, hash, false)
        .status
        .success());
    let changed_id = text.replace("APP-KTQ63DPSMF19", "APP-KTQ63DPX18Q7");
    assert!(
        !edit_call(temp.path(), "save", path, &changed_id, hash, false)
            .status
            .success()
    );
    assert!(
        !edit_call(temp.path(), "save", "../outside.md", text, "", true)
            .status
            .success()
    );
    assert_eq!(fs::read_to_string(path).unwrap(), text);
}
#[test]
fn templates_are_unsaved_and_creation_is_no_clobber() {
    let temp = tempfile::tempdir().unwrap();
    copy_tree(&fixture(), temp.path());
    let draft=call_bytes(&serde_json::to_vec(&json!({"protocol":1,"operation":"template","project":temp.path(),"query":"new.md","kind":"decision","title":"A new native decision"})).unwrap());
    assert!(draft.status.success());
    let data = value(&draft);
    let path = data["path"].as_str().unwrap();
    assert!(!Path::new(path).exists());
    let text = data["text"].as_str().unwrap();
    assert!(text.contains("# A new native decision"));
    let saved = edit_call(temp.path(), "save", path, text, "", true);
    assert!(
        saved.status.success(),
        "{}",
        String::from_utf8_lossy(&saved.stderr)
    );
    assert_eq!(fs::read_to_string(path).unwrap(), text);
    assert!(!edit_call(temp.path(), "save", path, text, "", true)
        .status
        .success());
}
#[test]
fn initialization_is_explicit_and_never_replaces_a_project() {
    let temp = tempfile::tempdir().unwrap();
    assert!(!call(temp.path(), "initialize", "bad key").status.success());
    assert!(!temp.path().join(".decided").exists());
    assert!(call(temp.path(), "initialize", "APP").status.success());
    assert_eq!(
        value(&call(temp.path(), "open", ""))["documents"]
            .as_array()
            .unwrap()
            .len(),
        0
    );
    assert!(!call(temp.path(), "initialize", "OTHER").status.success());
    assert!(fs::read_to_string(temp.path().join(".decided/config.yaml"))
        .unwrap()
        .contains("APP"));
}
#[test]
fn inherited_documents_cannot_enter_the_write_path() {
    let temp = tempfile::tempdir().unwrap();
    copy_tree(
        &Path::new(env!("CARGO_MANIFEST_DIR")).join("../tests/federation"),
        temp.path(),
    );
    let path = temp
        .path()
        .join("vendor/platform/decisions/platform-policy.md");
    let path = path.to_str().unwrap();
    let original = fs::read_to_string(path).unwrap();
    assert!(!call(temp.path(), "read", path).status.success());
    assert!(!edit_call(temp.path(), "save", path, &original, "", false)
        .status
        .success());
    assert_eq!(fs::read_to_string(path).unwrap(), original);
}
