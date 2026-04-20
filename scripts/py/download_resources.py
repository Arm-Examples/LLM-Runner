#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
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
import os
import netrc

from argparse import ArgumentParser


def get_huggingface_token():
    """
    Method to get Hugging Face token from environment or .netrc file

    Returns:
    str or None: The Hugging Face token if found, otherwise None.
    """

    token = os.environ.get("HF_TOKEN")
    if token:
        return token

    try:
        # Expect the .netrc file to be located in the user's home directory
        netrc_filepath = os.path.expanduser("~/.netrc")
        auths = netrc.netrc(netrc_filepath).authenticators("huggingface.co")
        if auths:
            _, _, token = auths
            return token
    except FileNotFoundError:
        logging.warning(".netrc file not found.")
    except netrc.NetrcParseError as e:
        logging.error(f"Failed to parse .netrc: {e}")
    except Exception as e:
        logging.error(f"Unexpected error reading .netrc: {e}")

    return None

def download_file(url: str, dest: Path,huggingface_token:str =None) -> None:
    """
    Download a file

    @param url:              The URL of the file to download
    @param dest:             The destination of downloaded file
    @param huggingface_token The access token from users' Hugging Face account
                             This token is optional and only used to download models
    """
    try:
        req = urllib.request.Request(url)

        if huggingface_token:
            req.add_header("Authorization", f"Bearer {huggingface_token}")

        with urllib.request.urlopen(req) as g:
            with open(dest, "b+w") as f:
                f.write(g.read())
                logging.info("Downloaded %s to %s.", url, dest)
    except urllib.error.HTTPError as e:
         if e.code == 403:
                logging.error( f"Access denied (403) while downloading from {url}.\n"
                    f"This may be a gated model. Please ensure your HF token is correct "
                    f"and you have accepted the license terms on the model's Hugging Face page.")
         elif e.code == 401:
                logging.error(f"Unauthorized (401) while accessing {url}.\n"
                    f"Your Hugging Face token may be invalid or expired.")
         else:
                logging.error(f"HTTPError {e.code} while downloading {url}: {e.reason}")


    except urllib.error.URLError as e:
        logging.error(f"URLError while downloading {url}: {e.reason}")


    except Exception as e:
        logging.error(f"Unexpected error while downloading {url}: {str(e)}")



def validate_download(filepath, expected_hash):
    """
    Validate downloaded file against expected hash

    @param filepath:       The path to downloaded file
    @param expected_hash:  Expected sha256sum
    """
    sha256_hash = hashlib.sha256()
    try :
        with open(filepath, "rb") as f:
            for byte_block in iter(lambda: f.read(4096), b""):
                sha256_hash.update(byte_block)
        actual_hash = sha256_hash.hexdigest()
        return actual_hash == expected_hash
    except FileNotFoundError:
        logging.warning(f"{filepath} is not downloaded")
        return False


def download_resources(resources_file: Path, download_dir: Path,huggingface_token:str=None) -> None:
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
        resources = []
        for resource_type in resource_list:
            resource_dir = Path(download_dir / resource_type)
            resource_dir.mkdir(exist_ok=True)

            if resource_type == "models":
                model_resources = resource_list[resource_type][llm_framework]
                for model_name in model_resources:
                    model_dir = Path(resource_dir / llm_framework / model_name)
                    model_dir.mkdir(parents=True, exist_ok=True)
                    resources.extend(model_resources[model_name])
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
                    if resource_type == "models":
                       download_file(url, dest,huggingface_token)
                    else :
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
        choices=["llama.cpp", "mediapipe", "onnxruntime-genai", "mnn", "executorch"],
        default=default_llm_framework)

    args = parser.parse_args()
    req_file = Path(args.requirements_file)
    download_dir = Path(args.download_dir)
    llm_framework = args.llm_framework
    hf_token = get_huggingface_token()
    if not hf_token:
        logging.error("HF_TOKEN is not set in the environment")

    if not req_file.exists():
        raise FileNotFoundError(f'{req_file} does not exist')

    download_resources(req_file, download_dir,hf_token)
