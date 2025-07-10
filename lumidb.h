#pragma once

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    LUMIDB_SUCCESS = 0,
    LUMIDB_ERROR_GENERIC = 1,
    LUMIDB_ERROR_CURL = 2,
    LUMIDB_ERROR_HTTP = 3,
    LUMIDB_ERROR_JSON_PARSE = 4,
    LUMIDB_ERROR_FILE_IO = 5,
} LumiDBError;

struct lumidb_response_data {
    char *data;
    size_t size;
    size_t capacity;
};

typedef struct lumidb {
    char *base_url;
    char *api_key;
} LumiDB;

typedef struct lumidb_import_asset {
    char *asset_id;
    char *proj;
} LumiDBImportAsset;

static size_t __lumidb_write_callback(void *contents, size_t size, size_t nmemb,
                                      struct lumidb_response_data *response) {
    size_t total_size = size * nmemb;
    size_t new_size = response->size + total_size;

    if (new_size + 1 > response->capacity) {
        // Not enough space, reallocate. Double the required size for future growth.
        size_t new_capacity = (new_size + 1) * 2;
        char *new_data = (char *)realloc(response->data, new_capacity);
        if (!new_data) {
            printf("Failed to reallocate memory in write callback\n");
            return 0;  // Signal error to curl
        }
        response->data = new_data;
        response->capacity = new_capacity;
    }

    memcpy(&(response->data[response->size]), contents, total_size);
    response->size = new_size;
    response->data[response->size] = '\0';

    return total_size;
}

static size_t __lumidb_read_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    FILE *file = (FILE *)userp;
    return fread(contents, size, nmemb, file);
}

static inline void lumidb_free(LumiDB *db) {
    if (!db) {
        return;
    }

    free(db->base_url);
    free(db->api_key);
    free(db);
}

static inline LumiDB *lumidb_init(const char *url, const char *api_key) {
    if (!url || !api_key) {
        printf("Invalid URL or API key\n");
        return NULL;
    }

    LumiDB *db = (LumiDB *)malloc(sizeof(struct lumidb));
    if (!db) {
        return NULL;
    }

    if (url) {
        db->base_url = (char *)malloc(strlen(url) + 2);
        if (!db->base_url) {
            lumidb_free(db);
            return NULL;
        }
        strcpy(db->base_url, url);
        if (url[strlen(url) - 1] != '/') {
            strcat(db->base_url, "/");
        }
    }

    if (api_key) {
        db->api_key = (char *)malloc(strlen(api_key) + 1);
        if (!db->api_key) {
            lumidb_free(db);
            return NULL;
        }
        strcpy(db->api_key, api_key);
    }

    return db;
}

static inline char *__lumidb_get_filename(const char *path) {
    if (path == NULL) {
        return NULL;
    }

    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *last_separator = NULL;

    // Find the rightmost separator (whichever comes last)
    if (last_slash && last_backslash) {
        last_separator = (last_slash > last_backslash) ? last_slash : last_backslash;
    } else if (last_slash) {
        last_separator = last_slash;
    } else if (last_backslash) {
        last_separator = last_backslash;
    }

    if (last_separator == NULL) {
        return (char *)path;
    }

    return (char *)(last_separator + 1);
}

static inline LumiDBError lumidb_upload_asset(const LumiDB *db, const char *file_path, char **lumidb_asset_id) {
    LumiDBError ret = LUMIDB_ERROR_GENERIC;
    long file_size = 0;

    // initialize dynamically allocated variables
    struct curl_slist *req_headers = NULL;
    struct lumidb_response_data response = {0};
    cJSON *json = NULL;
    cJSON *upload_url = NULL;
    cJSON *asset_id = NULL;
    FILE *file = NULL;
    char *filename = __lumidb_get_filename(file_path);

    *lumidb_asset_id = NULL;

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: apikey %s", db->api_key);
    req_headers = curl_slist_append(req_headers, auth_header);
    req_headers = curl_slist_append(req_headers, "Content-Type: application/json");

    char request_body[1024];
    snprintf(request_body, sizeof(request_body), "{\"name\":\"%s\"}", filename);

    char upload_api_url[256];
    snprintf(upload_api_url, sizeof(upload_api_url), "%s%s", db->base_url, "api/assets/upload");

    CURL *curl = NULL;
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_URL, upload_api_url);
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __lumidb_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    if (req_headers) curl_slist_free_all(req_headers);

    if (res != CURLE_OK) {
        printf("curl failed: %s\n", curl_easy_strerror(res));
        ret = LUMIDB_ERROR_CURL;
        goto cleanup;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code != 200) {
        printf("Unexpected response code: %ld\n", response_code);
        ret = LUMIDB_ERROR_HTTP;
        goto cleanup;
    }

    json = cJSON_Parse(response.data);

    if (!json) {
        printf("Failed to parse JSON\n");
        ret = LUMIDB_ERROR_JSON_PARSE;
        goto cleanup;
    }

    upload_url = cJSON_GetObjectItem(json, "upload_url");
    if (!upload_url || !cJSON_IsString(upload_url)) {
        printf("No upload_url in response\n");
        ret = LUMIDB_ERROR_JSON_PARSE;
        goto cleanup;
    }

    asset_id = cJSON_GetObjectItem(json, "asset_id");
    if (!asset_id || !cJSON_IsString(asset_id)) {
        printf("No asset_id in response\n");
        ret = LUMIDB_ERROR_JSON_PARSE;
        goto cleanup;
    }

    *lumidb_asset_id = strdup(asset_id->valuestring);

    file = fopen(file_path, "rb");
    if (!file) {
        printf("Cannot open file: %s\n", file_path);
        ret = LUMIDB_ERROR_FILE_IO;
        goto cleanup;
    }

    fseek(file, 0L, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    printf("Uploading %s (%ld MB)\n", file_path, file_size / (1 << 20));

    free(response.data);
    response.data = NULL;
    response.size = 0;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, upload_url->valuestring);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, __lumidb_read_callback);
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_size);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __lumidb_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (res != CURLE_OK) {
        printf("upload failed: %s\n", curl_easy_strerror(res));
        ret = LUMIDB_ERROR_CURL;
        goto cleanup;
    } else {
        if (response_code != 200) {
            ret = LUMIDB_ERROR_HTTP;
            goto cleanup;
        }
    }

    ret = LUMIDB_SUCCESS;

cleanup:
    if (json) cJSON_Delete(json);
    if (response.data) free(response.data);
    if (file) fclose(file);
    if (curl) curl_easy_cleanup(curl);

    if (ret && *lumidb_asset_id) {
        free(*lumidb_asset_id);
        *lumidb_asset_id = NULL;
    }

    return ret;
}

static inline char *lumidb_build_import_manifest(const char *table_name, const char *table_proj,
                                                 const LumiDBImportAsset *assets, int asset_count) {
    if (!table_name || !assets || asset_count < 1) {
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON *table_name_json = cJSON_CreateString(table_name);
    if (!table_name_json) {
        return NULL;
    }
    cJSON *table_proj_json = cJSON_CreateString(table_proj);
    if (!table_proj_json) {
        return NULL;
    }

    cJSON_AddItemToObject(root, "table_name", table_name_json);
    cJSON_AddItemToObject(root, "table_proj", table_proj_json);

    // Create inputs array
    cJSON *inputs_array = cJSON_CreateArray();
    if (!inputs_array) {
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < asset_count; i++) {
        cJSON *asset_obj = cJSON_CreateObject();
        if (!asset_obj) {
            cJSON_Delete(inputs_array);
            cJSON_Delete(root);
            return NULL;
        }

        // Add id field (from asset_id)
        if (assets[i].asset_id) {
            cJSON *id_json = cJSON_CreateString(assets[i].asset_id);
            if (!id_json) {
                cJSON_Delete(asset_obj);
                cJSON_Delete(inputs_array);
                cJSON_Delete(root);
                return NULL;
            }
            cJSON_AddItemToObject(asset_obj, "id", id_json);
        }

        // Add proj field
        if (assets[i].proj) {
            cJSON *proj_json = cJSON_CreateString(assets[i].proj);
            if (!proj_json) {
                cJSON_Delete(asset_obj);
                cJSON_Delete(inputs_array);
                cJSON_Delete(root);
                return NULL;
            }
            cJSON_AddItemToObject(asset_obj, "proj", proj_json);
        }

        // Add asset object to inputs array
        cJSON_AddItemToArray(inputs_array, asset_obj);
    }

    // Add inputs array to root object
    cJSON_AddItemToObject(root, "inputs", inputs_array);

    char *json_string = cJSON_Print(root);
    cJSON_Delete(root);
    return json_string;
}

static inline LumiDBError lumidb_start_import(LumiDB *db, char *manifest, char **table_id) {
    LumiDBError ret = LUMIDB_ERROR_GENERIC;
    cJSON *resp_json = NULL;
    cJSON *table_version = NULL;

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: apikey %s", db->api_key);
    struct curl_slist *req_headers = NULL;

    req_headers = curl_slist_append(req_headers, auth_header);
    req_headers = curl_slist_append(req_headers, "Content-Type: application/json");

    char upload_api_url[256];
    snprintf(upload_api_url, sizeof(upload_api_url), "%s%s", db->base_url, "api/tables/import");

    struct lumidb_response_data response = {0};
    CURL *curl = NULL;
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, upload_api_url);
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, manifest);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __lumidb_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    long response_code = -1;

    if (res != CURLE_OK) {
        // Return response data if requested
        printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        ret = LUMIDB_ERROR_CURL;
        goto cleanup;

    } else {
        // Get response code
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        printf("request finished with status: %ld\n", response_code);
    }

    resp_json = cJSON_Parse(response.data);

    if (!resp_json) {
        printf("Failed to parse JSON\n");
        ret = LUMIDB_ERROR_JSON_PARSE;
        goto cleanup;
    }

    table_version = cJSON_GetObjectItem(resp_json, "table_version");
    if (!table_version || !cJSON_IsString(table_version)) {
        printf("No table_version in response\n");
        ret = LUMIDB_ERROR_JSON_PARSE;
        goto cleanup;
    }

    *table_id = strdup(table_version->valuestring);

    printf("table_version: %s\n", table_version->valuestring);

    ret = LUMIDB_SUCCESS;

cleanup:
    if (resp_json) cJSON_Delete(resp_json);
    if (req_headers) curl_slist_free_all(req_headers);

    if (curl) curl_easy_cleanup(curl);
    if (response.data) free(response.data);

    return ret;
}

static inline LumiDBError lumidb_poll_import_status(LumiDB *db, char *table_id, int *done) {
    LumiDBError ret = LUMIDB_ERROR_GENERIC;
    struct curl_slist *req_headers = NULL;
    struct lumidb_response_data response = {0};
    cJSON *resp_json = NULL;
    cJSON *status = NULL;
    *done = 0;
    CURL *curl = NULL;
    curl = curl_easy_init();

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: apikey %s", db->api_key);
    req_headers = curl_slist_append(req_headers, auth_header);
    req_headers = curl_slist_append(req_headers, "Content-Type: application/json");

    char status_api_url[512];
    snprintf(status_api_url, sizeof(status_api_url), "%s%s%s", db->base_url,
             "api/tables/import_status/", table_id);

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, status_api_url);
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, req_headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, __lumidb_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    long response_code = -1;

    if (res != CURLE_OK) {
        // Return response data if requested
        printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        ret = LUMIDB_ERROR_CURL;
        goto cleanup;
    } else {
        // Get response code
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if (response_code != 200) {
            printf("request finished with status: %ld\n", response_code);
            ret = LUMIDB_ERROR_HTTP;
            goto cleanup;
        }
    }

    printf("response: %s\n", response.data);

    resp_json = cJSON_Parse(response.data);
    if (!resp_json) {
        printf("Failed to parse JSON\n");
        ret = LUMIDB_ERROR_JSON_PARSE;
        goto cleanup;
    }
    status = cJSON_GetObjectItem(resp_json, "status");
    if (!status || !cJSON_IsString(status)) {
        printf("No status in response\n");
        ret = LUMIDB_ERROR_JSON_PARSE;
        goto cleanup;
    }

    if (strcmp(status->valuestring, "ready") == 0) {
        *done = 1;
    }

    ret = LUMIDB_SUCCESS;

cleanup:
    if (curl) curl_easy_cleanup(curl);
    if (resp_json) cJSON_Delete(resp_json);
    if (response.data) free(response.data);
    if (req_headers) curl_slist_free_all(req_headers);

    return ret;
}

static inline const char *lumidb_error_string(LumiDBError error) {
    switch (error) {
        case LUMIDB_SUCCESS:
            return "Success";
        case LUMIDB_ERROR_GENERIC:
            return "Generic error";
        case LUMIDB_ERROR_CURL:
            return "Network/CURL error";
        case LUMIDB_ERROR_HTTP:
            return "HTTP error";
        case LUMIDB_ERROR_JSON_PARSE:
            return "JSON parsing error";
        case LUMIDB_ERROR_FILE_IO:
            return "File I/O error";
        default:
            return "Unknown error";
    }
}
