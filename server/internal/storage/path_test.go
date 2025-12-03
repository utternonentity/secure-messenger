package storage

import (
	"os"
	"path/filepath"
	"testing"
)

func TestResolveDataPathPrefersRootCopy(t *testing.T) {
	tempDir := t.TempDir()
	repoRoot := filepath.Join(tempDir, "repo")
	if err := os.MkdirAll(filepath.Join(repoRoot, "data"), 0o755); err != nil {
		t.Fatalf("create root data dir: %v", err)
	}
	// Simulate a stale copy in a nested module directory.
	moduleDir := filepath.Join(repoRoot, "server")
	if err := os.MkdirAll(filepath.Join(moduleDir, "data"), 0o755); err != nil {
		t.Fatalf("create module data dir: %v", err)
	}

	rootStore := filepath.Join(repoRoot, "data", "identity.db")
	nestedStore := filepath.Join(moduleDir, "data", "identity.db")
	if err := os.WriteFile(rootStore, []byte("root"), 0o600); err != nil {
		t.Fatalf("seed root store: %v", err)
	}
	if err := os.WriteFile(nestedStore, []byte("nested"), 0o600); err != nil {
		t.Fatalf("seed nested store: %v", err)
	}

	// Run the resolver from inside the module directory.
	origWD, err := os.Getwd()
	if err != nil {
		t.Fatalf("getwd: %v", err)
	}
	if err := os.Chdir(moduleDir); err != nil {
		t.Fatalf("chdir: %v", err)
	}
	t.Cleanup(func() {
		_ = os.Chdir(origWD)
	})

	res, err := ResolveDataPath("data/identity.db")
	if err != nil {
		t.Fatalf("resolve path: %v", err)
	}
	if res.Primary != rootStore {
		t.Fatalf("expected primary %s, got %s", rootStore, res.Primary)
	}
	if len(res.Redundant) != 1 || res.Redundant[0] != nestedStore {
		t.Fatalf("unexpected redundant list: %#v", res.Redundant)
	}
}

func TestResolveDataPathCreatesUnderRootDirectory(t *testing.T) {
	tempDir := t.TempDir()
	repoRoot := filepath.Join(tempDir, "repo")
	if err := os.MkdirAll(filepath.Join(repoRoot, "data"), 0o755); err != nil {
		t.Fatalf("create root data dir: %v", err)
	}
	moduleDir := filepath.Join(repoRoot, "server")
	if err := os.MkdirAll(moduleDir, 0o755); err != nil {
		t.Fatalf("create module dir: %v", err)
	}

	origWD, err := os.Getwd()
	if err != nil {
		t.Fatalf("getwd: %v", err)
	}
	if err := os.Chdir(moduleDir); err != nil {
		t.Fatalf("chdir: %v", err)
	}
	t.Cleanup(func() {
		_ = os.Chdir(origWD)
	})

	res, err := ResolveDataPath("data/messages.db")
	if err != nil {
		t.Fatalf("resolve path: %v", err)
	}
	expected := filepath.Join(repoRoot, "data", "messages.db")
	if res.Primary != expected {
		t.Fatalf("expected primary %s, got %s", expected, res.Primary)
	}
}

func TestResolveDataPathAbsolute(t *testing.T) {
	res, err := ResolveDataPath("/var/lib/sm/users.db")
	if err != nil {
		t.Fatalf("resolve absolute: %v", err)
	}
	if res.Primary != "/var/lib/sm/users.db" {
		t.Fatalf("unexpected primary: %s", res.Primary)
	}
	if len(res.Redundant) != 0 {
		t.Fatalf("expected no redundant paths, got %#v", res.Redundant)
	}
}

func TestResolveDataPathRejectsEmpty(t *testing.T) {
	if _, err := ResolveDataPath(" "); err == nil {
		t.Fatal("expected error for empty path")
	}
}
