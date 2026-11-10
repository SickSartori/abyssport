"""A path made from a name read out of a dump (a PFS index, a PKG table, a FAT16 image), kept inside the folder
being written: a crafted or damaged dump must not write anywhere else (abyssport's, not Project Nativ's)."""
import os


def inside(out_dir: str, name: str) -> str:
    """out_dir/name, '/'-separated; ValueError when the name is empty, absolute, or steps out of out_dir"""
    parts = name.replace("\\", "/").split("/")
    if not name or "\0" in name or any(p in ("", ".", "..") or ":" in p for p in parts):
        raise ValueError(f"unsafe path in the dump: {name!r}")
    root = os.path.realpath(out_dir)
    dest = os.path.realpath(os.path.join(root, *parts))
    if os.path.commonpath([dest, root]) != root:
        raise ValueError(f"unsafe path in the dump: {name!r}")
    return dest


if __name__ == "__main__":   # self-check
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        assert inside(d, "sce_sys/param.sfo") == os.path.join(os.path.realpath(d), "sce_sys", "param.sfo")
        for bad in ("", "..", "../x", "a/../../x", "/etc/x", "C:/x", "a\\..\\..\\x", "a//b", "a/./b", "x\0y"):
            try: inside(d, bad)
            except ValueError: continue
            raise AssertionError(bad)
    print("safepath: ok")
