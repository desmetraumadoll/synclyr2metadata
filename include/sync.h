/*
 * sync.h — Shared lyrics sync engine
 *
 * Provides the core logic for processing tracks: looking up lyrics
 * on LRCLIB and writing them into audio files.  Used by both CLI
 * mode (main.c) and Lidarr mode (lidarr.c).
 */

#ifndef SYNC_H
#define SYNC_H

#include "metadata.h"

/* ── Types ─────────────────────────────────────────────────────────────── */

/*
 * Aggregated results from a sync run.
 */
typedef struct {
    int synced;
    int plain;
    int skipped;
    int not_found;
    int errors;
} SyncResult;

/*
 * Callback invoked after each track is processed.
 *
 *   idx    — 0-based track index
 *   total  — total number of tracks
 *   title  — track title (may be NULL)
 *   status — status string (e.g. "✓ synced")
 *   user   — opaque pointer passed through from sync_tracks()
 *
 * The callback is called under a mutex, so it may safely write
 * to shared state or output streams without extra locking.
 */
typedef void (*SyncProgressFn)(int idx, int total,
                                const char *title, const char *status,
                                void *user);

/* ── Public API ────────────────────────────────────────────────────────── */

/*
 * Configuration for a synchronization run.
 */
typedef struct {
    int   force;         /* 1 = overwrite existing lyrics, 0 = skip */
    int   clean_lrc;     /* 1 = delete local .lrc file after embedding */
    int   prefer_synced; /* 1 = prefer synced lyrics over plain matches */
    int   num_threads;   /* number of parallel workers */
    char *out_plain;     /* file path for plain lyrics log */
    char *out_missing;   /* file path for missing lyrics log */
} SyncConfig;

/*
 * Sync lyrics for all tracks in `list`.
 *
 *   list     — pre-scanned track list (caller owns it)
 *   config   — settings for the sync run
 *   progress — per-track callback (may be NULL)
 *   user     — opaque pointer forwarded to the callback
 *
 * Returns aggregated results.
 */
SyncResult sync_tracks(const TrackMetaList *list, const SyncConfig *config,
                         SyncProgressFn progress, void *user);

#endif /* SYNC_H */
