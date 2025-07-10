# LumiDB SDK for C

A header-only library for interacting with LumiDB.

- **`lumidb_init(url, api_key)`**
    - Initialize a new LumiDB instance with base URL and API key. Return a handle that is passed to all other functions.
- **`lumidb_free(db)`**
    - Free memory allocated for a LumiDB instance.
- **`lumidb_upload_asset(db, file_path, asset_id)`**
    - Upload a file asset and return its asset ID.
- **`lumidb_build_import_manifest(table_name, table_proj, assets, asset_count)`**
    - Build JSON manifest for importing assets into a table.
- **`lumidb_start_import(db, manifest, table_id)`**
    - Start an import operation and return the table version ID.
- **`lumidb_poll_import_status(db, table_id, done)`**
    - Check if an import operation has completed.
- **`lumidb_error_string(error)`**
    - Convert error code to human-readable string.

The repository also contains an example program (`lumilapio.c`) that uploads assets, starts an import, and polls for import progress.

## Building

`lumidb.h` depends on two external libraries: `curl` and `cjson`.

### Linux / MacOS
```bash
# on ubuntu
sudo apt-get install -y build-essential libcurl4-openssl-dev libcjson-dev

# on macos
brew install pkg-config cjson # `curl` should be already installed.
```

Run the build with:
```bash
make
```

### Windows

You need to have Visual Studio Build Tools installed (for `cl.exe`).

1. Set up vcpkg
   ```bat
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   cd ..
   ```

2. Install deps
   ```bat
   .\vcpkg\vcpkg.exe install
   ```

3. Build
   ```bat
   :: cl.exe should be in %PATH%
   .\build.bat
   ```

## Using the example program

> [!NOTE]
LumiDB supports defining a separate projection for each input file, but the example program simplifies
things by assuming every input file to be in the same projection.

For authentication, you need to have your API key set in the environment variable `LUMIDB_API_KEY`.

```bash
    # Usage:
    #   lumilapio <lumidb_url> <table_name> <table_proj> <file1> [file2] ...

    # for example
    ./build/lumilapio https://api.lumidb.com new_york_2024 EPSG:3857 ./path/to/scan1.laz ./path/to/scan2.e57
```
