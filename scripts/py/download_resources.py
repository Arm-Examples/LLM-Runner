#
# SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
import json
import hashlib
from pathlib import Path
from urllib.error import URLError
import urllib.request
import logging
import sys
from argparse import ArgumentParser

def download_file(url: str, dest: Path) -> None:
    """
    Download a file

    @param url:     The URL of the file to download
    @param dest:    The destination of downloaded file
    """
    try:
        with urllib.request.urlopen(url) as g:
            with open(dest, "b+w") as f:
                f.write(g.read())
                logging.info("Downloaded %s to %s.", url, dest)
    except URLError:
        logging.error("URLError while downloading %s.", url)
        raise


def validate_download(filepath, expected_hash):
    """
    Validate downloaded file against expected hash

    @param filepath:       The path to downloaded file
    @param expected_hash:  Expected sha256sum
"""
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    actual_hash = sha256_hash.hexdigest()
    return actual_hash == expected_hash


def download_resources(resources_file: Path, download_dir: Path) -> None:
    """
    Downloads resource files as per the resource file json into the
    download dir.

    @param resources_file:  Path to the resource file (JSON) to read URLs from
    @param download_dir:    Download location (parent directory) where files should
                            be placed.
    """
    download_dir.mkdir(exist_ok=True)
    with (open(resources_file, encoding="utf8") as f):
        resource_list = json.load(f)
        for resource_type in resource_list:
            resource_dir = Path(download_dir / resource_type)
            resource_dir.mkdir(exist_ok=True)

            if resource_type == "models":
                resources = resource_list[resource_type][llm_framework]
                model_dir = Path(resource_dir / llm_framework)
                model_dir.mkdir(exist_ok=True)
            else:
                resources = resource_list[resource_type]

            for resource_data in resources:
                logging.info(f'Name:      {resource_data["name"]}')
                logging.info(f'Purpose:   {resource_data["purpose"]}')
                logging.info(f'Dest:      {resource_data["destination"]}')
                logging.info(f'URL:       {resource_data["url"]}')
                logging.info(f'SHA256:    {resource_data["sha256sum"]}')

                url = resource_data['url']
                dest = resource_dir / resource_data['destination']

                if dest.exists():
                    logging.info(f'{dest} exists; skipping download')
                else:
                    logging.info(f'Downloading {url} -> {dest}')
                    download_file(url, dest)
                    if validate_download(dest, resource_data["sha256sum"]):
                        print("Validated successfully!")
                    else:
                        print("Did not validate sha256sum!")


current_file_dir = Path(__file__).parent.resolve()
default_requirements_file = current_file_dir / 'requirements.json'
default_download_location = current_file_dir / '..' / '..' / 'resources_downloaded'
default_llm_framework = "llama.cpp"


if __name__ == "__main__":
    logging.basicConfig(filename="download.log", level=logging.DEBUG)
    logging.getLogger().addHandler(logging.StreamHandler(sys.stdout))

    parser = ArgumentParser()
    parser.add_argument(
        "--requirements-file",
        help="Path to requirements file.",
        default=default_requirements_file)
    parser.add_argument(
        "--download-dir",
        help="Path to where resources should be downloaded.",
        default=default_download_location)
    parser.add_argument(
        "--llm-framework",
        help="LLM framework from which the model will be downloaded.",
        default=default_llm_framework)

    args = parser.parse_args()
    req_file = Path(args.requirements_file)
    download_dir = Path(args.download_dir)
    llm_framework = args.llm_framework

    if not req_file.exists():
        raise FileNotFoundError(f'{req_file} does not exist')

    download_resources(req_file, download_dir)
