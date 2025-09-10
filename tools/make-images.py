#!/usr/bin/env python3

# Standard library imports
import os
import shutil
import zipfile
import subprocess
import urllib.request

# Other library imports

# Constants
SWF_URL = "https://homestarrunner.com/media/pages/trogdor/15e6a71350-1660831052/trogdor.swf"
FFDEC_URL = "https://github.com/jindrapetrik/jpexs-decompiler/releases/download/version24.0.1/ffdec_24.0.1.zip"

# Main application
def main():
    os.makedirs("output", exist_ok=True)
    swf_path = os.path.join("output", "trogdor.swf")
    ffdec_zip = os.path.join("output", "ffdec.zip")
    ffdec_path = os.path.join("output", "ffdec")
    export_path = os.path.join("output", "export")
    java_exe = "java"

    print("Downloading trogdor.swf")
    urllib.request.urlretrieve(SWF_URL, swf_path)
    print("Downloading ffdec.zip")
    urllib.request.urlretrieve(FFDEC_URL, ffdec_zip)

    print("Unpacking ffdec.zip")
    with zipfile.ZipFile(ffdec_zip, 'r') as zip_obj:
        zip_obj.extractall(ffdec_path)

    print(f"Exporting {swf_path} to {export_path}")
    shutil.rmtree(export_path, ignore_errors=True)
    command = [
        java_exe,
        "-jar", os.path.join(ffdec_path, "ffdec-cli.jar"),
        "-export", "all", export_path, swf_path,
    ]
    subprocess.check_call(command)

if __name__ == "__main__":
    main()