#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

#include "lumidb.h"

char* upload_file(LumiDB* lumidb, char* filename) {
    char* asset_id = NULL;
    LumiDBError err = lumidb_upload_asset(lumidb, filename, &asset_id);

    if (err) {
        printf("LumiDB Error: %s (%d)\n", lumidb_error_string(err), err);
        return NULL;
    }

    return asset_id;
}

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    int ret = 1;

    if (argc < 5) {
        printf("Usage: %s <lumidb_url> <table_name> <table_proj> <file1> [file2] ...\n", argv[0]);
        return 1;
    }

    const char* api_key = getenv("LUMIDB_API_KEY");
    if (api_key == NULL) {
        printf("Error: LUMIDB_API_KEY environment variable not set\n");
        return 1;
    }

    char* lumidb_url = argv[1];
    char* table_name = argv[2];
    char* table_proj = argv[3];
    int num_files = argc - 4;

    LumiDB* lumidb = lumidb_init(lumidb_url, api_key);

    char* manifest = NULL;
    char* table_id = NULL;
    LumiDBImportAsset* assets = malloc(sizeof(LumiDBImportAsset) * (unsigned long)num_files);

    if (lumidb == NULL) {
        printf("Error: Failed to initialize LumiDB\n");
        goto cleanup;
    }

    for (int i = 0; i < num_files; i++) {
        char* filename = argv[4 + i];
        char* asset_id = upload_file(lumidb, filename);
        if (asset_id == NULL) {
            printf("Failed to upload asset %d: %s\n", i, filename);
            goto cleanup;
        }

        printf("asset %d/%d: %s\n", i + 1, num_files, asset_id);
        assets[i].asset_id = asset_id;
        assets[i].proj = table_proj;
    }

    manifest = lumidb_build_import_manifest(table_name, table_proj, assets, num_files);

    if (manifest == NULL) {
        printf("Error: Failed to build import manifest\n");
        goto cleanup;
    }

    printf("MANIFEST: %s\n", manifest);

    LumiDBError err = lumidb_start_import(lumidb, manifest, &table_id);

    if (err) {
        printf("LumiDB Error: %d %s\n", err, lumidb_error_string(err));
        goto cleanup;
    }

    int done = 0;
    while (!done) {
        err = lumidb_poll_import_status(lumidb, table_id, &done);
        if (err) {
            printf("LumiDB Error: %d %s\n", err, lumidb_error_string(err));
            goto cleanup;
        }
        sleep(5);
    }

    ret = 0;

cleanup:
    if (lumidb) lumidb_free(lumidb);
    if (table_id) free(table_id);
    if (manifest) free(manifest);
    if (assets) {
        for (int i = 0; i < num_files; i++) {
            free(assets[i].asset_id);
        }
        free(assets);
    }

    curl_global_cleanup();

    return ret;
}
