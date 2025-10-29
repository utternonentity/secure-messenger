package storage

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// Resolution represents the final location of a data file alongside
// any legacy locations that were discovered during resolution.
type Resolution struct {
	// Primary is the canonical path that should be used by the application.
	Primary string
	// Redundant contains additional paths that exist on disk but are not used.
	Redundant []string
}

// ResolveDataPath normalises a possibly relative data path so that the server
// always uses a single canonical location for its databases. When multiple
// copies of the file exist, the one located closest to the repository root is
// preferred and the rest are reported as redundant.
func ResolveDataPath(path string) (Resolution, error) {
	if strings.TrimSpace(path) == "" {
		return Resolution{}, errors.New("storage: path must not be empty")
	}
	if filepath.IsAbs(path) {
		return Resolution{Primary: filepath.Clean(path)}, nil
	}

	rel := filepath.Clean(path)
	wd, err := os.Getwd()
	if err != nil {
		return Resolution{}, fmt.Errorf("storage: get working directory: %w", err)
	}

	segments := strings.Split(rel, string(filepath.Separator))
	first := segments[0]

	var (
		matches  []string
		createAt string
		visited  = make(map[string]struct{})
		dir      = wd
	)
	for {
		candidate := filepath.Join(dir, rel)
		if _, err := os.Stat(candidate); err == nil {
			if _, seen := visited[candidate]; !seen {
				matches = append(matches, candidate)
				visited[candidate] = struct{}{}
			}
		}
		// Track the highest existing parent directory for creation.
		if info, err := os.Stat(filepath.Join(dir, first)); err == nil && info.IsDir() {
			createAt = filepath.Join(dir, rel)
		}

		parent := filepath.Dir(dir)
		if parent == dir {
			break
		}
		dir = parent
	}

	if len(matches) > 0 {
		// The slice is ordered from the nearest directory to the furthest (root).
		// Choose the last entry as the canonical path and treat the rest as redundant.
		primary := matches[len(matches)-1]
		redundant := make([]string, 0, len(matches)-1)
		for i := 0; i < len(matches)-1; i++ {
			redundant = append(redundant, matches[i])
		}
		return Resolution{Primary: primary, Redundant: redundant}, nil
	}

	if createAt != "" {
		return Resolution{Primary: createAt}, nil
	}

	return Resolution{Primary: filepath.Join(wd, rel)}, nil
}
