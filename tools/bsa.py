"""Read Skyrim SE archives (.bsa, version 105): list files or pull some out. No BSArch needed.

  python -I tools/bsa.py list    <archive> [text]          paths containing the text
  python -I tools/bsa.py extract <archive> <path> <out file>
"""
import struct
import sys

import lz4.frame


def entries(path):
    """{lower-case path inside the archive: (offset, size, compressed)}"""
    with open(path, "rb") as f:
        d = f.read()
    magic, version, _, flags, folders, files, _, _, _ = struct.unpack_from("<4sIIIIIIIH", d, 0)
    assert magic == b"BSA\0" and version == 105, "not a Skyrim SE archive"
    compressed_default = bool(flags & 0x4)
    embed_names = bool(flags & 0x100)
    records = [struct.unpack_from("<QIIQ", d, 36 + i * 24) for i in range(folders)]
    pos = 36 + folders * 24
    folder_files = []
    for _, count, _, _ in records:
        n = d[pos]
        name = d[pos + 1:pos + n].decode("cp1252")  # bstring with a closing null
        pos += 1 + n
        recs = [struct.unpack_from("<QII", d, pos + k * 16) for k in range(count)]
        pos += count * 16
        folder_files.append((name, recs))
    names = d[pos:].split(b"\0", files)[:files]
    out, k = {}, 0
    for folder, recs in folder_files:
        for _, size, offset in recs:
            compressed = compressed_default ^ bool(size & 0x40000000)
            out[(folder + "\\" + names[k].decode("cp1252")).lower()] = (offset, size & 0x3FFFFFFF, compressed)
            k += 1
    return out, d, embed_names


def read(path, inner):
    table, d, embed = entries(path)
    offset, size, compressed = table[inner.lower().replace("/", "\\")]
    if embed:
        n = d[offset]
        offset, size = offset + 1 + n, size - 1 - n
    data = d[offset:offset + size]
    return lz4.frame.decompress(data[4:]) if compressed else data


if __name__ == "__main__":
    cmd, archive = sys.argv[1], sys.argv[2]
    if cmd == "list":
        text = sys.argv[3].lower() if len(sys.argv) > 3 else ""
        for p in sorted(entries(archive)[0]):
            if text in p:
                print(p)
    elif cmd == "extract":
        with open(sys.argv[4], "wb") as f:
            f.write(read(archive, sys.argv[3]))
