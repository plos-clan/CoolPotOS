#!/usr/bin/env python3
# tools/sign_module.py
# Usage: python3 sign_module.py <module_file> <private_key.pem>
#
# - Keeps the CLI the same as your build system expects.
# - Removes any existing trailing CPOS signature before signing.
# - Signs the raw module bytes with ECDSA P-256 (SHA256).
# - Appends packed signature header:
#     uint32_t magic;      // CPOS_SIG_MAGIC (we pack as little-endian "<I")
#     uint8_t  hash_algo;  // 1 = SHA256
#     uint8_t  sig_len;    // 64
#     uint8_t  reserved[2];
#     uint8_t  signature[64]; // r||s (big-endian each, 32+32)
#
import os
import sys
import struct
import hashlib

from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
from cryptography.hazmat.backends import default_backend

# Constants (match kernel side)
CPOS_SIG_MAGIC = 0x43504F53  # 'CPOS' as integer
HASH_SHA256 = 1
ECC_SIG_LEN = 64
SIG_HEADER_FMT = "<IBB2s"   # <I magic, B hash_algo, B sig_len, 2 bytes reserved>
SIG_HEADER_FIXED_LEN = struct.calcsize(SIG_HEADER_FMT)  # 8
SIG_TOTAL_LEN = SIG_HEADER_FIXED_LEN + ECC_SIG_LEN       # 72

# scan window (how many bytes from file end we search for existing magic)
SCAN_TAIL_BYTES = 16 * 1024


def load_private_key(path):
    with open(path, "rb") as f:
        data = f.read()
    try:
        return serialization.load_pem_private_key(data, password=None, backend=default_backend())
    except Exception:
        # Try to strip EC PARAMETERS block if present (compat with some openssl outputs)
        lines = data.splitlines()
        out = []
        skip = False
        for L in lines:
            if L.startswith(b"-----BEGIN EC PARAMETERS-----"):
                skip = True
                continue
            if L.startswith(b"-----END EC PARAMETERS-----"):
                skip = False
                continue
            if not skip:
                out.append(L)
        fixed = b"\n".join(out) + b"\n"
        return serialization.load_pem_private_key(fixed, password=None, backend=default_backend())


def find_last_magic_offset(path, magic_bytes):
    """Search backwards (within SCAN_TAIL_BYTES) for last occurrence of magic_bytes.
       Return file offset or None if not found."""
    st = os.stat(path)
    size = st.st_size
    if size < len(magic_bytes):
        return None
    to_read = min(size, SCAN_TAIL_BYTES)
    with open(path, "rb") as f:
        f.seek(size - to_read)
        tail = f.read(to_read)
    idx = tail.rfind(magic_bytes)
    if idx == -1:
        return None
    return (size - to_read + idx)


def strip_existing_signature(path):
    """If a CPOS signature header exists somewhere near the end, truncate file there.
       Returns True if we truncated, False otherwise."""
    # We will search for either little-endian or big-endian representation of magic,
    # because different prior scripts might have appended different byte-orders.
    magic_le = struct.pack("<I", CPOS_SIG_MAGIC)
    magic_be = struct.pack(">I", CPOS_SIG_MAGIC)

    off_le = find_last_magic_offset(path, magic_le)
    off_be = find_last_magic_offset(path, magic_be)

    # choose the later offset if both present
    candidates = [(off_le, magic_le), (off_be, magic_be)]
    candidates = [(o, m) for (o, m) in candidates if o is not None]
    if not candidates:
        return False

    # pick the maximum offset (last occurrence)
    off, mbytes = max(candidates, key=lambda x: x[0])
    # It's possible the magic occurs inside unrelated data; ensure there's enough room after it
    # for a full signature structure; if not, ignore it.
    st = os.stat(path)
    if st.st_size - off < SIG_TOTAL_LEN:
        # not enough bytes after magic -> ignore
        return False

    # Truncate file at magic offset (removes this and any trailing signatures)
    with open(path, "rb+") as f:
        f.truncate(off)
    return True


def write_debug_files(module_path, priv):
    # write pubraw and (later) sigraw from priv
    pub = priv.public_key()
    nums = pub.public_numbers()
    x = nums.x.to_bytes(32, "big")
    y = nums.y.to_bytes(32, "big")
    pubraw = x + y
    try:
        with open(module_path + ".pubraw", "wb") as g:
            g.write(pubraw)
    except Exception:
        pass


def sign_module(module_path, privkey_path, verbose=True):
    if not os.path.exists(module_path):
        print(f"[ERROR] module file not found: {module_path}")
        sys.exit(2)
    if not os.path.exists(privkey_path):
        print(f"[ERROR] private key not found: {privkey_path}")
        sys.exit(2)

    # load key
    priv = load_private_key(privkey_path)

    # remove existing signature (if present) to avoid double-append
    removed = strip_existing_signature(module_path)
    if verbose and removed:
        print("[SIGN] Existing signature removed from", module_path)

    # read clean module bytes
    with open(module_path, "rb") as f:
        module_data = f.read()

    # compute hash (for debug printing)
    module_hash = hashlib.sha256(module_data).digest()
    if verbose:
        print("[SIGN] module hash:", module_hash.hex())

    # Sign the raw module bytes (cryptography will hash internally with SHA256)
    der_sig = priv.sign(module_data, ec.ECDSA(hashes.SHA256()))

    # decode DER to r,s integers, then to 32-byte big-endian each
    r, s = decode_dss_signature(der_sig)
    r_bytes = r.to_bytes(32, "big")
    s_bytes = s.to_bytes(32, "big")
    raw_sig = r_bytes + s_bytes
    if verbose:
        print("[SIGN] r:", r_bytes.hex())
        print("[SIGN] s:", s_bytes.hex())

    # pack header (little-endian uint32 magic to match typical C packing with __attribute__((packed)))
    header = struct.pack(SIG_HEADER_FMT, CPOS_SIG_MAGIC, HASH_SHA256, ECC_SIG_LEN, b"\x00\x00")
    # header is 8 bytes, append signature 64 bytes -> total 72 bytes
    sig_struct = header + raw_sig

    # append to module file
    with open(module_path, "ab") as f:
        f.write(sig_struct)

    # dump debug files pubraw / sigraw (use private key's public component)
    try:
        pub = priv.public_key()
        nums = pub.public_numbers()
        pubraw = nums.x.to_bytes(32, "big") + nums.y.to_bytes(32, "big")
        with open(module_path + ".pubraw", "wb") as g:
            g.write(pubraw)
    except Exception:
        pass
    try:
        with open(module_path + ".sigraw", "wb") as g2:
            g2.write(raw_sig)
    except Exception:
        pass

    if verbose:
        print("[SIGN] Appended signature header to", module_path)
        print("[SIGN] pubraw written to", module_path + ".pubraw")
        print("[SIGN] sigraw written to", module_path + ".sigraw")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 sign_module.py <module_file> <private_key.pem>")
        sys.exit(1)
    sign_module(sys.argv[1], sys.argv[2])
