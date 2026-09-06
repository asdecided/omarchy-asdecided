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
        vec![b' '; 65_537],
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
