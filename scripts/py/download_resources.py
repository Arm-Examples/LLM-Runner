#
# SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#
import json
import hashlib
import netrc
import os
from pathlib import Path
from urllib.error import URLError
import shutil
import urllib.request
import logging
import sys
import os
import netrc

from argparse import ArgumentParser
from dataclasses import dataclass
from typing import Optional, Any, Dict

HF_TOKEN_ENV = "HF_TOKEN"
NETRC_MACHINE = "huggingface.co"
HF_INSTALL_HINT = "python -m pip install \"huggingface_hub>=0.20.0\""

DOWNLOAD_TIMEOUT_ENV = "STT_DOWNLOAD_TIMEOUT_SECONDS"
DEFAULT_DOWNLOAD_TIMEOUT_SECONDS = 60

SHA256_HEX_LENGTH = 64
SHA256_HEX_ALPHABET = "0123456789abcdef"
SHA256_PREFIX = "sha256:"

def is_sha256_hex_digest(value: str) -> bool:
    """
    Check if value is a hex-encoded SHA256 digest.

    @param value: String to validate (quotes/whitespace are tolerated).
    @return: True if value looks like a valid SHA256 hex digest, otherwise False.
    """
    if not isinstance(value, str):
        return False
    value = value.strip().lower().strip('"')
    # SHA256 is 32 bytes. Hex encoding uses 2 characters per byte => 64 characters.
    # Validate that all characters are valid hex digits.
    return len(value) == SHA256_HEX_LENGTH and all(c in SHA256_HEX_ALPHABET for c in value)


def is_huggingface_hub_available() -> bool:
    """
    Check if the `huggingface_hub` Python package is importable.
    @return: True if `huggingface_hub` can be imported, otherwise False.
    """
    try:
        import huggingface_hub  # noqa: F401 (imported for availability check)
        return True
    except ModuleNotFoundError:
        return False


def normalize_sha256_hex_digest(value: str):
    """
    Normalize a SHA256 hex digest from config/metadata.

    Accepts strings that may be quoted, uppercased, or prefixed with "sha256:".

    @param value: Raw SHA256 string.
    @return: Normalized lowercase 64-hex digest, or None if value is falsy.
    """
    if not value:
        logging.warning("No SHA256 provided; skipping normalization.")
        return None
    if not isinstance(value, str):
        logging.warning(
            "Expected SHA256 as string, got %s; skipping normalization.",
            type(value).__name__,
        )
        return None
    normalized = value.strip().strip('"').strip().lower()
    if normalized.startswith(SHA256_PREFIX):
        normalized = normalized.split(":", 1)[1]
    return normalized


def get_huggingface_hub_error_types():
    """
    Best-effort lookup for Hugging Face Hub exception types.

    This stays lazy to avoid hard requirement for `huggingface_hub` just to import this script.
    """
    try:
        from huggingface_hub.utils import (
            GatedRepoError,
            HfHubHTTPError,
            HFValidationError,
            RepositoryNotFoundError,
            RevisionNotFoundError,
        )
        return {
            "repo_not_found": RepositoryNotFoundError,
            "revision_not_found": RevisionNotFoundError,
            "gated_repo": GatedRepoError,
            "http_error": HfHubHTTPError,
            "validation_error": HFValidationError,
        }
    except Exception:
        return {}


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


def get_hf_remote_sha256(repo_id: str, filename: str, revision: str, huggingface_token: str = None):
    """
    Best-effort remote SHA256 fetch from Hugging Face Hub.

    Prefer the Git-LFS sha256 when available. Some Hub versions may expose the
    digest as `lfs.sha256` instead of `lfs.oid`.

    @param repo_id:            The Hugging Face repo id.
    @param filename:           The filename in the repo.
    @param revision:           The revision (branch/tag/commit).
    @param huggingface_token:  Optional token for gated/private models.

    @return: str or None: Lowercase SHA256 digest if available, otherwise None.
    """
    try:
        from huggingface_hub import HfApi
    except ModuleNotFoundError as e:
        logging.warning("huggingface_hub is not installed; cannot fetch remote SHA256 (%s).", e)
        return None

    api = HfApi(token=huggingface_token)
    try:
        info = api.model_info(repo_id=repo_id, revision=revision, files_metadata=True)
    except Exception as e:
        error_types = get_huggingface_hub_error_types()
        if error_types.get("repo_not_found") is not None and isinstance(e, error_types["repo_not_found"]):
            logging.warning("Hugging Face repo not found: %s", repo_id)
        elif error_types.get("revision_not_found") is not None and isinstance(e, error_types["revision_not_found"]):
            logging.warning("Hugging Face revision not found for %s: %s", repo_id, revision)
        elif error_types.get("gated_repo") is not None and isinstance(e, error_types["gated_repo"]):
            logging.warning(
                "Hugging Face repo is gated/private: %s (set HF_TOKEN or ~/.netrc credentials).",
                repo_id,
            )
        elif error_types.get("validation_error") is not None and isinstance(e, error_types["validation_error"]):
            logging.warning("Invalid Hugging Face repo_id/revision for %s@%s.", repo_id, revision)
        elif error_types.get("http_error") is not None and isinstance(e, error_types["http_error"]):
            status_code = None
            try:
                status_code = e.response.status_code  # type: ignore[attr-defined]
            except Exception:
                pass
            if status_code is not None:
                logging.warning(
                    "Hugging Face HTTP error (%s) while fetching metadata for %s@%s:%s.",
                    status_code,
                    repo_id,
                    revision,
                    filename,
                )
            else:
                logging.warning(
                    "Hugging Face HTTP error while fetching metadata for %s@%s:%s.",
                    repo_id,
                    revision,
                    filename,
                )
        else:
            logging.warning(
                "Failed to fetch Hugging Face metadata for %s@%s:%s (%s).",
                repo_id,
                revision,
                filename,
                type(e).__name__,
            )
        return None

    for sibling in getattr(info, "siblings", []) or []:
        if getattr(sibling, "rfilename", None) != filename:
            continue

        lfs = getattr(sibling, "lfs", None)
        sha = getattr(lfs, "sha256", None) if lfs else None
        if is_sha256_hex_digest(sha):
            logging.debug("Remote SHA256 (lfs.sha256) for %s@%s:%s = %s", repo_id, revision, filename, sha.lower())
            return sha.lower()

        oid = getattr(lfs, "oid", None) if lfs else None
        if isinstance(oid, str) and oid.lower().startswith(SHA256_PREFIX):
            oid = oid.split(":", 1)[1]
        if is_sha256_hex_digest(oid):
            logging.debug("Remote SHA256 (lfs.oid) for %s@%s:%s = %s", repo_id, revision, filename, oid.lower())
            return oid.lower()

    logging.warning("Could not determine remote SHA256 for %s@%s:%s.", repo_id, revision, filename)
    return None


def download_hf_file(repo_id: str, filename: str, revision: str, dest: Path, huggingface_token: str = None) -> None:
    """
    Download a file from Hugging Face Hub into dest.

    Uses the HF Hub cache and then copies the cached artifact into the project
    destination path.

    @param repo_id:            The Hugging Face repo id.
    @param filename:           The filename in the repo.
    @param revision:           The revision (branch/tag/commit).
    @param dest:               The destination of downloaded file.
    @param huggingface_token:  Optional token for gated/private models.
    """
    try:
        from huggingface_hub import hf_hub_download
    except ModuleNotFoundError as e:
        raise ModuleNotFoundError(
            "huggingface_hub is not installed; cannot download model resources. "
            f"Install it with: {HF_INSTALL_HINT}"
        ) from e

    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp_dest = Path(str(dest) + ".tmp")
    try:
        cached_path = hf_hub_download(
            repo_id=repo_id,
            filename=filename,
            revision=revision,
            token=huggingface_token,
        )
        shutil.copyfile(cached_path, tmp_dest)
        tmp_dest.replace(dest)
        logging.info(f"Downloaded {repo_id}@{revision}:{filename} (Hub) -> {dest}.")
    finally:
        try:
            if tmp_dest.exists():
                tmp_dest.unlink()
        except Exception:
            pass


def download_file(url: str, dest: Path, huggingface_token:str = None) -> None:
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
            with open(dest, "wb") as f:
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


@dataclass(frozen=True)
class ResourceDetails:
    resource_type: str
    name: str
    purpose: str
    destination: str
    dest: Path
    expected_sha256: Optional[str]
    url: Optional[str]
    repo_id: Optional[str]
    filename: Optional[str]
    revision: str
    huggingface_token: Optional[str]

    @property
    def is_hf(self) -> bool:
        return bool(self.repo_id and self.filename)


def extract_resource_details(
        resource_type: str,
        resource_data: Dict[str, Any],
        download_dir: Path,
        hf_token: Optional[str],
) -> ResourceDetails:
    """
    Extract resource details from a requirements.json entry

    @param resource_type: Group key in requirements.json (e.g. "models")
    @param resource_data: Resource entry dictionary
    @param download_dir: Base download directory
    @param hf_token: Hugging Face token (optional)
    @return: ResourceDetails instance
    """
    name = resource_data["name"]
    purpose = resource_data["purpose"]
    destination = resource_data["destination"]

    resource_dir = Path(download_dir / resource_type)
    dest = resource_dir / destination

    expected_sha256 = normalize_sha256_hex_digest(resource_data.get("sha256sum"))
    url = resource_data.get("url")
    repo_id = resource_data.get("repo_id")
    filename = resource_data.get("filename")
    revision = resource_data.get("revision", "main")

    token = hf_token if resource_type == "models" else None

    return ResourceDetails(
        resource_type=resource_type,
        name=name,
        purpose=purpose,
        destination=destination,
        dest=dest,
        expected_sha256=expected_sha256,
        url=url,
        repo_id=repo_id,
        filename=filename,
        revision=revision,
        huggingface_token=token,
    )


def log_resource_details(resource: ResourceDetails) -> None:
    """
    Log resource details

    @param resource: ResourceDetails instance
    """
    logging.info(f"Name:    {resource.name}")
    logging.info(f"Purpose: {resource.purpose}")
    logging.info(f"Dest:    {resource.destination}")

    if resource.is_hf:
        logging.info(f"Repo:    {resource.repo_id}")
        logging.info(f"File:    {resource.filename}")
        logging.info(f"Rev:     {resource.revision}")
    elif resource.url:
        logging.info(f"URL:     {resource.url}")

    if resource.expected_sha256:
        logging.info(f"SHA256:  {resource.expected_sha256}")


def require_hf_repo_and_filename(resource: ResourceDetails) -> tuple[str, str]:
    """
    Get required Hugging Face fields for HF resources

    @param resource: ResourceDetails instance
    @return: (repo_id, filename)
    """
    # HF resources must define both repo_id and filename; enforce this instead of using type ignores.
    if not resource.repo_id or not resource.filename:
        raise KeyError(f"HF resource entry missing 'repo_id'/'filename': {resource}")
    return resource.repo_id, resource.filename


def get_sha256_candidate(resource: ResourceDetails) -> str:
    """
    Determine the candidate SHA256 to validate against for this resource

    @param resource: ResourceDetails instance
    @return: Candidate SHA256 hex digest (64 lowercase hex chars)
    """
    expected_sha256 = resource.expected_sha256
    if expected_sha256 and not is_sha256_hex_digest(expected_sha256):
        raise ValueError(
            f"Invalid sha256sum in requirements.json for {resource.resource_type}/{resource.destination}: "
            f"{resource.expected_sha256}"
        )

    if resource.is_hf:
        repo_id, filename = require_hf_repo_and_filename(resource)
        if not is_huggingface_hub_available():
            raise ModuleNotFoundError(
                "huggingface_hub is not installed; cannot download model resources "
                f"({repo_id}@{resource.revision}:{filename})."
                f" Install it with: {HF_INSTALL_HINT}"
            )

        remote_sha256 = get_hf_remote_sha256(
            repo_id=repo_id,
            filename=filename,
            revision=resource.revision,
            huggingface_token=resource.huggingface_token,
        )

        if remote_sha256 is None and not expected_sha256:
            raise ValueError(
                f"Could not determine remote SHA256 for {repo_id}@{resource.revision}:{filename}"
            )

        if remote_sha256 is None and expected_sha256:
            logging.warning(
                "Could not fetch remote SHA256 for %s@%s:%s; falling back to sha256sum provided in requirements.json .",
                repo_id,
                resource.revision,
                filename,
            )

        if remote_sha256 and expected_sha256 and remote_sha256.lower() != expected_sha256.lower():
            raise ValueError(
                f"requirements.json sha256sum does not match Hub SHA256 for {repo_id}@{resource.revision}:{filename} "
                f"(requirements.json={expected_sha256}, hub={remote_sha256}). Pin revision or update sha256sum."
            )

        candidate = remote_sha256 or expected_sha256
        if not candidate or not is_sha256_hex_digest(candidate):
            raise ValueError(f"Invalid SHA256 candidate for {repo_id}@{resource.revision}:{filename}")
        return candidate

    if not resource.url:
        raise KeyError(f"Resource entry missing 'url' (or 'repo_id'/'filename'): {resource}")
    if not expected_sha256:
        raise ValueError(f"Missing sha256sum for URL resource: {resource.url}")
    return expected_sha256

def download_resource(resource: ResourceDetails) -> None:
    """
    Download a resource to its destination path

    @param resource: ResourceDetails instance
    """
    if resource.is_hf:
        repo_id, filename = require_hf_repo_and_filename(resource)
        download_hf_file(
            repo_id=repo_id,
            filename=filename,
            revision=resource.revision,
            dest=resource.dest,
            huggingface_token=resource.huggingface_token,
        )
        return

    if not resource.url:
        raise KeyError(f"Resource entry missing 'url' (or 'repo_id'/'filename'): {resource}")
    download_file(resource.url, resource.dest)

def ensure_resource_present(resource: ResourceDetails) -> None:
    """
    Ensure a resource exists on disk and validates against its candidate SHA256

    If the destination exists and validates, skip download. Otherwise download and
    validate, retrying once if validation fails.

    @param resource: ResourceDetails instance
    """
    candidate_sha256 = get_sha256_candidate(resource)

    if resource.dest.exists() and validate_download(resource.dest, candidate_sha256):
        logging.info(f"{resource.dest} exists and validates; skipping download")
        if resource.resource_type == "models":
            print(f"Model '{resource.name}' already present; validated successfully.")
        return

    if resource.dest.exists():
        logging.warning(f"{resource.dest} exists but does not validate; re-downloading")

    if resource.resource_type == "models" and resource.is_hf:
        print(f"Downloading model '{resource.name}'...")

    for attempt in (1, 2):
        if resource.is_hf:
            repo_id, filename = require_hf_repo_and_filename(resource)
            logging.info(f"Downloading {repo_id}@{resource.revision}:{filename} -> {resource.dest}")
        else:
            logging.info(f"Downloading {resource.url} -> {resource.dest}")

        download_resource(resource)

        if validate_download(resource.dest, candidate_sha256):
            if resource.resource_type == "models":
                print(f"Model '{resource.name}': validated successfully.")
            return

        logging.warning("Download validation failed for %s (attempt %s/2).", resource.dest, attempt)

    try:
        resource.dest.unlink(missing_ok=True)
    except Exception:
        pass
    raise ValueError(f"Did not validate sha256sum for {resource.dest}")


def download_resources(resources_file: Path, download_dir: Path,
                       download_models: bool = True,
                       hf_token: Optional[str] = None) -> None:
    """
    Downloads resource files as per the requirements json into the download dir.

    Integrity: Enforces SHA256 verification end-to-end (URL entries require sha256sum;
    HF entries use Hub lfs.sha256 and, when provided, must match the manifest sha256sum;
    the downloaded file is always hashed locally and removed on mismatch).

    @param resources_file: Path to the requirements file (JSON) to read URLs/repo ids from
    @param download_dir:   Download location (parent directory) where files should be placed.
    @param download_models: Whether to download models from the requirements file.
    """
    download_dir.mkdir(parents=True, exist_ok=True)
    with (open(resources_file, encoding="utf8") as f):
        resource_list = json.load(f)
        resources = []
        for resource_type in resource_list:
            resource_dir = Path(download_dir / resource_type)
            resource_dir.mkdir(exist_ok=True)

            if resource_type == "models":
                if not download_models:
                    logging.info("Skipping model downloads (download_models=False)")
                    continue
                model_resources = resource_list[resource_type][llm_framework]
                for model_name in model_resources:
                    model_dir = Path(resource_dir / llm_framework / model_name)
                    model_dir.mkdir(parents=True, exist_ok=True)
                    resources.extend(model_resources[model_name])
            else:
                resources = resource_list[resource_type]

            for resource_data in resources:
                resource = extract_resource_details(resource_type, resource_data, download_dir, hf_token)
                log_resource_details(resource)
                ensure_resource_present(resource)


current_file_dir = Path(__file__).parent.resolve()
default_requirements_file = current_file_dir / 'requirements.json'
default_download_location = current_file_dir / '..' / '..' / 'resources_downloaded'
default_llm_framework = "llama.cpp"


if __name__ == "__main__":
    logging.basicConfig(filename="download.log", level=logging.DEBUG)
    console_handler = logging.StreamHandler(sys.stdout)
    # Keep CMake configure output readable; detailed logs go to download.log.
    console_handler.setLevel(logging.WARNING)
    logging.getLogger().addHandler(console_handler)

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
        choices=["llama.cpp", "mediapipe", "onnxruntime-genai", "mnn"],
        default=None)
    parser.add_argument(
        "--download-models",
        help="Whether to download LLM models (ON/OFF).",
        choices=["ON", "OFF"],
        default="ON")
    args = parser.parse_args()
    req_file = Path(args.requirements_file)
    download_dir = Path(args.download_dir)
    llm_framework = args.llm_framework
    download_models = args.download_models == "ON"
    hf_token = None
    if download_models:
        hf_token = get_huggingface_token()
        if not hf_token:
            logging.error("HF_TOKEN is not set in the environment")

    if not req_file.exists():
        raise FileNotFoundError(f'{req_file} does not exist')

    download_resources(
        req_file,
        download_dir,
        download_models,
        hf_token)
