package logging

import (
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestSetupCreatesFileAndCleansOldLogs(t *testing.T) {
	dir := t.TempDir()

	now := time.Now()
	oldLog := filepath.Join(dir, "old.log")
	recentLog := filepath.Join(dir, "recent.log")

	if err := os.WriteFile(oldLog, []byte("old"), 0o644); err != nil {
		t.Fatalf("write old log: %v", err)
	}
	if err := os.WriteFile(recentLog, []byte("recent"), 0o644); err != nil {
		t.Fatalf("write recent log: %v", err)
	}

	cutoff := now.AddDate(0, 0, -retentionDays-1)
	if err := os.Chtimes(oldLog, cutoff, cutoff); err != nil {
		t.Fatalf("set old log time: %v", err)
	}

	recentTime := now.AddDate(0, 0, -5)
	if err := os.Chtimes(recentLog, recentTime, recentTime); err != nil {
		t.Fatalf("set recent log time: %v", err)
	}

	logFile, err := Setup(dir)
	if err != nil {
		t.Fatalf("setup logging: %v", err)
	}
	t.Cleanup(func() {
		logFile.Close()
	})

	if _, err := os.Stat(oldLog); !os.IsNotExist(err) {
		t.Fatalf("expected old log to be removed, got err=%v", err)
	}

	if _, err := os.Stat(recentLog); err != nil {
		t.Fatalf("expected recent log to remain: %v", err)
	}

	todayLog := filepath.Join(dir, now.Format(logFilenameFormat))
	if _, err := os.Stat(todayLog); err != nil {
		t.Fatalf("expected today log to be created: %v", err)
	}
}
