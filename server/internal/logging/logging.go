package logging

import (
	"fmt"
	"io"
	"log"
	"os"
	"path/filepath"
	"time"
)

const logFilenameFormat = "server-2006-01-02.log"
const retentionDays = 30

// Setup configures the standard logger to write to stdout and a daily log file.
//
// It also purges log files older than the configured retention period to ensure
// only the most recent month of logs is kept.
func Setup(logDir string) (*os.File, error) {
	now := time.Now()
	if err := os.MkdirAll(logDir, 0o755); err != nil {
		return nil, fmt.Errorf("logging: create directory: %w", err)
	}

	cutoff := now.AddDate(0, 0, -retentionDays)
	if err := cleanupOldLogs(logDir, cutoff); err != nil {
		return nil, err
	}

	logPath := filepath.Join(logDir, now.Format(logFilenameFormat))
	logFile, err := os.OpenFile(logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0o644)
	if err != nil {
		return nil, fmt.Errorf("logging: open file: %w", err)
	}

	log.SetOutput(io.MultiWriter(os.Stdout, logFile))
	log.SetFlags(log.LstdFlags | log.LUTC)
	return logFile, nil
}

func cleanupOldLogs(logDir string, cutoff time.Time) error {
	entries, err := os.ReadDir(logDir)
	if err != nil {
		return fmt.Errorf("logging: read directory: %w", err)
	}

	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		info, err := entry.Info()
		if err != nil {
			return fmt.Errorf("logging: stat %s: %w", entry.Name(), err)
		}
		if info.ModTime().Before(cutoff) {
			if err := os.Remove(filepath.Join(logDir, entry.Name())); err != nil {
				return fmt.Errorf("logging: remove %s: %w", entry.Name(), err)
			}
		}
	}
	return nil
}
