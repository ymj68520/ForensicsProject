"""WeChat (Android) SQLCipher database decryption.

Pure-Python page-level decryption that sidesteps the "file is not a
database" trap of stock SQLCipher builds: when the key is correct but the
HMAC/page parameters differ, SQLCipher reports the same error as a wrong
key. Here we PBKDF2-derive the key, AES-256-CBC decrypt page 1, and check
for SQLite page-header signatures instead — one check covers every HMAC
combination because the page payload layout never depends on HMAC config.

Verified against WeChat 8.0.76 (Android):
  scheme A — main DBs (EnMicroMsg.db etc.): 1024-byte pages,
             PBKDF2-HMAC-SHA1 x4000 -> 32-byte key, AES-256-CBC per page,
             no HMAC (SQLCipher v1 layout), IV = last 16 bytes of page,
             page 1 = 16-byte plaintext salt + ciphertext + IV.
  scheme B — FTS index DB: 4096-byte pages, PBKDF2-HMAC-SHA1 x64000,
             48-byte reserve (IV first 16 bytes + 32-byte digest).

Passwords are derived from backup key material:
  main DBs : MD5(IMEI + UIN)[:7]          (IMEI fixed "1234567890ABCDEF"
                                            when the device IMEI is absent)
  FTS DB   : MD5(UIN + IMEI + wxid)[:7]   (also stored in plaintext in
                                            mmkv ConfigStorage2)
"""

import hashlib
import os
import struct
from typing import Dict, List, Optional, Tuple

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# Fixed IMEI substitute used by WeChat when the real IMEI is unavailable.
FIXED_IMEI = "1234567890ABCDEF"

SCHEMES: Dict[str, Dict] = {
    "A": {"page": 1024, "kdf_iter": 4000, "kdf_hash": "sha1", "reserve": 16},
    "B": {"page": 4096, "kdf_iter": 64000, "kdf_hash": "sha1", "reserve": 48},
}

SQLITE_MAGIC = b"SQLite format 3\x00"


class WeChatDecryptError(Exception):
    """Raised when a database cannot be decrypted."""


def derive_candidates(uin: str, imei: str = FIXED_IMEI, wxid: str = "") -> List[Dict[str, str]]:
    """Return candidate passwords ordered by historical likelihood."""
    combos = [
        ("MD5(IMEI+UIN)[:7]", imei + uin),
        ("MD5(UIN+IMEI)[:7]", uin + imei),
    ]
    if wxid:
        combos.append(("MD5(UIN+IMEI+wxid)[:7]", uin + imei + wxid))
        combos.append(("MD5(IMEI+UIN+wxid)[:7]", imei + uin + wxid))
    return [
        {"formula": name, "password": hashlib.md5(text.encode()).hexdigest()[:7]}
        for name, text in combos
    ]


def _derive_key(password: bytes, salt: bytes, iters: int, kdf_hash: str) -> bytes:
    return hashlib.pbkdf2_hmac(kdf_hash, password, salt, iters, 32)


def _aes_cbc_decrypt(key: bytes, iv: bytes, data: bytes) -> bytes:
    d = Cipher(algorithms.AES(key), modes.CBC(iv)).decryptor()
    return d.update(data) + d.finalize()


def _is_sqlite_page(pt: bytes, page_size: int) -> bool:
    """SQLite page-header signature: big-endian page size, file format
    read/write versions in {1,2}, payload fractions 64/32/32."""
    return (
        int.from_bytes(pt[:2], "big") == page_size
        and pt[2] in (1, 2)
        and pt[3] in (1, 2)
        and pt[5:8] == bytes([64, 32, 32])
    )


def detect_scheme(db_path: str, password: bytes) -> Optional[Tuple[str, Dict]]:
    """Page-level detection: decrypt page 1 and match SQLite header.

    Returns the matching (scheme_name, params) or None. This never raises
    for a wrong password — a wrong password simply produces no hit.
    """
    try:
        with open(db_path, "rb") as f:
            header = f.read(4096)
    except OSError:
        return None
    if header[:16] == SQLITE_MAGIC:
        return ("plaintext", {"page": 0, "kdf_iter": 0, "kdf_hash": "", "reserve": 0})
    for name, s in SCHEMES.items():
        page_size = s["page"]
        if len(header) < page_size:
            continue
        salt = header[:16]
        key = _derive_key(password, salt, s["kdf_iter"], s["kdf_hash"])
        body = header[16:page_size]
        reserve = s["reserve"]
        ct, iv = body[:-reserve], body[-reserve:][:16]
        if not ct or len(ct) % 16:
            continue
        pt = _aes_cbc_decrypt(key, iv, ct)
        if _is_sqlite_page(pt, page_size):
            return (name, s)
    return None


def _wal_checksum(data: bytes, s0: int = 0, s1: int = 0, big_endian: bool = False):
    """SQLite WAL checksum (documented; magic 0x377f0682 => little-endian)."""
    fmt = ">" if big_endian else "<"
    n = len(data) // 8
    ints = struct.unpack(f"{fmt}{n * 2}I", data[: n * 8])
    for i in range(0, n * 2, 2):
        s0 = (s0 + ints[i] + s1) & 0xFFFFFFFF
        s1 = (s1 + ints[i + 1] + s0) & 0xFFFFFFFF
    return s0, s1


def decrypt_file(
    src: str,
    dst: str,
    password: bytes,
    wal_src: Optional[str] = None,
    scheme: Optional[str] = None,
) -> Dict:
    """Decrypt a WeChat SQLCipher database (and its WAL) into plaintext.

    Args:
        src: encrypted database path (a plaintext SQLite file is copied).
        dst: output path for the plaintext database.
        password: 7-char derived password (or any passphrase).
        wal_src: optional path to the encrypted ``-wal`` file; frames are
            decrypted and their checksum chain recomputed so a standard
            SQLite engine can replay them on open.
        scheme: force ``"A"``/``"B"``; otherwise page-level auto-detect.

    Returns:
        Dict with scheme, params, page count and WAL frame stats.

    Raises:
        WeChatDecryptError when the password/scheme does not match.
    """
    hit = detect_scheme(src, password)
    if scheme is not None and hit is not None and hit[0] not in (scheme, "plaintext"):
        raise WeChatDecryptError(
            f"scheme mismatch: auto-detected {hit[0]} but {scheme} requested"
        )
    if hit is None:
        raise WeChatDecryptError(
            "page-level detection failed: password or parameters do not match"
        )
    name, s = hit

    os.makedirs(os.path.dirname(os.path.abspath(dst)) or ".", exist_ok=True)
    if name == "plaintext":
        with open(src, "rb") as fin, open(dst, "wb") as fout:
            fout.write(fin.read())
        return {"scheme": "plaintext", "params": {}, "pages": 0, "wal_frames": {}}

    page_size, iters, kdf_hash, reserve = s["page"], s["kdf_iter"], s["kdf_hash"], s["reserve"]
    with open(src, "rb") as f:
        d = f.read()
    if len(d) < page_size or len(d) % page_size:
        raise WeChatDecryptError(f"db size {len(d)} is not a multiple of page {page_size}")

    key = _derive_key(password, d[:16], iters, kdf_hash)
    out = bytearray()
    pages = 0
    for off in range(0, len(d), page_size):
        chunk = d[off : off + page_size]
        body = chunk[16:] if off == 0 else chunk  # page 1: 16-byte plaintext salt
        ct, iv = body[:-reserve], body[-reserve:][:16]
        pt = _aes_cbc_decrypt(key, iv, ct)
        # pad the reserve region with zeros to form a full plaintext page
        pt += b"\x00" * (page_size - len(pt))
        if off == 0:
            # pt holds plaintext offsets 16..page_size; restore the header magic
            pt = SQLITE_MAGIC + pt[: page_size - 16]
        out += pt
        pages += 1
    with open(dst, "wb") as f:
        f.write(out)

    wal_stats: Dict[str, int] = {}
    if wal_src and os.path.exists(wal_src):
        wal_out, wal_stats = _decrypt_wal(wal_src, key, page_size, reserve)
        if wal_out:
            with open(dst + "-wal", "wb") as f:
                f.write(wal_out)
    return {"scheme": name, "params": s, "pages": pages, "wal": wal_stats}


def _decrypt_wal(
    wal_path: str, key: bytes, page_size: int, reserve: int
) -> Tuple[Optional[bytes], Dict[str, int]]:
    """Decrypt WAL frames and recompute the checksum chain.

    Frame header (24B, plaintext): page_no(4) + commit_size(4) + salt1(4) +
    salt2(4) + checksum(8). Frames whose salts differ from the WAL header
    salts are stale checkpoint generations and are skipped per the WAL
    protocol. Returns (plaintext WAL bytes or None, frame stats).
    """
    stats = {"total": 0, "kept": 0, "skipped": 0}
    with open(wal_path, "rb") as f:
        w = f.read()
    if len(w) < 32:
        return None, stats
    magic = int.from_bytes(w[0:4], "big")
    big_endian = bool(magic & 1)  # 0x377f0682 => little-endian checksums
    hdr_salt = w[16:24]
    frame_size = 24 + page_size
    n_frames = (len(w) - 32) // frame_size
    stats["total"] = n_frames
    out = bytearray(w[:32])
    s0 = s1 = 0
    kept = 0
    for i in range(n_frames):
        off = 32 + i * frame_size
        hdr, pdata = w[off : off + 24], w[off + 24 : off + frame_size]
        if hdr[8:16] != hdr_salt:
            stats["skipped"] += 1
            continue
        ct, iv = pdata[:-reserve], pdata[-reserve:][:16]
        pt = _aes_cbc_decrypt(key, iv, ct)
        pt += b"\x00" * (page_size - len(pt))
        s0, s1 = _wal_checksum(hdr[:8], s0, s1, big_endian)
        s0, s1 = _wal_checksum(pt, s0, s1, big_endian)
        out += hdr[:16] + struct.pack(">II", s0, s1) + pt
        kept += 1
        stats["kept"] += 1
    if not kept:
        return None, stats
    return bytes(out), stats


def merge_wal(db_path: str) -> None:
    """Checkpoint any ``-wal`` companion into the main database file."""
    import sqlite3

    con = sqlite3.connect(db_path)
    try:
        con.execute("PRAGMA wal_checkpoint(TRUNCATE)")
        con.execute("PRAGMA journal_mode=DELETE")
        con.commit()
    finally:
        con.close()
    for suffix in ("-wal", "-shm"):
        try:
            os.remove(db_path + suffix)
        except FileNotFoundError:
            pass


def verify_db(db_path: str) -> Dict:
    """Run integrity_check and count rows of key forensic tables."""
    import sqlite3

    result: Dict = {"path": db_path, "integrity": "unknown", "tables": 0}
    try:
        con = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    except sqlite3.Error as e:
        result["integrity"] = f"open error: {e}"
        return result
    try:
        result["integrity"] = con.execute("PRAGMA integrity_check").fetchone()[0]
        result["tables"] = con.execute(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table'"
        ).fetchone()[0]
        for table in ("message", "rcontact", "chatroom"):
            try:
                result[table] = con.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
            except sqlite3.Error:
                result[table] = None
    except sqlite3.Error as e:
        result["integrity"] = f"error: {e}"
    finally:
        con.close()
    return result
