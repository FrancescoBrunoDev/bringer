#!/usr/bin/env python3
"""
Generate a C header with credential defines from a .env file.

Usage: run from repository root before building (CI can call this).
It reads `.env` and writes `src/generated_secrets.h`.
This script does not commit any files.
"""
import os

ENV_PATH = ".env"
OUT_PATH = os.path.join("src", "generated_secrets.h")

def parse_env(path):
    vals = {}
    if not os.path.exists(path):
        return vals
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            vals[k.strip()] = v.strip()
    return vals

def write_header(vals, outpath):
    lines = []
    lines.append("// generated_secrets.h - AUTO GENERATED, do not commit\n")
    lines.append("#ifndef GENERATED_SECRETS_H\n")
    lines.append("#define GENERATED_SECRETS_H\n\n")

    def define(key, macro):
        if key in vals and vals[key]:
            v = vals[key].replace('"', '\\"')
            lines.append(f'#define {macro} "{v}"\n')

    define("WIFI_SSID", "WIFI_SSID")
    define("WIFI_PASSWORD", "WIFI_PASSWORD")
    define("AP_SSID", "AP_SSID")
    define("AP_PASSWORD", "AP_PASSWORD")
    define("AP_IP_ADDRESS", "AP_IP_ADDRESS")
    define("BESZEL_TOKEN", "BESZEL_TOKEN")

    lines.append("\n#endif // GENERATED_SECRETS_H\n")

    with open(outpath, "w", encoding="utf-8") as f:
        f.writelines(lines)

def main():
    vals = parse_env(ENV_PATH)
    write_header(vals, OUT_PATH)
    print(f"Wrote {OUT_PATH} from {ENV_PATH} (keys: {', '.join(sorted(vals.keys()))})")

if __name__ == "__main__":
    main()
