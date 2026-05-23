/*
 * main.c — CLI entry point for synclyr2metadata
 *
 * Usage:
 *   synclyr2metadata --folder  "/path/to/folder"
 *   synclyr2metadata --album   "/path/to/album"
 *   synclyr2metadata --artist  "/path/to/artist"
 *   synclyr2metadata --library "/path/to/music"
 */

#include "http_client.h"
#include "lidarr.h"
#include "metadata.h"
#include "sync.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ── Usage ─────────────────────────────────────────────────────────────── */

static void print_usage(const char *progname)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s --folder  \"/path/to/folder\"  [--force] [--threads N]\n"
        "  %s --album   \"/path/to/album\"   [--force] [--threads N]\n"
        "  %s --artist  \"/path/to/artist\"  [--force] [--threads N]\n"
        "  %s --library \"/path/to/music\"   [--force] [--threads N]\n"
        "\n"
        "Options:\n"
        "  --folder       Sync lyrics for audio files directly in one folder\n"
        "  --album        Sync lyrics for a single album directory\n"
        "  --artist       Sync lyrics for all albums of an artist\n"
        "  --library      Sync lyrics for an entire library (artist/album)\n"
        "  --force        Overwrite existing lyrics\n"
        "  --clean-lrc    Delete local .lrc file after successfully embedding it\n"
        "  --threads      Number of parallel threads (default: 4, max: 16)\n"
        "  --out-plain    File to log paths of tracks that got plain lyrics\n"
        "  --out-missing  File to log paths of tracks with no lyrics found\n"
        "  --help         Show this help message\n",
        progname, progname, progname, progname);
}


/*
 * CLI progress callback: prints each track's status to stdout.
 */
static void cli_progress(int idx, int total, const char *title,
                          const char *status, void *user)
{
    (void)user;
    printf("  [%2d/%d] %-40.40s %s\n", idx + 1, total, title, status);
}

/*
 * Check if a path is a directory.
 */
static int is_directory(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

/*
 * Print the totals summary.
 */
static void print_summary(const SyncResult *r)
{
    printf("\n\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n");
    printf("  \xe2\x9c\x93 Synced:     %d\n", r->synced);
    if (r->plain > 0) {
        printf("  \xe2\x9c\x93 Plain:      %d\n", r->plain);
    }
    printf("  \xe2\x8a\x98 Skipped:    %d\n", r->skipped);
    printf("  \xe2\x9c\x97 Not found:  %d\n", r->not_found);
    if (r->errors > 0) {
        printf("  \xe2\x9c\x97 Errors:     %d\n", r->errors);
    }
    printf("\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n");
}

/*
 * Accumulate results from one sync run into a total.
 */
static void result_add(SyncResult *total, const SyncResult *r)
{
    total->synced    += r->synced;
    total->plain     += r->plain;
    total->skipped   += r->skipped;
    total->not_found += r->not_found;
    total->errors    += r->errors;
}

#define SYNC_DEFAULT_THREADS 4
/*
 * --album: sync a single album directory
 */
static int cmd_album(const char *dirpath, const SyncConfig *config)
{
    TrackMetaList *list = metadata_scan_dir(dirpath);
    if (!list || list->count == 0) {
        if (list) metadata_list_free(list);
        printf("No audio files found in '%s'.\n", dirpath);
        return 0;
    }

    printf("Syncing lyrics for %d track(s) in '%s' [%d threads]...\n\n",
           list->count, dirpath, config->num_threads);

    SyncResult r = sync_tracks(list, config, cli_progress, NULL);
    metadata_list_free(list);
    print_summary(&r);

    return (r.errors > 0) ? 1 : 0;
}

/*
 * --folder: sync audio files directly under any one directory
 */
static int cmd_folder(const char *dirpath, const SyncConfig *config)
{
    TrackMetaList *list = metadata_scan_dir(dirpath);
    if (!list || list->count == 0) {
        if (list) metadata_list_free(list);
        printf("No audio files found in '%s'.\n", dirpath);
        return 0;
    }

    printf("Syncing lyrics for %d track(s) in folder '%s' [%d threads]...\n\n",
           list->count, dirpath, config->num_threads);

    SyncResult r = sync_tracks(list, config, cli_progress, NULL);
    metadata_list_free(list);
    print_summary(&r);

    return (r.errors > 0) ? 1 : 0;
}

/*
 * --artist: sync all album subdirectories under an artist
 */
static int cmd_artist(const char *artist_path, const SyncConfig *config)
{
    DIR *dir = opendir(artist_path);
    if (!dir) {
        fprintf(stderr, "error: could not open '%s'\n", artist_path);
        return 1;
    }

    const char *artist_name = strrchr(artist_path, '/');
    artist_name = artist_name ? artist_name + 1 : artist_path;

    printf("\u2550\u2550\u2550 %s \u2550\u2550\u2550\n\n", artist_name);

    SyncResult total = {0};
    int album_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        size_t len = strlen(artist_path) + strlen(entry->d_name) + 2;
        char *sub = malloc(len);
        if (!sub) continue;
        snprintf(sub, len, "%s/%s", artist_path, entry->d_name);

        if (!is_directory(sub)) {
            free(sub);
            continue;
        }

        /* Check if this subdir has audio files */
        TrackMetaList *list = metadata_scan_dir(sub);
        if (!list || list->count == 0) {
            if (list) metadata_list_free(list);
            free(sub);
            continue;
        }

        album_count++;
        printf("\u25b6 %s (%d tracks)\n", entry->d_name, list->count);

        SyncResult r = sync_tracks(list, config, cli_progress, NULL);
        result_add(&total, &r);
        metadata_list_free(list);
        printf("\n");
        free(sub);
    }

    closedir(dir);

    if (album_count == 0) {
        printf("No albums found.\n");
        return 0;
    }

    printf("%d album(s) processed", album_count);
    print_summary(&total);

    return (total.errors > 0) ? 1 : 0;
}

/*
 * --library: scan artist/album structure and sync everything
 */
static int cmd_library(const char *library_path, const SyncConfig *config)
{
    DIR *dir = opendir(library_path);
    if (!dir) {
        fprintf(stderr, "error: could not open '%s'\n", library_path);
        return 1;
    }

    printf("\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\n");
    printf("  synclyr2metadata \u2014 Library Sync\n");
    printf("  Path:     %s\n", library_path);
    printf("  Threads:  %d\n", config->num_threads);
    printf("\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\n\n");

    SyncResult total = {0};
    int artist_count = 0, album_count = 0;

    /* Iterate artists */
    struct dirent *artist_entry;
    while ((artist_entry = readdir(dir)) != NULL) {
        if (artist_entry->d_name[0] == '.') continue;

        size_t alen = strlen(library_path) + strlen(artist_entry->d_name) + 2;
        char *artist_dir = malloc(alen);
        if (!artist_dir) continue;
        snprintf(artist_dir, alen, "%s/%s", library_path, artist_entry->d_name);

        if (!is_directory(artist_dir)) {
            free(artist_dir);
            continue;
        }

        /* Iterate albums under this artist */
        DIR *adir = opendir(artist_dir);
        if (!adir) {
            free(artist_dir);
            continue;
        }

        int artist_albums = 0;

        struct dirent *album_entry;
        while ((album_entry = readdir(adir)) != NULL) {
            if (album_entry->d_name[0] == '.') continue;

            size_t blen = strlen(artist_dir) + strlen(album_entry->d_name) + 2;
            char *album_dir = malloc(blen);
            if (!album_dir) continue;
            snprintf(album_dir, blen, "%s/%s", artist_dir, album_entry->d_name);

            if (!is_directory(album_dir)) {
                free(album_dir);
                continue;
            }

            TrackMetaList *list = metadata_scan_dir(album_dir);
            if (!list || list->count == 0) {
                if (list) metadata_list_free(list);
                free(album_dir);
                continue;
            }

            if (artist_albums == 0) {
                printf("\u2550\u2550\u2550 %s\n", artist_entry->d_name);
            }

            artist_albums++;
            album_count++;
            printf("  \u25b6 %s (%d tracks)\n", album_entry->d_name, list->count);

            SyncResult r = sync_tracks(list, config, cli_progress, NULL);
            result_add(&total, &r);
            metadata_list_free(list);
            free(album_dir);
        }

        closedir(adir);

        if (artist_albums > 0) {
            artist_count++;
            printf("\n");
        }

        free(artist_dir);
    }

    closedir(dir);

    printf("\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\n");
    printf("  Library Sync Complete\n");
    printf("  Artists:  %d\n", artist_count);
    printf("  Albums:   %d\n", album_count);
    print_summary(&total);

    return (total.errors > 0) ? 1 : 0;
}

/* ── Argument parsing ──────────────────────────────────────────────────── */

static const char *find_arg(int argc, char **argv, const char *flag)
{
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return argv[i + 1];
        }
    }
    return NULL;
}

static int has_flag(int argc, char **argv, const char *flag)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0) {
            return 1;
        }
    }
    return 0;
}

/* ── Main ──────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    /* Auto-detect Lidarr: no CLI args + Lidarr env vars present */
    if (argc < 2 && lidarr_detect()) {
        return lidarr_run(argv[0]);
    }

    if (argc < 2 || has_flag(argc, argv, "--help")) {
        print_usage(argv[0]);
        return (argc < 2) ? 1 : 0;
    }

    int exit_code = 0;
    int force = has_flag(argc, argv, "--force");
    int clean_lrc = has_flag(argc, argv, "--clean-lrc");

    const char *threads_str = find_arg(argc, argv, "--threads");
    int num_threads = threads_str ? atoi(threads_str) : SYNC_DEFAULT_THREADS;
    if (num_threads < 1) num_threads = 1;
    if (num_threads > 16) num_threads = 16;

    const char *folder_dir  = find_arg(argc, argv, "--folder");
    const char *album_dir   = find_arg(argc, argv, "--album");
    const char *artist_dir  = find_arg(argc, argv, "--artist");
    const char *library_dir = find_arg(argc, argv, "--library");
    const char *out_plain   = find_arg(argc, argv, "--out-plain");
    const char *out_missing = find_arg(argc, argv, "--out-missing");

    SyncConfig config = {
        .force         = force,
        .clean_lrc     = clean_lrc,
        .prefer_synced = 1,
        .num_threads   = num_threads,
        .out_plain     = (char *)out_plain,
        .out_missing   = (char *)out_missing
    };

    if (folder_dir || album_dir || artist_dir || library_dir) {
        /* ── All sync modes require HTTP ─────────────────────────── */
        if (http_init() != 0) {
            fprintf(stderr, "error: failed to initialize HTTP client\n");
            return 1;
        }

        if (library_dir) {
            exit_code = cmd_library(library_dir, &config);
        } else if (artist_dir) {
            exit_code = cmd_artist(artist_dir, &config);
        } else if (album_dir) {
            exit_code = cmd_album(album_dir, &config);
        } else {
            exit_code = cmd_folder(folder_dir, &config);
        }

        http_cleanup();

    } else {
        fprintf(stderr, "error: invalid arguments\n\n");
        print_usage(argv[0]);
        exit_code = 1;
    }

    return exit_code;
}
