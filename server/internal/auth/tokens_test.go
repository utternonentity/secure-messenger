package auth

import (
	"testing"
	"time"

	"github.com/utternonentity/secure-messenger/server/internal/identity"
)

func TestTokenManagerIssueAndValidate(t *testing.T) {
	mgr := NewTokenManager(2 * time.Second)

	ident := identity.Identity{UserID: "user-123", Nickname: "alice", Roles: []string{"user"}}
	token, expires, err := mgr.IssueToken(ident)
	if err != nil {
		t.Fatalf("IssueToken: %v", err)
	}
	if token == "" {
		t.Fatalf("IssueToken: expected non-empty token")
	}
	if expires.Before(time.Now()) {
		t.Fatalf("IssueToken: expiry is in the past")
	}

	resolved, expiry, err := mgr.ValidateToken(token)
	if err != nil {
		t.Fatalf("ValidateToken: %v", err)
	}
	if expiry != expires {
		t.Fatalf("ValidateToken: expiry mismatch, got %v want %v", expiry, expires)
	}
	if resolved.UserID != ident.UserID || resolved.Nickname != ident.Nickname {
		t.Fatalf("ValidateToken: unexpected identity %+v", resolved)
	}

	mgr.Revoke(token)
	if _, _, err := mgr.ValidateToken(token); err == nil {
		t.Fatalf("expected revoked token to be invalid")
	}
}

func TestTokenManagerExpiresTokens(t *testing.T) {
	mgr := NewTokenManager(50 * time.Millisecond)
	ident := identity.Identity{UserID: "user-456"}
	token, _, err := mgr.IssueToken(ident)
	if err != nil {
		t.Fatalf("IssueToken: %v", err)
	}

	time.Sleep(120 * time.Millisecond)

	if _, _, err := mgr.ValidateToken(token); err == nil {
		t.Fatalf("expected expired token to be invalid")
	}
}

func TestParseBearerToken(t *testing.T) {
	token, err := ParseBearerToken("Bearer abc123")
	if err != nil {
		t.Fatalf("ParseBearerToken: %v", err)
	}
	if token != "abc123" {
		t.Fatalf("ParseBearerToken: got %q", token)
	}

	token, err = ParseBearerToken("  bearer XYZ ")
	if err != nil {
		t.Fatalf("ParseBearerToken lowercase prefix: %v", err)
	}
	if token != "XYZ" {
		t.Fatalf("ParseBearerToken lowercase prefix: got %q", token)
	}

	cases := []string{"", "Bearer", "Token abc", "bearer"}
	for _, header := range cases {
		if _, err := ParseBearerToken(header); err == nil {
			t.Fatalf("ParseBearerToken(%q) expected error", header)
		}
	}
}
