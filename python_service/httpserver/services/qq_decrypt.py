"""QQ NT (Android NTQQ) database decryption.

Android QQ 9.x stores chat data under ``db/nt_db/nt_qq_<path_hash>/`` as
SQLCipher-4 databases. Every file starts with a 1024-byte custom header::

    "SQLite header 3\\0" ... "QQ_NT DB" <len32> <protobuf: rand, version, hmac_algo, ts>

Offline key derivation (verified against the QQBackup/QQDecrypt docs):

1. ``nt_uid``        e.g. ``u_UJEcIMaQtYqv4WxT5Bl30Q`` (from ``f/uid/<uin>###u_xxx``
                     file names or the ``gpro_v1-6_u_<nt_uid>.db`` file name)
2. ``uid_hash``      = md5(nt_uid)
3. ``path_hash``     = md5(uid_hash + "nt_kernel")   (== the ``nt_qq_<path_hash>`` dir)
4. ``rand``          = header protobuf field2 (8-char ASCII, per database)
5. ``key``           = md5(uid_hash + rand)          (32-hex, passed as a string)

Cipher parameters (sweep-verified with sqlcipher): page size 4096, kdf_iter
4000, HMAC_SHA1, PBKDF2_HMAC_SHA512. The 1024-byte header must be stripped
before handing the file to sqlcipher; an accompanying ``-wal`` is replayed
transparently by sqlcipher itself.
"""

from __future__ import annotations

import hashlib
import logging
import os
import shutil
import sqlite3
from typing import Any, Dict, Optional

logger = logging.getLogger(__name__)

QQ_NT_HEADER = b"QQ_NT DB"
HEADER_SIZE = 1024
SQLITE_MAGIC = b"SQLite format 3\x00"
# QQ marks the custom header with a *non-standard* magic so the encrypted
# page layout stays intact:
QQ_SQLITE_MAGIC = b"SQLite header 3\x00"


class QQDecryptError(Exception):
    """Raised when a QQ database cannot be decrypted."""


def _sqlcipher():
    try:
        import sqlcipher3  # type: ignore

        return sqlcipher3
    except ImportError as e:  # pragma: no cover - depends on environment
        raise QQDecryptError(
            "sqlcipher3 is not installed; run `pip install sqlcipher3-binary`"
        ) from e


# ---------------------------------------------------------------------- #
# key derivation
# ---------------------------------------------------------------------- #

def uid_hash(nt_uid: str) -> str:
    """md5(nt_uid) — the QQ_UID_hash."""
    return hashlib.md5(nt_uid.encode("utf-8")).hexdigest()


def path_hash(nt_uid: str) -> str:
    """md5(md5(nt_uid) + 'nt_kernel') — the nt_qq_<path_hash> dir name."""
    return hashlib.md5((uid_hash(nt_uid) + "nt_kernel").encode("utf-8")).hexdigest()


def derive_key(nt_uid: str, rand: str) -> str:
    """Database key = md5(md5(nt_uid) + rand), returned as 32-char hex."""
    return hashlib.md5((uid_hash(nt_uid) + rand).encode("utf-8")).hexdigest()


# ---------------------------------------------------------------------- #
# header parsing
# ---------------------------------------------------------------------- #

def _read_varint(data: bytes, pos: int) -> tuple:
    value, shift = 0, 0
    while True:
        byte = data[pos]
        pos += 1
        value |= (byte & 0x7F) << shift
        shift += 7
        if not byte & 0x80:
            return value, pos


def parse_header(data: bytes) -> Dict[str, Any]:
    """Parse the 1024-byte QQ_NT DB header.

    Returns ``{"rand", "version", "hmac_algorithm", "timestamp", "present"}``.
    ``rand`` is empty when the file has no custom header (already plaintext).
    """
    info: Dict[str, Any] = {
        "present": False,
        "rand": "",
        "version": "",
        "hmac_algorithm": "",
        "timestamp": 0,
    }
    idx = data[:HEADER_SIZE].find(QQ_NT_HEADER)
    if idx < 0:
        return info
    info["present"] = True
    pos = idx + len(QQ_NT_HEADER)
    # 4-byte little-endian payload length follows the tag
    if pos + 4 <= len(data):
        pos += 4
    end = min(len(data), HEADER_SIZE)
    while pos < end:
        try:
            tag, pos = _read_varint(data, pos)
        except (IndexError, ValueError):
            break
        field, wire = tag >> 3, tag & 7
        if wire == 2:
            try:
                length, pos = _read_varint(data, pos)
            except (IndexError, ValueError):
                break
            chunk = data[pos : pos + length]
            pos += length
            text = chunk.decode("utf-8", "replace")
            if field == 2:
                info["rand"] = text
            elif field == 3:
                info["version"] = text
            elif field == 4:
                info["hmac_algorithm"] = text
        elif wire == 0:
            try:
                value, pos = _read_varint(data, pos)
            except (IndexError, ValueError):
                break
            if field == 5:
                info["timestamp"] = value
        else:
            break
    return info


def looks_encrypted(data: bytes) -> bool:
    """True when the file carries the QQ_NT DB custom header."""
    return QQ_NT_HEADER in data[:HEADER_SIZE]


# ---------------------------------------------------------------------- #
# decryption
# ---------------------------------------------------------------------- #

def _connect_encrypted(work_db: str, key: str, hmac_algorithm: str = "HMAC_SHA1"):
    sc = _sqlcipher()
    conn = sc.connect(work_db)
    # order matters: cipher_page_size before key
    conn.execute("PRAGMA cipher_page_size = 4096;")
    conn.execute(f"PRAGMA key = '{key}';")
    conn.execute("PRAGMA kdf_iter = 4000;")
    conn.execute(f"PRAGMA cipher_hmac_algorithm = {hmac_algorithm};")
    conn.execute("PRAGMA cipher_kdf_algorithm = PBKDF2_HMAC_SHA512;")
    return conn


def verify_key(db_path: str, key: str) -> bool:
    """Check whether ``key`` decrypts ``db_path`` (QQ NT encrypted or plain)."""
    head = open(db_path, "rb").read(HEADER_SIZE)
    if not looks_encrypted(head):
        # plaintext SQLite needs no key
        return head[:16] in (SQLITE_MAGIC, QQ_SQLITE_MAGIC)
    tmp = db_path + ".keytest"
    try:
        with open(tmp, "wb") as f:
            f.write(open(db_path, "rb").read()[HEADER_SIZE:])
        hmac_algo = parse_header(head).get("hmac_algorithm") or "HMAC_SHA1"
        conn = _connect_encrypted(tmp, key, hmac_algo)
        try:
            conn.execute("SELECT count(*) FROM sqlite_master").fetchone()
            return True
        except Exception:
            return False
        finally:
            conn.close()
    finally:
        for p in (tmp, tmp + "-wal", tmp + "-shm", tmp + "-journal"):
            if os.path.exists(p):
                os.remove(p)


def decrypt_file(
    src_db: str,
    dst_db: str,
    key: str,
    wal_path: Optional[str] = None,
) -> Dict[str, Any]:
    """Decrypt one QQ NT database to a plaintext SQLite file.

    - strips the 1024-byte custom header into a working copy
    - copies the ``-wal`` next to it so sqlcipher replays it
    - exports plaintext via ``sqlcipher_export``

    Returns stats: ``{"pages", "header", "wal"}``.
    """
    sc = _sqlcipher()
    if not os.path.isfile(src_db):
        raise QQDecryptError(f"database not found: {src_db}")
    head = open(src_db, "rb").read(HEADER_SIZE)

    # Already plaintext? Just copy through.
    if not looks_encrypted(head):
        if head[:16] not in (SQLITE_MAGIC, QQ_SQLITE_MAGIC):
            raise QQDecryptError("not a QQ NT database (unknown magic)")
        shutil.copy2(src_db, dst_db)
        return {"pages": os.path.getsize(dst_db) // 4096, "header": None, "wal": None}

    info = parse_header(head)
    hmac_algo = info.get("hmac_algorithm") or "HMAC_SHA1"
    if os.path.exists(dst_db):
        os.remove(dst_db)

    work_dir = os.path.dirname(dst_db) or "."
    work_db = os.path.join(work_dir, f".qq_work_{os.path.basename(src_db)}")
    wal_copy = None
    try:
        with open(src_db, "rb") as f:
            f.seek(HEADER_SIZE)
            with open(work_db, "wb") as out:
                while True:
                    chunk = f.read(4 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
        src_wal = wal_path or (src_db + "-wal")
        if wal_path is None and not os.path.isfile(src_wal):
            src_wal = None
        if src_wal and os.path.isfile(src_wal):
            wal_copy = work_db + "-wal"
            shutil.copy2(src_wal, wal_copy)

        conn = _connect_encrypted(work_db, key, hmac_algo)
        try:
            conn.execute("SELECT count(*) FROM sqlite_master").fetchone()
            conn.execute("ATTACH DATABASE ? AS plaintext KEY '';", (dst_db,))
            conn.execute("SELECT sqlcipher_export('plaintext');")
            conn.execute("DETACH DATABASE plaintext;")
        finally:
            conn.close()
    except sc.DatabaseError as e:  # type: ignore[attr-defined]
        if os.path.exists(dst_db):
            os.remove(dst_db)
        raise QQDecryptError(f"wrong key or corrupt database: {e}") from e
    finally:
        for p in (work_db, work_db + "-wal", work_db + "-shm", work_db + "-journal"):
            if os.path.exists(p):
                os.remove(p)

    stats = {
        "pages": os.path.getsize(dst_db) // 4096,
        "header": {
            "rand": info.get("rand", ""),
            "version": info.get("version", ""),
            "hmac_algorithm": hmac_algo,
            "timestamp": info.get("timestamp", 0),
        },
        "wal": {"present": bool(src_wal)} if src_wal else None,
    }
    return stats


def verify_db(db_path: str) -> Dict[str, Any]:
    """Run integrity_check + table count on a plaintext database."""
    try:
        conn = sqlite3.connect(db_path)
        try:
            status = conn.execute("PRAGMA integrity_check").fetchone()[0]
            tables = conn.execute(
                "SELECT count(*) FROM sqlite_master WHERE type='table'"
            ).fetchone()[0]
            return {"integrity": "ok" if status == "ok" else status, "tables": tables}
        finally:
            conn.close()
    except sqlite3.Error as e:
        return {"integrity": f"error: {e}", "tables": 0}
